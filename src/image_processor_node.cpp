// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_cv/image_processor_node.hpp"

#include <cstring>
#include <filesystem>

#include "topsbot_cv/imgproc.hpp"
#include "BufferAllocator/BufferAllocatorWrapper.h"
#include "topsbot_cv/format_convert.hpp"
#include "topsbot_cv/pipeline_executor.hpp"
#include "topsbot_cv/pipeline_log.hpp"

namespace topsbot_cv
{

namespace
{
std::string encoding_to_string(const std::array<uint8_t, 12> & raw)
{
  const char * cstr = reinterpret_cast<const char *>(raw.data());
  return std::string(cstr, strnlen(cstr, raw.size()));
}

const char * input_msg_label(const InputMsg msg)
{
  switch (msg) {
    case InputMsg::File:
      return "file";
    case InputMsg::TbJpeg:
      return "tb_jpeg";
    case InputMsg::TbImg:
      return "tb_img";
    case InputMsg::SensorCompressed:
      return "sensor_compressed";
    case InputMsg::SensorImage:
    default:
      return "sensor_image";
  }
}

/// Match usb_cam and other drivers that publish CompressedImage with reliable QoS.
rclcpp::QoS subscription_qos()
{
  return rclcpp::QoS(10);
}
}  // namespace

ImageProcessorNode::ImageProcessorNode(
  const rclcpp::NodeOptions & options,
  const std::string & node_name)
: Node(
    node_name,
    rclcpp::NodeOptions(options).automatically_declare_parameters_from_overrides(true))
{
  {
    const char * be = std::getenv("FASTDDS_DMABUF_BACKEND");
    dmabuf_mode_ = (be != nullptr && strcmp(be, "1") == 0);
  }
  RCLCPP_INFO(get_logger(), "topsbot_cv image_processor: node created (dmabuf_mode=%s)",
              dmabuf_mode_ ? "ON" : "OFF");
}

void ImageProcessorNode::schedule_initialize()
{
  init_timer_ = create_wall_timer(
    std::chrono::milliseconds(1),
    [this]() {
      if (init_timer_) {
        init_timer_->cancel();
      }
      initialize();
    });
}

void ImageProcessorNode::initialize()
{
  if (initialized_) {
    return;
  }

  RCLCPP_INFO(get_logger(), "topsbot_cv image_processor: loading parameters");
  params_ = load_node_params(*this);

  RCLCPP_INFO(
    get_logger(),
    "parameters ready: input_msg=%s input_topic=%s "
    "publish_output=%s publish_zero_copy=%s output_topic=%s",
    input_msg_label(params_.input_msg),
    params_.input_topic.c_str(),
    params_.publish_output ? "true" : "false",
    params_.publish_zero_copy ? "true" : "false",
    params_.output_topic.c_str());
  RCLCPP_INFO(
    get_logger(), "pipeline config: %s",
    pipeline_config_summary(params_.pipeline_config).c_str());

  setup_subscriptions();

  if (params_.publish_output && !params_.publish_zero_copy) {
    image_pub_ = create_publisher<sensor_msgs::msg::Image>(params_.output_topic, 10);
    RCLCPP_INFO(
      get_logger(), "sensor_msgs publisher created on %s", params_.output_topic.c_str());
  } else if (params_.publish_output && params_.publish_zero_copy) {
    RCLCPP_INFO(
      get_logger(),
      "tb_img zero-copy publisher will be created on first frame (%s)",
      params_.output_topic.c_str());
  }

  initialized_ = true;
  RCLCPP_INFO(get_logger(), "topsbot_cv image_processor: ready");
}

void ImageProcessorNode::setup_subscriptions()
{
  if (params_.input_msg == InputMsg::SensorImage) {
    RCLCPP_INFO(
      get_logger(),
      "subscription: sensor_msgs/Image (nv12/jpeg) on %s",
      params_.input_topic.c_str());
    image_sub_ = create_subscription<sensor_msgs::msg::Image>(
      params_.input_topic, subscription_qos(),
      std::bind(&ImageProcessorNode::on_sensor_image, this, std::placeholders::_1));
    return;
  }

  if (params_.input_msg == InputMsg::SensorCompressed) {
    RCLCPP_INFO(
      get_logger(),
      "subscription: sensor_msgs/CompressedImage (mjpeg/jpeg) on %s",
      params_.input_topic.c_str());
    compressed_sub_ = create_subscription<sensor_msgs::msg::CompressedImage>(
      params_.input_topic, subscription_qos(),
      std::bind(&ImageProcessorNode::on_compressed_image, this, std::placeholders::_1));
    return;
  }

  if (params_.input_msg == InputMsg::TbJpeg) {
    RCLCPP_INFO(
      get_logger(),
      "subscription: TbJpegFrame zero-copy on %s",
      params_.input_topic.c_str());
    tb_jpeg_sub_ = create_subscription<tb_img_msgs::msg::TbJpegFrame>(
      params_.input_topic, rclcpp::SensorDataQoS(),
      std::bind(&ImageProcessorNode::on_tb_jpeg, this, std::placeholders::_1));
    return;
  }

  if (params_.input_msg == InputMsg::TbImg) {
    std::string profile = params_.tb_img_profile;
    if (profile == "auto") {
      RCLCPP_WARN(
        get_logger(),
        "tb_img_profile=auto: defaulting subscription to 480p; "
        "set tb_img_profile explicitly or resolve it in launch from input resolution");
      profile = "480p";
    }
    RCLCPP_INFO(
      get_logger(),
      "subscription: tb_img zero-copy on %s (profile=%s)",
      params_.input_topic.c_str(),
      profile.c_str());
    const auto qos = rclcpp::SensorDataQoS();
    if (profile == "540p") {
      tb_sub_540_ = create_subscription<tb_img_msgs::msg::TbMsg540P>(
        params_.input_topic, qos,
        std::bind(&ImageProcessorNode::on_tb_img_540, this, std::placeholders::_1));
    } else if (profile == "1080p") {
      tb_sub_1080_ = create_subscription<tb_img_msgs::msg::TbMsg1080P>(
        params_.input_topic, qos,
        std::bind(&ImageProcessorNode::on_tb_img_1080, this, std::placeholders::_1));
    } else {
      tb_sub_480_ = create_subscription<tb_img_msgs::msg::TbMsg480P>(
        params_.input_topic, qos,
        std::bind(&ImageProcessorNode::on_tb_img_480, this, std::placeholders::_1));
    }
    return;
  }

  if (params_.input_msg == InputMsg::File) {
    if (params_.image_file.empty()) {
      RCLCPP_ERROR(get_logger(), "input_msg=file requires non-empty image_file");
      return;
    }
    if (!std::filesystem::exists(params_.image_file)) {
      RCLCPP_ERROR(get_logger(), "image_file does not exist: %s", params_.image_file.c_str());
      return;
    }
    if (params_.publish_rate <= 0.0) {
      file_timer_ = create_wall_timer(
        std::chrono::milliseconds(1),
        [this]() {this->on_file_timer_once();});
      return;
    }
    const auto period_ms = static_cast<int64_t>(1000.0 / params_.publish_rate);
    file_timer_ = create_wall_timer(
      std::chrono::milliseconds(period_ms),
      std::bind(&ImageProcessorNode::on_file_timer, this));
  }
}

void ImageProcessorNode::on_file_timer_once()
{
  if (file_once_done_) {
    return;
  }
  file_once_done_ = true;
  if (file_timer_) {
    file_timer_->cancel();
  }
  on_file_timer();
}

bool ImageProcessorNode::upload_tb_nv12(
  const uint8_t * data,
  const uint32_t data_size,
  const int width,
  const int height,
  const std::array<uint8_t, 12> & encoding)
{
  const std::string enc = encoding_to_string(encoding);
  if (enc != "nv12" && enc != "yuv420") {
    RCLCPP_ERROR_THROTTLE(
      get_logger(), *get_clock(), 5000,
      "tb_img encoding '%s' not supported (need nv12 from mjpeg2nv12; "
      "use TbJpegFrame for MJPEG or mjpeg2nv12 for /tbmem_img)",
      enc.c_str());
    return false;
  }

  const size_t expected =
    static_cast<size_t>(width) * static_cast<size_t>(height) * 3U / 2U;
  if (data_size < expected) {
    RCLCPP_ERROR_THROTTLE(
      get_logger(), *get_clock(), 5000,
      "tb_img NV12 payload too small: %u bytes for %dx%d",
      data_size, width, height);
    return false;
  }

  const bool need_alloc = input_nv12_.fd() < 0 ||
    input_nv12_width_ != width || input_nv12_height_ != height;
  if (need_alloc) {
    input_nv12_ = TaImage::create_nv12(allocator_, width, height);
    if (input_nv12_.fd() < 0) {
      RCLCPP_ERROR_THROTTLE(
        get_logger(), *get_clock(), 5000,
        "failed to allocate NV12 dmabuf for tb_img input %dx%d", width, height);
      return false;
    }
    input_nv12_width_ = width;
    input_nv12_height_ = height;
  }

  if (!input_nv12_.upload_nv12(data, width, height)) {
    RCLCPP_ERROR_THROTTLE(
      get_logger(), *get_clock(), 5000,
      "failed to upload tb_img NV12 %dx%d", width, height);
    return false;
  }
  return true;
}

void ImageProcessorNode::on_tb_img_480(const tb_img_msgs::msg::TbMsg480P::ConstSharedPtr & msg)
{
  std_msgs::msg::Header header;
  header.stamp = msg->time_stamp;
  if (dmabuf_mode_ && params_.publish_zero_copy &&
      try_process_tb_img_zero_copy(
        msg->data.data(), msg->data_size,
        static_cast<int>(msg->width), static_cast<int>(msg->height),
        msg->phys_addr, header))
  {
    return;
  }
  if (!upload_tb_nv12(
      msg->data.data(), msg->data_size,
      static_cast<int>(msg->width), static_cast<int>(msg->height), msg->encoding))
  {
    return;
  }
  if (process_nv12(input_nv12_, input_nv12_width_, input_nv12_height_)) {
    publish_output(working_output_, header);
  }
}

void ImageProcessorNode::on_tb_img_540(const tb_img_msgs::msg::TbMsg540P::ConstSharedPtr & msg)
{
  std_msgs::msg::Header header;
  header.stamp = msg->time_stamp;
  if (dmabuf_mode_ && params_.publish_zero_copy &&
      try_process_tb_img_zero_copy(
        msg->data.data(), msg->data_size,
        static_cast<int>(msg->width), static_cast<int>(msg->height),
        msg->phys_addr, header))
  {
    return;
  }
  if (!upload_tb_nv12(
      msg->data.data(), msg->data_size,
      static_cast<int>(msg->width), static_cast<int>(msg->height), msg->encoding))
  {
    return;
  }
  if (process_nv12(input_nv12_, input_nv12_width_, input_nv12_height_)) {
    publish_output(working_output_, header);
  }
}

void ImageProcessorNode::on_tb_img_1080(const tb_img_msgs::msg::TbMsg1080P::ConstSharedPtr & msg)
{
  std_msgs::msg::Header header;
  header.stamp = msg->time_stamp;
  if (dmabuf_mode_ && params_.publish_zero_copy &&
      try_process_tb_img_zero_copy(
        msg->data.data(), msg->data_size,
        static_cast<int>(msg->width), static_cast<int>(msg->height),
        msg->phys_addr, header))
  {
    return;
  }
  if (!upload_tb_nv12(
      msg->data.data(), msg->data_size,
      static_cast<int>(msg->width), static_cast<int>(msg->height), msg->encoding))
  {
    return;
  }
  if (process_nv12(input_nv12_, input_nv12_width_, input_nv12_height_)) {
    publish_output(working_output_, header);
  }
}

void ImageProcessorNode::on_tb_jpeg(const tb_img_msgs::msg::TbJpegFrame::ConstSharedPtr & msg)
{
  if (msg->data_size == 0) {
    RCLCPP_WARN(get_logger(), "TbJpegFrame data_size is 0, skip");
    return;
  }

  RCLCPP_INFO_THROTTLE(
    get_logger(), *get_clock(), 3000,
    "TbJpegFrame: size=%u phys_addr=0x%llx", msg->data_size,
    static_cast<unsigned long long>(msg->phys_addr));
  auto nv12 = ImageSource::from_jpeg_bytes(
    allocator_, msg->data.data(), msg->data_size,
    static_cast<int>(msg->width), static_cast<int>(msg->height),
    msg->phys_addr);
  RCLCPP_INFO_THROTTLE(
    get_logger(), *get_clock(), 3000,
    "decode result: %s (phys=0x%llx)", nv12 ? "OK" : "FAIL",
    static_cast<unsigned long long>(msg->phys_addr));
  if (!nv12) {
    RCLCPP_ERROR(get_logger(), "failed to decode TbJpegFrame (%u bytes)", msg->data_size);
    return;
  }
  std_msgs::msg::Header header;
  header.stamp = msg->time_stamp;

  // dmabuf fast path: the PP stage writes directly into the loaned TbMsg
  // payload (zero-copy). Requires the dmabuf backend + publish settings +
  // NV12 output + enough capacity; otherwise fall back to the normal path.
  if (dmabuf_mode_ && params_.publish_zero_copy &&
      try_publish_zero_copy(nv12->image, nv12->width, nv12->height, header))
  {
    return;
  }

  if (process_nv12(nv12->image, nv12->width, nv12->height)) {
    publish_output(working_output_, header);
  }
}

void ImageProcessorNode::on_sensor_image(const sensor_msgs::msg::Image::ConstSharedPtr & msg)
{
  auto nv12 = ImageSource::from_sensor_image(allocator_, *msg);
  if (!nv12) {
    RCLCPP_ERROR(
      get_logger(),
      "unsupported sensor_msgs encoding '%s' (supported: nv12, jpeg/mjpeg)",
      msg->encoding.c_str());
    return;
  }
  if (process_nv12(nv12->image, nv12->width, nv12->height)) {
    publish_output(working_output_, msg->header);
  }
}

void ImageProcessorNode::on_compressed_image(
  const sensor_msgs::msg::CompressedImage::ConstSharedPtr & msg)
{
  auto nv12 = ImageSource::from_compressed_image(allocator_, *msg);
  if (!nv12) {
    RCLCPP_ERROR(
      get_logger(),
      "unsupported CompressedImage format '%s' (supported: jpeg/mjpeg)",
      msg->format.c_str());
    return;
  }
  if (process_nv12(nv12->image, nv12->width, nv12->height)) {
    publish_output(working_output_, msg->header);
  }
}

void ImageProcessorNode::on_file_timer()
{
  const auto total_start = SteadyClock::now();
  RCLCPP_INFO(get_logger(), "file mode: loading %s", params_.image_file.c_str());

  if (!allocator_.get()) {
    RCLCPP_ERROR(get_logger(), "failed to create dmabuf heap allocator");
    return;
  }

  const auto decode_start = SteadyClock::now();
  auto nv12 = ImageSource::from_file(allocator_, params_.image_file);
  const auto decode_end = SteadyClock::now();
  if (!nv12) {
    RCLCPP_ERROR(get_logger(), "failed to load image_file: %s", params_.image_file.c_str());
    return;
  }

  std_msgs::msg::Header header;
  header.stamp = now();
  header.frame_id = "file";

  const auto pipeline_start = SteadyClock::now();
  if (!process_nv12(nv12->image, nv12->width, nv12->height)) {
    RCLCPP_ERROR(get_logger(), "file mode: pipeline failed");
    return;
  }
  const auto pipeline_end = SteadyClock::now();

  double dump_ms = 0.0;
  std::string dump_path;
  if (params_.need_dump) {
    std::filesystem::create_directories(params_.dump_dir);
    const std::string dump_base = params_.dump_dir + "/" + params_.dump_prefix +
      std::to_string(dump_index_++);
    const auto dump_start = SteadyClock::now();
    if (image_dump_to_file(allocator_, working_output_, dump_base, dump_path)) {
      dump_ms = elapsed_ms(dump_start, SteadyClock::now());
    } else {
      RCLCPP_ERROR(
        get_logger(), "failed to dump output format %s",
        image_format_name(output_format_).c_str());
      dump_path.clear();
    }
  }

  if (params_.publish_output) {
    publish_output(working_output_, header);
  }

  PipelineTiming timing;
  timing.decode_ms = elapsed_ms(decode_start, decode_end);
  timing.pipeline_ms = elapsed_ms(pipeline_start, pipeline_end);
  timing.dump_ms = dump_ms;
  timing.total_ms = elapsed_ms(total_start, SteadyClock::now());

  RCLCPP_INFO(
    get_logger(),
    "file mode: source %dx%d, decode %.2f ms, pipeline %dx%d %s %.2f ms, total %.2f ms",
    nv12->width,
    nv12->height,
    timing.decode_ms,
    output_width_,
    output_height_,
    image_format_name(output_format_).c_str(),
    timing.pipeline_ms,
    timing.total_ms);
  if (!dump_path.empty()) {
    RCLCPP_INFO(
      get_logger(), "file mode: dump %.2f ms -> %s", timing.dump_ms, dump_path.c_str());
  }
}

bool ImageProcessorNode::process_nv12(const TaImage & input, const int width, const int height)
{
  if (!execute_pipeline(
      allocator_, input, width, height, params_.pipeline_config,
      working_output_, output_width_, output_height_))
  {
    RCLCPP_ERROR(get_logger(), "pipeline execution failed");
    return false;
  }
  output_format_ = working_output_.format();
  return true;
}

bool ImageProcessorNode::try_process_tb_img_zero_copy(
  const uint8_t * data, const uint32_t data_size, const int width, const int height,
  const uint64_t phys_addr, const std_msgs::msg::Header & header)
{
  if (width <= 0 || height <= 0 || phys_addr == 0 || !allocator_.get()) {
    return false;
  }
  // Input: wrap the upstream loaned payload via LOOKUP (skip the
  // upload_nv12 CPU copy)
  uint64_t view_len = data_size;
  const int in_fd = DmabufHeapLookupByPhys(allocator_.get(), phys_addr, &view_len);
  if (in_fd < 0) {
    return false;
  }
  ta_image_t native{};
  if (ta_cv_image_create_ext(height, width, FORMAT_NV12, &native, in_fd) != 0) {
    close(in_fd);
    return false;
  }
  TaImage input = TaImage::adopt_nv12(in_fd, width, height, data_size, native);
  if (input.fd() < 0) {
    return false;
  }
  RCLCPP_INFO_THROTTLE(
    get_logger(), *get_clock(), 3000,
    "zero-copy input: LOOKUP phys=0x%llx (%dx%d)",
    static_cast<unsigned long long>(phys_addr), width, height);
  // Output: PP writes the loaned payload directly (zero-copy)
  return try_publish_zero_copy(input, width, height, header);
}

bool ImageProcessorNode::try_publish_zero_copy(
  const TaImage & input,
  const int width,
  const int height,
  const std_msgs::msg::Header & header)
{
  if (!params_.publish_output || !allocator_.get()) {
    return false;
  }

  // Predict the PP output size (same as execute_pipeline)
  int out_w = 0;
  int out_h = 0;
  if (!predict_output_size(params_.pipeline_config, width, height, out_w, out_h)) {
    return false;
  }
  if (out_w <= 0 || out_h <= 0) {
    return false;
  }
  const uint32_t out_size = static_cast<uint32_t>(out_w) * out_h * 3 / 2;
  if (out_size == 0) {
    return false;
  }

  // Non-NV12 outputs (e.g. csc to RGB) cannot fit a TbMsg -> fall back
  if (params_.pipeline_config.features.convert_padding) {
    // convert_padding outputs NV12, usable
  } else if (params_.pipeline_config.features.csc) {
    return false;
  }

  if (!ensure_tb_img_publisher_for(out_w, out_h)) {
    return false;
  }
  if (!tb_img_pub_) {
    return false;
  }

  // borrow payload -> touch pages (pagemap needs present PTEs) -> LOOKUP
  // -> PP writes -> publish
  const bool ok = tb_img_pub_->borrow_fill_publish(
    [&](auto & msg) -> bool {
      // touch：PP（DMA）写不建 CPU 页表，pagemap 反推前必须 CPU 触碰
      {
        volatile uint8_t * base = msg.data.data();
        for (size_t off = 0; off < out_size; off += 4096) {
          (void)base[off];
        }
      }
      uint64_t m1_phys = 0;
      DmabufHeapLookupPhysByPtr(allocator_.get(), msg.data.data(), &m1_phys);
      if (m1_phys == 0) {
        return false;
      }
      uint64_t view_len = out_size;
      const int out_fd = DmabufHeapLookupByPhys(allocator_.get(), m1_phys, &view_len);
      if (out_fd < 0) {
        return false;
      }

      // Wrap the external fd into a ta_image_t (SDK external-memory
      // interface: ta_cv_image_create_ext) for the PP stage output
      ta_image_t native{};
      if (ta_cv_image_create_ext(out_h, out_w, FORMAT_NV12, &native, out_fd) != 0) {
        close(out_fd);
        return false;
      }
      TaImage out_img = TaImage::adopt_nv12(
        out_fd, out_w, out_h, out_size, native);

      // PP (crop/resize/csc/convert_padding) writes the payload directly
      int exec_w = 0;
      int exec_h = 0;
      if (!execute_pipeline(
          allocator_, input, width, height, params_.pipeline_config,
          out_img, exec_w, exec_h))
      {
        return false;
      }
      if (exec_w != out_w || exec_h != out_h) {
        RCLCPP_WARN(get_logger(), "pipeline output size mismatch (%dx%d vs %dx%d)",
                    exec_w, exec_h, out_w, out_h);
        return false;
      }

      // 填字段 + phys_addr
      msg.width = static_cast<uint32_t>(out_w);
      msg.height = static_cast<uint32_t>(out_h);
      msg.step = static_cast<uint32_t>(out_w);
      msg.data_size = out_size;
      fill_tb_img_encoding(msg.encoding, "nv12");
      msg.time_stamp = header.stamp;
      msg.index = tb_frame_index_++;
      msg.phys_addr = m1_phys;
      return true;
    });

  if (ok) {
    RCLCPP_INFO_THROTTLE(get_logger(), *get_clock(), 3000,
                         "zero-copy publish: %dx%d -> loaned M1",
                         out_w, out_h);
  }
  return ok;
}

bool ImageProcessorNode::ensure_tb_img_publisher_for(const int out_w, const int out_h)
{
  if (tb_img_pub_ && output_width_ == out_w && output_height_ == out_h) {
    return true;
  }
  if (tb_img_pub_) {
    RCLCPP_WARN(get_logger(),
                "tb_img publisher profile mismatch (%dx%d vs %dx%d); recreate",
                output_width_, output_height_, out_w, out_h);
    tb_img_pub_.reset();
  }

  const uint32_t payload_size = static_cast<uint32_t>(out_w) * out_h * 3 / 2;
  TbImgProfile profile;
  if (params_.output_tb_img_profile == "auto") {
    profile = select_tb_img_profile(
      static_cast<uint32_t>(out_w), static_cast<uint32_t>(out_h), payload_size);
  } else {
    profile = tb_img_profile_from_string(params_.output_tb_img_profile);
  }
  if (payload_size > tb_img_profile_max_size(profile)) {
    RCLCPP_WARN(get_logger(), "output %ux%u (%u B) exceeds TbMsg profile capacity",
                out_w, out_h, payload_size);
    return false;
  }

  tb_img_pub_ = std::make_unique<LoanedTbImgPublisher>(this, params_.output_topic, profile);
  output_width_ = out_w;
  output_height_ = out_h;
  RCLCPP_INFO(get_logger(), "tb_img zero-copy publisher created on %s (%dx%d, %s)",
              params_.output_topic.c_str(), out_w, out_h,
              tb_img_profile_name(profile));
  return true;
}

void ImageProcessorNode::ensure_tb_img_publisher()
{
  if (tb_img_pub_ || output_width_ <= 0 || output_height_ <= 0) {
    return;
  }

  const auto payload_size = static_cast<uint32_t>(working_output_.byte_size());
  TbImgProfile profile;
  if (params_.output_tb_img_profile == "auto") {
    profile = select_tb_img_profile(
      static_cast<uint32_t>(output_width_),
      static_cast<uint32_t>(output_height_),
      payload_size);
  } else {
    profile = tb_img_profile_from_string(params_.output_tb_img_profile);
  }

  tb_img_pub_ = std::make_unique<LoanedTbImgPublisher>(this, params_.output_topic, profile);
  RCLCPP_INFO(
    get_logger(),
    "tb_img zero-copy publisher created on %s (%dx%d)",
    params_.output_topic.c_str(),
    output_width_, output_height_);
}

void ImageProcessorNode::publish_output(
  const TaImage & image,
  const std_msgs::msg::Header & header)
{
  if (!params_.publish_output || !image.mapped() || output_width_ <= 0 || output_height_ <= 0) {
    return;
  }

  if (params_.publish_zero_copy) {
    if (!working_output_.is_nv12()) {
      RCLCPP_ERROR_ONCE(
        get_logger(),
        "publish_zero_copy requires NV12 pipeline output (use sensor_msgs for rgb8/bgr8)");
      return;
    }
    ensure_tb_img_publisher();
    if (!tb_img_pub_) {
      return;
    }
    tb_img_pub_->publish_nv12(
      static_cast<const uint8_t *>(image.mapped()),
      static_cast<uint32_t>(image.byte_size()),
      static_cast<uint32_t>(output_width_),
      static_cast<uint32_t>(output_height_),
      header.stamp,
      tb_frame_index_,
      get_logger());
    return;
  }

  if (!image_pub_) {
    return;
  }

  const auto encoding = ros_encoding_for_format(image.format());
  if (!encoding) {
    RCLCPP_ERROR_ONCE(
      get_logger(),
      "unsupported pipeline output format for sensor_msgs publish");
    return;
  }

  sensor_msgs::msg::Image out;
  out.header = header;
  out.height = static_cast<uint32_t>(output_height_);
  out.width = static_cast<uint32_t>(output_width_);
  out.encoding = *encoding;
  out.is_bigendian = 0;
  if (image.is_nv12()) {
    out.step = static_cast<uint32_t>(output_width_);
  } else {
    out.step = static_cast<uint32_t>(output_width_) * 3U;
  }
  out.data.resize(image.byte_size());
  std::memcpy(out.data.data(), image.mapped(), image.byte_size());
  image_pub_->publish(out);
}

}  // namespace topsbot_cv

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(topsbot_cv::ImageProcessorNode)

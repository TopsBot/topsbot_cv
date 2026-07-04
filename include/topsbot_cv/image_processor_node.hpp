// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__IMAGE_PROCESSOR_NODE_HPP_
#define TOPSBOT_CV__IMAGE_PROCESSOR_NODE_HPP_

#include <array>
#include <memory>
#include <optional>

#include <std_msgs/msg/header.hpp>
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/compressed_image.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "tb_img_msgs/msg/tb_jpeg_frame.hpp"
#include "tb_img_msgs/msg/tb_msg1080_p.hpp"
#include "tb_img_msgs/msg/tb_msg480_p.hpp"
#include "tb_img_msgs/msg/tb_msg540_p.hpp"

#include "topsbot_cv/dmabuf_allocator.hpp"
#include "topsbot_cv/image_source.hpp"
#include "topsbot_cv/loaned_tb_img_publisher.hpp"
#include "topsbot_cv/node_params.hpp"
#include "topsbot_cv/ta_image.hpp"

namespace topsbot_cv
{

class ImageProcessorNode : public rclcpp::Node
{
public:
  explicit ImageProcessorNode(
    const rclcpp::NodeOptions & options = rclcpp::NodeOptions(),
    const std::string & node_name = "topsbot_cv_image_processor");

  /// Load parameters and create pub/sub after executor is spinning.
  void schedule_initialize();

  /// Called by schedule_initialize(); do not call before spin().
  void initialize();

private:
  void setup_subscriptions();
  void on_sensor_image(const sensor_msgs::msg::Image::ConstSharedPtr & msg);
  void on_compressed_image(const sensor_msgs::msg::CompressedImage::ConstSharedPtr & msg);
  void on_tb_img_480(const tb_img_msgs::msg::TbMsg480P::ConstSharedPtr & msg);
  void on_tb_img_540(const tb_img_msgs::msg::TbMsg540P::ConstSharedPtr & msg);
  void on_tb_img_1080(const tb_img_msgs::msg::TbMsg1080P::ConstSharedPtr & msg);
  void on_tb_jpeg(const tb_img_msgs::msg::TbJpegFrame::ConstSharedPtr & msg);
  void on_file_timer();
  void on_file_timer_once();

  bool process_nv12(const TaImage & input, int width, int height);
  void publish_output(const TaImage & image, const std_msgs::msg::Header & header);
  void ensure_tb_img_publisher();
  bool upload_tb_nv12(
    const uint8_t * data,
    uint32_t data_size,
    int width,
    int height,
    const std::array<uint8_t, 12> & encoding);

  bool initialized_{false};
  NodeParams params_;

  bool file_once_done_{false};
  DmabufAllocator allocator_;
  TaImage input_nv12_;
  int input_nv12_width_{0};
  int input_nv12_height_{0};
  TaImage working_output_;
  int output_width_{0};
  int output_height_{0};
  ta_image_format_ext_t output_format_{FORMAT_NV12};
  uint32_t dump_index_{0};

  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
  rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr compressed_sub_;
  rclcpp::Subscription<tb_img_msgs::msg::TbMsg480P>::SharedPtr tb_sub_480_;
  rclcpp::Subscription<tb_img_msgs::msg::TbMsg540P>::SharedPtr tb_sub_540_;
  rclcpp::Subscription<tb_img_msgs::msg::TbMsg1080P>::SharedPtr tb_sub_1080_;
  rclcpp::Subscription<tb_img_msgs::msg::TbJpegFrame>::SharedPtr tb_jpeg_sub_;
  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
  std::unique_ptr<LoanedTbImgPublisher> tb_img_pub_;
  int32_t tb_frame_index_{0};
  rclcpp::TimerBase::SharedPtr init_timer_;
  rclcpp::TimerBase::SharedPtr file_timer_;
};

}  // namespace topsbot_cv

#endif  // TOPSBOT_CV__IMAGE_PROCESSOR_NODE_HPP_

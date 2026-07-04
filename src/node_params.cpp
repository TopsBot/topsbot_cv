// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_cv/node_params.hpp"

#include <cstring>

#include <algorithm>
#include <cctype>

#include "topsbot_cv/format_convert.hpp"
#include "topsbot_cv/nv12_fill.hpp"

namespace topsbot_cv
{

namespace
{
template<typename ParameterT>
ParameterT get_or_default(
  rclcpp::Node & node,
  const std::string & name,
  const ParameterT & default_value)
{
  if (!node.has_parameter(name)) {
    return default_value;
  }
  return node.get_parameter(name).get_value<ParameterT>();
}

std::string to_lower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
  return value;
}

constexpr int kTbImgMax480 = 921600;
constexpr int kTbImgMax540 = 1555200;

Rgb888 load_convert_padding_border(rclcpp::Node & node)
{
  const auto preset = get_or_default<std::string>(node, "convert_padding.border.preset", "");
  if (!preset.empty()) {
    if (const auto resolved = parse_convert_padding_border_preset(preset)) {
      return *resolved;
    }
  }

  const std::vector<int64_t> default_rgb{
    kDefaultConvertPaddingBorder.r,
    kDefaultConvertPaddingBorder.g,
    kDefaultConvertPaddingBorder.b};
  const auto rgb = get_or_default<std::vector<int64_t>>(node, "convert_padding.border.rgb", default_rgb);
  if (rgb.size() >= 3) {
    return Rgb888{
      static_cast<uint8_t>(std::max<int64_t>(0, std::min<int64_t>(255, rgb[0]))),
      static_cast<uint8_t>(std::max<int64_t>(0, std::min<int64_t>(255, rgb[1]))),
      static_cast<uint8_t>(std::max<int64_t>(0, std::min<int64_t>(255, rgb[2])))};
  }
  return kDefaultConvertPaddingBorder;
}
}  // namespace

InputMsg parse_input_msg(const std::string & value)
{
  const std::string v = to_lower(value);
  if (v == "file") {
    return InputMsg::File;
  }
  if (v == "tb_jpeg" || v == "subscription_jpeg_zc") {
    return InputMsg::TbJpeg;
  }
  if (v == "tb_img" || v == "subscription_zc" || v == "tbmem") {
    return InputMsg::TbImg;
  }
  if (v == "sensor_image" || v == "sensor") {
    return InputMsg::SensorImage;
  }
  if (v == "sensor_compressed" || v == "compressed") {
    return InputMsg::SensorCompressed;
  }
  if (v == "subscription" || v == "topic" || v == "ros") {
    return InputMsg::SensorImage;
  }
  return InputMsg::SensorImage;
}

InputMsg parse_legacy_input_mode(const std::string & input_mode, const bool input_compressed)
{
  if (to_lower(input_mode) == "subscription" && input_compressed) {
    return InputMsg::SensorCompressed;
  }
  if (to_lower(input_mode) == "file") {
    return InputMsg::File;
  }
  return parse_input_msg(input_mode);
}

std::string input_msg_to_string(const InputMsg msg)
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

std::string resolve_tb_img_profile(const std::string & profile, int width, int height)
{
  const std::string v = to_lower(profile);
  if (!v.empty() && v != "auto") {
    if (v == "tbmsg480p") {
      return "480p";
    }
    if (v == "tbmsg540p") {
      return "540p";
    }
    if (v == "tbmsg1080p") {
      return "1080p";
    }
    return v;
  }
  const int payload = std::max(width, 1) * std::max(height, 1) * 3;
  if (payload <= kTbImgMax480) {
    return "480p";
  }
  if (payload <= kTbImgMax540) {
    return "540p";
  }
  return "1080p";
}

NodeParams load_node_params(rclcpp::Node & node)
{
  NodeParams params;
  const std::string input_msg_str = get_or_default<std::string>(node, "input_msg", "");
  if (!input_msg_str.empty()) {
    params.input_msg = parse_input_msg(input_msg_str);
  } else {
    const auto input_mode_str = get_or_default<std::string>(node, "input_mode", "subscription");
    const bool input_compressed = get_or_default<bool>(node, "input_compressed", false);
    params.input_msg = parse_legacy_input_mode(input_mode_str, input_compressed);
    if (!input_mode_str.empty() || input_compressed) {
      RCLCPP_WARN_ONCE(
        node.get_logger(),
        "Deprecated input_mode / input_compressed; prefer input_msg "
        "(tb_jpeg | tb_img | sensor_image | sensor_compressed | file)");
    }
    const bool is_zero_copy = get_or_default<bool>(node, "is_zero_copy", false);
    if (
      params.input_msg == InputMsg::SensorImage && is_zero_copy &&
      input_mode_str != "subscription_jpeg_zc")
    {
      params.input_msg = InputMsg::TbImg;
    }
  }
  params.image_file = get_or_default<std::string>(node, "image_file", "");
  params.input_topic = get_or_default<std::string>(node, "input_topic", "/image");
  params.output_topic = get_or_default<std::string>(node, "output_topic", "/image_cv_out");
  params.tb_img_profile = get_or_default<std::string>(node, "tb_img_profile", "auto");
  params.output_tb_img_profile = get_or_default<std::string>(
    node, "output_tb_img_profile", "auto");
  params.publish_rate = get_or_default<double>(node, "publish_rate", 1.0);
  params.need_dump = get_or_default<bool>(node, "need_dump", false);
  params.publish_output = get_or_default<bool>(node, "publish_output", true);
  params.publish_zero_copy = get_or_default<bool>(node, "publish_zero_copy", false);
  params.dump_dir = get_or_default<std::string>(node, "dump_dir", "/tmp/topsbot_cv");
  params.dump_prefix = get_or_default<std::string>(node, "dump_prefix", "out_");

  auto & cfg = params.pipeline_config;
  cfg.crop_rect.start_x = get_or_default<int>(node, "crop.start_x", 0);
  cfg.crop_rect.start_y = get_or_default<int>(node, "crop.start_y", 0);
  cfg.crop_rect.crop_w = get_or_default<int>(node, "crop.crop_w", 640);
  cfg.crop_rect.crop_h = get_or_default<int>(node, "crop.crop_h", 360);

  std::memset(&cfg.resize_rect, 0, sizeof(cfg.resize_rect));
  cfg.resize_rect.start_x = get_or_default<int>(node, "resize.start_x", 0);
  cfg.resize_rect.start_y = get_or_default<int>(node, "resize.start_y", 0);
  cfg.resize_rect.in_width = get_or_default<int>(node, "resize.in_width", 0);
  cfg.resize_rect.in_height = get_or_default<int>(node, "resize.in_height", 0);
  cfg.resize_rect.out_width = get_or_default<int>(node, "resize.out_width", 640);
  cfg.resize_rect.out_height = get_or_default<int>(node, "resize.out_height", 360);
  cfg.bind_resize_attr();
  cfg.resize_attr.interpolation = static_cast<unsigned int>(
    get_or_default<int>(node, "resize.interpolation", 1));
  cfg.resize_attr.stretch_fit = get_or_default<int>(node, "resize.stretch_fit", 0);

  cfg.convert_padding_dst_width = get_or_default<int>(node, "convert_padding.dst_width", 640);
  cfg.convert_padding_dst_height = get_or_default<int>(node, "convert_padding.dst_height", 640);
  cfg.convert_padding_crop_rect.start_x = get_or_default<int>(node, "convert_padding.crop.start_x", 0);
  cfg.convert_padding_crop_rect.start_y = get_or_default<int>(node, "convert_padding.crop.start_y", 0);
  cfg.convert_padding_crop_rect.crop_w = get_or_default<int>(node, "convert_padding.crop.crop_w", 0);
  cfg.convert_padding_crop_rect.crop_h = get_or_default<int>(node, "convert_padding.crop.crop_h", 0);
  cfg.convert_padding_attr.dst_crop_stx = static_cast<unsigned int>(
    get_or_default<int>(node, "convert_padding.padding.dst_crop_stx", 0));
  cfg.convert_padding_attr.dst_crop_sty = static_cast<unsigned int>(
    get_or_default<int>(node, "convert_padding.padding.dst_crop_sty", 0));
  cfg.convert_padding_attr.dst_crop_w = static_cast<unsigned int>(
    get_or_default<int>(node, "convert_padding.padding.dst_crop_w", 640));
  cfg.convert_padding_attr.dst_crop_h = static_cast<unsigned int>(
    get_or_default<int>(node, "convert_padding.padding.dst_crop_h", 640));
  cfg.convert_padding_algorithm = static_cast<ta_cv_resize_algorithm_t>(
    get_or_default<int>(node, "convert_padding.algorithm", 1));
  cfg.convert_padding_border = load_convert_padding_border(node);

  const auto csc_type_str = get_or_default<std::string>(node, "csc.csc_type", "YCbCr2RGB_BT601");
  if (const auto parsed = parse_csc_type(csc_type_str)) {
    cfg.csc_type = *parsed;
  }
  const auto dst_fmt_str = get_or_default<std::string>(node, "csc.dst_format", "rgb_packed");
  if (const auto parsed = parse_image_format(dst_fmt_str)) {
    cfg.csc_dst_format = *parsed;
  }

  const std::vector<std::string> default_pipeline{"crop"};
  for (const auto & op_name : get_or_default<std::vector<std::string>>(
      node, "pipeline", default_pipeline))
  {
    apply_pipeline_feature(cfg, op_name);
  }
  if (!cfg.features.convert_padding &&
    !cfg.features.crop && !cfg.features.resize && !cfg.features.csc)
  {
    cfg.features.crop = true;
  }

  return params;
}

}  // namespace topsbot_cv

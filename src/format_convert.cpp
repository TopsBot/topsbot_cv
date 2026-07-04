// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_cv/format_convert.hpp"

#include <algorithm>
#include <cctype>

namespace topsbot_cv
{

namespace
{
std::string to_lower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
  return value;
}
}  // namespace

std::optional<ta_image_format_ext_t> parse_image_format(const std::string & name)
{
  const auto lower = to_lower(name);
  if (lower == "nv12" || lower == "yuv420") {
    return FORMAT_NV12;
  }
  if (lower == "nv21") {
    return FORMAT_NV21;
  }
  if (lower == "rgb_packed" || lower == "rgb8" || lower == "rgb") {
    return FORMAT_RGB_PACKED;
  }
  if (lower == "bgr_packed" || lower == "bgr8" || lower == "bgr") {
    return FORMAT_BGR_PACKED;
  }
  if (lower == "rgb_planar") {
    return FORMAT_RGB_PLANAR;
  }
  if (lower == "bgr_planar") {
    return FORMAT_BGR_PLANAR;
  }
  if (lower == "gray" || lower == "mono8") {
    return FORMAT_GRAY;
  }
  if (lower == "argb_packed" || lower == "argb") {
    return FORMAT_ARGB_PACKED;
  }
  if (lower == "abgr_packed" || lower == "abgr") {
    return FORMAT_ABGR_PACKED;
  }
  return std::nullopt;
}

std::optional<csc_type_t> parse_csc_type(const std::string & name)
{
  const auto lower = to_lower(name);
  if (lower == "none") {
    return CSC_NONE;
  }
  if (lower == "ycbcr2rgb_bt601" || lower == "yuv2rgb_bt601") {
    return CSC_YCbCr2RGB_BT601;
  }
  if (lower == "rgb2ycbcr_bt601" || lower == "rgb2yuv_bt601") {
    return CSC_RGB2YCbCr_BT601;
  }
  if (lower == "ycbcr2rgb_bt709" || lower == "yuv2rgb_bt709") {
    return CSC_YCbCr2RGB_BT709;
  }
  if (lower == "rgb2ycbcr_bt709" || lower == "rgb2yuv_bt709") {
    return CSC_RGB2YCbCr_BT709;
  }
  if (lower == "ycbcr2rgb_bt2020" || lower == "yuv2rgb_bt2020") {
    return CSC_YCbCr2RGB_BT2020;
  }
  if (lower == "rgb2ycbcr_bt2020" || lower == "rgb2yuv_bt2020") {
    return CSC_RGB2YCbCr_BT2020;
  }
  return std::nullopt;
}

size_t byte_size_for_format(const ta_image_format_ext_t format, const int width, const int height)
{
  if (width <= 0 || height <= 0) {
    return 0;
  }
  const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
  switch (format) {
    case FORMAT_NV12:
    case FORMAT_NV21:
      return pixels * 3U / 2U;
    case FORMAT_RGB_PACKED:
    case FORMAT_BGR_PACKED:
    case FORMAT_RGB_PLANAR:
    case FORMAT_BGR_PLANAR:
      return pixels * 3U;
    case FORMAT_ARGB_PACKED:
    case FORMAT_ABGR_PACKED:
      return pixels * 4U;
    case FORMAT_GRAY:
      return pixels;
    default:
      return 0;
  }
}

std::optional<std::string> ros_encoding_for_format(const ta_image_format_ext_t format)
{
  switch (format) {
    case FORMAT_NV12:
      return "nv12";
    case FORMAT_RGB_PACKED:
      return "rgb8";
    case FORMAT_BGR_PACKED:
      return "bgr8";
    default:
      return std::nullopt;
  }
}

int align_down16(const int value)
{
  return value & ~15;
}

}  // namespace topsbot_cv

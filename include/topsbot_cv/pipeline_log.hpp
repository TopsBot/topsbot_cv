// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__PIPELINE_LOG_HPP_
#define TOPSBOT_CV__PIPELINE_LOG_HPP_

#include <chrono>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "topsbot_cv/format_convert.hpp"
#include "topsbot_cv/pipeline_executor.hpp"

namespace topsbot_cv
{

using SteadyClock = std::chrono::steady_clock;
using SteadyTimePoint = SteadyClock::time_point;

inline double elapsed_ms(const SteadyTimePoint & start, const SteadyTimePoint & end)
{
  return std::chrono::duration<double, std::milli>(end - start).count();
}

inline std::string image_format_name(const ta_image_format_ext_t format)
{
  switch (format) {
    case FORMAT_YUV420P:
      return "yuv420p";
    case FORMAT_NV12:
      return "nv12";
    case FORMAT_NV21:
      return "nv21";
    case FORMAT_RGB_PACKED:
      return "rgb_packed";
    case FORMAT_BGR_PACKED:
      return "bgr_packed";
    case FORMAT_RGB_PLANAR:
      return "rgb_planar";
    case FORMAT_BGR_PLANAR:
      return "bgr_planar";
    case FORMAT_GRAY:
      return "gray";
    case FORMAT_ARGB_PACKED:
      return "argb_packed";
    case FORMAT_ABGR_PACKED:
      return "abgr_packed";
    default:
      return "unknown";
  }
}

inline std::string pipeline_features_label(const PipelineFeatures & features)
{
  std::vector<std::string> enabled;
  if (features.crop) {
    enabled.push_back("crop");
  }
  if (features.resize) {
    enabled.push_back("resize");
  }
  if (features.csc) {
    enabled.push_back("csc");
  }
  if (features.convert_padding) {
    enabled.push_back("convert_padding");
  }
  if (enabled.empty()) {
    return "none";
  }
  std::ostringstream stream;
  for (size_t i = 0; i < enabled.size(); ++i) {
    if (i > 0) {
      stream << '+';
    }
    stream << enabled[i];
  }
  return stream.str();
}

inline std::string pipeline_config_summary(const PipelineConfig & config)
{
  std::ostringstream stream;
  stream << "features=" << pipeline_features_label(config.features);
  if (config.features.crop) {
    stream << " crop=(" << config.crop_rect.start_x << ',' << config.crop_rect.start_y << ','
           << config.crop_rect.crop_w << 'x' << config.crop_rect.crop_h << ')';
  }
  if (config.features.resize) {
    stream << " resize_out=" << config.resize_rect.out_width << 'x'
           << config.resize_rect.out_height;
  }
  if (config.features.csc) {
    stream << " csc_dst=" << image_format_name(config.csc_dst_format);
  }
  if (config.features.convert_padding) {
    stream << " padding_dst=" << config.convert_padding_dst_width << 'x'
           << config.convert_padding_dst_height
           << " border_rgb=(" << static_cast<int>(config.convert_padding_border.r) << ','
           << static_cast<int>(config.convert_padding_border.g) << ','
           << static_cast<int>(config.convert_padding_border.b) << ')';
  }
  return stream.str();
}

struct PipelineTiming
{
  double decode_ms{0.0};
  double pipeline_ms{0.0};
  double dump_ms{0.0};
  double total_ms{0.0};
};

inline void print_pipeline_timing(
  std::ostream & out,
  const std::string & input_label,
  const int src_w,
  const int src_h,
  const int out_w,
  const int out_h,
  const ta_image_format_ext_t out_format,
  const PipelineTiming & timing,
  const std::string & output_path = {})
{
  out << std::fixed << std::setprecision(2);
  out << "[topsbot_cv] input: " << input_label << '\n';
  out << "[topsbot_cv] source: " << src_w << 'x' << src_h
      << ", decode " << timing.decode_ms << " ms\n";
  out << "[topsbot_cv] pipeline: " << out_w << 'x' << out_h << ' '
      << image_format_name(out_format) << ", " << timing.pipeline_ms << " ms\n";
  if (!output_path.empty()) {
    out << "[topsbot_cv] dump: " << output_path << ", " << timing.dump_ms << " ms\n";
  }
  out << "[topsbot_cv] total: " << timing.total_ms << " ms\n";
}

}  // namespace topsbot_cv

#endif  // TOPSBOT_CV__PIPELINE_LOG_HPP_

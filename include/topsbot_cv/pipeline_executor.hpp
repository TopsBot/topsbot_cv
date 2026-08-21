// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__PIPELINE_EXECUTOR_HPP_
#define TOPSBOT_CV__PIPELINE_EXECUTOR_HPP_

#include <string>
#include <vector>

#include "ta_cv_api_ext_c.h"
#include "topsbot_cv/dmabuf_allocator.hpp"
#include "topsbot_cv/nv12_fill.hpp"
#include "topsbot_cv/ta_image.hpp"

namespace topsbot_cv
{

/// Enabled capabilities; list order in YAML is ignored.
struct PipelineFeatures
{
  bool crop{false};
  bool resize{false};
  bool csc{false};
  bool convert_padding{false};
};

struct PipelineConfig
{
  PipelineFeatures features;

  ta_cv_rect_t crop_rect{};
  ta_cv_resize_t resize_rect{};
  ta_cv_resize_image_t resize_attr{};
  ta_image_format_ext_t csc_dst_format{FORMAT_RGB_PACKED};
  csc_type_t csc_type{CSC_YCbCr2RGB_BT601};

  int convert_padding_dst_width{640};
  int convert_padding_dst_height{640};
  ta_cv_rect_t convert_padding_crop_rect{};
  ta_cv_padding_attr_t convert_padding_attr{};
  ta_cv_resize_algorithm_t convert_padding_algorithm{TA_CV_INTER_LINEAR};
  /// Letterbox border RGB; converted to NV12 and pre-filled before ta_cv_image_convert_padding.
  Rgb888 convert_padding_border{kDefaultConvertPaddingBorder};

  void bind_resize_attr();
};

/// Parse one pipeline entry (order-independent feature flag).
void apply_pipeline_feature(PipelineConfig & config, const std::string & name);

/// Run configured pipeline; returns false on operator failure.
/// Predict the pipeline output size (must match execute_pipeline's internal
/// computation), used to pick the TbMsg profile / borrow the loaned output
/// buffer ahead of time.
bool predict_output_size(
  const PipelineConfig & config,
  int input_width,
  int input_height,
  int & out_width,
  int & out_height);

bool execute_pipeline(
  DmabufAllocator & alloc,
  const TaImage & input,
  int input_width,
  int input_height,
  const PipelineConfig & config,
  TaImage & output,
  int & output_width,
  int & output_height);

}  // namespace topsbot_cv

#endif  // TOPSBOT_CV__PIPELINE_EXECUTOR_HPP_

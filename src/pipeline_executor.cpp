// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_cv/pipeline_executor.hpp"

#include <cstring>

#include "topsbot_cv/format_convert.hpp"
#include "topsbot_cv/imgproc.hpp"

namespace topsbot_cv
{

void apply_pipeline_feature(PipelineConfig & config, const std::string & name)
{
  if (name == "crop") {
    config.features.crop = true;
    return;
  }
  if (name == "resize") {
    config.features.resize = true;
    return;
  }
  if (name == "csc") {
    config.features.csc = true;
    return;
  }
  if (name == "convert_padding") {
    config.features.convert_padding = true;
  }
}

void PipelineConfig::bind_resize_attr()
{
  std::memset(&resize_attr, 0, sizeof(resize_attr));
  resize_attr.resize_img_attr = &resize_rect;
}

namespace
{
bool any_csc_path_feature(const PipelineFeatures & features)
{
  return features.crop || features.resize || features.csc;
}

ta_cv_rect_t resolve_crop_rect(
  const PipelineFeatures & features,
  const ta_cv_rect_t & configured,
  const int frame_w,
  const int frame_h)
{
  ta_cv_rect_t rect = configured;
  if (!features.crop || rect.crop_w <= 0 || rect.crop_h <= 0) {
    rect.start_x = 0;
    rect.start_y = 0;
    rect.crop_w = frame_w;
    rect.crop_h = frame_h;
  }
  return rect;
}

ta_cv_resize_image_t build_csc_resize_attr(
  const PipelineFeatures & features,
  PipelineConfig & config,
  const ta_cv_rect_t & crop_rect)
{
  config.bind_resize_attr();
  if (config.resize_rect.in_width <= 0) {
    config.resize_rect.in_width = crop_rect.crop_w;
  }
  if (config.resize_rect.in_height <= 0) {
    config.resize_rect.in_height = crop_rect.crop_h;
  }
  if (!features.resize ||
    config.resize_rect.out_width <= 0 || config.resize_rect.out_height <= 0)
  {
    config.resize_rect.out_width = align_down16(config.resize_rect.in_width);
    config.resize_rect.out_height = config.resize_rect.in_height;
  } else {
    config.resize_rect.out_width = align_down16(config.resize_rect.out_width);
  }
  if (config.resize_attr.interpolation == 0) {
    config.resize_attr.interpolation = TA_CV_INTER_LINEAR;
  }
  return config.resize_attr;
}
}  // namespace

bool predict_output_size(
  const PipelineConfig & config,
  const int input_width,
  const int input_height,
  int & out_width,
  int & out_height)
{
  if (input_width <= 0 || input_height <= 0) {
    return false;
  }

  PipelineConfig mc = config;
  if (!mc.features.convert_padding && !any_csc_path_feature(mc.features)) {
    mc.features.crop = true;
  }

  if (mc.features.convert_padding) {
    out_width = mc.convert_padding_dst_width;
    out_height = mc.convert_padding_dst_height;
    return out_width > 0 && out_height > 0;
  }

  const ta_cv_rect_t crop_rect = resolve_crop_rect(
    mc.features, mc.crop_rect, input_width, input_height);
  build_csc_resize_attr(mc.features, mc, crop_rect);
  out_width = mc.resize_rect.out_width;
  out_height = mc.resize_rect.out_height;
  return out_width > 0 && out_height > 0;
}

bool execute_pipeline(
  DmabufAllocator & alloc,
  const TaImage & input,
  const int input_width,
  const int input_height,
  const PipelineConfig & config,
  TaImage & output,
  int & output_width,
  int & output_height)
{
  if (input_width <= 0 || input_height <= 0) {
    return false;
  }

  PipelineConfig mutable_config = config;
  if (!mutable_config.features.convert_padding && !any_csc_path_feature(mutable_config.features)) {
    mutable_config.features.crop = true;
  }

  if (mutable_config.features.convert_padding) {
    ta_cv_rect_t crop_rect = mutable_config.convert_padding_crop_rect;
    if (crop_rect.crop_w <= 0) {
      crop_rect.crop_w = input_width;
    }
    if (crop_rect.crop_h <= 0) {
      crop_rect.crop_h = input_height;
    }
    const auto status = image_convert_padding(
      input,
      mutable_config.convert_padding_dst_width,
      mutable_config.convert_padding_dst_height,
      crop_rect,
      mutable_config.convert_padding_attr,
      mutable_config.convert_padding_algorithm,
      mutable_config.convert_padding_border,
      alloc,
      output);
    if (status != TACV_SUCCESS) {
      return false;
    }
    output_width = mutable_config.convert_padding_dst_width;
    output_height = mutable_config.convert_padding_dst_height;
    return true;
  }

  const ta_cv_rect_t crop_rect = resolve_crop_rect(
    mutable_config.features, mutable_config.crop_rect, input_width, input_height);
  const ta_cv_resize_image_t resize_attr = build_csc_resize_attr(
    mutable_config.features, mutable_config, crop_rect);

  const ta_image_format_ext_t dst_format = mutable_config.features.csc ?
    mutable_config.csc_dst_format : FORMAT_NV12;
  const csc_type_t csc_type = mutable_config.features.csc ?
    mutable_config.csc_type : CSC_NONE;

  const auto status = image_csc(
    input,
    crop_rect,
    resize_attr,
    csc_type,
    dst_format,
    alloc,
    output);
  if (status != TACV_SUCCESS) {
    return false;
  }

  output_width = mutable_config.resize_rect.out_width;
  output_height = mutable_config.resize_rect.out_height;
  return true;
}

}  // namespace topsbot_cv

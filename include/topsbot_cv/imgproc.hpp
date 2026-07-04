// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__IMGPROC_HPP_
#define TOPSBOT_CV__IMGPROC_HPP_

#include <string>

#include "ta_cv_api_ext_c.h"
#include "topsbot_cv/dmabuf_allocator.hpp"
#include "topsbot_cv/format_convert.hpp"
#include "topsbot_cv/nv12_fill.hpp"
#include "topsbot_cv/ta_image.hpp"

namespace topsbot_cv
{

/// Wraps ta_cv_image_csc_convert_to (crop + resize + CSC in one hardware call).
tacv_status_t image_csc(
  const TaImage & input,
  const ta_cv_rect_t & crop_rect,
  const ta_cv_resize_image_t & resize_attr,
  csc_type_t csc_type,
  ta_image_format_ext_t dst_format,
  DmabufAllocator & alloc,
  TaImage & output);

/// Wraps ta_cv_image_convert_padding (letterbox; independent of csc_convert_to).
tacv_status_t image_convert_padding(
  const TaImage & input,
  int dst_width,
  int dst_height,
  const ta_cv_rect_t & crop_rect,
  const ta_cv_padding_attr_t & padding_attr,
  ta_cv_resize_algorithm_t algorithm,
  const Rgb888 & border_rgb,
  DmabufAllocator & alloc,
  TaImage & output);

/// Wraps ta_cv_image_jpeg_enc (NV12 only), writes encoded bitstream to path.
bool image_jpeg_enc_to_file(
  DmabufAllocator & alloc,
  const TaImage & nv12,
  const std::string & path,
  int quality = 90);

/// Write RGB/BGR packed dmabuf as PPM (visual check for CSC output).
bool image_write_ppm(const TaImage & image, const std::string & path);

/// NV12 -> .jpg; RGB/BGR packed -> .ppm. Returns false if format unsupported.
bool image_dump_to_file(
  DmabufAllocator & alloc,
  const TaImage & image,
  const std::string & path_without_ext,
  std::string & out_path);

}  // namespace topsbot_cv

#endif  // TOPSBOT_CV__IMGPROC_HPP_

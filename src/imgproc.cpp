// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_cv/imgproc.hpp"

#include <fstream>

#include "topsbot_cv/format_convert.hpp"
#include "topsbot_cv/nv12_fill.hpp"

#include <sys/mman.h>
#include <unistd.h>

namespace topsbot_cv
{

tacv_status_t image_csc(
  const TaImage & input,
  const ta_cv_rect_t & crop_rect,
  const ta_cv_resize_image_t & resize_attr,
  const csc_type_t csc_type,
  const ta_image_format_ext_t dst_format,
  DmabufAllocator & alloc,
  TaImage & output)
{
  if (!resize_attr.resize_img_attr) {
    return TACV_ILLEGAL_PARAMETER;
  }

  const int dst_w = resize_attr.resize_img_attr->out_width;
  const int dst_h = resize_attr.resize_img_attr->out_height;
  if (dst_w <= 0 || dst_h <= 0 || crop_rect.crop_w <= 0 || crop_rect.crop_h <= 0) {
    return TACV_ILLEGAL_PARAMETER;
  }

  const bool need_output = output.fd() < 0 ||
    output.width() != dst_w || output.height() != dst_h ||
    output.format() != dst_format;
  if (need_output) {
    output = TaImage::create(alloc, dst_format, dst_w, dst_h);
  }
  if (output.fd() < 0) {
    return TACV_NOMEM;
  }

  ta_cv_rect_t rect = crop_rect;
  const auto status = ta_cv_image_csc_convert_to(
    input.native(),
    output.native(),
    rect,
    const_cast<ta_cv_resize_image_t *>(&resize_attr),
    csc_type);
  if (status == TACV_SUCCESS) {
    output.refresh_from_native(dst_format);
  }
  return status;
}

tacv_status_t image_convert_padding(
  const TaImage & input,
  const int dst_width,
  const int dst_height,
  const ta_cv_rect_t & crop_rect,
  const ta_cv_padding_attr_t & padding_attr,
  const ta_cv_resize_algorithm_t algorithm,
  const Rgb888 & border_rgb,
  DmabufAllocator & alloc,
  TaImage & output)
{
  if (dst_width <= 0 || dst_height <= 0) {
    return TACV_ILLEGAL_PARAMETER;
  }

  const bool need_output = output.fd() < 0 ||
    output.width() != dst_width || output.height() != dst_height ||
    !output.is_nv12();
  if (need_output) {
    output = TaImage::create_nv12(alloc, dst_width, dst_height);
  }
  if (output.fd() < 0) {
    return TACV_NOMEM;
  }

  const Nv12SolidColor border_nv12 = rgb_to_nv12_bt601(border_rgb.r, border_rgb.g, border_rgb.b);
  fill_nv12_solid(output.mapped(), dst_width, dst_height, border_nv12);

  ta_cv_rect_t rect = crop_rect;
  ta_cv_padding_attr_t padding = padding_attr;
  const auto status = ta_cv_image_convert_padding(
    input.native(), output.native(), rect, padding, algorithm);
  if (status == TACV_SUCCESS) {
    output.refresh_from_native(FORMAT_NV12);
  }
  return status;
}

bool image_jpeg_enc_to_file(
  DmabufAllocator & alloc,
  const TaImage & nv12,
  const std::string & path,
  const int quality)
{
  if (!nv12.is_nv12() || !nv12.mapped()) {
    return false;
  }

  const size_t buf_size = nv12.byte_size();
  const int enc_fd = alloc.alloc(buf_size);
  if (enc_fd < 0) {
    return false;
  }

  size_t jpeg_size = buf_size;
  ta_image_t src = nv12.native();
  const auto ret = ta_cv_image_jpeg_enc(
    &src, static_cast<unsigned int>(enc_fd), &jpeg_size, quality);
  if (ret != TACV_SUCCESS) {
    close(enc_fd);
    return false;
  }

  void * enc_data = mmap(nullptr, buf_size, PROT_READ, MAP_SHARED, enc_fd, 0);
  if (enc_data == MAP_FAILED) {
    close(enc_fd);
    return false;
  }

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    munmap(enc_data, buf_size);
    close(enc_fd);
    return false;
  }
  out.write(static_cast<const char *>(enc_data), static_cast<std::streamsize>(jpeg_size));
  const bool ok = out.good();
  munmap(enc_data, buf_size);
  close(enc_fd);
  return ok;
}

bool image_write_ppm(const TaImage & image, const std::string & path)
{
  if (!image.mapped() || image.width() <= 0 || image.height() <= 0) {
    return false;
  }

  const auto format = image.format();
  if (format != FORMAT_RGB_PACKED && format != FORMAT_BGR_PACKED) {
    return false;
  }

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }

  out << "P6\n" << image.width() << ' ' << image.height() << "\n255\n";
  const auto * src = static_cast<const uint8_t *>(image.mapped());
  const size_t pixel_count =
    static_cast<size_t>(image.width()) * static_cast<size_t>(image.height());

  if (format == FORMAT_RGB_PACKED) {
    out.write(reinterpret_cast<const char *>(src), static_cast<std::streamsize>(pixel_count * 3U));
    return out.good();
  }

  for (size_t i = 0; i < pixel_count; ++i) {
    const char rgb[3] = {
      static_cast<char>(src[i * 3U + 2U]),
      static_cast<char>(src[i * 3U + 1U]),
      static_cast<char>(src[i * 3U])};
    out.write(rgb, 3);
  }
  return out.good();
}

bool image_dump_to_file(
  DmabufAllocator & alloc,
  const TaImage & image,
  const std::string & path_without_ext,
  std::string & out_path)
{
  if (image.is_nv12()) {
    out_path = path_without_ext + ".jpg";
    return image_jpeg_enc_to_file(alloc, image, out_path);
  }

  if (image.format() == FORMAT_RGB_PACKED || image.format() == FORMAT_BGR_PACKED) {
    out_path = path_without_ext + ".ppm";
    return image_write_ppm(image, out_path);
  }

  return false;
}

}  // namespace topsbot_cv

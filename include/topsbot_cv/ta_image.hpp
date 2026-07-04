// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__TA_IMAGE_HPP_
#define TOPSBOT_CV__TA_IMAGE_HPP_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "ta_cv_api_ext_c.h"

namespace topsbot_cv
{

class DmabufAllocator;

/// RAII wrapper for ta_image_t backed by dmabuf.
class TaImage
{
public:
  TaImage();
  ~TaImage();

  TaImage(TaImage && other) noexcept;
  TaImage & operator=(TaImage && other) noexcept;

  TaImage(const TaImage &) = delete;
  TaImage & operator=(const TaImage &) = delete;

  static TaImage create_nv12(DmabufAllocator & alloc, int width, int height);

  static TaImage create(
    DmabufAllocator & alloc,
    ta_image_format_ext_t format,
    int width,
    int height);

  /// Take ownership of an existing NV12 dmabuf + ta_image_t (e.g. after JPEG decode).
  static TaImage adopt_nv12(
    int fd, int width, int height, size_t byte_size, ta_image_t image);

  ta_image_format_ext_t format() const {return format_;}
  bool is_nv12() const {return format_ == FORMAT_NV12;}

  ta_image_t & native() {return image_;}
  const ta_image_t & native() const {return image_;}

  int width() const {return width_;}
  int height() const {return height_;}
  int fd() const {return fd_;}
  size_t byte_size() const {return byte_size_;}
  void * mapped() const {return mapped_;}

  /// Copy host NV12 (Y + interleaved UV) into dmabuf.
  bool upload_nv12(const uint8_t * src, int width, int height);

  /// Sync width/height/format/mapped pointer from ta_image_t after SDK calls.
  void refresh_from_native(ta_image_format_ext_t fallback_format);

  void reset();

private:
  void cleanup();

  ta_image_t image_;
  ta_image_format_ext_t format_{FORMAT_NV12};
  int fd_{-1};
  int width_{0};
  int height_{0};
  size_t byte_size_{0};
  void * mapped_{nullptr};
  bool owns_image_{false};
  bool self_mmaped_{false};
};

}  // namespace topsbot_cv

#endif  // TOPSBOT_CV__TA_IMAGE_HPP_

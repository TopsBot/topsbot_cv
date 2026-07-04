// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_cv/ta_image.hpp"

#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

#include "topsbot_cv/dmabuf_allocator.hpp"
#include "topsbot_cv/format_convert.hpp"

namespace topsbot_cv
{

TaImage::TaImage()
: image_{},
  format_(FORMAT_NV12),
  fd_(-1),
  width_(0),
  height_(0),
  byte_size_(0),
  mapped_(nullptr),
  owns_image_(false),
  self_mmaped_(false)
{
  std::memset(&image_, 0, sizeof(image_));
}

TaImage::~TaImage()
{
  cleanup();
}

TaImage::TaImage(TaImage && other) noexcept
: image_(other.image_),
  format_(other.format_),
  fd_(other.fd_),
  width_(other.width_),
  height_(other.height_),
  byte_size_(other.byte_size_),
  mapped_(other.mapped_),
  owns_image_(other.owns_image_),
  self_mmaped_(other.self_mmaped_)
{
  other.fd_ = -1;
  other.mapped_ = nullptr;
  other.owns_image_ = false;
  other.self_mmaped_ = false;
  other.format_ = FORMAT_NV12;
  std::memset(&other.image_, 0, sizeof(other.image_));
}

TaImage & TaImage::operator=(TaImage && other) noexcept
{
  if (this != &other) {
    cleanup();
    image_ = other.image_;
    format_ = other.format_;
    fd_ = other.fd_;
    width_ = other.width_;
    height_ = other.height_;
    byte_size_ = other.byte_size_;
    mapped_ = other.mapped_;
    owns_image_ = other.owns_image_;
    self_mmaped_ = other.self_mmaped_;
    other.fd_ = -1;
    other.mapped_ = nullptr;
    other.owns_image_ = false;
    other.self_mmaped_ = false;
    other.format_ = FORMAT_NV12;
    std::memset(&other.image_, 0, sizeof(other.image_));
  }
  return *this;
}

void TaImage::refresh_from_native(const ta_image_format_ext_t fallback_format)
{
  if (image_.image_format != 0) {
    format_ = image_.image_format;
  } else if (fallback_format != 0) {
    format_ = fallback_format;
  }
  if (image_.width > 0) {
    width_ = image_.width;
  }
  if (image_.height > 0) {
    height_ = image_.height;
  }
  if (image_.frame.data[0] != nullptr) {
    mapped_ = image_.frame.data[0];
    self_mmaped_ = false;
  }
}

void TaImage::cleanup()
{
  void * const mapped_to_unmap =
    (self_mmaped_ && mapped_ && mapped_ != MAP_FAILED) ? mapped_ : nullptr;
  const size_t unmap_size = byte_size_;

  if (owns_image_) {
    ta_cv_image_destroy_ext(&image_);
  }
  if (mapped_to_unmap != nullptr) {
    munmap(mapped_to_unmap, unmap_size);
  }
  if (fd_ >= 0) {
    close(fd_);
  }
  fd_ = -1;
  width_ = 0;
  height_ = 0;
  byte_size_ = 0;
  mapped_ = nullptr;
  owns_image_ = false;
  self_mmaped_ = false;
  format_ = FORMAT_NV12;
  std::memset(&image_, 0, sizeof(image_));
}

void TaImage::reset()
{
  cleanup();
}

TaImage TaImage::adopt_nv12(
  const int fd,
  const int width,
  const int height,
  const size_t byte_size,
  ta_image_t image)
{
  TaImage out;
  out.fd_ = fd;
  out.width_ = width;
  out.height_ = height;
  out.byte_size_ = byte_size;
  out.format_ = FORMAT_NV12;
  out.image_ = image;
  out.owns_image_ = true;
  out.refresh_from_native(FORMAT_NV12);
  if (!out.mapped_ && fd >= 0 && byte_size > 0) {
    out.mapped_ = mmap(nullptr, byte_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (out.mapped_ == MAP_FAILED) {
      out.mapped_ = nullptr;
      ta_cv_image_destroy_ext(&out.image_);
      std::memset(&out.image_, 0, sizeof(out.image_));
      out.fd_ = -1;
      out.owns_image_ = false;
      return out;
    }
    out.self_mmaped_ = true;
  }
  return out;
}

TaImage TaImage::create_nv12(DmabufAllocator & alloc, const int width, const int height)
{
  return create(alloc, FORMAT_NV12, width, height);
}

TaImage TaImage::create(
  DmabufAllocator & alloc,
  const ta_image_format_ext_t format,
  const int width,
  const int height)
{
  TaImage out;
  out.width_ = width;
  out.height_ = height;
  out.format_ = format;
  out.byte_size_ = byte_size_for_format(format, width, height);
  if (out.byte_size_ == 0) {
    out.reset();
    return out;
  }

  out.fd_ = alloc.alloc(out.byte_size_);
  if (out.fd_ < 0) {
    out.reset();
    return out;
  }

  std::memset(&out.image_, 0, sizeof(out.image_));
  const auto ret = ta_cv_image_create_ext(
    height, width, format, &out.image_, out.fd_);
  if (ret != TACV_SUCCESS) {
    out.reset();
    return out;
  }

  out.owns_image_ = true;
  out.refresh_from_native(format);
  if (!out.mapped_ && out.fd_ >= 0 && out.byte_size_ > 0) {
    out.mapped_ = mmap(nullptr, out.byte_size_, PROT_READ | PROT_WRITE, MAP_SHARED, out.fd_, 0);
    if (out.mapped_ == MAP_FAILED) {
      out.mapped_ = nullptr;
      out.reset();
      return out;
    }
    out.self_mmaped_ = true;
  }
  if (out.mapped_ != nullptr) {
    std::memset(out.mapped_, 0, out.byte_size_);
  }
  return out;
}

bool TaImage::upload_nv12(const uint8_t * src, const int width, const int height)
{
  if (!is_nv12() || !src || width != width_ || height != height_ || !mapped_) {
    return false;
  }
  std::memcpy(mapped_, src, byte_size_);
  return true;
}

}  // namespace topsbot_cv

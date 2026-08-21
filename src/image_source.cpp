// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_cv/image_source.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

#include "ffmpeg_decode_util.h"

namespace topsbot_cv
{

namespace
{
std::string to_lower_ascii(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
  return value;
}

bool encoding_is_nv12(const std::string & encoding)
{
  const auto lower = to_lower_ascii(encoding);
  return lower == "nv12" || lower == "yuv420";
}

bool encoding_is_jpeg(const std::string & encoding)
{
  const auto lower = to_lower_ascii(encoding);
  return lower == "mjpeg" || lower == "mjpg" || lower == "jpeg" || lower == "jpg";
}

bool compressed_format_is_jpeg(const std::string & format)
{
  const auto lower = to_lower_ascii(format);
  if (encoding_is_jpeg(lower)) {
    return true;
  }
  return lower.find("jpeg") != std::string::npos;
}

std::optional<Nv12Buffer> adopt_decoded_frame(DmabufAllocator & alloc, NV12Frame & frame)
{
  if (!alloc.get()) {
    release_nv12(&frame);
    return std::nullopt;
  }

  Nv12Buffer out;
  out.width = frame.width;
  out.height = frame.height;
  out.image = TaImage::adopt_nv12(
    frame.buf_fd,
    frame.width,
    frame.height,
    static_cast<size_t>(frame.data_size),
    frame.src_image);
  if (out.image.fd() < 0) {
    release_nv12(&frame);
    return std::nullopt;
  }
  frame.buf_fd = -1;
  std::memset(&frame.src_image, 0, sizeof(frame.src_image));
  return out;
}
}  // namespace

std::optional<Nv12Buffer> ImageSource::from_file(
  DmabufAllocator & alloc,
  const std::string & path)
{
  if (!alloc.get()) {
    return std::nullopt;
  }

  NV12Frame frame{};
  if (decode_to_nv12(alloc.get(), path.c_str(), &frame) != 0) {
    return std::nullopt;
  }
  return adopt_decoded_frame(alloc, frame);
}

std::optional<Nv12Buffer> ImageSource::from_jpeg_bytes(
  DmabufAllocator & alloc,
  const uint8_t * jpeg_data,
  const size_t jpeg_size,
  const int width_hint,
  const int height_hint,
  const uint64_t dma_phys)
{
  if (!alloc.get() || !jpeg_data || jpeg_size == 0) {
    return std::nullopt;
  }

  NV12Frame frame{};
  if (decode_jpeg_bytes_to_nv12(
      alloc.get(), jpeg_data, jpeg_size, &frame, width_hint, height_hint,
      dma_phys) != 0)
  {
    return std::nullopt;
  }
  return adopt_decoded_frame(alloc, frame);
}

std::optional<Nv12Buffer> ImageSource::from_nv12_bytes(
  DmabufAllocator & alloc,
  const std::vector<uint8_t> & data,
  const int width,
  const int height)
{
  const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 3U / 2U;
  if (data.size() < expected) {
    return std::nullopt;
  }

  Nv12Buffer out;
  out.width = width;
  out.height = height;
  out.image = TaImage::create_nv12(alloc, width, height);
  if (out.image.fd() < 0) {
    return std::nullopt;
  }
  if (!out.image.upload_nv12(data.data(), width, height)) {
    return std::nullopt;
  }
  return out;
}

std::optional<Nv12Buffer> ImageSource::from_sensor_image(
  DmabufAllocator & alloc,
  const sensor_msgs::msg::Image & msg)
{
  if (encoding_is_jpeg(msg.encoding)) {
    if (msg.data.empty()) {
      return std::nullopt;
    }
    const int width = static_cast<int>(msg.width);
    const int height = static_cast<int>(msg.height);
    return from_jpeg_bytes(alloc, msg.data.data(), msg.data.size(), width, height);
  }

  const int width = static_cast<int>(msg.width);
  const int height = static_cast<int>(msg.height);
  if (width <= 0 || height <= 0) {
    return std::nullopt;
  }

  if (encoding_is_nv12(msg.encoding)) {
    return from_nv12_bytes(alloc, msg.data, width, height);
  }

  return std::nullopt;
}

std::optional<Nv12Buffer> ImageSource::from_compressed_image(
  DmabufAllocator & alloc,
  const sensor_msgs::msg::CompressedImage & msg)
{
  if (!compressed_format_is_jpeg(msg.format) || msg.data.empty()) {
    return std::nullopt;
  }
  return from_jpeg_bytes(alloc, msg.data.data(), msg.data.size());
}

}  // namespace topsbot_cv

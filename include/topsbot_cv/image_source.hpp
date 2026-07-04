// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__IMAGE_SOURCE_HPP_
#define TOPSBOT_CV__IMAGE_SOURCE_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "sensor_msgs/msg/compressed_image.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "topsbot_cv/dmabuf_allocator.hpp"
#include "topsbot_cv/ta_image.hpp"

namespace topsbot_cv
{

struct Nv12Buffer
{
  TaImage image;
  int width{0};
  int height{0};
};

/// JPEG (hw decode) and NV12 → NV12 dmabuf. RGB/BGR inputs are not supported.
class ImageSource
{
public:
  static std::optional<Nv12Buffer> from_file(
    DmabufAllocator & alloc,
    const std::string & path);

  static std::optional<Nv12Buffer> from_sensor_image(
    DmabufAllocator & alloc,
    const sensor_msgs::msg::Image & msg);

  static std::optional<Nv12Buffer> from_compressed_image(
    DmabufAllocator & alloc,
    const sensor_msgs::msg::CompressedImage & msg);

  /// width_hint/height_hint > 0: skip JPEG header parse (use message metadata).
  static std::optional<Nv12Buffer> from_jpeg_bytes(
    DmabufAllocator & alloc,
    const uint8_t * jpeg_data,
    size_t jpeg_size,
    int width_hint = 0,
    int height_hint = 0);

  static std::optional<Nv12Buffer> from_nv12_bytes(
    DmabufAllocator & alloc,
    const std::vector<uint8_t> & data,
    int width,
    int height);
};

}  // namespace topsbot_cv

#endif  // TOPSBOT_CV__IMAGE_SOURCE_HPP_

// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_cv/nv12_fill.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace topsbot_cv
{

namespace
{
uint8_t clamp_u8(const int value)
{
  return static_cast<uint8_t>(std::max(0, std::min(255, value)));
}
}  // namespace

Nv12SolidColor rgb_to_nv12_bt601(const uint8_t r, const uint8_t g, const uint8_t b)
{
  const int y = static_cast<int>(std::lround(0.299 * r + 0.587 * g + 0.114 * b));
  const int u = static_cast<int>(std::lround(-0.169 * r - 0.331 * g + 0.500 * b + 128.0));
  const int v = static_cast<int>(std::lround(0.500 * r - 0.419 * g - 0.081 * b + 128.0));
  return Nv12SolidColor{clamp_u8(y), clamp_u8(u), clamp_u8(v)};
}

std::optional<Rgb888> parse_convert_padding_border_preset(const std::string & preset)
{
  if (preset == "yolo_gray") {
    return kDefaultConvertPaddingBorder;
  }
  if (preset == "black") {
    return Rgb888{0, 0, 0};
  }
  return std::nullopt;
}

void fill_nv12_solid(void * data, const int width, const int height, const Nv12SolidColor & color)
{
  if (!data || width <= 0 || height <= 0) {
    return;
  }

  const size_t y_size = static_cast<size_t>(width) * static_cast<size_t>(height);
  auto * bytes = static_cast<uint8_t *>(data);
  std::memset(bytes, color.y, y_size);

  uint8_t * uv = bytes + y_size;
  const size_t uv_size = y_size / 2;
  if (color.u == color.v) {
    std::memset(uv, color.u, uv_size);
    return;
  }

  for (size_t i = 0; i < uv_size; i += 2) {
    uv[i] = color.u;
    uv[i + 1] = color.v;
  }
}

}  // namespace topsbot_cv

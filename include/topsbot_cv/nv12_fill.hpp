// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__NV12_FILL_HPP_
#define TOPSBOT_CV__NV12_FILL_HPP_

#include <cstdint>
#include <optional>
#include <string>

namespace topsbot_cv
{

struct Rgb888
{
  uint8_t r{114};
  uint8_t g{114};
  uint8_t b{114};
};

/// YOLO letterbox default border color.
constexpr Rgb888 kDefaultConvertPaddingBorder{114, 114, 114};

struct Nv12SolidColor
{
  uint8_t y{0};
  uint8_t u{128};
  uint8_t v{128};
};

/// BT.601 full-range RGB to NV12 Y/U/V (for padding pre-fill before ta-cv).
Nv12SolidColor rgb_to_nv12_bt601(uint8_t r, uint8_t g, uint8_t b);

/// Named border presets; returns nullopt if name is unknown.
std::optional<Rgb888> parse_convert_padding_border_preset(const std::string & preset);

/// Fill NV12 dmabuf memory with a solid color (Y plane + interleaved UV).
void fill_nv12_solid(void * data, int width, int height, const Nv12SolidColor & color);

}  // namespace topsbot_cv

#endif  // TOPSBOT_CV__NV12_FILL_HPP_

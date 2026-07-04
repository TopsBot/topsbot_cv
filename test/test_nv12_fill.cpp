// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include "topsbot_cv/nv12_fill.hpp"

TEST(Nv12Fill, YoloGrayRgbToNv12)
{
  const auto color = topsbot_cv::rgb_to_nv12_bt601(114, 114, 114);
  EXPECT_EQ(color.y, 114);
  EXPECT_EQ(color.u, 128);
  EXPECT_EQ(color.v, 128);
}

TEST(Nv12Fill, BlackRgbToNv12)
{
  const auto color = topsbot_cv::rgb_to_nv12_bt601(0, 0, 0);
  EXPECT_EQ(color.y, 0);
  EXPECT_EQ(color.u, 128);
  EXPECT_EQ(color.v, 128);
}

TEST(Nv12Fill, FillNv12SolidGrayUsesSingleMemsetOnUv)
{
  constexpr int width = 4;
  constexpr int height = 4;
  uint8_t buffer[width * height * 3 / 2]{};
  const topsbot_cv::Nv12SolidColor gray{114, 128, 128};
  topsbot_cv::fill_nv12_solid(buffer, width, height, gray);

  for (int i = 0; i < width * height; ++i) {
    EXPECT_EQ(buffer[i], 114);
  }
  for (int i = width * height; i < width * height * 3 / 2; ++i) {
    EXPECT_EQ(buffer[i], 128);
  }
}

TEST(Nv12Fill, BorderPresetYoloGray)
{
  const auto preset = topsbot_cv::parse_convert_padding_border_preset("yolo_gray");
  ASSERT_TRUE(preset.has_value());
  EXPECT_EQ(preset->r, 114);
  EXPECT_EQ(preset->g, 114);
  EXPECT_EQ(preset->b, 114);
}

TEST(Nv12Fill, DefaultBorderMatchesYoloGray)
{
  EXPECT_EQ(topsbot_cv::kDefaultConvertPaddingBorder.r, 114);
  EXPECT_EQ(topsbot_cv::kDefaultConvertPaddingBorder.g, 114);
  EXPECT_EQ(topsbot_cv::kDefaultConvertPaddingBorder.b, 114);
}

// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include <vector>

#include "gtest/gtest.h"
#include "sensor_msgs/msg/image.hpp"
#include "topsbot_cv/image_source.hpp"

TEST(TopsbotCvImageSource, RejectsBgr8)
{
  constexpr int width = 4;
  constexpr int height = 4;
  std::vector<uint8_t> bgr(static_cast<size_t>(width) * static_cast<size_t>(height) * 3U, 0);

  sensor_msgs::msg::Image msg;
  msg.width = static_cast<uint32_t>(width);
  msg.height = static_cast<uint32_t>(height);
  msg.encoding = "bgr8";
  msg.step = static_cast<uint32_t>(width * 3);
  msg.data = bgr;

  topsbot_cv::DmabufAllocator alloc;
  EXPECT_FALSE(topsbot_cv::ImageSource::from_sensor_image(alloc, msg).has_value());
}

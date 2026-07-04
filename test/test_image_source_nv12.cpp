// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include <vector>

#include "gtest/gtest.h"
#include "sensor_msgs/msg/image.hpp"
#include "test_common.hpp"
#include "topsbot_cv/image_source.hpp"

TEST(TopsbotCvImageSource, SensorMsgNv12)
{
  constexpr int width = 64;
  constexpr int height = 64;
  const size_t nv12_size = static_cast<size_t>(width) * static_cast<size_t>(height) * 3U / 2U;

  std::vector<uint8_t> nv12(nv12_size, 0x10);

  sensor_msgs::msg::Image msg;
  msg.width = static_cast<uint32_t>(width);
  msg.height = static_cast<uint32_t>(height);
  msg.encoding = "nv12";
  msg.step = static_cast<uint32_t>(width);
  msg.data = nv12;

  topsbot_cv::DmabufAllocator alloc;
  auto buffer = topsbot_cv::ImageSource::from_sensor_image(alloc, msg);
  ASSERT_TRUE(buffer.has_value());
  EXPECT_EQ(buffer->width, width);
  EXPECT_EQ(buffer->height, height);
  EXPECT_GE(buffer->image.fd(), 0);
}

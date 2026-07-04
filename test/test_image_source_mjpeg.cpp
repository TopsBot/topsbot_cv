// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include <fstream>
#include <vector>

#include "gtest/gtest.h"
#include "sensor_msgs/msg/compressed_image.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "test_common.hpp"
#include "topsbot_cv/image_source.hpp"

namespace
{
std::vector<uint8_t> read_file_bytes(const std::string & path)
{
  std::ifstream in(path, std::ios::binary);
  EXPECT_TRUE(in.good());
  return std::vector<uint8_t>(
    std::istreambuf_iterator<char>(in),
    std::istreambuf_iterator<char>());
}
}  // namespace

TEST(TopsbotCvImageSource, SensorMsgMjpeg)
{
  topsbot_cv_test::skip_if_no_test_image();

  const auto jpeg = read_file_bytes(topsbot_cv_test::test_image_path());
  ASSERT_FALSE(jpeg.empty());

  sensor_msgs::msg::Image msg;
  msg.encoding = "mjpeg";
  msg.data = jpeg;

  topsbot_cv::DmabufAllocator alloc;
  auto buffer = topsbot_cv::ImageSource::from_sensor_image(alloc, msg);
  ASSERT_TRUE(buffer.has_value());
  EXPECT_GT(buffer->width, 0);
  EXPECT_GT(buffer->height, 0);
  EXPECT_GE(buffer->image.fd(), 0);
}

TEST(TopsbotCvImageSource, CompressedImageJpeg)
{
  topsbot_cv_test::skip_if_no_test_image();

  const auto jpeg = read_file_bytes(topsbot_cv_test::test_image_path());
  ASSERT_FALSE(jpeg.empty());

  sensor_msgs::msg::CompressedImage msg;
  msg.format = "jpeg";
  msg.data = jpeg;

  topsbot_cv::DmabufAllocator alloc;
  auto buffer = topsbot_cv::ImageSource::from_compressed_image(alloc, msg);
  ASSERT_TRUE(buffer.has_value());
  EXPECT_GT(buffer->width, 0);
  EXPECT_GT(buffer->height, 0);
  EXPECT_GE(buffer->image.fd(), 0);
}

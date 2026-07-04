// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "gtest/gtest.h"
#include "ffmpeg_decode_util.h"
#include "test_common.hpp"
#include "topsbot_cv/dmabuf_allocator.hpp"

#include <fstream>
#include <vector>

TEST(TopsbotCvJpegDec, ParseHeaderDimensions)
{
  topsbot_cv_test::skip_if_no_test_image();

  std::ifstream in(topsbot_cv_test::test_image_path(), std::ios::binary);
  ASSERT_TRUE(in.good());
  const std::vector<uint8_t> bytes(
    std::istreambuf_iterator<char>(in),
    std::istreambuf_iterator<char>());
  ASSERT_FALSE(bytes.empty());

  int width = 0;
  int height = 0;
  ASSERT_EQ(parse_jpeg_dimensions(bytes.data(), bytes.size(), &width, &height), 0);
  EXPECT_EQ(width, 1920);
  EXPECT_EQ(height, 1080);
}

TEST(TopsbotCvJpegDec, FileDecode)
{
  topsbot_cv_test::skip_if_no_test_image();

  topsbot_cv::DmabufAllocator alloc;
  NV12Frame frame{};
  ASSERT_EQ(decode_to_nv12(alloc.get(), topsbot_cv_test::test_image_path().c_str(), &frame), 0);
  EXPECT_EQ(frame.width, 1920);
  EXPECT_EQ(frame.height, 1080);
  release_nv12(&frame);
}

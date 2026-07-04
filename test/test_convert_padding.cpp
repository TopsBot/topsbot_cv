// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "test_common.hpp"
#include "topsbot_cv/imgproc.hpp"

namespace
{
ta_cv_padding_attr_t padding_fill_dst(const int dst_w, const int dst_h)
{
  ta_cv_padding_attr_t padding{};
  padding.dst_crop_stx = 0;
  padding.dst_crop_sty = 0;
  padding.dst_crop_w = static_cast<unsigned int>(dst_w);
  padding.dst_crop_h = static_cast<unsigned int>(dst_h);
  return padding;
}
}  // namespace

TEST(TopsbotCvConvertPadding, FileCropResizePaddingAndJpegEnc)
{
  topsbot_cv_test::skip_if_no_test_image();

  topsbot_cv::DmabufAllocator alloc;
  auto input = topsbot_cv_test::load_test_nv12(alloc);
  ASSERT_TRUE(input.has_value());

  const int dst_w = topsbot_cv_test::align_down16(std::min(640, input->width));
  const int dst_h = std::min(640, input->height);

  ta_cv_rect_t crop_rect{};
  crop_rect.start_x = 0;
  crop_rect.start_y = 0;
  crop_rect.crop_w = input->width;
  crop_rect.crop_h = input->height;

  topsbot_cv::TaImage output;
  ASSERT_EQ(
    topsbot_cv::image_convert_padding(
      input->image,
      dst_w,
      dst_h,
      crop_rect,
      padding_fill_dst(dst_w, dst_h),
      TA_CV_INTER_LINEAR,
      topsbot_cv::kDefaultConvertPaddingBorder,
      alloc,
      output),
    TACV_SUCCESS);
  EXPECT_TRUE(
    topsbot_cv::image_jpeg_enc_to_file(alloc, output, "/tmp/topsbot_cv_test_convert_padding.jpg"));
}

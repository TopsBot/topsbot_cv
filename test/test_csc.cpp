// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "test_common.hpp"
#include "topsbot_cv/format_convert.hpp"
#include "topsbot_cv/imgproc.hpp"
#include "topsbot_cv/pipeline_executor.hpp"

TEST(TopsbotCvCsc, Nv12ToRgbPacked)
{
  topsbot_cv_test::skip_if_no_test_image();

  topsbot_cv::DmabufAllocator alloc;
  auto input = topsbot_cv_test::load_test_nv12(alloc);
  ASSERT_TRUE(input.has_value());

  const int out_w = topsbot_cv_test::align_down16(input->width);
  const int out_h = input->height;

  ta_cv_rect_t crop{};
  crop.start_x = 0;
  crop.start_y = 0;
  crop.crop_w = input->width;
  crop.crop_h = input->height;

  ta_cv_resize_t resize{};
  resize.start_x = 0;
  resize.start_y = 0;
  resize.in_width = crop.crop_w;
  resize.in_height = crop.crop_h;
  resize.out_width = out_w;
  resize.out_height = out_h;

  ta_cv_resize_image_t resize_attr{};
  resize_attr.resize_img_attr = &resize;
  resize_attr.interpolation = TA_CV_INTER_LINEAR;
  resize_attr.stretch_fit = 0;

  topsbot_cv::TaImage output;
  ASSERT_EQ(
    topsbot_cv::image_csc(
      input->image, crop, resize_attr, CSC_YCbCr2RGB_BT601, FORMAT_RGB_PACKED, alloc, output),
    TACV_SUCCESS);
  EXPECT_EQ(output.format(), FORMAT_RGB_PACKED);
  EXPECT_EQ(output.width(), out_w);
  EXPECT_EQ(output.height(), out_h);
  EXPECT_GE(output.fd(), 0);
}

TEST(TopsbotCvCsc, PipelineCscStep)
{
  topsbot_cv_test::skip_if_no_test_image();

  topsbot_cv::DmabufAllocator alloc;
  auto input = topsbot_cv_test::load_test_nv12(alloc);
  ASSERT_TRUE(input.has_value());

  topsbot_cv::PipelineConfig config;
  topsbot_cv::apply_pipeline_feature(config, "csc");
  config.csc_dst_format = FORMAT_BGR_PACKED;
  config.csc_type = CSC_YCbCr2RGB_BT601;

  topsbot_cv::TaImage output;
  int out_w = 0;
  int out_h = 0;
  ASSERT_TRUE(topsbot_cv::execute_pipeline(
    alloc, input->image, input->width, input->height, config, output, out_w, out_h));
  EXPECT_EQ(output.format(), FORMAT_BGR_PACKED);
  EXPECT_GT(out_w, 0);
  EXPECT_GT(out_h, 0);
}

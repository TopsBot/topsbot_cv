// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "test_common.hpp"
#include "topsbot_cv/imgproc.hpp"
#include "topsbot_cv/pipeline_executor.hpp"

namespace
{
topsbot_cv::PipelineConfig make_config()
{
  topsbot_cv::PipelineConfig config;
  config.bind_resize_attr();
  config.resize_attr.interpolation = TA_CV_INTER_LINEAR;
  config.resize_attr.stretch_fit = 0;
  return config;
}

bool run_pipeline(
  topsbot_cv::DmabufAllocator & alloc,
  const topsbot_cv::TaImage & input,
  const int width,
  const int height,
  topsbot_cv::PipelineConfig & config,
  topsbot_cv::TaImage & output,
  int & out_w,
  int & out_h)
{
  return topsbot_cv::execute_pipeline(alloc, input, width, height, config, output, out_w, out_h);
}
}  // namespace

TEST(TopsbotCvPipeline, CropAndJpegEnc)
{
  topsbot_cv_test::skip_if_no_test_image();

  topsbot_cv::DmabufAllocator alloc;
  auto input = topsbot_cv_test::load_test_nv12(alloc);
  ASSERT_TRUE(input.has_value());

  auto config = make_config();
  topsbot_cv::apply_pipeline_feature(config, "crop");
  config.crop_rect.start_x = 0;
  config.crop_rect.start_y = 0;
  config.crop_rect.crop_w = 960;
  config.crop_rect.crop_h = 540;

  topsbot_cv::TaImage output;
  int out_w = 0;
  int out_h = 0;
  ASSERT_TRUE(run_pipeline(alloc, input->image, input->width, input->height, config, output, out_w, out_h));
  EXPECT_EQ(out_w, 960);
  EXPECT_EQ(out_h, 540);
  EXPECT_TRUE(output.is_nv12());
  EXPECT_TRUE(topsbot_cv::image_jpeg_enc_to_file(alloc, output, "/tmp/topsbot_cv_test_crop.jpg"));
}

TEST(TopsbotCvPipeline, ResizeDownsampleAndJpegEnc)
{
  topsbot_cv_test::skip_if_no_test_image();

  topsbot_cv::DmabufAllocator alloc;
  auto input = topsbot_cv_test::load_test_nv12(alloc);
  ASSERT_TRUE(input.has_value());

  const int out_w = topsbot_cv_test::align_down16(std::max(16, input->width / 2));
  const int out_h = std::max(16, input->height / 2);
  if (out_w >= input->width || out_h >= input->height) {
    GTEST_SKIP() << "test image too small for downsample resize";
  }

  auto config = make_config();
  topsbot_cv::apply_pipeline_feature(config, "resize");
  config.resize_rect.out_width = out_w;
  config.resize_rect.out_height = out_h;

  topsbot_cv::TaImage output;
  int actual_w = 0;
  int actual_h = 0;
  ASSERT_TRUE(
    run_pipeline(alloc, input->image, input->width, input->height, config, output, actual_w, actual_h));
  EXPECT_EQ(actual_w, out_w);
  EXPECT_EQ(actual_h, out_h);
  EXPECT_TRUE(output.is_nv12());
  EXPECT_TRUE(topsbot_cv::image_jpeg_enc_to_file(alloc, output, "/tmp/topsbot_cv_test_resize.jpg"));
}

TEST(TopsbotCvPipeline, CropResizeAndJpegEnc)
{
  topsbot_cv_test::skip_if_no_test_image();

  topsbot_cv::DmabufAllocator alloc;
  auto input = topsbot_cv_test::load_test_nv12(alloc);
  ASSERT_TRUE(input.has_value());

  const int crop_w = std::min(1280, input->width);
  const int crop_h = std::min(720, input->height);
  const int out_w = topsbot_cv_test::align_down16(640);
  const int out_h = 360;

  auto config = make_config();
  topsbot_cv::apply_pipeline_feature(config, "crop");
  topsbot_cv::apply_pipeline_feature(config, "resize");
  config.crop_rect.start_x = 0;
  config.crop_rect.start_y = 0;
  config.crop_rect.crop_w = crop_w;
  config.crop_rect.crop_h = crop_h;
  config.resize_rect.out_width = out_w;
  config.resize_rect.out_height = out_h;

  topsbot_cv::TaImage output;
  int actual_w = 0;
  int actual_h = 0;
  ASSERT_TRUE(
    run_pipeline(alloc, input->image, input->width, input->height, config, output, actual_w, actual_h));
  EXPECT_EQ(actual_w, out_w);
  EXPECT_EQ(actual_h, out_h);
  EXPECT_TRUE(output.is_nv12());
  EXPECT_TRUE(topsbot_cv::image_jpeg_enc_to_file(alloc, output, "/tmp/topsbot_cv_test_pipeline.jpg"));
}

TEST(TopsbotCvPipeline, FeatureOrderIndependent)
{
  topsbot_cv_test::skip_if_no_test_image();

  topsbot_cv::DmabufAllocator alloc;
  auto input = topsbot_cv_test::load_test_nv12(alloc);
  ASSERT_TRUE(input.has_value());

  const int out_w = topsbot_cv_test::align_down16(640);
  const int out_h = 360;

  auto config_a = make_config();
  topsbot_cv::apply_pipeline_feature(config_a, "crop");
  topsbot_cv::apply_pipeline_feature(config_a, "resize");
  config_a.crop_rect.crop_w = 960;
  config_a.crop_rect.crop_h = 540;
  config_a.resize_rect.out_width = out_w;
  config_a.resize_rect.out_height = out_h;

  auto config_b = make_config();
  topsbot_cv::apply_pipeline_feature(config_b, "resize");
  topsbot_cv::apply_pipeline_feature(config_b, "crop");
  config_b.crop_rect.crop_w = 960;
  config_b.crop_rect.crop_h = 540;
  config_b.resize_rect.out_width = out_w;
  config_b.resize_rect.out_height = out_h;

  topsbot_cv::TaImage output_a;
  topsbot_cv::TaImage output_b;
  int w_a = 0;
  int h_a = 0;
  int w_b = 0;
  int h_b = 0;
  ASSERT_TRUE(
    run_pipeline(alloc, input->image, input->width, input->height, config_a, output_a, w_a, h_a));
  ASSERT_TRUE(
    run_pipeline(alloc, input->image, input->width, input->height, config_b, output_b, w_b, h_b));
  EXPECT_EQ(w_a, w_b);
  EXPECT_EQ(h_a, h_b);
}

// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_cv/format_convert.hpp"
#include "topsbot_cv/imgproc.hpp"
#include "topsbot_cv/image_source.hpp"
#include "topsbot_cv/pipeline_executor.hpp"
#include "topsbot_cv/pipeline_log.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

void print_usage(const char * prog)
{
  std::cerr
    << "Usage: " << prog << " <image.jpg> [options]\n"
    << "  --dump-dir DIR --dump-prefix PREFIX\n"
    << "  --pipeline crop[,resize|csc|convert_padding]  (order ignored)\n"
    << "  --crop-x X --crop-y Y --crop-w W --crop-h H\n"
    << "  --resize-out-w W --resize-out-h H --resize-in-w W --resize-in-h H\n"
    << "  --csc-dst-format rgb_packed|bgr_packed|nv12\n"
    << "  --cp-dst-w W --cp-dst-h H\n"
    << "  --cp-crop-x X --cp-crop-y Y --cp-crop-w W --cp-crop-h H\n"
    << "  --cp-pad-x X --cp-pad-y Y --cp-pad-w W --cp-pad-h H\n";
}

int arg_int(int argc, char ** argv, int & i, const int fallback)
{
  if (i + 1 >= argc) {
    return fallback;
  }
  return std::atoi(argv[++i]);
}

std::vector<std::string> split_csv(const std::string & value)
{
  std::vector<std::string> parts;
  std::stringstream stream(value);
  std::string item;
  while (std::getline(stream, item, ',')) {
    if (!item.empty()) {
      parts.push_back(item);
    }
  }
  return parts;
}

}  // namespace

int main(int argc, char ** argv)
{
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const auto total_start = topsbot_cv::SteadyClock::now();

  const std::string image_file = argv[1];
  std::string dump_dir = "/tmp/topsbot_cv";
  std::string dump_prefix = "out_";
  std::string pipeline_csv = "crop";

  topsbot_cv::PipelineConfig config;
  config.crop_rect.start_x = 0;
  config.crop_rect.start_y = 0;
  config.crop_rect.crop_w = 960;
  config.crop_rect.crop_h = 540;
  config.resize_rect.out_width = 640;
  config.resize_rect.out_height = 360;
  config.convert_padding_dst_width = 640;
  config.convert_padding_dst_height = 640;
  config.convert_padding_attr.dst_crop_w = 640;
  config.convert_padding_attr.dst_crop_h = 640;

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--dump-dir" && i + 1 < argc) {
      dump_dir = argv[++i];
    } else if (arg == "--dump-prefix" && i + 1 < argc) {
      dump_prefix = argv[++i];
    } else if (arg == "--pipeline" && i + 1 < argc) {
      pipeline_csv = argv[++i];
    } else if (arg == "--crop-x") {
      config.crop_rect.start_x = arg_int(argc, argv, i, config.crop_rect.start_x);
    } else if (arg == "--crop-y") {
      config.crop_rect.start_y = arg_int(argc, argv, i, config.crop_rect.start_y);
    } else if (arg == "--crop-w") {
      config.crop_rect.crop_w = arg_int(argc, argv, i, config.crop_rect.crop_w);
    } else if (arg == "--crop-h") {
      config.crop_rect.crop_h = arg_int(argc, argv, i, config.crop_rect.crop_h);
    } else if (arg == "--resize-in-w") {
      config.resize_rect.in_width = arg_int(argc, argv, i, config.resize_rect.in_width);
    } else if (arg == "--resize-in-h") {
      config.resize_rect.in_height = arg_int(argc, argv, i, config.resize_rect.in_height);
    } else if (arg == "--resize-out-w") {
      config.resize_rect.out_width = arg_int(argc, argv, i, config.resize_rect.out_width);
    } else if (arg == "--resize-out-h") {
      config.resize_rect.out_height = arg_int(argc, argv, i, config.resize_rect.out_height);
    } else if (arg == "--csc-dst-format" && i + 1 < argc) {
      const auto parsed = topsbot_cv::parse_image_format(argv[++i]);
      if (!parsed) {
        std::cerr << "unsupported csc dst format: " << argv[i] << '\n';
        return 1;
      }
      config.csc_dst_format = *parsed;
    } else if (arg == "--cp-dst-w") {
      config.convert_padding_dst_width = arg_int(argc, argv, i, config.convert_padding_dst_width);
    } else if (arg == "--cp-dst-h") {
      config.convert_padding_dst_height = arg_int(argc, argv, i, config.convert_padding_dst_height);
    } else if (arg == "--cp-crop-x") {
      config.convert_padding_crop_rect.start_x = arg_int(
        argc, argv, i, config.convert_padding_crop_rect.start_x);
    } else if (arg == "--cp-crop-y") {
      config.convert_padding_crop_rect.start_y = arg_int(
        argc, argv, i, config.convert_padding_crop_rect.start_y);
    } else if (arg == "--cp-pad-x") {
      config.convert_padding_attr.dst_crop_stx = static_cast<unsigned int>(
        arg_int(argc, argv, i, static_cast<int>(config.convert_padding_attr.dst_crop_stx)));
    } else if (arg == "--cp-pad-y") {
      config.convert_padding_attr.dst_crop_sty = static_cast<unsigned int>(
        arg_int(argc, argv, i, static_cast<int>(config.convert_padding_attr.dst_crop_sty)));
    } else if (arg == "--cp-crop-w") {
      config.convert_padding_crop_rect.crop_w = arg_int(
        argc, argv, i, config.convert_padding_crop_rect.crop_w);
    } else if (arg == "--cp-crop-h") {
      config.convert_padding_crop_rect.crop_h = arg_int(
        argc, argv, i, config.convert_padding_crop_rect.crop_h);
    } else if (arg == "--cp-pad-w") {
      config.convert_padding_attr.dst_crop_w = static_cast<unsigned int>(
        arg_int(argc, argv, i, static_cast<int>(config.convert_padding_attr.dst_crop_w)));
    } else if (arg == "--cp-pad-h") {
      config.convert_padding_attr.dst_crop_h = static_cast<unsigned int>(
        arg_int(argc, argv, i, static_cast<int>(config.convert_padding_attr.dst_crop_h)));
    } else {
      std::cerr << "unknown argument: " << arg << '\n';
      print_usage(argv[0]);
      return 1;
    }
  }

  for (const auto & op_name : split_csv(pipeline_csv)) {
    topsbot_cv::apply_pipeline_feature(config, op_name);
  }
  if (!config.features.convert_padding &&
    !config.features.crop && !config.features.resize && !config.features.csc)
  {
    config.features.crop = true;
  }

  if (!std::filesystem::exists(image_file)) {
    std::cerr << "image not found: " << image_file << '\n';
    return 1;
  }

  std::cout << "[topsbot_cv] config: " << topsbot_cv::pipeline_config_summary(config) << '\n';

  topsbot_cv::DmabufAllocator alloc;
  if (!alloc.get()) {
    std::cerr << "failed to create dmabuf allocator\n";
    return 1;
  }

  const auto decode_start = topsbot_cv::SteadyClock::now();
  auto nv12 = topsbot_cv::ImageSource::from_file(alloc, image_file);
  const auto decode_end = topsbot_cv::SteadyClock::now();
  if (!nv12) {
    std::cerr << "failed to decode: " << image_file << '\n';
    return 1;
  }

  topsbot_cv::TaImage output;
  int out_w = 0;
  int out_h = 0;
  const auto pipeline_start = topsbot_cv::SteadyClock::now();
  if (!topsbot_cv::execute_pipeline(
      alloc, nv12->image, nv12->width, nv12->height, config, output, out_w, out_h))
  {
    std::cerr << "pipeline failed\n";
    return 1;
  }
  const auto pipeline_end = topsbot_cv::SteadyClock::now();

  std::filesystem::create_directories(dump_dir);
  const std::string dump_base = dump_dir + "/" + dump_prefix + "0";
  std::string out_path;
  const auto dump_start = topsbot_cv::SteadyClock::now();
  if (!topsbot_cv::image_dump_to_file(alloc, output, dump_base, out_path)) {
    std::cerr << "failed to dump pipeline output (format="
              << topsbot_cv::image_format_name(output.format()) << ")\n";
    return 1;
  }
  const auto dump_end = topsbot_cv::SteadyClock::now();

  topsbot_cv::PipelineTiming timing;
  timing.decode_ms = topsbot_cv::elapsed_ms(decode_start, decode_end);
  timing.pipeline_ms = topsbot_cv::elapsed_ms(pipeline_start, pipeline_end);
  timing.dump_ms = topsbot_cv::elapsed_ms(dump_start, dump_end);
  timing.total_ms = topsbot_cv::elapsed_ms(total_start, dump_end);

  topsbot_cv::print_pipeline_timing(
    std::cout,
    image_file,
    nv12->width,
    nv12->height,
    out_w,
    out_h,
    output.format(),
    timing,
    out_path);

  return 0;
}

// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__TEST_COMMON_HPP_
#define TOPSBOT_CV__TEST_COMMON_HPP_

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include "gtest/gtest.h"
#include "topsbot_cv/dmabuf_allocator.hpp"
#include "topsbot_cv/image_source.hpp"

namespace topsbot_cv_test
{

inline std::string default_test_image_path()
{
  try {
    const auto share = ament_index_cpp::get_package_share_directory("topsbot_cv");
    const auto path = share + "/imgs/test.jpg";
    if (std::filesystem::exists(path)) {
      return path;
    }
  } catch (const std::exception &) {
  }
  return "";
}

inline std::string test_image_path()
{
  if (const char * env = std::getenv("TOPSBOT_CV_TEST_IMAGE")) {
    if (env[0] != '\0') {
      return env;
    }
  }
  return default_test_image_path();
}

inline std::optional<topsbot_cv::Nv12Buffer> load_test_nv12(topsbot_cv::DmabufAllocator & alloc)
{
  const auto path = test_image_path();
  if (path.empty()) {
    return std::nullopt;
  }
  return topsbot_cv::ImageSource::from_file(alloc, path);
}

inline void skip_if_no_test_image()
{
  if (test_image_path().empty()) {
    GTEST_SKIP() << "no test image: install topsbot_cv imgs or set TOPSBOT_CV_TEST_IMAGE";
  }
}

inline int align_down16(const int value)
{
  return value & ~15;
}

}  // namespace topsbot_cv_test

#endif  // TOPSBOT_CV__TEST_COMMON_HPP_

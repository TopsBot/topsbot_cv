// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__FORMAT_CONVERT_HPP_
#define TOPSBOT_CV__FORMAT_CONVERT_HPP_

#include <cstddef>
#include <optional>
#include <string>

#include "ta_cv_api_ext_c.h"

namespace topsbot_cv
{

std::optional<ta_image_format_ext_t> parse_image_format(const std::string & name);

std::optional<csc_type_t> parse_csc_type(const std::string & name);

/// Byte size for dmabuf allocation (0 if unknown).
size_t byte_size_for_format(ta_image_format_ext_t format, int width, int height);

/// ROS sensor_msgs encoding string for supported publish formats.
std::optional<std::string> ros_encoding_for_format(ta_image_format_ext_t format);

int align_down16(int value);

}  // namespace topsbot_cv

#endif  // TOPSBOT_CV__FORMAT_CONVERT_HPP_

// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__NODE_PARAMS_HPP_
#define TOPSBOT_CV__NODE_PARAMS_HPP_

#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "topsbot_cv/pipeline_executor.hpp"

namespace topsbot_cv
{

/// Image input message family (TOPSBOT soft convention); ``File`` is non-subscription.
enum class InputMsg
{
  File,
  TbJpeg,
  TbImg,
  SensorImage,
  SensorCompressed,
};

struct NodeParams
{
  InputMsg input_msg{InputMsg::SensorImage};
  std::string image_file;
  std::string input_topic{"/image"};
  std::string output_topic{"/image_cv_out"};
  std::string tb_img_profile{"auto"};
  std::string output_tb_img_profile{"auto"};
  double publish_rate{1.0};
  bool need_dump{false};
  bool publish_output{true};
  bool publish_zero_copy{false};
  std::string dump_dir{"/tmp/topsbot_cv"};
  std::string dump_prefix{"out_"};
  PipelineConfig pipeline_config;
};

InputMsg parse_input_msg(const std::string & value);
InputMsg parse_legacy_input_mode(const std::string & input_mode, bool input_compressed);
std::string input_msg_to_string(InputMsg msg);
std::string resolve_tb_img_profile(const std::string & profile, int width, int height);

/// Read parameters already declared via yaml overrides; never calls declare_parameter.
NodeParams load_node_params(rclcpp::Node & node);

}  // namespace topsbot_cv

#endif  // TOPSBOT_CV__NODE_PARAMS_HPP_

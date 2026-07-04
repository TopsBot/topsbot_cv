// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#include "topsbot_cv/image_processor_node.hpp"

#include "rclcpp/rclcpp.hpp"

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::NodeOptions options;
  options.use_global_arguments(true);
  options.automatically_declare_parameters_from_overrides(true);
  auto node = std::make_shared<topsbot_cv::ImageProcessorNode>(options);
  node->schedule_initialize();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}

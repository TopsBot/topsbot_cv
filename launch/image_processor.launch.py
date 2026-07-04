# Copyright 2026 TOPSBOT contributors.
# SPDX-License-Identifier: Apache-2.0
#
# Parameter merge order (later wins):
#   1. params_file — scene template yaml under config/
#   2. launch overrides — input_topic, input_msg, output_topic, tb_img_profile,
#      pipeline, need_dump, dump_dir (non-empty only)
#
# List built-in configs:
#   ls $(ros2 pkg prefix topsbot_cv)/share/topsbot_cv/config/*.yaml

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _optional_bool(value: str):
    if not value:
        return None
    return value.lower() in ('true', '1', 'yes')


def _resolve_params_path(pkg_share: str, params_file: str) -> str:
    if not params_file:
        raise RuntimeError('image_processor.launch: params_file is empty')
    if os.path.isabs(params_file):
        path = params_file
    else:
        path = os.path.join(pkg_share, 'config', params_file)
    if not os.path.isfile(path):
        raise RuntimeError(f'image_processor.launch: params_file not found: {path}')
    return path


def _launch_setup(context, *args, **kwargs):
    pkg_share = get_package_share_directory('topsbot_cv')
    params_path = _resolve_params_path(
        pkg_share, LaunchConfiguration('params_file').perform(context))

    parameters = [params_path]
    overrides = {}

    input_topic = LaunchConfiguration('input_topic').perform(context)
    if input_topic:
        overrides['input_topic'] = input_topic

    input_msg = LaunchConfiguration('input_msg').perform(context)
    if input_msg:
        overrides['input_msg'] = input_msg

    output_topic = LaunchConfiguration('output_topic').perform(context)
    if output_topic:
        overrides['output_topic'] = output_topic

    tb_img_profile = LaunchConfiguration('tb_img_profile').perform(context)
    if tb_img_profile:
        overrides['tb_img_profile'] = tb_img_profile

    need_dump = _optional_bool(LaunchConfiguration('need_dump').perform(context))
    if need_dump is not None:
        overrides['need_dump'] = need_dump

    dump_dir = LaunchConfiguration('dump_dir').perform(context)
    if dump_dir:
        overrides['dump_dir'] = dump_dir

    pipeline = LaunchConfiguration('pipeline').perform(context)
    if pipeline:
        overrides['pipeline'] = [op.strip() for op in pipeline.split(',') if op.strip()]

    if overrides:
        parameters.append(overrides)

    return [
        Node(
            package='topsbot_cv',
            executable='topsbot_cv_image_processor',
            name='image_processor',
            output='screen',
            parameters=parameters,
        ),
    ]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'params_file',
            default_value='tb_jpeg_image_csc_640x480.yaml',
            description='Full params yaml under config/ (or absolute path)',
        ),
        DeclareLaunchArgument(
            'input_topic',
            default_value='',
            description='Override input_topic',
        ),
        DeclareLaunchArgument(
            'input_msg',
            default_value='',
            description='Override input_msg: tb_jpeg | tb_img | sensor_image | sensor_compressed',
        ),
        DeclareLaunchArgument(
            'output_topic',
            default_value='',
            description='Override output_topic',
        ),
        DeclareLaunchArgument(
            'tb_img_profile',
            default_value='',
            description='Override TbMsg tier for input_msg=tb_img: 480p | 540p | 1080p',
        ),
        DeclareLaunchArgument(
            'need_dump',
            default_value='',
            description='Override need_dump (true/false)',
        ),
        DeclareLaunchArgument(
            'dump_dir',
            default_value='',
            description='Override dump_dir when need_dump is true',
        ),
        DeclareLaunchArgument(
            'pipeline',
            default_value='',
            description=(
                'Override pipeline ops (comma-separated), e.g. crop,resize,csc '
                'or convert_padding'
            ),
        ),
        OpaqueFunction(function=_launch_setup),
    ])

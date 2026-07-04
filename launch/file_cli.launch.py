# Copyright 2026 TOPSBOT contributors.
# SPDX-License-Identifier: Apache-2.0
#
# Offline file pipeline (topsbot_cv_file_cli).
# Merge order: params_file → image_file / pipeline / dump overrides.

import os

from ament_index_python.packages import get_package_prefix, get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def _load_yaml_params(path: str) -> dict:
    import yaml
    with open(path, 'r', encoding='utf-8') as handle:
        data = yaml.safe_load(handle) or {}
    return data.get('/**', {}).get('ros__parameters', data)


def _resolve_params_path(pkg_share: str, params_file: str) -> str:
    if not params_file:
        raise RuntimeError('file_cli.launch: params_file is empty')
    if os.path.isabs(params_file):
        path = params_file
    else:
        path = os.path.join(pkg_share, 'config', params_file)
    if not os.path.isfile(path):
        raise RuntimeError(f'file_cli.launch: params_file not found: {path}')
    return path


def _append_flag(cmd: list, params: dict, key: str, flag: str) -> None:
    if key in params:
        cmd.extend([flag, str(params[key])])


def _launch_setup(context, *args, **kwargs):
    pkg_share = get_package_share_directory('topsbot_cv')
    params_path = _resolve_params_path(
        pkg_share, LaunchConfiguration('params_file').perform(context))
    params = _load_yaml_params(params_path)

    image_file = LaunchConfiguration('image_file').perform(context)
    prefix = get_package_prefix('topsbot_cv')
    cli = os.path.join(prefix, 'lib', 'topsbot_cv', 'topsbot_cv_file_cli')
    pipeline_ops = params.get('pipeline', ['crop'])
    pipeline_override = LaunchConfiguration('pipeline').perform(context)
    if pipeline_override:
        pipeline_ops = [op.strip() for op in pipeline_override.split(',') if op.strip()]

    cmd = [
        cli,
        image_file,
        '--dump-dir', LaunchConfiguration('dump_dir').perform(context) or str(
            params.get('dump_dir', '/tmp/topsbot_cv')),
        '--dump-prefix', str(params.get('dump_prefix', 'out_')),
        '--pipeline', ','.join(pipeline_ops),
    ]
    _append_flag(cmd, params, 'crop.start_x', '--crop-x')
    _append_flag(cmd, params, 'crop.start_y', '--crop-y')
    _append_flag(cmd, params, 'crop.crop_w', '--crop-w')
    _append_flag(cmd, params, 'crop.crop_h', '--crop-h')
    _append_flag(cmd, params, 'resize.in_width', '--resize-in-w')
    _append_flag(cmd, params, 'resize.in_height', '--resize-in-h')
    _append_flag(cmd, params, 'resize.out_width', '--resize-out-w')
    _append_flag(cmd, params, 'resize.out_height', '--resize-out-h')
    if 'csc.dst_format' in params:
        cmd.extend(['--csc-dst-format', str(params['csc.dst_format'])])
    _append_flag(cmd, params, 'convert_padding.dst_width', '--cp-dst-w')
    _append_flag(cmd, params, 'convert_padding.dst_height', '--cp-dst-h')
    _append_flag(cmd, params, 'convert_padding.crop.start_x', '--cp-crop-x')
    _append_flag(cmd, params, 'convert_padding.crop.start_y', '--cp-crop-y')
    _append_flag(cmd, params, 'convert_padding.crop.crop_w', '--cp-crop-w')
    _append_flag(cmd, params, 'convert_padding.crop.crop_h', '--cp-crop-h')
    _append_flag(cmd, params, 'convert_padding.padding.dst_crop_stx', '--cp-pad-x')
    _append_flag(cmd, params, 'convert_padding.padding.dst_crop_sty', '--cp-pad-y')
    _append_flag(cmd, params, 'convert_padding.padding.dst_crop_w', '--cp-pad-w')
    _append_flag(cmd, params, 'convert_padding.padding.dst_crop_h', '--cp-pad-h')

    return [ExecuteProcess(cmd=cmd, output='screen')]


def generate_launch_description():
    default_image = PathJoinSubstitution([
        FindPackageShare('topsbot_cv'),
        'imgs',
        'test.jpg',
    ])

    return LaunchDescription([
        DeclareLaunchArgument(
            'params_file',
            default_value='file_image_csc.yaml',
            description='Full file-mode params yaml under config/',
        ),
        DeclareLaunchArgument(
            'image_file',
            default_value=default_image,
            description='Input JPEG for offline pipeline',
        ),
        DeclareLaunchArgument(
            'dump_dir',
            default_value='',
            description='Override dump_dir; empty = use params_file value',
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

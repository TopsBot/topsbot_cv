# topsbot_cv

[English](README.md)

# 功能介绍

`topsbot_cv` 在 MES20 开发板上做硬件加速图像处理（基于 **ta-cv**），提供裁剪、缩放、色彩转换和 letterbox 填充等能力。

- **离线单张 JPG**：不接相机，对本地图片跑 pipeline 并落盘验证
- **订阅相机话题**：配合 [`topsbot_usb_cam`](../topsbot_usb_cam) 实时处理画面
- **两类算子路径**：`image_csc`（crop / resize / CSC 合一）与 `convert_padding`（letterbox，常用于 YOLO 前处理）

处理结果可发布到 ROS 话题，或由 `need_dump` 保存到 `/tmp/topsbot_cv`。高实时推理场景也可直接链接 C++ 库 `libtopsbot_cv.so` 在应用内调用。

# 物料清单

| 序号 | 名称 | 说明 |
| ---- | ---- | ---- |
| 1 | TOPSBOT MES20 板卡 | ta-cv（`libtacocv.so`）与 dmabuf 环境 |
| 2 | `topsbot_usb_cam`（在线模式） | 提供 `/tbmem_jpeg`、`/tbmem_img` 或 `/image_raw` |
| 3 | 测试图（离线模式） | 包内 `share/topsbot_cv/imgs/test.jpg`（1920×1080） |

# 编译与安装

## PC 端（交叉编译）

```bash
# PC：topsbot_cc 工具链环境
cd topsbot_cc/topsbot_ws
./tools/build.sh -s topsbot_cv -c "-DBUILD_TESTING=ON"
```

将 `install/` 同步到板卡。

## MES20 板端（加载工作空间）

```bash
# MES20
source /opt/ros/humble/setup.bash
source <install>/setup.bash
```

板端单元测试（需硬件）：

```bash
# MES20
colcon test --packages-select topsbot_cv --cmake-args -DBUILD_TESTING=ON
colcon test-result --verbose
```

# 使用方式

场景配置为全量 yaml，位于 `config/`。两类模板与成对关系见 [`config/README.md`](config/README.md)。快速命令见 [`QUICKSTART.md`](QUICKSTART.md)。

**`pipeline` 是功能开关**：`image_csc` 模板里写了 `resize.*` / `csc.*`，但 `pipeline` 只有 `[crop]` 时不会执行对应算子；用 launch `pipeline:=crop,resize,csc` 组合，不必为每种组合单独建 yaml。

## file 模式（离线 JPG）

适用：不接相机，验证 crop / resize / CSC 或 letterbox。Launch：`file_cli.launch.py`。

**默认启动**

```bash
# MES20
ros2 launch topsbot_cv file_cli.launch.py
```

| 项 | 默认值 |
|----|--------|
| 场景 yaml | `file_image_csc.yaml`（`image_csc` API） |
| 输入 | `share/topsbot_cv/imgs/test.jpg` |
| 输出 | `/tmp/topsbot_cv/`（`need_dump: true`） |

**一条命令改场景 / 输入 / pipeline**

```bash
# MES20
ros2 launch topsbot_cv file_cli.launch.py \
  params_file:=file_convert_padding.yaml \
  image_file:=/path/to/test.jpg \
  pipeline:=convert_padding
```

| launch 参数 | 作用 |
|-------------|------|
| `params_file` | `file_image_csc.yaml` / `file_convert_padding.yaml` |
| `image_file` | 输入 JPG 路径 |
| `pipeline` | 算子组合，如 `crop,resize,csc` 或 `convert_padding` |
| `dump_dir` | 落盘目录 |

落盘规则：输出 NV12 → `.jpg`（硬件编码）；RGB/BGR → `.ppm`（便于目视检查 CSC）。

## 单节点模式（在线订阅）

适用：上游已发布图像（先启动 `topsbot_usb_cam`）。Launch：`image_processor.launch.py`。

**默认启动**（`tb_jpeg` + crop，`image_csc` 模板）

```bash
# MES20
ros2 launch topsbot_usb_cam usb_cam.launch.py params_file:=mjpeg_640x480_zc.yaml
ros2 launch topsbot_cv image_processor.launch.py
```

**一条命令改输入类型 / pipeline / 话题**

```bash
# MES20 — NV12 零拷贝
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_img_image_csc_640x480.yaml \
  pipeline:=crop,resize,csc

# MES20 — 多相机
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_img_image_csc_640x480.yaml input_topic:=/cam0/tbmem_img

# MES20 — letterbox（须用 convert_padding 模板）
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_jpeg_convert_padding_640x480.yaml
```

| launch 参数 | 作用 |
|-------------|------|
| `params_file` | 场景 yaml（`input_msg`、话题、默认 pipeline） |
| `pipeline` | 覆盖算子，如 `crop,resize,csc` |
| `input_topic` | 覆盖订阅话题 |
| `need_dump` / `dump_dir` | 是否落盘及目录 |

查看全部 launch 参数：`ros2 launch topsbot_cv image_processor.launch.py -s`

### 与 topsbot_usb_cam 成对（640×480）

| usb_cam `params_file` | cv `params_file`（image_csc） | cv `params_file`（letterbox） |
|-----------------------|-------------------------------|-------------------------------|
| `mjpeg_640x480_zc.yaml` | `tb_jpeg_image_csc_640x480.yaml` | `tb_jpeg_convert_padding_640x480.yaml` |
| `mjpeg2nv12_640x480_zc.yaml` | `tb_img_image_csc_640x480.yaml` | `tb_img_convert_padding_640x480.yaml` |
| `mjpeg2nv12_640x480_sensor.yaml` | `sensor_image_image_csc_640x480.yaml` | `sensor_image_convert_padding_640x480.yaml` |
| `mjpeg_640x480_compressed.yaml` | `sensor_compressed_image_csc_640x480.yaml` | `sensor_compressed_convert_padding_640x480.yaml` |

预览处理结果：零拷贝输出用 [`topsbot_webviz`](../topsbot_webviz)；`sensor_msgs` 输出可用 `rqt_image_view` 订阅 `output_topic`（默认 `/image_cv_out` 或 `/tbmem_cv_out`）。

# 接口说明

## ROS 话题

| 方向 | 话题 | 说明 |
|------|------|------|
| 订阅 | yaml 中 `input_topic` | 由 `input_msg` 决定消息类型 |
| 发布 | yaml 中 `output_topic` | 默认 `/image_cv_out` 或 `/tbmem_cv_out`（`publish_zero_copy: true` 时） |

`file` 模式不订阅 ROS 话题；结果写入 `dump_dir`。

## 节点参数

字段字典见 [`config/params_reference.yaml`](config/params_reference.yaml)（不自动加载）。按功能分组如下：

| 分组 | 主要参数 | 说明 |
|------|----------|------|
| 输入 | `input_msg`、`input_topic`、`tb_img_profile` | `file` / `tb_jpeg` / `tb_img` / `sensor_image` / `sensor_compressed` |
| 输出 | `output_topic`、`publish_output`、`publish_zero_copy`、`output_tb_img_profile` | 发布或仅落盘 |
| pipeline | `pipeline` | 功能开关：`crop`、`resize`、`csc`（image_csc）或 `convert_padding` |
| crop / resize / csc | `crop.*`、`resize.*`、`csc.*` | 仅 `pipeline` 含对应项时生效 |
| letterbox | `convert_padding.*` | 独立 API，与 image_csc 路径互斥 |
| 离线 / 调试 | `publish_rate`、`need_dump`、`dump_dir` | file 模式频率与落盘 |

内置场景 yaml（10 份）：

| 类型 | 命名 | 用途 |
|------|------|------|
| 板端 640×480 | `{input_msg}_image_csc_640x480.yaml` | 在线 crop / resize / CSC |
| 板端 640×480 | `{input_msg}_convert_padding_640x480.yaml` | 在线 letterbox |
| 离线 | `file_image_csc.yaml` / `file_convert_padding.yaml` | `file_cli.launch.py` |

参数合并顺序：`params_file` yaml → launch 覆盖（`pipeline`、`input_topic` 等，非空才生效）。详见 `config/README.md`。

## Launch 文件

| Launch | 用途 |
|--------|------|
| `file_cli.launch.py` | 离线 file 模式 |
| `image_processor.launch.py` | 在线单节点 |

各 launch 参数：`ros2 launch topsbot_cv <launch名> -s`

## C++ 库（可选）

应用内可直接链接 `libtopsbot_cv.so`：`execute_pipeline`、`image_csc`、`image_convert_padding`、`ImageSource` 等。类型与 ta-cv 一致，详见 `doc/taCV开发参考手册（内部版）.md`。

---

## 许可

Apache License 2.0

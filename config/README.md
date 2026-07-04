# config/ 目录说明

## 两类处理 API（对应两份模板）

| 模板后缀 | ta-cv API | `pipeline` 可启用算子 | 说明 |
|----------|-----------|----------------------|------|
| `*_image_csc*.yaml` | `ta_cv_image_csc_convert_to` | `crop`、`resize`、`csc` | 三算子合并一次硬件调用，顺序无关 |
| `*_convert_padding*.yaml` | `ta_cv_image_convert_padding` | `convert_padding` | 裁剪+缩放+画布填充（letterbox），**独立路径** |

模板里已写好对应算子的默认参数；换 crop/resize/csc 组合时**不必换 yaml**，改 `pipeline` 即可（见下文）。

## 内置场景 yaml（10 份）

### 板端订阅（640×480，配合 topsbot_usb_cam）

```
{input_msg}_{image_csc|convert_padding}_640x480.yaml
```

| `input_msg` | 典型 `input_topic` | 配合 usb_cam `params_file` |
|-------------|-------------------|------------------------------|
| `tb_jpeg` | `/tbmem_jpeg` | `mjpeg_640x480_zc.yaml` |
| `tb_img` | `/tbmem_img` | `mjpeg2nv12_640x480_zc.yaml` |
| `sensor_image` | `/image_raw` | `mjpeg2nv12_640x480_sensor.yaml` |
| `sensor_compressed` | `/image_raw/compressed` | `mjpeg_640x480_compressed.yaml` |

每种 `input_msg` 各 2 份：`image_csc`（crop/resize/csc）与 `convert_padding`（letterbox）。

### 离线文件（1920×1080 测试图）

| 文件 | API |
|------|-----|
| `file_image_csc.yaml` | crop / resize / csc |
| `file_convert_padding.yaml` | letterbox |

使用 `file_cli.launch.py`；默认输入 `share/topsbot_cv/imgs/test.jpg`。

## 图像订阅参数（TOPSBOT 软性约定）

| 参数 | 说明 |
|------|------|
| `input_topic` | 订阅话题（与上游成对写在 yaml） |
| `input_msg` | `tb_jpeg` / `tb_img` / `sensor_image` / `sensor_compressed` / `file` |
| `tb_img_profile` | **仅** `input_msg=tb_img`：`480p` / `540p` / `1080p` / `auto` |

旧参数 `input_mode` / `input_compressed` 节点内仍可映射（`WARN_ONCE`），新 yaml 请用 `input_msg`。

## 参数合并顺序（后者覆盖前者）

1. `params_file` — 上表场景 yaml
2. launch 覆盖 — `input_topic`、`input_msg`、`output_topic`、`tb_img_profile`、`pipeline`、`need_dump`、`dump_dir`（非空才写入）

### 常用启动

```bash
# 默认：tb_jpeg + crop（image_csc 模板）
ros2 launch topsbot_cv image_processor.launch.py

# 换输入类型
ros2 launch topsbot_cv image_processor.launch.py params_file:=tb_img_image_csc_640x480.yaml

# 同一 image_csc 模板，改 pipeline 组合（不必另建 yaml）
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_jpeg_image_csc_640x480.yaml pipeline:=crop,resize
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_jpeg_image_csc_640x480.yaml pipeline:=crop,resize,csc

# letterbox（convert_padding 模板）
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_jpeg_convert_padding_640x480.yaml

# 多相机：覆盖话题
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_img_image_csc_640x480.yaml input_topic:=/cam0/tbmem_img

# 离线
ros2 launch topsbot_cv file_cli.launch.py params_file:=file_image_csc.yaml
ros2 launch topsbot_cv file_cli.launch.py \
  params_file:=file_image_csc.yaml image_file:=/path/to/test.jpg
```

### 细调算子参数（无需新 yaml）

launch 未封装的字段可用 `--ros-args -p` 覆盖，例如：

```bash
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_jpeg_image_csc_640x480.yaml pipeline:=crop \
  --ros-args -p crop.crop_w:=480 -p crop.crop_h:=270
```

更完整的字段见 `params_reference.yaml`。

## 其他文件

| 文件 | 说明 |
|------|------|
| `params_reference.yaml` | 参数字典（**不自动加载**） |

列出已安装配置：

```bash
ls $(ros2 pkg prefix topsbot_cv)/share/topsbot_cv/config/*.yaml
```

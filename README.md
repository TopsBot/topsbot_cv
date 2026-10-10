# topsbot_cv

[中文文档](README_cn.md)

# Overview

`topsbot_cv` runs hardware-accelerated image processing on the MES20 board (**ta-cv**): crop, resize, color conversion, and letterbox padding.

- **Offline single JPG**: run a pipeline on a local file without a camera
- **Live subscription**: process frames from [`topsbot_usb_cam`](../topsbot_usb_cam)
- **Two API paths**: `image_csc` (crop / resize / CSC combined) and `convert_padding` (letterbox, common before YOLO)

Results can be published on ROS topics or saved under `/tmp/topsbot_cv` when `need_dump` is enabled. For low-latency inference, link `libtopsbot_cv.so` directly in your application.

# Bill of Materials

| # | Item | Notes |
| - | ---- | ----- |
| 1 | TOPSBOT MES20 board | ta-cv (`libtacocv.so`) and dmabuf support |
| 2 | `topsbot_usb_cam` (online mode) | Publishes `/tbmem_jpeg`, `/tbmem_img`, or `/image_raw` |
| 3 | Test image (offline) | Bundled `share/topsbot_cv/imgs/test.jpg` (1920×1080) |

# Build and Install

## PC (cross-compile)

```bash
# PC: topsbot_cc toolchain
cd topsbot_cc/topsbot_ws
./tools/scripts/build.sh -s topsbot_cv -c "-DBUILD_TESTING=ON"
```

Sync `install/` to the board.

## MES20 board (source workspace)

```bash
# MES20
source /opt/tbros/humble/setup.bash
source <install>/setup.bash
```

On-board unit tests (hardware required):

```bash
# MES20
colcon test --packages-select topsbot_cv --cmake-args -DBUILD_TESTING=ON
colcon test-result --verbose
```

# Usage

Scene configs are full yaml files under `config/`. Template families and usb_cam pairing: [`config/README.md`](config/README.md). Quick commands: [`QUICKSTART.md`](QUICKSTART.md).

**`pipeline` is a feature switch**: an `image_csc` yaml may define `resize.*` / `csc.*`, but only ops listed in `pipeline` run. Combine with `pipeline:=crop,resize,csc` at launch—no separate yaml per combo.

## File mode (offline JPG)

Use for pipeline validation without a camera. Launch: `file_cli.launch.py`.

**Default launch**

```bash
# MES20
ros2 launch topsbot_cv file_cli.launch.py
```

| Item | Default |
|------|---------|
| Scene yaml | `file_image_csc.yaml` (`image_csc` API) |
| Input | `share/topsbot_cv/imgs/test.jpg` |
| Output | `/tmp/topsbot_cv/` (`need_dump: true`) |

**One command for scene / input / pipeline** (include only what you need)

```bash
# MES20
ros2 launch topsbot_cv file_cli.launch.py \
  params_file:=file_convert_padding.yaml \
  image_file:=/path/to/test.jpg \
  pipeline:=convert_padding
```

| Launch argument | Purpose |
|-----------------|---------|
| `params_file` | `file_image_csc.yaml` / `file_convert_padding.yaml` |
| `image_file` | Input JPG path |
| `pipeline` | Op list, e.g. `crop,resize,csc` or `convert_padding` |
| `dump_dir` | Output directory |

Dump rules: NV12 → `.jpg` (hardware encode); RGB/BGR → `.ppm` (visual CSC check).

## Single-node mode (online subscription)

Use when upstream is already publishing (start `topsbot_usb_cam` first). Launch: `image_processor.launch.py`.

**Default launch** (`tb_jpeg` + crop, `image_csc` template)

```bash
# MES20
ros2 launch topsbot_usb_cam usb_cam.launch.py params_file:=mjpeg_640x480_zc.yaml
ros2 launch topsbot_cv image_processor.launch.py
```

**One command for input type / pipeline / topic**

```bash
# MES20 — NV12 zero-copy
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_img_image_csc_640x480.yaml \
  pipeline:=crop,resize,csc

# MES20 — multi-camera
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_img_image_csc_640x480.yaml input_topic:=/cam0/tbmem_img

# MES20 — letterbox (use convert_padding template)
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_jpeg_convert_padding_640x480.yaml
```

| Launch argument | Purpose |
|-----------------|---------|
| `params_file` | Scene yaml (`input_msg`, topic, default pipeline) |
| `pipeline` | Override ops, e.g. `crop,resize,csc` |
| `input_topic` | Override subscription topic |
| `need_dump` / `dump_dir` | Save to disk |

List all launch arguments: `ros2 launch topsbot_cv image_processor.launch.py -s`

### Pairing with topsbot_usb_cam (640×480)

| usb_cam `params_file` | cv `params_file` (image_csc) | cv `params_file` (letterbox) |
|-----------------------|------------------------------|------------------------------|
| `mjpeg_640x480_zc.yaml` | `tb_jpeg_image_csc_640x480.yaml` | `tb_jpeg_convert_padding_640x480.yaml` |
| `mjpeg2nv12_640x480_zc.yaml` | `tb_img_image_csc_640x480.yaml` | `tb_img_convert_padding_640x480.yaml` |
| `mjpeg2nv12_640x480_sensor.yaml` | `sensor_image_image_csc_640x480.yaml` | `sensor_image_convert_padding_640x480.yaml` |
| `mjpeg_640x480_compressed.yaml` | `sensor_compressed_image_csc_640x480.yaml` | `sensor_compressed_convert_padding_640x480.yaml` |

Preview output: zero-copy via [`topsbot_webviz`](../topsbot_webviz); `sensor_msgs` via `rqt_image_view` on `output_topic` (default `/image_cv_out` or `/tbmem_cv_out`).

# Interface Reference

## ROS topics

| Direction | Topic | Notes |
|-----------|-------|-------|
| Subscribe | `input_topic` in yaml | Message type from `input_msg` |
| Publish | `output_topic` in yaml | Default `/image_cv_out` or `/tbmem_cv_out` when `publish_zero_copy: true` |

File mode does not subscribe; results go to `dump_dir`.

## Node parameters

Field dictionary: [`config/params_reference.yaml`](config/params_reference.yaml) (reference only). Grouped summary:

| Group | Key parameters | Notes |
|-------|----------------|-------|
| Input | `input_msg`, `input_topic`, `tb_img_profile` | `file` / `tb_jpeg` / `tb_img` / `sensor_image` / `sensor_compressed` |
| Output | `output_topic`, `publish_output`, `publish_zero_copy`, `output_tb_img_profile` | Publish or dump only |
| Pipeline | `pipeline` | Feature flags: `crop`, `resize`, `csc` (image_csc) or `convert_padding` |
| crop / resize / csc | `crop.*`, `resize.*`, `csc.*` | Active only when listed in `pipeline` |
| Letterbox | `convert_padding.*` | Separate API, mutually exclusive with image_csc path |
| Offline / debug | `publish_rate`, `need_dump`, `dump_dir` | File mode rate and dump |

Built-in scene yaml (10 files):

| Kind | Name pattern | Use |
|------|--------------|-----|
| Board 640×480 | `{input_msg}_image_csc_640x480.yaml` | Online crop / resize / CSC |
| Board 640×480 | `{input_msg}_convert_padding_640x480.yaml` | Online letterbox |
| Offline | `file_image_csc.yaml` / `file_convert_padding.yaml` | `file_cli.launch.py` |

Merge order: `params_file` yaml → launch overrides (`pipeline`, `input_topic`, …, non-empty only). See `config/README.md`.

## Launch files

| Launch | Purpose |
|--------|---------|
| `file_cli.launch.py` | Offline file mode |
| `image_processor.launch.py` | Online single node |

List arguments: `ros2 launch topsbot_cv <launch> -s`

## C++ library (optional)

Link `libtopsbot_cv.so` in applications: `execute_pipeline`, `image_csc`, `image_convert_padding`, `ImageSource`, etc. Types match ta-cv; see `doc/taCV开发参考手册（内部版）.md`.

---

## License

Apache License 2.0

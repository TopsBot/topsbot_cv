# topsbot_cv 快速入门

完整说明见 `README_cn.md`。

## 1. 离线 file 模式

```bash
# MES20 — 默认（file_image_csc + test.jpg → /tmp/topsbot_cv）
ros2 launch topsbot_cv file_cli.launch.py

# MES20 — 改场景 / 输入 / pipeline
ros2 launch topsbot_cv file_cli.launch.py \
  params_file:=file_convert_padding.yaml \
  image_file:=/path/to/test.jpg \
  pipeline:=convert_padding
```

## 2. 单节点模式（在线）

```bash
# MES20 — 默认（需先启动 usb_cam）
ros2 launch topsbot_usb_cam usb_cam.launch.py params_file:=mjpeg_640x480_zc.yaml
ros2 launch topsbot_cv image_processor.launch.py

# MES20 — 换输入 / pipeline / 话题
ros2 launch topsbot_cv image_processor.launch.py params_file:=tb_img_image_csc_640x480.yaml
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_jpeg_image_csc_640x480.yaml pipeline:=crop,resize,csc
ros2 launch topsbot_cv image_processor.launch.py \
  params_file:=tb_img_image_csc_640x480.yaml input_topic:=/cam0/tbmem_img

# MES20 — letterbox
ros2 launch topsbot_cv image_processor.launch.py params_file:=tb_jpeg_convert_padding_640x480.yaml
```

## 3. 查看 launch 参数

```bash
ros2 launch topsbot_cv image_processor.launch.py -s
ros2 launch topsbot_cv file_cli.launch.py -s
```

## 4. 配置与成对关系

- 场景 yaml：`config/README.md`
- 参数字典：`config/params_reference.yaml`
- 与 `topsbot_usb_cam` 成对表见 `README_cn.md`

`pipeline` 为功能开关：`pipeline:=crop,resize,csc` 不必另建 yaml。

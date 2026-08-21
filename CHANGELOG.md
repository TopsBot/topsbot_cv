# Changelog

All notable changes to this package are documented here.

## 0.2.0 (2026-08-19)

### Added

- FastDDS dmabuf zero-copy I/O for the online processor (`FASTDDS_DMABUF_BACKEND=1` and `publish_zero_copy: true`):
  - **Input (`tb_img`)**: LOOKUP `phys_addr` and wrap the upstream NV12 payload as a ta-cv image (skip CPU `upload_nv12`).
  - **Input (`tb_jpeg`)**: JPEG decode can consume the JPEG dmabuf via `phys_addr` (`ImageSource::from_jpeg_bytes` / ffmpeg decode).
  - **Output**: pipeline (crop / resize / convert_padding) writes NV12 directly into the loaned `TbMsg` payload; publish without memcpy. Output `phys_addr` is filled for downstream hardware consumers.
- `LoanedTbImgPublisher::borrow_fill_publish()` so a callback can fill a loaned payload (including DMA writes) before publish.
- `predict_output_size()` to choose the `TbMsg` profile and borrow the output buffer before running the pipeline.

### Fixed

- Copy-publish path now sets `phys_addr = 0`. Reused loaned slots previously could leak the last frame's physical address, which downstream hardware consumers would LOOKUP incorrectly.

### Changed

- Zero-copy path is opportunistic: on borrow / LOOKUP / size / non-NV12 (e.g. CSC to RGB) failure, the node falls back to the existing copy path. ROS topics, yaml, and launch APIs are unchanged.

## 0.1.0 (2026-07-04)

### Added

- Hardware-accelerated `topsbot_cv` image processing package based on ta-cv.
- Offline file CLI for JPG pipeline validation.
- Online image processor node for ROS image and `tb_img` inputs.
- Scene configs for image CSC, convert padding, sensor image, sensor compressed image, `tb_img`, and `tb_jpeg` paths.
- Quickstart, bilingual README files, config reference, tests, and bundled sample image.

#!/usr/bin/env bash
# Smoke-test USB 640×480 pipeline scenes (camera on /dev/video0 required).
# Starts topsbot_usb_cam + topsbot_cv separately (no integrated cv launch).
# Usage: ./run_usb_cam_pipeline_regression.sh [setup.bash] [seconds_per_case]

set -euo pipefail

SETUP="${1:-/root/install/setup.bash}"
HOLD_SEC="${2:-10}"

# image_csc API: crop / resize / csc via pipeline launch override
IMAGE_CSC_PIPELINES=(
  crop
  resize
  csc
  crop,resize
  crop,resize,csc
)

pass=0
fail=0
skip=0

check_hz() {
  local topic="$1"
  local sec="$2"
  timeout "$sec" ros2 topic hz "$topic" --window 3 2>/dev/null | grep -q 'average rate'
}

kill_tree() {
  local pid="$1"
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}

run_compressed() {
  local pipeline="$1"
  local api="$2"
  local id="CMP-${api}-${pipeline}"
  local cv_params
  if [[ "$api" == "convert_padding" ]]; then
    cv_params="sensor_compressed_convert_padding_640x480.yaml"
    pipeline_arg=""
  else
    cv_params="sensor_compressed_image_csc_640x480.yaml"
    pipeline_arg="pipeline:=${pipeline}"
  fi
  echo ""
  echo ">>> [$id] params_file=${cv_params} ${pipeline_arg}"
  ros2 launch topsbot_usb_cam usb_cam.launch.py \
    params_file:=mjpeg_640x480_compressed.yaml \
    video_device:=/dev/video0 \
    image_width:=640 image_height:=480 framerate:=15 \
    >/tmp/topsbot_cv_regress_cam_"${api}"_"${pipeline}".log 2>&1 &
  local cam_pid=$!
  sleep 3
  ros2 launch topsbot_cv image_processor.launch.py \
    params_file:="${cv_params}" \
    ${pipeline_arg} \
    >/tmp/topsbot_cv_regress_"${api}"_"${pipeline}".log 2>&1 &
  local cv_pid=$!
  sleep 5
  if check_hz /image_cv_out "$HOLD_SEC"; then
    echo "PASS $id (/image_cv_out)"
    pass=$((pass + 1))
  else
    echo "FAIL $id — cam: /tmp/topsbot_cv_regress_cam_${api}_${pipeline}.log cv: /tmp/topsbot_cv_regress_${api}_${pipeline}.log"
    fail=$((fail + 1))
  fi
  kill_tree "$cv_pid"
  kill_tree "$cam_pid"
  sleep 2
}

run_zc() {
  local pipeline="$1"
  local api="$2"
  local id="ZC-${api}-${pipeline}"
  if [[ "$api" == "image_csc" && "$pipeline" == *csc* ]]; then
    echo "SKIP $id (CSC rgb output incompatible with publish_zero_copy on zc path)"
    skip=$((skip + 1))
    return
  fi
  local cv_params
  local pipeline_arg=""
  if [[ "$api" == "convert_padding" ]]; then
    cv_params="tb_img_convert_padding_640x480.yaml"
  else
    cv_params="tb_img_image_csc_640x480.yaml"
    pipeline_arg="pipeline:=${pipeline}"
  fi
  echo ""
  echo ">>> [$id] params_file=${cv_params} ${pipeline_arg}"
  ros2 launch topsbot_usb_cam usb_cam.launch.py \
    params_file:=mjpeg2nv12_640x480_zc.yaml \
    video_device:=/dev/video0 \
    >/tmp/topsbot_cv_regress_zc_cam_"${api}"_"${pipeline}".log 2>&1 &
  local cam_pid=$!
  sleep 3
  ros2 launch topsbot_cv image_processor.launch.py \
    params_file:="${cv_params}" \
    ${pipeline_arg} \
    >/tmp/topsbot_cv_regress_zc_"${api}"_"${pipeline}".log 2>&1 &
  local cv_pid=$!
  sleep 5
  if check_hz /tbmem_cv_out "$HOLD_SEC"; then
    echo "PASS $id (/tbmem_cv_out)"
    pass=$((pass + 1))
  else
    echo "FAIL $id — cam: /tmp/topsbot_cv_regress_zc_cam_${api}_${pipeline}.log cv: /tmp/topsbot_cv_regress_zc_${api}_${pipeline}.log"
    fail=$((fail + 1))
  fi
  kill_tree "$cv_pid"
  kill_tree "$cam_pid"
  sleep 2
}

echo "topsbot_cv USB pipeline regression (image_csc + convert_padding templates)"
echo "hold=${HOLD_SEC}s per case"

for s in "${IMAGE_CSC_PIPELINES[@]}"; do
  run_compressed "$s" image_csc
done
run_compressed letterbox convert_padding

for s in "${IMAGE_CSC_PIPELINES[@]}"; do
  run_zc "$s" image_csc
done
run_zc letterbox convert_padding

echo ""
echo "Done: pass=$pass fail=$fail skip=$skip"
[[ "$fail" -eq 0 ]]

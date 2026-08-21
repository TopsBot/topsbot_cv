#ifndef TOPSBOT_CV__FFMPEG_DECODE_UTIL_H_
#define TOPSBOT_CV__FFMPEG_DECODE_UTIL_H_

#include "BufferAllocator/BufferAllocatorWrapper.h"
#include "ta_cv_api_ext_c.h"

#include <cstddef>
#include <sys/mman.h>
#include <unistd.h>

struct NV12Frame
{
  int width{0};
  int height{0};
  int data_size{0};
  int buf_fd{-1};
  ta_image_t src_image{};
};

/// Parse JPEG SOF marker; returns 0 on success.
int parse_jpeg_dimensions(
  const unsigned char * jpeg_data,
  size_t jpeg_size,
  int * width,
  int * height);

int decode_to_nv12(BufferAllocator * allocator, const char * file_path, NV12Frame * out_frame);

/// Hardware JPEG decode to NV12. If width_hint/height_hint > 0, skip header parse.
int decode_jpeg_bytes_to_nv12(
  BufferAllocator * allocator,
  const unsigned char * jpeg_data,
  size_t jpeg_size,
  NV12Frame * out_frame,
  int width_hint = 0,
  int height_hint = 0,
  uint64_t dma_phys = 0);

void release_nv12(NV12Frame * frame);

#endif  // TOPSBOT_CV__FFMPEG_DECODE_UTIL_H_

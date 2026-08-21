#include "ffmpeg_decode_util.h"

#include "ta_cv_api_ext_c.h"
#include "ta_cv_log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#define TACV_ERROR(...) TA_CV_LOG(TA_CV_LOG_LEVEL_ERROR, __VA_ARGS__)
#define TACV_INFO(...) TA_CV_LOG(TA_CV_LOG_LEVEL_INFO, __VA_ARGS__)

namespace
{
bool is_sof_marker(const unsigned char marker)
{
  return (marker >= 0xC0 && marker <= 0xC3) ||
         (marker >= 0xC5 && marker <= 0xCB) ||
         marker == 0xC9;
}

int resolve_jpeg_dimensions(
  const unsigned char * jpeg_data,
  const size_t jpeg_size,
  const int width_hint,
  const int height_hint,
  int * width,
  int * height)
{
  if (!jpeg_data || jpeg_size == 0 || !width || !height) {
    return -1;
  }

  if (width_hint > 0 && height_hint > 0) {
    *width = width_hint;
    *height = height_hint;
    return 0;
  }

  return parse_jpeg_dimensions(jpeg_data, jpeg_size, width, height);
}

int decode_jpeg_buffer_to_nv12(
  BufferAllocator * allocator,
  const unsigned char * jpeg_data,
  const size_t jpeg_size,
  NV12Frame * out_frame,
  const int width_hint,
  const int height_hint,
  const uint64_t dma_phys)
{
  if (!allocator || !jpeg_data || jpeg_size == 0 || !out_frame) {
    TACV_ERROR("invalid arguments");
    return -1;
  }

  *out_frame = NV12Frame{};

  int width = 0;
  int height = 0;
  if (resolve_jpeg_dimensions(jpeg_data, jpeg_size, width_hint, height_hint, &width, &height) != 0) {
    TACV_ERROR("failed to resolve JPEG dimensions");
    return -1;
  }

  const int nv12_buf_size = width * height * 3 / 2;
  const int out_fd = DmabufHeapAlloc(allocator, "carveout-heap", static_cast<size_t>(nv12_buf_size));
  if (out_fd < 0) {
    TACV_ERROR("failed to allocate output block");
    return -1;
  }

  ta_image_t tmp_image{};
  const int create_ret = ta_cv_image_create_ext(height, width, FORMAT_NV12, &tmp_image, out_fd);
  if (create_ret != 0) {
    TACV_ERROR("failed to create image");
    close(out_fd);
    return -1;
  }

  int dec_ret = -1;
  int in_fd = -1;
  if (dma_phys != 0) {
    // Zero-copy path: the JPEG payload already lives in a carveout segment
    // (FastDDS dmabuf backend). Wrap its phys into an fd (offset view) and
    // hand it straight to the hardware decoder — no copy into a fresh buffer.
    // View length must cover the whole JPEG (kernel page-aligns it); a too
    // small view breaks the HW decoder's mmap of the input.
    uint64_t len = jpeg_size;
    in_fd = DmabufHeapLookupByPhys(allocator, dma_phys, &len);
    if (in_fd >= 0) {
      dec_ret = ta_cv_image_jpeg_dec(jpeg_size, in_fd, &tmp_image);
      close(in_fd);
      if (dec_ret != 0) {
        TACV_ERROR("zero-copy JPEG decode failed (%zu bytes, phys 0x%llx)",
                   jpeg_size, static_cast<unsigned long long>(dma_phys));
      } else {
        TACV_INFO("JPEG zero-copy decode: %dx%d -> NV12 (%d bytes), phys 0x%llx",
                  width, height, nv12_buf_size,
                  static_cast<unsigned long long>(dma_phys));
      }
    } else {
      TACV_ERROR("LOOKUP_BY_PHYS failed for 0x%llx (%d), falling back to copy",
                 static_cast<unsigned long long>(dma_phys), in_fd);
    }
  }

  if (dec_ret != 0) {
    // Fallback path: copy JPEG bytes into a fresh carveout buffer (also used
    // when the dmabuf backend is not active, dma_phys == 0).
    in_fd = DmabufHeapAlloc(allocator, "carveout-heap", jpeg_size);
    if (in_fd < 0) {
      TACV_ERROR("failed to allocate input block");
      ta_cv_image_destroy_ext(&tmp_image);
      close(out_fd);
      return -1;
    }
    void * in_buf = mmap(nullptr, jpeg_size, PROT_READ | PROT_WRITE, MAP_SHARED, in_fd, 0);
    if (in_buf == MAP_FAILED) {
      TACV_ERROR("failed to mmap input block");
      close(in_fd);
      ta_cv_image_destroy_ext(&tmp_image);
      close(out_fd);
      return -1;
    }
    std::memcpy(in_buf, jpeg_data, jpeg_size);
    munmap(in_buf, jpeg_size);

    dec_ret = ta_cv_image_jpeg_dec(jpeg_size, in_fd, &tmp_image);
    close(in_fd);
    if (dec_ret != 0) {
      TACV_ERROR("failed to decode JPEG (%zu bytes)", jpeg_size);
      ta_cv_image_destroy_ext(&tmp_image);
      close(out_fd);
      return -1;
    }
  }

  out_frame->data_size = nv12_buf_size;
  out_frame->width = width;
  out_frame->height = height;
  out_frame->src_image = tmp_image;
  out_frame->buf_fd = out_fd;

  TACV_INFO("JPEG decoded: %dx%d -> NV12 (%d bytes)", width, height, nv12_buf_size);
  return 0;
}
}  // namespace

int parse_jpeg_dimensions(
  const unsigned char * jpeg_data,
  const size_t jpeg_size,
  int * width,
  int * height)
{
  if (!jpeg_data || jpeg_size < 4 || !width || !height) {
    return -1;
  }

  size_t pos = 0;
  if (jpeg_data[0] == 0xFF && jpeg_data[1] == 0xD8) {
    pos = 2;
  }

  while (pos + 1 < jpeg_size) {
    if (jpeg_data[pos] != 0xFF) {
      ++pos;
      continue;
    }
    ++pos;
    while (pos < jpeg_size && jpeg_data[pos] == 0xFF) {
      ++pos;
    }
    if (pos >= jpeg_size) {
      break;
    }

    const unsigned char marker = jpeg_data[pos++];
    if (marker == 0xD9) {
      break;
    }
    if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
      continue;
    }

    if (pos + 2 > jpeg_size) {
      break;
    }
    const uint16_t seg_len =
      (static_cast<uint16_t>(jpeg_data[pos]) << 8) | jpeg_data[pos + 1];
    if (seg_len < 2 || pos + seg_len > jpeg_size) {
      break;
    }

    if (is_sof_marker(marker) && seg_len >= 7) {
      const int parsed_h =
        (static_cast<int>(jpeg_data[pos + 3]) << 8) | jpeg_data[pos + 4];
      const int parsed_w =
        (static_cast<int>(jpeg_data[pos + 5]) << 8) | jpeg_data[pos + 6];
      if (parsed_w > 0 && parsed_h > 0) {
        *width = parsed_w;
        *height = parsed_h;
        return 0;
      }
      return -1;
    }

    pos += seg_len;
  }

  return -1;
}

int is_jpeg_file(const char * path)
{
  const char * dot = strrchr(path, '.');
  if (!dot) {
    return 0;
  }
  return (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0);
}

int decode_jpeg_bytes_to_nv12(
  BufferAllocator * allocator,
  const unsigned char * jpeg_data,
  const size_t jpeg_size,
  NV12Frame * out_frame,
  const int width_hint,
  const int height_hint,
  const uint64_t dma_phys)
{
  return decode_jpeg_buffer_to_nv12(
    allocator, jpeg_data, jpeg_size, out_frame, width_hint, height_hint, dma_phys);
}

int decode_to_nv12(BufferAllocator * allocator, const char * file_path, NV12Frame * out_frame)
{
  if (!allocator || !file_path || !out_frame) {
    TACV_ERROR("invalid arguments");
    return -1;
  }

  *out_frame = NV12Frame{};

  if (!is_jpeg_file(file_path)) {
    TACV_ERROR("only JPEG files are supported: %s", file_path);
    return -1;
  }

  FILE * fp = fopen(file_path, "rb");
  if (!fp) {
    TACV_ERROR("failed to open JPEG file: %s", file_path);
    return -1;
  }

  fseek(fp, 0, SEEK_END);
  const size_t jpeg_size = static_cast<size_t>(ftell(fp));
  fseek(fp, 0, SEEK_SET);
  if (jpeg_size == 0) {
    TACV_ERROR("empty JPEG file: %s", file_path);
    fclose(fp);
    return -1;
  }

  const int in_fd = DmabufHeapAlloc(allocator, "carveout-heap", jpeg_size);
  if (in_fd < 0) {
    TACV_ERROR("failed to allocate input block");
    fclose(fp);
    return -1;
  }

  void * in_buf = mmap(nullptr, jpeg_size, PROT_READ | PROT_WRITE, MAP_SHARED, in_fd, 0);
  if (in_buf == MAP_FAILED) {
    TACV_ERROR("failed to mmap input block");
    close(in_fd);
    fclose(fp);
    return -1;
  }

  if (fread(in_buf, 1, jpeg_size, fp) != jpeg_size) {
    TACV_ERROR("failed to read JPEG data from %s", file_path);
    munmap(in_buf, jpeg_size);
    close(in_fd);
    fclose(fp);
    return -1;
  }
  fclose(fp);

  int width = 0;
  int height = 0;
  if (parse_jpeg_dimensions(static_cast<const unsigned char *>(in_buf), jpeg_size, &width, &height) != 0) {
    TACV_ERROR("failed to parse JPEG dimensions from %s", file_path);
    munmap(in_buf, jpeg_size);
    close(in_fd);
    return -1;
  }

  munmap(in_buf, jpeg_size);

  ta_image_t tmp_image{};
  const int nv12_buf_size = width * height * 3 / 2;
  const int out_fd = DmabufHeapAlloc(allocator, "carveout-heap", static_cast<size_t>(nv12_buf_size));
  if (out_fd < 0) {
    TACV_ERROR("failed to allocate output block");
    close(in_fd);
    return -1;
  }

  const int create_ret = ta_cv_image_create_ext(height, width, FORMAT_NV12, &tmp_image, out_fd);
  if (create_ret != 0) {
    TACV_ERROR("failed to create image");
    close(out_fd);
    close(in_fd);
    return -1;
  }

  const int dec_ret = ta_cv_image_jpeg_dec(jpeg_size, in_fd, &tmp_image);
  close(in_fd);
  if (dec_ret != 0) {
    TACV_ERROR("failed to decode JPEG file %s", file_path);
    ta_cv_image_destroy_ext(&tmp_image);
    close(out_fd);
    return -1;
  }

  out_frame->data_size = nv12_buf_size;
  out_frame->width = width;
  out_frame->height = height;
  out_frame->src_image = tmp_image;
  out_frame->buf_fd = out_fd;

  TACV_INFO("JPEG decoded: %dx%d -> NV12 (%d bytes)", width, height, nv12_buf_size);
  return 0;
}

void release_nv12(NV12Frame * frame)
{
  if (!frame) {
    return;
  }
  ta_cv_image_destroy_ext(&frame->src_image);
  if (frame->buf_fd >= 0) {
    close(frame->buf_fd);
  }
  *frame = NV12Frame{};
}

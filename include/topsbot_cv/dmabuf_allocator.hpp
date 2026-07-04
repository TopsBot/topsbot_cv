// Copyright 2025 TOPSBOT
// SPDX-License-Identifier: Apache-2.0

#ifndef TOPSBOT_CV__DMABUF_ALLOCATOR_HPP_
#define TOPSBOT_CV__DMABUF_ALLOCATOR_HPP_

#include <cstddef>

struct BufferAllocator;

namespace topsbot_cv
{

class DmabufAllocator
{
public:
  DmabufAllocator() = default;
  ~DmabufAllocator();

  DmabufAllocator(const DmabufAllocator &) = delete;
  DmabufAllocator & operator=(const DmabufAllocator &) = delete;

  /// Returns nullptr if dmabuf heap allocator failed to initialize.
  BufferAllocator * get();

  /// Allocate dmabuf from carveout heap; returns fd or -1.
  int alloc(size_t size, const char * heap = "carveout-heap");

private:
  bool ensure_initialized();

  BufferAllocator * allocator_{nullptr};
};

}  // namespace topsbot_cv

#endif  // TOPSBOT_CV__DMABUF_ALLOCATOR_HPP_

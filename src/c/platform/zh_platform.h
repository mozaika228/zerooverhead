#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct zh_os_map_header {
  void* base;
  size_t size;
} zh_os_map_header_t;

size_t zh_platform_page_size(void);
void* zh_platform_map(size_t size);
void zh_platform_unmap(void* ptr, size_t size);
void* zh_platform_map_aligned(size_t size, size_t alignment);
void zh_platform_unmap_aligned(void* aligned_ptr);

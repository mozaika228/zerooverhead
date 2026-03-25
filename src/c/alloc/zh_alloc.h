#pragma once
#include <stddef.h>
#include <stdint.h>

struct zh_slab;

typedef struct zh_small_header {
  uint64_t magic;
  struct zh_slab* slab;
  uint32_t requested;
  uint32_t reserved;
} zh_small_header_t;

typedef struct zh_large_header {
  uint64_t magic;
  size_t size;
} zh_large_header_t;

typedef struct zh_medium_header {
  uint64_t magic;
  uint32_t requested;
  uint16_t arena_idx;
  uint16_t class_idx;
  struct zh_medium_header* next;
} zh_medium_header_t;

void* zh_alloc_small(size_t size);
void* zh_alloc_medium(size_t size);
void* zh_alloc_large(size_t size);
void zh_free_small(void* ptr);
void zh_free_medium(void* ptr);
void zh_free_large(void* ptr);
size_t zh_usable_small(void* ptr);
size_t zh_usable_medium(void* ptr);
size_t zh_usable_large(void* ptr);

void zh_tls_shutdown(void);

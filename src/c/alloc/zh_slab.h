#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../threads/zh_spinlock.h"

typedef struct zh_slab {
  uint64_t magic;
  uint16_t class_index;
  uint16_t block_size;
  uint32_t free_count;
  uint32_t total_count;
  uint8_t in_list;
  void* free_list;
  struct zh_slab* next;
  struct zh_slab* prev;
} zh_slab_t;

typedef struct zh_slab_class {
  size_t block_size;
  zh_spinlock_t lock;
  zh_slab_t* partial;
} zh_slab_class_t;

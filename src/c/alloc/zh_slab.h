#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include "../threads/zh_spinlock.h"

typedef enum zh_slab_list_kind {
  ZH_SLAB_LIST_NONE = 0,
  ZH_SLAB_LIST_PARTIAL = 1,
  ZH_SLAB_LIST_EMPTY = 2
} zh_slab_list_kind_t;

typedef struct zh_slab {
  uint64_t magic;
  uint16_t class_index;
  uint16_t block_size;
  uint32_t owner_thread;
  zh_spinlock_t lock;
  atomic_uintptr_t remote_head;
  uint32_t free_count;
  uint32_t total_count;
  uint64_t empty_epoch;
  uint8_t list_kind;
  uint8_t in_list;
  uint16_t reserved;
  void* free_list;
  struct zh_slab* next;
  struct zh_slab* prev;
} zh_slab_t;

typedef struct zh_slab_class {
  size_t block_size;
  zh_spinlock_t lock;
  zh_slab_t* partial;
  zh_slab_t* empty;
  uint32_t empty_count;
} zh_slab_class_t;

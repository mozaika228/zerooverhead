#pragma once
#include <stddef.h>

typedef struct zh_stats {
  size_t bytes_active;
  size_t bytes_allocated;
  size_t bytes_mapped;
  size_t alloc_count;
  size_t free_count;
} zh_stats_t;

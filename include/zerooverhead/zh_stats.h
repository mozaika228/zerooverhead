#pragma once
#include <stddef.h>

typedef struct zh_stats {
  size_t bytes_active;
  size_t bytes_allocated;
  size_t bytes_mapped;
  size_t alloc_count;
  size_t free_count;
  size_t remote_free_count;
  size_t quarantine_count;
  size_t invalid_free_count;
} zh_stats_t;

void zh_stats_get(zh_stats_t* out);

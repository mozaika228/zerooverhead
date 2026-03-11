#include "zerooverhead/zerooverhead.h"
#include "../zh_internal.h"
#include <stdatomic.h>

static atomic_size_t g_bytes_active = 0;
static atomic_size_t g_bytes_allocated = 0;
static atomic_size_t g_bytes_mapped = 0;
static atomic_size_t g_alloc_count = 0;
static atomic_size_t g_free_count = 0;

void zh_stats_on_alloc(size_t requested, size_t allocated) {
  atomic_fetch_add_explicit(&g_bytes_active, requested, memory_order_relaxed);
  atomic_fetch_add_explicit(&g_bytes_allocated, allocated, memory_order_relaxed);
  atomic_fetch_add_explicit(&g_alloc_count, 1, memory_order_relaxed);
}

void zh_stats_on_free(size_t requested, size_t allocated) {
  atomic_fetch_sub_explicit(&g_bytes_active, requested, memory_order_relaxed);
  atomic_fetch_sub_explicit(&g_bytes_allocated, allocated, memory_order_relaxed);
  atomic_fetch_add_explicit(&g_free_count, 1, memory_order_relaxed);
}

void zh_stats_on_map(size_t size) {
  atomic_fetch_add_explicit(&g_bytes_mapped, size, memory_order_relaxed);
}

void zh_stats_on_unmap(size_t size) {
  atomic_fetch_sub_explicit(&g_bytes_mapped, size, memory_order_relaxed);
}

void zh_stats_get(zh_stats_t* out) {
  if (!out) return;
  out->bytes_active = atomic_load_explicit(&g_bytes_active, memory_order_relaxed);
  out->bytes_allocated = atomic_load_explicit(&g_bytes_allocated, memory_order_relaxed);
  out->bytes_mapped = atomic_load_explicit(&g_bytes_mapped, memory_order_relaxed);
  out->alloc_count = atomic_load_explicit(&g_alloc_count, memory_order_relaxed);
  out->free_count = atomic_load_explicit(&g_free_count, memory_order_relaxed);
}

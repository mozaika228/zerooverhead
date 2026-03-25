#include "zerooverhead/zerooverhead.h"
#include "../alloc/zh_alloc.h"
#include "../zh_internal.h"
#include <stdatomic.h>

#define ZH_EPOCH_MAX_THREADS 256u

static atomic_uint g_thread_id_next = 1;
static ZH_THREAD_LOCAL uint32_t g_thread_id = 0;
static atomic_ullong g_epoch_global = 1;
static atomic_uint g_epoch_slot_used[ZH_EPOCH_MAX_THREADS];
static atomic_ullong g_epoch_slot_value[ZH_EPOCH_MAX_THREADS];
static ZH_THREAD_LOCAL uint32_t g_epoch_slot = UINT32_MAX;
static ZH_THREAD_LOCAL uint32_t g_epoch_depth = 0;

uint32_t zh_thread_id(void) {
  if (g_thread_id != 0) return g_thread_id;
  g_thread_id = atomic_fetch_add_explicit(&g_thread_id_next, 1, memory_order_relaxed);
  if (g_thread_id == 0) g_thread_id = atomic_fetch_add_explicit(&g_thread_id_next, 1, memory_order_relaxed);
  return g_thread_id;
}

static uint32_t zh_epoch_bind_slot(void) {
  if (g_epoch_slot != UINT32_MAX) return g_epoch_slot;
  for (uint32_t i = 0; i < ZH_EPOCH_MAX_THREADS; i++) {
    unsigned expected = 0;
    if (atomic_compare_exchange_strong_explicit(
      &g_epoch_slot_used[i], &expected, 1, memory_order_acq_rel, memory_order_relaxed
    )) {
      g_epoch_slot = i;
      return i;
    }
  }
  g_epoch_slot = zh_thread_id() % ZH_EPOCH_MAX_THREADS;
  return g_epoch_slot;
}

void zh_epoch_enter(void) {
  uint32_t slot = zh_epoch_bind_slot();
  if (g_epoch_depth++ == 0) {
    uint64_t e = atomic_load_explicit(&g_epoch_global, memory_order_acquire);
    atomic_store_explicit(&g_epoch_slot_value[slot], e, memory_order_release);
  }
}

void zh_epoch_leave(void) {
  if (g_epoch_depth == 0) return;
  g_epoch_depth--;
  if (g_epoch_depth == 0 && g_epoch_slot != UINT32_MAX) {
    atomic_store_explicit(&g_epoch_slot_value[g_epoch_slot], 0, memory_order_release);
  }
}

uint64_t zh_epoch_advance(void) {
  return atomic_fetch_add_explicit(&g_epoch_global, 1, memory_order_acq_rel) + 1;
}

uint64_t zh_epoch_min_active(void) {
  uint64_t min_e = atomic_load_explicit(&g_epoch_global, memory_order_acquire);
  int found = 0;
  for (uint32_t i = 0; i < ZH_EPOCH_MAX_THREADS; i++) {
    if (atomic_load_explicit(&g_epoch_slot_used[i], memory_order_acquire) == 0) continue;
    uint64_t e = atomic_load_explicit(&g_epoch_slot_value[i], memory_order_acquire);
    if (e == 0) continue;
    if (!found || e < min_e) min_e = e;
    found = 1;
  }
  return found ? min_e : 0;
}

void zh_thread_shutdown(void) {
  zh_tls_shutdown();
  if (g_epoch_slot != UINT32_MAX) {
    atomic_store_explicit(&g_epoch_slot_value[g_epoch_slot], 0, memory_order_release);
    atomic_store_explicit(&g_epoch_slot_used[g_epoch_slot], 0, memory_order_release);
    g_epoch_slot = UINT32_MAX;
  }
  g_epoch_depth = 0;
  g_thread_id = 0;
}

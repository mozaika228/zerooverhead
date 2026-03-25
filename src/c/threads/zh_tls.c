#include "zerooverhead/zerooverhead.h"
#include "../alloc/zh_alloc.h"
#include "../zh_internal.h"
#include <stdatomic.h>

static atomic_uint g_thread_id_next = 1;
static ZH_THREAD_LOCAL uint32_t g_thread_id = 0;

uint32_t zh_thread_id(void) {
  if (g_thread_id != 0) return g_thread_id;
  g_thread_id = atomic_fetch_add_explicit(&g_thread_id_next, 1, memory_order_relaxed);
  if (g_thread_id == 0) g_thread_id = atomic_fetch_add_explicit(&g_thread_id_next, 1, memory_order_relaxed);
  return g_thread_id;
}

void zh_thread_shutdown(void) {
  zh_tls_shutdown();
  g_thread_id = 0;
}

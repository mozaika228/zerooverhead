#include "../zh_internal.h"
#include "../platform/zh_platform.h"
#include "../utils/zh_align.h"

static atomic_int g_init = 0;
static size_t g_page_size = 0;
static size_t g_slab_size = ZH_SLAB_SIZE_DEFAULT;

size_t zh_core_page_size(void) {
  return g_page_size ? g_page_size : zh_platform_page_size();
}

size_t zh_core_slab_size(void) {
  return g_slab_size;
}

void zh_core_init_once(void) {
  int expected = 0;
  if (!atomic_compare_exchange_strong(&g_init, &expected, 1)) return;
  g_page_size = zh_platform_page_size();
  if (!zh_is_pow2(g_slab_size)) g_slab_size = ZH_SLAB_SIZE_DEFAULT;
  if (g_slab_size < g_page_size) g_slab_size = g_page_size;
}

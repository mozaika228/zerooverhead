#include "zerooverhead/zerooverhead.h"
#include "alloc/zh_alloc.h"
#include "zh_internal.h"
#include <stdatomic.h>

static atomic_int g_mode = ZH_MODE_PERF;

void zh_set_mode(zh_mode_t mode) {
  atomic_store_explicit(&g_mode, (int)mode, memory_order_relaxed);
}

zh_mode_t zh_get_mode(void) {
  return (zh_mode_t)atomic_load_explicit(&g_mode, memory_order_relaxed);
}

int zh_mode_is_hardened(void) {
  return zh_get_mode() == ZH_MODE_HARDENED;
}

void* zh_malloc(size_t size) {
  zh_epoch_enter();
  void* out = 0;
  if (size <= ZH_SMALL_MAX) {
    void* p = zh_alloc_small(size);
    if (p) { out = p; goto done; }
  } else if (size <= ZH_MEDIUM_MAX) {
    void* p = zh_alloc_medium(size);
    if (p) { out = p; goto done; }
  }
  out = zh_alloc_large(size);
done:
  zh_epoch_leave();
  return out;
}

void zh_free(void* ptr) {
  if (!ptr) return;
  zh_epoch_enter();
  zh_small_header_t* h = ((zh_small_header_t*)ptr) - 1;
  if (h->magic == ZH_SMALL_MAGIC) {
    zh_free_small(ptr);
    zh_epoch_leave();
    return;
  }
  zh_medium_header_t* m = ((zh_medium_header_t*)ptr) - 1;
  if (m->magic == ZH_MEDIUM_MAGIC) {
    zh_free_medium(ptr);
    zh_epoch_leave();
    return;
  }
  zh_free_large(ptr);
  zh_epoch_leave();
}

void* zh_realloc(void* ptr, size_t size) {
  if (!ptr) return zh_malloc(size);
  if (size == 0) { zh_free(ptr); return 0; }
  size_t old_size = zh_usable_size(ptr);
  void* next = zh_malloc(size);
  if (!next) return 0;
  size_t copy = old_size < size ? old_size : size;
  if (copy) {
    unsigned char* d = (unsigned char*)next;
    unsigned char* s = (unsigned char*)ptr;
    for (size_t i = 0; i < copy; i++) d[i] = s[i];
  }
  zh_free(ptr);
  return next;
}

size_t zh_usable_size(void* ptr) {
  if (!ptr) return 0;
  zh_epoch_enter();
  size_t out = 0;
  zh_small_header_t* h = ((zh_small_header_t*)ptr) - 1;
  if (h->magic == ZH_SMALL_MAGIC) { out = zh_usable_small(ptr); goto done; }
  zh_medium_header_t* m = ((zh_medium_header_t*)ptr) - 1;
  if (m->magic == ZH_MEDIUM_MAGIC) { out = zh_usable_medium(ptr); goto done; }
  out = zh_usable_large(ptr);
done:
  zh_epoch_leave();
  return out;
}

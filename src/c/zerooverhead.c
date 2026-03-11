#include "zerooverhead/zerooverhead.h"
#include "alloc/zh_alloc.h"
#include "zh_internal.h"

void* zh_malloc(size_t size) {
  void* p = zh_alloc_small(size);
  if (p) return p;
  return zh_alloc_large(size);
}

void zh_free(void* ptr) {
  if (!ptr) return;
  zh_small_header_t* h = ((zh_small_header_t*)ptr) - 1;
  if (h->magic == ZH_SMALL_MAGIC) {
    zh_free_small(ptr);
    return;
  }
  zh_large_header_t* l = ((zh_large_header_t*)ptr) - 1;
  if (l->magic == ZH_LARGE_MAGIC) {
    zh_free_large(ptr);
    return;
  }
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
  zh_small_header_t* h = ((zh_small_header_t*)ptr) - 1;
  if (h->magic == ZH_SMALL_MAGIC) return zh_usable_small(ptr);
  zh_large_header_t* l = ((zh_large_header_t*)ptr) - 1;
  if (l->magic == ZH_LARGE_MAGIC) return zh_usable_large(ptr);
  return 0;
}

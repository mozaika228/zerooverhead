#include "zh_alloc.h"
#include "zh_slab.h"
#include "zh_buddy.h"
#include "../zh_internal.h"
#include "../utils/zh_align.h"
#include "../platform/zh_platform.h"

#define ZH_CLASS_COUNT 7
#define ZH_EMPTY_RETAIN 2u

static const size_t g_class_sizes[ZH_CLASS_COUNT] = { 16, 32, 64, 128, 256, 512, 1024 };
static zh_slab_class_t g_classes[ZH_CLASS_COUNT];
static ZH_THREAD_LOCAL zh_slab_t* g_tls_current[ZH_CLASS_COUNT];
static atomic_int g_classes_init = 0;

static void zh_classes_init(void) {
  int expected = 0;
  if (!atomic_compare_exchange_strong(&g_classes_init, &expected, 1)) return;
  for (size_t i = 0; i < ZH_CLASS_COUNT; i++) {
    g_classes[i].block_size = g_class_sizes[i];
    g_classes[i].partial = 0;
    g_classes[i].empty = 0;
    g_classes[i].empty_count = 0;
    zh_spinlock_init(&g_classes[i].lock);
  }
}

static size_t zh_class_index(size_t size) {
  for (size_t i = 0; i < ZH_CLASS_COUNT; i++) {
    if (size <= g_class_sizes[i]) return i;
  }
  return (size_t)-1;
}

static void zh_slab_list_push(zh_slab_class_t* cls, zh_slab_t* slab, uint8_t kind) {
  zh_slab_t** head = (kind == ZH_SLAB_LIST_EMPTY) ? &cls->empty : &cls->partial;
  slab->next = *head;
  slab->prev = 0;
  if (*head) (*head)->prev = slab;
  *head = slab;
  slab->in_list = 1;
  slab->list_kind = kind;
}

static void zh_slab_list_remove(zh_slab_class_t* cls, zh_slab_t* slab) {
  if (!slab->in_list) return;
  zh_slab_t** head = (slab->list_kind == ZH_SLAB_LIST_EMPTY) ? &cls->empty : &cls->partial;
  if (slab->prev) slab->prev->next = slab->next;
  if (slab->next) slab->next->prev = slab->prev;
  if (*head == slab) *head = slab->next;
  slab->next = slab->prev = 0;
  slab->in_list = 0;
  slab->list_kind = ZH_SLAB_LIST_NONE;
}

static zh_slab_t* zh_slab_list_pop(zh_slab_class_t* cls, uint8_t kind) {
  zh_slab_t* slab = (kind == ZH_SLAB_LIST_EMPTY) ? cls->empty : cls->partial;
  if (!slab) return 0;
  zh_slab_list_remove(cls, slab);
  if (kind == ZH_SLAB_LIST_EMPTY && cls->empty_count > 0) cls->empty_count--;
  return slab;
}

static zh_slab_t* zh_slab_create(size_t class_index) {
  size_t slab_size = zh_core_slab_size();
  void* mem = zh_platform_map_aligned(slab_size, slab_size);
  if (!mem) return 0;
  zh_stats_on_map(slab_size);

  zh_slab_t* slab = (zh_slab_t*)mem;
  slab->magic = ZH_SLAB_MAGIC;
  slab->class_index = (uint16_t)class_index;
  slab->block_size = (uint16_t)g_classes[class_index].block_size;
  slab->free_list = 0;
  slab->next = slab->prev = 0;
  slab->in_list = 0;
  slab->list_kind = ZH_SLAB_LIST_NONE;

  uintptr_t start = (uintptr_t)(slab + 1);
  start = zh_align_up(start, zh_max_size(16, slab->block_size));
  size_t usable = slab_size - (size_t)(start - (uintptr_t)slab);
  size_t blocks = usable / slab->block_size;
  slab->total_count = (uint32_t)blocks;
  slab->free_count = (uint32_t)blocks;

  uint8_t* p = (uint8_t*)start;
  for (size_t i = 0; i < blocks; i++) {
    void* next = (i + 1 < blocks) ? (void*)(p + slab->block_size) : 0;
    *(void**)p = next;
    p += slab->block_size;
  }
  slab->free_list = (void*)start;
  return slab;
}

void* zh_alloc_small(size_t size) {
  if (size == 0) size = 1;
  zh_core_init_once();
  zh_classes_init();

  size_t total = size + sizeof(zh_small_header_t);
  size_t idx = zh_class_index(total);
  if (idx == (size_t)-1) return 0;

  zh_slab_t* slab = g_tls_current[idx];
  if (!slab || !slab->free_list) {
    zh_slab_class_t* cls = &g_classes[idx];
    zh_spinlock_lock(&cls->lock);
    slab = zh_slab_list_pop(cls, ZH_SLAB_LIST_PARTIAL);
    if (!slab) slab = zh_slab_list_pop(cls, ZH_SLAB_LIST_EMPTY);
    zh_spinlock_unlock(&cls->lock);

    if (!slab) slab = zh_slab_create(idx);
    g_tls_current[idx] = slab;
  }

  if (!slab) return 0;
  void* block = slab->free_list;
  slab->free_list = *(void**)block;
  slab->free_count--;

  if (slab->free_count == 0) g_tls_current[idx] = 0;

  zh_small_header_t* h = (zh_small_header_t*)block;
  h->magic = ZH_SMALL_MAGIC;
  h->slab = slab;
  h->requested = (uint32_t)size;

  zh_stats_on_alloc(size, slab->block_size);
  return (void*)(h + 1);
}

void* zh_alloc_large(size_t size) {
  if (size == 0) size = 1;
  zh_core_init_once();

  size_t usable = 0;
  void* buddy = zh_buddy_alloc(size, &usable);
  if (buddy) {
    zh_stats_on_alloc(size, usable);
    return buddy;
  }

  size_t total = size + sizeof(zh_large_header_t);
  void* base = zh_platform_map(total);
  if (!base) return 0;
  zh_stats_on_map(total);
  zh_large_header_t* h = (zh_large_header_t*)base;
  h->magic = ZH_LARGE_MAGIC;
  h->size = size;
  zh_stats_on_alloc(size, size);
  return (void*)(h + 1);
}

void zh_free_small(void* ptr) {
  if (!ptr) return;
  zh_small_header_t* h = ((zh_small_header_t*)ptr) - 1;
  if (h->magic != ZH_SMALL_MAGIC) return;
  zh_slab_t* slab = (zh_slab_t*)h->slab;

  *(void**)h = slab->free_list;
  slab->free_list = h;
  uint32_t prev_free = slab->free_count;
  slab->free_count++;

  zh_stats_on_free(h->requested, slab->block_size);

  zh_slab_class_t* cls = &g_classes[slab->class_index];

  if (slab->free_count == slab->total_count) {
    if (g_tls_current[slab->class_index] == slab) g_tls_current[slab->class_index] = 0;
    zh_spinlock_lock(&cls->lock);
    if (cls->empty_count >= ZH_EMPTY_RETAIN) {
      zh_spinlock_unlock(&cls->lock);
      zh_stats_on_unmap(zh_core_slab_size());
      zh_platform_unmap_aligned(slab);
      return;
    }
    cls->empty_count++;
    zh_slab_list_push(cls, slab, ZH_SLAB_LIST_EMPTY);
    zh_spinlock_unlock(&cls->lock);
    return;
  }

  if (prev_free == 0 && g_tls_current[slab->class_index] != slab) {
    zh_spinlock_lock(&cls->lock);
    if (!slab->in_list) zh_slab_list_push(cls, slab, ZH_SLAB_LIST_PARTIAL);
    zh_spinlock_unlock(&cls->lock);
  }
}

void zh_free_large(void* ptr) {
  if (!ptr) return;
  zh_buddy_header_t* b = ((zh_buddy_header_t*)ptr) - 1;
  if (b->magic == ZH_BUDDY_MAGIC) {
    size_t usable = ((size_t)1 << b->order) - sizeof(zh_buddy_header_t);
    zh_stats_on_free(b->requested, usable);
    zh_buddy_free(ptr);
    return;
  }
  zh_large_header_t* h = ((zh_large_header_t*)ptr) - 1;
  if (h->magic == ZH_LARGE_MAGIC) {
    zh_stats_on_free(h->size, h->size);
    zh_stats_on_unmap(h->size + sizeof(zh_large_header_t));
    zh_platform_unmap((void*)h, h->size + sizeof(zh_large_header_t));
  }
}

size_t zh_usable_small(void* ptr) {
  if (!ptr) return 0;
  zh_small_header_t* h = ((zh_small_header_t*)ptr) - 1;
  if (h->magic != ZH_SMALL_MAGIC) return 0;
  return ((zh_slab_t*)h->slab)->block_size - sizeof(zh_small_header_t);
}

size_t zh_usable_large(void* ptr) {
  if (!ptr) return 0;
  zh_buddy_header_t* b = ((zh_buddy_header_t*)ptr) - 1;
  if (b->magic == ZH_BUDDY_MAGIC) {
    return ((size_t)1 << b->order) - sizeof(zh_buddy_header_t);
  }
  zh_large_header_t* h = ((zh_large_header_t*)ptr) - 1;
  if (h->magic == ZH_LARGE_MAGIC) return h->size;
  return 0;
}

void zh_tls_shutdown(void) {
  zh_classes_init();
  for (size_t i = 0; i < ZH_CLASS_COUNT; i++) {
    zh_slab_t* slab = g_tls_current[i];
    if (!slab) continue;
    g_tls_current[i] = 0;

    zh_slab_class_t* cls = &g_classes[i];
    zh_spinlock_lock(&cls->lock);
    if (slab->free_count == slab->total_count) {
      if (cls->empty_count >= ZH_EMPTY_RETAIN) {
        zh_spinlock_unlock(&cls->lock);
        zh_stats_on_unmap(zh_core_slab_size());
        zh_platform_unmap_aligned(slab);
        continue;
      }
      cls->empty_count++;
      zh_slab_list_push(cls, slab, ZH_SLAB_LIST_EMPTY);
    } else if (slab->free_count > 0) {
      if (!slab->in_list) zh_slab_list_push(cls, slab, ZH_SLAB_LIST_PARTIAL);
    }
    zh_spinlock_unlock(&cls->lock);
  }
}

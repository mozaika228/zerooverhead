#include "zh_alloc.h"
#include "zh_slab.h"
#include "zh_buddy.h"
#include "../zh_internal.h"
#include "../utils/zh_align.h"
#include "../platform/zh_platform.h"
#include "zerooverhead/zh_stats.h"

#define ZH_CLASS_COUNT 7
#define ZH_EMPTY_RETAIN 2u
#define ZH_TLS_CACHE_LIMIT 64u
#define ZH_TLS_CACHE_FLUSH 32u
#define ZH_TLS_QUARANTINE_LIMIT 64u
#define ZH_TLS_QUARANTINE_FLUSH 32u
#define ZH_MAINTENANCE_PERIOD 256u
#define ZH_SCAVENGE_DECAY_TICKS 200000ull
#define ZH_RSS_CAP_BYTES (512ull * 1024ull * 1024ull)

static const size_t g_class_sizes[ZH_CLASS_COUNT] = { 16, 32, 64, 128, 256, 512, 1024 };
static zh_slab_class_t g_classes[ZH_CLASS_COUNT];
static ZH_THREAD_LOCAL zh_slab_t* g_tls_current[ZH_CLASS_COUNT];
static atomic_int g_classes_init = 0;

typedef struct zh_tls_cache {
  void* head;
  uint32_t count;
} zh_tls_cache_t;

static ZH_THREAD_LOCAL zh_tls_cache_t g_tls_cache[ZH_CLASS_COUNT];
static ZH_THREAD_LOCAL zh_tls_cache_t g_tls_quarantine[ZH_CLASS_COUNT];
static ZH_THREAD_LOCAL uint32_t g_tls_ops = 0;
static atomic_ullong g_maintenance_epoch = 1;

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
  slab->owner_thread = zh_thread_id();
  zh_spinlock_init(&slab->lock);
  atomic_init(&slab->remote_head, (uintptr_t)0);
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
  slab->empty_epoch = 0;

  uint8_t* p = (uint8_t*)start;
  for (size_t i = 0; i < blocks; i++) {
    void* next = (i + 1 < blocks) ? (void*)(p + slab->block_size) : 0;
    *(void**)p = next;
    p += slab->block_size;
  }
  slab->free_list = (void*)start;
  return slab;
}

static void zh_slab_remote_push(zh_slab_t* slab, zh_small_header_t* h) {
  h->magic = ZH_FREED_MAGIC;
  uintptr_t head = atomic_load_explicit(&slab->remote_head, memory_order_acquire);
  do {
    *(void**)h = (void*)head;
  } while (!atomic_compare_exchange_weak_explicit(
    &slab->remote_head,
    &head,
    (uintptr_t)h,
    memory_order_release,
    memory_order_relaxed
  ));
  zh_stats_on_remote_free();
}

static void zh_slab_drain_remote(zh_slab_t* slab) {
  uintptr_t head = atomic_exchange_explicit(&slab->remote_head, (uintptr_t)0, memory_order_acq_rel);
  if (head == 0) return;

  void* list = (void*)head;
  uint32_t count = 0;
  void* tail = list;
  while (tail) {
    count++;
    void* next = *(void**)tail;
    if (!next) break;
    tail = next;
  }

  zh_spinlock_lock(&slab->lock);
  uint32_t prev_free = slab->free_count;
  *(void**)tail = slab->free_list;
  slab->free_list = list;
  slab->free_count += count;
  zh_spinlock_unlock(&slab->lock);

  if (prev_free == 0) {
    zh_slab_class_t* cls = &g_classes[slab->class_index];
    zh_spinlock_lock(&cls->lock);
    if (!slab->in_list) zh_slab_list_push(cls, slab, ZH_SLAB_LIST_PARTIAL);
    zh_spinlock_unlock(&cls->lock);
  }
}

static void* zh_tls_cache_pop(size_t idx) {
  zh_tls_cache_t* cache = &g_tls_cache[idx];
  void* block = cache->head;
  if (!block) return 0;
  cache->head = *(void**)block;
  cache->count--;
  return block;
}

static void zh_tls_cache_push(size_t idx, void* block) {
  zh_tls_cache_t* cache = &g_tls_cache[idx];
  *(void**)block = cache->head;
  cache->head = block;
  cache->count++;
}

static void zh_set_empty_epoch(zh_slab_t* slab) {
  slab->empty_epoch = atomic_load_explicit(&g_maintenance_epoch, memory_order_relaxed);
}

static void zh_flush_block_to_slab(zh_small_header_t* h) {
  zh_slab_t* slab = (zh_slab_t*)h->slab;
  zh_slab_class_t* cls = &g_classes[slab->class_index];

  zh_spinlock_lock(&slab->lock);
  h->magic = ZH_FREED_MAGIC;
  *(void**)h = slab->free_list;
  slab->free_list = h;
  slab->free_count++;
  uint32_t free_count = slab->free_count;
  uint32_t total_count = slab->total_count;
  zh_spinlock_unlock(&slab->lock);

  if (free_count == 1) {
    zh_spinlock_lock(&cls->lock);
    if (!slab->in_list) zh_slab_list_push(cls, slab, ZH_SLAB_LIST_PARTIAL);
    zh_spinlock_unlock(&cls->lock);
  } else if (free_count == total_count) {
    int do_unmap = 0;
    zh_spinlock_lock(&cls->lock);
    if (slab->in_list) zh_slab_list_remove(cls, slab);
    if (cls->empty_count >= ZH_EMPTY_RETAIN) {
      do_unmap = 1;
    } else {
      zh_set_empty_epoch(slab);
      cls->empty_count++;
      zh_slab_list_push(cls, slab, ZH_SLAB_LIST_EMPTY);
    }
    zh_spinlock_unlock(&cls->lock);

    if (do_unmap) {
      zh_stats_on_unmap(zh_core_slab_size());
      zh_platform_unmap_aligned(slab);
    }
  }
}

static void zh_tls_cache_flush(size_t idx, uint32_t max_flush) {
  zh_tls_cache_t* cache = &g_tls_cache[idx];
  uint32_t flushed = 0;
  while (cache->head && flushed < max_flush) {
    void* block = zh_tls_cache_pop(idx);
    zh_small_header_t* h = (zh_small_header_t*)block;
    zh_flush_block_to_slab(h);
    flushed++;
  }
}

static void zh_tls_quarantine_flush(size_t idx, uint32_t max_flush) {
  zh_tls_cache_t* q = &g_tls_quarantine[idx];
  uint32_t flushed = 0;
  while (q->head && flushed < max_flush) {
    void* block = q->head;
    q->head = *(void**)block;
    q->count--;
    zh_small_header_t* h = (zh_small_header_t*)block;
    zh_flush_block_to_slab(h);
    flushed++;
  }
  zh_stats_on_quarantine(q->count);
}

static void zh_maintenance_scavenge(void) {
  zh_stats_t st;
  zh_stats_get(&st);
  uint64_t now = atomic_load_explicit(&g_maintenance_epoch, memory_order_relaxed);
  int over_cap = st.bytes_mapped > ZH_RSS_CAP_BYTES;
  uint32_t released = 0;
  size_t slab_size = zh_core_slab_size();

  for (size_t i = 0; i < ZH_CLASS_COUNT && released < 2; i++) {
    zh_slab_class_t* cls = &g_classes[i];
    zh_spinlock_lock(&cls->lock);
    zh_slab_t* cur = cls->empty;
    while (cur && released < 2) {
      zh_slab_t* next = cur->next;
      uint64_t age = now - cur->empty_epoch;
      if (over_cap || age >= ZH_SCAVENGE_DECAY_TICKS) {
        zh_slab_list_remove(cls, cur);
        if (cls->empty_count > 0) cls->empty_count--;
        zh_spinlock_unlock(&cls->lock);
        zh_stats_on_unmap(slab_size);
        zh_platform_unmap_aligned(cur);
        released++;
        zh_spinlock_lock(&cls->lock);
        cur = cls->empty;
        continue;
      }
      cur = next;
    }
    zh_spinlock_unlock(&cls->lock);
  }
}

static void zh_maintenance_tick(void) {
  g_tls_ops++;
  atomic_fetch_add_explicit(&g_maintenance_epoch, 1, memory_order_relaxed);
  if (g_tls_ops % ZH_MAINTENANCE_PERIOD == 0) {
    zh_maintenance_scavenge();
  }
}

void* zh_alloc_small(size_t size) {
  if (size == 0) size = 1;
  zh_core_init_once();
  zh_classes_init();

  size_t total = size + sizeof(zh_small_header_t);
  size_t idx = zh_class_index(total);
  if (idx == (size_t)-1) return 0;

  uint32_t tid = zh_thread_id();
  void* cached = zh_tls_cache_pop(idx);
  if (cached) {
    zh_small_header_t* h = (zh_small_header_t*)cached;
    h->magic = ZH_SMALL_MAGIC;
    h->requested = (uint32_t)size;
    zh_stats_on_alloc(size, ((zh_slab_t*)h->slab)->block_size);
    return (void*)(h + 1);
  }

  zh_slab_t* slab = g_tls_current[idx];
  if (!slab) {
    zh_slab_class_t* cls = &g_classes[idx];
    zh_spinlock_lock(&cls->lock);
    slab = zh_slab_list_pop(cls, ZH_SLAB_LIST_PARTIAL);
    if (!slab) slab = zh_slab_list_pop(cls, ZH_SLAB_LIST_EMPTY);
    zh_spinlock_unlock(&cls->lock);

    if (!slab) slab = zh_slab_create(idx);
    if (slab) slab->owner_thread = tid;
    g_tls_current[idx] = slab;
  }

  if (!slab) return 0;
  zh_slab_drain_remote(slab);
  zh_spinlock_lock(&slab->lock);
  void* block = slab->free_list;
  if (block) {
    slab->free_list = *(void**)block;
    slab->free_count--;
  }
  zh_spinlock_unlock(&slab->lock);

  if (!block) {
    g_tls_current[idx] = 0;
    return zh_alloc_small(size);
  }

  zh_small_header_t* h = (zh_small_header_t*)block;
  h->magic = ZH_SMALL_MAGIC;
  h->slab = slab;
  h->requested = (uint32_t)size;

  zh_stats_on_alloc(size, slab->block_size);
  zh_maintenance_tick();
  return (void*)(h + 1);
}

void* zh_alloc_large(size_t size) {
  if (size == 0) size = 1;
  zh_core_init_once();

  if (size < ZH_HUGE_THRESHOLD) {
    size_t usable = 0;
    void* buddy = zh_buddy_alloc(size, &usable);
    if (buddy) {
      zh_stats_on_alloc(size, usable);
      zh_maintenance_tick();
      return buddy;
    }
  }

  size_t total = size + sizeof(zh_large_header_t);
  void* base = zh_platform_map(total);
  if (!base) return 0;
  zh_stats_on_map(total);
  zh_large_header_t* h = (zh_large_header_t*)base;
  h->magic = ZH_LARGE_MAGIC;
  h->size = size;
  zh_stats_on_alloc(size, size);
  zh_maintenance_tick();
  return (void*)(h + 1);
}

void zh_free_small(void* ptr) {
  if (!ptr) return;
  zh_small_header_t* h = ((zh_small_header_t*)ptr) - 1;
  if (h->magic == ZH_FREED_MAGIC) {
    zh_stats_on_invalid_free();
    return;
  }
  if (h->magic != ZH_SMALL_MAGIC) {
    zh_stats_on_invalid_free();
    return;
  }
  zh_slab_t* slab = (zh_slab_t*)h->slab;

  zh_stats_on_free(h->requested, slab->block_size);

  if (slab->owner_thread != zh_thread_id()) {
    zh_slab_remote_push(slab, h);
    zh_maintenance_tick();
    return;
  }

  size_t idx = slab->class_index;
  if (zh_mode_is_hardened()) {
    h->magic = ZH_FREED_MAGIC;
    *(void**)h = g_tls_quarantine[idx].head;
    g_tls_quarantine[idx].head = h;
    g_tls_quarantine[idx].count++;
    zh_stats_on_quarantine(g_tls_quarantine[idx].count);
    if (g_tls_quarantine[idx].count > ZH_TLS_QUARANTINE_LIMIT) {
      zh_tls_quarantine_flush(idx, ZH_TLS_QUARANTINE_FLUSH);
    }
  } else {
    h->magic = ZH_FREED_MAGIC;
    zh_tls_cache_push(idx, h);
    if (g_tls_cache[idx].count > ZH_TLS_CACHE_LIMIT) {
      zh_tls_cache_flush(idx, ZH_TLS_CACHE_FLUSH);
    }
  }
  zh_maintenance_tick();
}

void zh_free_large(void* ptr) {
  if (!ptr) return;
  zh_buddy_header_t* b = ((zh_buddy_header_t*)ptr) - 1;
  if (b->magic == ZH_BUDDY_MAGIC) {
    size_t usable = ((size_t)1 << b->order) - sizeof(zh_buddy_header_t);
    zh_stats_on_free(b->requested, usable);
    zh_buddy_free(ptr);
    zh_maintenance_tick();
    return;
  }
  zh_large_header_t* h = ((zh_large_header_t*)ptr) - 1;
  if (h->magic == ZH_LARGE_MAGIC) {
    zh_stats_on_free(h->size, h->size);
    zh_stats_on_unmap(h->size + sizeof(zh_large_header_t));
    zh_platform_unmap((void*)h, h->size + sizeof(zh_large_header_t));
    zh_maintenance_tick();
    return;
  }
  zh_stats_on_invalid_free();
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
    zh_tls_quarantine_flush(i, g_tls_quarantine[i].count);
    zh_tls_cache_flush(i, g_tls_cache[i].count);
  }

  for (size_t i = 0; i < ZH_CLASS_COUNT; i++) {
    zh_slab_t* slab = g_tls_current[i];
    if (!slab) continue;
    g_tls_current[i] = 0;

    zh_slab_class_t* cls = &g_classes[i];
    zh_spinlock_lock(&slab->lock);
    uint32_t free_count = slab->free_count;
    uint32_t total_count = slab->total_count;
    zh_spinlock_unlock(&slab->lock);

    if (free_count == total_count) {
      int do_unmap = 0;
      zh_spinlock_lock(&cls->lock);
      if (cls->empty_count >= ZH_EMPTY_RETAIN) {
        do_unmap = 1;
      } else {
        zh_set_empty_epoch(slab);
        cls->empty_count++;
        zh_slab_list_push(cls, slab, ZH_SLAB_LIST_EMPTY);
      }
      zh_spinlock_unlock(&cls->lock);

      if (do_unmap) {
        zh_stats_on_unmap(zh_core_slab_size());
        zh_platform_unmap_aligned(slab);
      }
    } else if (free_count > 0) {
      zh_spinlock_lock(&cls->lock);
      if (!slab->in_list) zh_slab_list_push(cls, slab, ZH_SLAB_LIST_PARTIAL);
      zh_spinlock_unlock(&cls->lock);
    }
  }
}

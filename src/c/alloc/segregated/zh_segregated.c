#include "../zh_alloc.h"
#include "../../zh_internal.h"
#include "../../platform/zh_platform.h"
#include "../../threads/zh_spinlock.h"

#define ZH_MEDIUM_CLASS_COUNT 6
#define ZH_MEDIUM_ARENA_COUNT 8
#define ZH_MEDIUM_RUN_MIN (256u * 1024u)

typedef struct zh_medium_run {
  struct zh_medium_run* next;
  size_t size;
} zh_medium_run_t;

typedef struct zh_medium_bin {
  size_t block_size;
  zh_medium_header_t* free_list;
} zh_medium_bin_t;

typedef struct zh_medium_arena {
  zh_spinlock_t lock;
  zh_medium_bin_t bins[ZH_MEDIUM_CLASS_COUNT];
  zh_medium_run_t* runs;
} zh_medium_arena_t;

static const size_t g_medium_sizes[ZH_MEDIUM_CLASS_COUNT] = {
  2048, 4096, 8192, 16384, 32768, 65536
};

static zh_medium_arena_t g_medium_arenas[ZH_MEDIUM_ARENA_COUNT];
static atomic_int g_medium_init = 0;

static void zh_medium_init_once(void) {
  int expected = 0;
  if (!atomic_compare_exchange_strong(&g_medium_init, &expected, 1)) return;
  for (size_t a = 0; a < ZH_MEDIUM_ARENA_COUNT; a++) {
    zh_spinlock_init(&g_medium_arenas[a].lock);
    g_medium_arenas[a].runs = 0;
    for (size_t i = 0; i < ZH_MEDIUM_CLASS_COUNT; i++) {
      g_medium_arenas[a].bins[i].block_size = g_medium_sizes[i];
      g_medium_arenas[a].bins[i].free_list = 0;
    }
  }
}

static size_t zh_medium_class_index(size_t total) {
  for (size_t i = 0; i < ZH_MEDIUM_CLASS_COUNT; i++) {
    if (total <= g_medium_sizes[i]) return i;
  }
  return (size_t)-1;
}

static uint16_t zh_medium_arena_index(void) {
  return (uint16_t)(zh_thread_id() % ZH_MEDIUM_ARENA_COUNT);
}

static int zh_medium_grow_bin(zh_medium_arena_t* arena, uint16_t arena_idx, uint16_t class_idx) {
  size_t block_size = arena->bins[class_idx].block_size;
  size_t run_size = block_size * 64u;
  if (run_size < ZH_MEDIUM_RUN_MIN) run_size = ZH_MEDIUM_RUN_MIN;
  void* mem = zh_platform_map(run_size);
  if (!mem) return 0;
  zh_stats_on_map(run_size);

  zh_medium_run_t* run = (zh_medium_run_t*)mem;
  run->next = arena->runs;
  run->size = run_size;
  arena->runs = run;

  size_t header_size = sizeof(zh_medium_run_t);
  size_t usable = run_size - header_size;
  size_t blocks = usable / block_size;
  uint8_t* p = ((uint8_t*)mem) + header_size;
  for (size_t i = 0; i < blocks; i++) {
    zh_medium_header_t* h = (zh_medium_header_t*)p;
    h->magic = ZH_MEDIUM_MAGIC;
    h->requested = 0;
    h->arena_idx = arena_idx;
    h->class_idx = class_idx;
    h->next = arena->bins[class_idx].free_list;
    arena->bins[class_idx].free_list = h;
    p += block_size;
  }
  return 1;
}

void* zh_alloc_medium(size_t size) {
  if (size == 0 || size > ZH_MEDIUM_MAX) return 0;
  zh_medium_init_once();

  size_t total = size + sizeof(zh_medium_header_t);
  size_t class_idx = zh_medium_class_index(total);
  if (class_idx == (size_t)-1) return 0;
  uint16_t arena_idx = zh_medium_arena_index();
  zh_medium_arena_t* arena = &g_medium_arenas[arena_idx];

  zh_spinlock_lock(&arena->lock);
  zh_medium_header_t* h = arena->bins[class_idx].free_list;
  if (!h) {
    if (!zh_medium_grow_bin(arena, arena_idx, (uint16_t)class_idx)) {
      zh_spinlock_unlock(&arena->lock);
      return 0;
    }
    h = arena->bins[class_idx].free_list;
  }
  arena->bins[class_idx].free_list = h->next;
  h->magic = ZH_MEDIUM_MAGIC;
  h->requested = (uint32_t)size;
  zh_spinlock_unlock(&arena->lock);

  zh_stats_on_alloc(size, arena->bins[class_idx].block_size - sizeof(zh_medium_header_t));
  return (void*)(h + 1);
}

void zh_free_medium(void* ptr) {
  if (!ptr) return;
  zh_medium_header_t* h = ((zh_medium_header_t*)ptr) - 1;
  if (h->magic != ZH_MEDIUM_MAGIC) {
    zh_stats_on_invalid_free();
    return;
  }

  uint16_t arena_idx = h->arena_idx;
  uint16_t class_idx = h->class_idx;
  if (arena_idx >= ZH_MEDIUM_ARENA_COUNT || class_idx >= ZH_MEDIUM_CLASS_COUNT) {
    zh_stats_on_invalid_free();
    return;
  }
  zh_medium_arena_t* arena = &g_medium_arenas[arena_idx];
  zh_spinlock_lock(&arena->lock);
  h->magic = ZH_FREED_MAGIC;
  h->next = arena->bins[class_idx].free_list;
  arena->bins[class_idx].free_list = h;
  zh_spinlock_unlock(&arena->lock);

  zh_stats_on_free(h->requested, arena->bins[class_idx].block_size - sizeof(zh_medium_header_t));
}

size_t zh_usable_medium(void* ptr) {
  if (!ptr) return 0;
  zh_medium_header_t* h = ((zh_medium_header_t*)ptr) - 1;
  if (h->magic != ZH_MEDIUM_MAGIC) return 0;
  if (h->arena_idx >= ZH_MEDIUM_ARENA_COUNT || h->class_idx >= ZH_MEDIUM_CLASS_COUNT) return 0;
  return g_medium_arenas[h->arena_idx].bins[h->class_idx].block_size - sizeof(zh_medium_header_t);
}

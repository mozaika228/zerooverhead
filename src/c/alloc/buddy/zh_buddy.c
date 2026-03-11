#include "../zh_buddy.h"
#include "../zh_internal.h"
#include "../threads/zh_spinlock.h"
#include "../platform/zh_platform.h"
#include "../utils/zh_align.h"

#define ZH_BUDDY_MIN_ORDER 12u
#define ZH_BUDDY_MAX_ORDER 25u
#define ZH_BUDDY_ARENA_SIZE (1u << ZH_BUDDY_MAX_ORDER)

typedef struct zh_buddy_state {
  void* base;
  size_t size;
  zh_spinlock_t lock;
  zh_buddy_header_t* free_lists[ZH_BUDDY_MAX_ORDER + 1];
  int initialized;
} zh_buddy_state_t;

static zh_buddy_state_t g_buddy;

static void zh_buddy_list_push(uint32_t order, zh_buddy_header_t* block) {
  block->next = g_buddy.free_lists[order];
  block->prev = 0;
  if (g_buddy.free_lists[order]) g_buddy.free_lists[order]->prev = block;
  g_buddy.free_lists[order] = block;
  block->order = order;
  block->magic = ZH_BUDDY_FREE_MAGIC;
}

static void zh_buddy_list_remove(uint32_t order, zh_buddy_header_t* block) {
  if (block->prev) block->prev->next = block->next;
  if (block->next) block->next->prev = block->prev;
  if (g_buddy.free_lists[order] == block) g_buddy.free_lists[order] = block->next;
  block->next = block->prev = 0;
}

static void zh_buddy_init(void) {
  if (g_buddy.initialized) return;
  zh_spinlock_init(&g_buddy.lock);
  g_buddy.size = ZH_BUDDY_ARENA_SIZE;
  g_buddy.base = zh_platform_map_aligned(g_buddy.size, g_buddy.size);
  if (!g_buddy.base) return;
  for (size_t i = 0; i <= ZH_BUDDY_MAX_ORDER; i++) g_buddy.free_lists[i] = 0;
  zh_buddy_header_t* root = (zh_buddy_header_t*)g_buddy.base;
  zh_buddy_list_push(ZH_BUDDY_MAX_ORDER, root);
  g_buddy.initialized = 1;
  zh_stats_on_map(g_buddy.size);
}

static zh_buddy_header_t* zh_buddy_pop(uint32_t order) {
  zh_buddy_header_t* block = g_buddy.free_lists[order];
  if (!block) return 0;
  zh_buddy_list_remove(order, block);
  return block;
}

void* zh_buddy_alloc(size_t size, size_t* out_usable) {
  if (size == 0) size = 1;
  if (!g_buddy.initialized) zh_buddy_init();
  if (!g_buddy.initialized) return 0;

  size_t total = size + sizeof(zh_buddy_header_t);
  uint32_t order = ZH_BUDDY_MIN_ORDER;
  while (((size_t)1 << order) < total && order <= ZH_BUDDY_MAX_ORDER) order++;
  if (order > ZH_BUDDY_MAX_ORDER) return 0;

  zh_spinlock_lock(&g_buddy.lock);
  uint32_t cur = order;
  while (cur <= ZH_BUDDY_MAX_ORDER && g_buddy.free_lists[cur] == 0) cur++;
  if (cur > ZH_BUDDY_MAX_ORDER) {
    zh_spinlock_unlock(&g_buddy.lock);
    return 0;
  }

  zh_buddy_header_t* block = zh_buddy_pop(cur);
  while (cur > order) {
    cur--;
    size_t half = (size_t)1 << cur;
    zh_buddy_header_t* buddy = (zh_buddy_header_t*)((uint8_t*)block + half);
    zh_buddy_list_push(cur, buddy);
  }
  block->order = order;
  block->magic = ZH_BUDDY_MAGIC;
  block->requested = (uint32_t)size;
  zh_spinlock_unlock(&g_buddy.lock);

  if (out_usable) *out_usable = ((size_t)1 << order) - sizeof(zh_buddy_header_t);
  return (void*)(block + 1);
}

void zh_buddy_free(void* ptr) {
  if (!ptr || !g_buddy.initialized) return;
  zh_buddy_header_t* block = ((zh_buddy_header_t*)ptr) - 1;
  if (block->magic != ZH_BUDDY_MAGIC) return;

  zh_spinlock_lock(&g_buddy.lock);
  uint32_t order = block->order;
  block->magic = ZH_BUDDY_FREE_MAGIC;

  while (order < ZH_BUDDY_MAX_ORDER) {
    uintptr_t base = (uintptr_t)g_buddy.base;
    uintptr_t addr = (uintptr_t)block;
    uintptr_t offset = addr - base;
    uintptr_t buddy_offset = offset ^ ((uintptr_t)1 << order);
    zh_buddy_header_t* buddy = (zh_buddy_header_t*)(base + buddy_offset);
    if (buddy->magic != ZH_BUDDY_FREE_MAGIC || buddy->order != order) break;
    zh_buddy_list_remove(order, buddy);
    if (buddy < block) block = buddy;
    order++;
    block->order = order;
  }
  zh_buddy_list_push(order, block);
  zh_spinlock_unlock(&g_buddy.lock);
}

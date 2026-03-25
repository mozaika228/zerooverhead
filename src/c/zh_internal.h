#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#if defined(_MSC_VER)
#define ZH_THREAD_LOCAL __declspec(thread)
#else
#define ZH_THREAD_LOCAL _Thread_local
#endif

#if defined(_MSC_VER)
#define ZH_INLINE __forceinline
#else
#define ZH_INLINE inline __attribute__((always_inline))
#endif

#if defined(_MSC_VER)
#define ZH_EXPORT __declspec(dllexport)
#else
#define ZH_EXPORT __attribute__((visibility("default")))
#endif

#define ZH_CACHELINE 64u
#define ZH_SLAB_MAGIC 0x5A484C534C41424Cull
#define ZH_SMALL_MAGIC 0x5A48534D414C4C4Cull
#define ZH_LARGE_MAGIC 0x5A484C4C41524745ull
#define ZH_BUDDY_MAGIC 0x5A48425544445900ull
#define ZH_BUDDY_FREE_MAGIC 0x5A484255444459FFull
#define ZH_SMALL_MAX 1024u
#define ZH_SLAB_SIZE_DEFAULT (64u * 1024u)

static ZH_INLINE size_t zh_min_size(size_t a, size_t b) { return a < b ? a : b; }
static ZH_INLINE size_t zh_max_size(size_t a, size_t b) { return a > b ? a : b; }

size_t zh_core_page_size(void);
size_t zh_core_slab_size(void);
void zh_core_init_once(void);

void zh_init(void);
void zh_shutdown(void);
void zh_thread_shutdown(void);
uint32_t zh_thread_id(void);

void zh_cpu_relax(void);
uint32_t zh_clz32(uint32_t v);
uint64_t zh_hash_u64(uint64_t x);

void zh_stats_on_alloc(size_t requested, size_t allocated);
void zh_stats_on_free(size_t requested, size_t allocated);
void zh_stats_on_map(size_t size);
void zh_stats_on_unmap(size_t size);

#include "zh_platform.h"
#include "../utils/zh_align.h"
#include "../zh_internal.h"

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <sys/mman.h>
  #include <unistd.h>
#endif

static size_t g_page_size = 0;

size_t zh_platform_page_size(void) {
  if (g_page_size != 0) return g_page_size;
#if defined(_WIN32)
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  g_page_size = (size_t)info.dwPageSize;
#else
  g_page_size = (size_t)sysconf(_SC_PAGESIZE);
#endif
  return g_page_size;
}

void* zh_platform_map(size_t size) {
  if (size == 0) return 0;
#if defined(_WIN32)
  return VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
  void* p = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return (p == MAP_FAILED) ? 0 : p;
#endif
}

void zh_platform_unmap(void* ptr, size_t size) {
  if (!ptr) return;
#if defined(_WIN32)
  (void)size;
  VirtualFree(ptr, 0, MEM_RELEASE);
#else
  munmap(ptr, size);
#endif
}

void* zh_platform_map_aligned(size_t size, size_t alignment) {
  if (size == 0) return 0;
  if (!zh_is_pow2(alignment)) return 0;
  size_t total = size + alignment + sizeof(zh_os_map_header_t);
  void* base = zh_platform_map(total);
  if (!base) return 0;

  uintptr_t raw = (uintptr_t)base + sizeof(zh_os_map_header_t);
  uintptr_t aligned = (uintptr_t)zh_align_up(raw, alignment);
  zh_os_map_header_t* header = (zh_os_map_header_t*)(aligned - sizeof(zh_os_map_header_t));
  header->base = base;
  header->size = total;
  return (void*)aligned;
}

void zh_platform_unmap_aligned(void* aligned_ptr) {
  if (!aligned_ptr) return;
  zh_os_map_header_t* header = (zh_os_map_header_t*)((uintptr_t)aligned_ptr - sizeof(zh_os_map_header_t));
  zh_platform_unmap(header->base, header->size);
}

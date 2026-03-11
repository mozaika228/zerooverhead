#include <stdint.h>
#include "../zh_internal.h"

uint32_t zh_clz32(uint32_t v) {
#if defined(_MSC_VER)
  unsigned long idx;
  if (_BitScanReverse(&idx, v)) return 31u - (uint32_t)idx;
  return 32u;
#elif defined(__GNUC__)
  return v ? (uint32_t)__builtin_clz(v) : 32u;
#else
  uint32_t n = 32u;
  uint32_t c = v;
  if (c) { n = 0; while ((c & 0x80000000u) == 0) { n++; c <<= 1; } }
  return n;
#endif
}

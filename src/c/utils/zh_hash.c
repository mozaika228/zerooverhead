#include <stdint.h>
#include "../zh_internal.h"

static ZH_INLINE uint64_t zh_mix64(uint64_t x) {
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}

uint64_t zh_hash_u64(uint64_t x) {
  return zh_mix64(x);
}

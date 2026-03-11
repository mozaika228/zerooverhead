#include "zh_align.h"

int zh_is_pow2(size_t x) {
  return x != 0 && ((x & (x - 1)) == 0);
}

size_t zh_align_up(size_t value, size_t alignment) {
  if (alignment == 0) return value;
  size_t mask = alignment - 1;
  return (value + mask) & ~mask;
}

size_t zh_align_down(size_t value, size_t alignment) {
  if (alignment == 0) return value;
  size_t mask = alignment - 1;
  return value & ~mask;
}

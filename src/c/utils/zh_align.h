#pragma once
#include <stddef.h>
#include <stdint.h>

int zh_is_pow2(size_t x);
size_t zh_align_up(size_t value, size_t alignment);
size_t zh_align_down(size_t value, size_t alignment);

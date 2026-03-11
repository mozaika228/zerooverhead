#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct zh_buddy_header {
  uint64_t magic;
  uint32_t order;
  uint32_t requested;
  struct zh_buddy_header* next;
  struct zh_buddy_header* prev;
} zh_buddy_header_t;

void* zh_buddy_alloc(size_t size, size_t* out_usable);
void zh_buddy_free(void* ptr);

#pragma once
#include <stdatomic.h>

typedef struct zh_spinlock {
  atomic_flag flag;
} zh_spinlock_t;

void zh_spinlock_init(zh_spinlock_t* lock);
void zh_spinlock_lock(zh_spinlock_t* lock);
void zh_spinlock_unlock(zh_spinlock_t* lock);

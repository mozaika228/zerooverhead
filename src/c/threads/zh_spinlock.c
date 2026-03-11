#include "zh_spinlock.h"
#include "../zh_internal.h"

void zh_spinlock_init(zh_spinlock_t* lock) {
  atomic_flag_clear(&lock->flag);
}

void zh_spinlock_lock(zh_spinlock_t* lock) {
  while (atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire)) {
    zh_cpu_relax();
  }
}

void zh_spinlock_unlock(zh_spinlock_t* lock) {
  atomic_flag_clear_explicit(&lock->flag, memory_order_release);
}

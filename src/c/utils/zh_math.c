#include "../zh_internal.h"

void zh_cpu_relax(void) {
#if defined(_MSC_VER)
  #include <immintrin.h>
  _mm_pause();
#elif defined(__i386__) || defined(__x86_64__)
  __asm__ __volatile__("pause");
#else
  (void)0;
#endif
}

#include "zerooverhead/zerooverhead.h"
#include <time.h>

static double now_seconds(void) {
  return (double)clock() / (double)CLOCKS_PER_SEC;
}

int main(void) {
  const int iters = 600000;
  double start = now_seconds();
  for (int i = 0; i < iters; i++) {
    void* p = zh_malloc(96);
    zh_free(p);
  }
  double elapsed = now_seconds() - start;
  return elapsed <= 6.0 ? 0 : 1;
}


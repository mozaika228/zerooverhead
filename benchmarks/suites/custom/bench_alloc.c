#include "zerooverhead/zerooverhead.h"
#include <stdio.h>
#include <time.h>

static double seconds_now(void) {
  return (double)clock() / (double)CLOCKS_PER_SEC;
}

int main(void) {
  const int iters = 1000000;
  double start = seconds_now();
  for (int i = 0; i < iters; i++) {
    void* p = zh_malloc(64);
    zh_free(p);
  }
  double end = seconds_now();
  printf("iters=%d time=%.6f s\n", iters, end - start);
  return 0;
}

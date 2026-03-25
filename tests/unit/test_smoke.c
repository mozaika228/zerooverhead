#include "zerooverhead/zerooverhead.h"

int main(void) {
  zh_init();
  zh_set_mode(ZH_MODE_HARDENED);
  void* p = zh_malloc(64);
  if (!p) return 1;
  zh_free(p);
  void* m = zh_malloc(4096);
  if (!m) return 2;
  zh_free(m);
  zh_set_mode(ZH_MODE_PERF);
  p = zh_malloc(64);
  if (!p) return 3;
  void* q = zh_realloc(p, 128);
  if (!q) return 4;
  zh_free(q);
  zh_shutdown();
  return 0;
}

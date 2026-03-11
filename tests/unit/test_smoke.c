#include "zerooverhead/zerooverhead.h"

int main(void) {
  zh_init();
  void* p = zh_malloc(64);
  if (!p) return 1;
  void* q = zh_realloc(p, 128);
  if (!q) return 2;
  zh_free(q);
  zh_shutdown();
  return 0;
}

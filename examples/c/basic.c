#include "zerooverhead/zerooverhead.h"

int main(void) {
  zh_init();
  void* p = zh_malloc(16);
  zh_free(p);
  zh_shutdown();
  return 0;
}

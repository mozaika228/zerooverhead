#include "zerooverhead/zerooverhead.h"
#include "../alloc/zh_alloc.h"

void zh_thread_shutdown(void) {
  zh_tls_shutdown();
}

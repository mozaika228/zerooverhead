#include "zerooverhead/zerooverhead.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cstddef>

static void worker(std::atomic<int>& ready, std::atomic<int>& start, int rounds) {
  ready.fetch_add(1, std::memory_order_relaxed);
  while (start.load(std::memory_order_acquire) == 0) {}
  for (int i = 0; i < rounds; i++) {
    void* p = zh_malloc(64 + (i % 128));
    zh_free(p);
  }
  zh_thread_shutdown();
}

int main() {
  const int threads = 8;
  const int rounds = 200000;
  std::atomic<int> ready{0};
  std::atomic<int> start{0};
  std::vector<std::thread> pool;
  pool.reserve(threads);
  for (int i = 0; i < threads; i++) {
    pool.emplace_back(worker, std::ref(ready), std::ref(start), rounds);
  }
  while (ready.load(std::memory_order_acquire) != threads) {}
  start.store(1, std::memory_order_release);
  for (auto& t : pool) t.join();
  return 0;
}


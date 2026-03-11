cmake_minimum_required(VERSION 3.20)
project(zerooverhead C CXX)

option(ZH_BUILD_TESTS "Build tests" ON)

add_library(zerooverhead
  src/c/zerooverhead.c
  src/c/core/zh_core.c
  src/c/core/zh_init.c
  src/c/core/zh_shutdown.c
  src/c/alloc/zh_alloc.c
  src/c/alloc/slab/zh_slab.c
  src/c/alloc/buddy/zh_buddy.c
  src/c/alloc/segregated/zh_segregated.c
  src/c/alloc/arena/zh_arena.c
  src/c/alloc/pool/zh_pool.c
  src/c/alloc/huge/zh_huge.c
  src/c/threads/zh_tls.c
  src/c/threads/zh_spinlock.c
  src/c/platform/zh_platform.c
  src/c/platform/windows/zh_win.c
  src/c/platform/linux/zh_linux.c
  src/c/platform/macos/zh_macos.c
  src/c/utils/zh_math.c
  src/c/utils/zh_align.c
  src/c/utils/zh_bits.c
  src/c/utils/zh_hash.c
  src/c/debug/zh_debug.c
  src/c/telemetry/zh_telemetry.c
  src/c/guard/zh_guard.c
  src/c/guard/asan/zh_asan.c
  src/c/guard/ubsan/zh_ubsan.c
)

target_include_directories(zerooverhead PUBLIC include)

target_compile_features(zerooverhead PUBLIC c_std_11)

if (ZH_BUILD_TESTS)
  enable_testing()
  add_executable(zh_smoke tests/unit/test_smoke.c)
  target_link_libraries(zh_smoke zerooverhead)
  add_test(NAME zh_smoke COMMAND zh_smoke)
endif()

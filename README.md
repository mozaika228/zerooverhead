# ZeroOverhead

ZeroOverhead is a custom allocator project with a working C allocator core and multi-language repository layout.

Current state:
- C API is available: `zh_malloc`, `zh_free`, `zh_realloc`, `zh_usable_size`, `zh_init`, `zh_shutdown`, `zh_thread_shutdown`.
- Small allocations use slab classes with thread-local caching and class-level lists.
- Large allocations use buddy allocator path with fallback direct OS mapping.
- Basic telemetry counters are implemented (`zh_stats_get`).
- CI build/test workflow exists for Linux and Windows.

## Layout
- `include/zerooverhead/` public C API and headers
- `src/c/` core allocator implementation
- `src/cpp/` C++ API and policy
- `src/rust/zerooverhead/` Rust crate
- `src/python/zerooverhead/` Python package
- `benchmarks/` benchmark suites and runners
- `tests/` unit, integration, stress, latency tests
- `fuzz/` fuzz targets
- `tools/` scripts, tracing, analysis, CI helpers
- `docs/` architecture, performance, and security notes

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

On multi-config generators (Visual Studio), run:

```bash
ctest --test-dir build -C Release --output-on-failure
```

## Current Limitations
- Cross-thread free optimization is partial and not yet lock-free.
- Fragmentation control and reclaim policies are basic.
- Benchmark and soak-test coverage is present as structure, but not yet complete as a full competitive suite.
- C++/Rust/Python integration layers are repository scaffolds and not production bindings.

## Architecture Notes
- Critical blocks for production-complete status are listed in `docs/architecture/critical-blocks.md`.

## License
See `LICENSE`.

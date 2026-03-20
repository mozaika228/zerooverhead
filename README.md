# ZeroOverhead

ZeroOverhead is a research-grade, multi-language memory allocator project targeting near-zero metadata overhead, high throughput, and low tail latency under real workloads.

This repository is intentionally structured for a full system allocator stack:
- Core C allocator and platform glue
- C++ policy and API layer
- Rust crate wrapping the C core
- Python package for experiments
- Benchmarks, fuzzing, and stress tests

Status: scaffolded. Implementation starts with the C core.

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

## Build (C core)
This is a placeholder build configuration. It will be expanded as the allocator matures.

```bash
cmake -S . -B build
cmake --build build
```

## Roadmap (high level)
1. Platform abstraction, page mapping, and alignment utilities
2. Small object slabs and per-thread caches
3. Large object path (buddy / segregated free lists)
4. Telemetry and safety guards
5. Benchmarks and regression tests

## License
See `LICENSE`.

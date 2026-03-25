# Critical Blocks To Reach "Completed" State

This document defines the minimum critical blocks required for ZeroOverhead to be considered production-complete.

## 1. Concurrency Model
- Per-thread caches with bounded memory growth.
- Cross-thread free path that does not require global locks.
- ABA-safe reclamation for lock-free structures (epoch or hazard pointers).
- NUMA-aware arena placement.

Done criteria:
- No data races under stress tests with 64+ threads.
- p99 latency does not collapse under cross-thread free storms.

## 2. Allocator Families Coverage
- Small allocations: slab/pool path.
- Medium allocations: segregated free lists with coalescing.
- Large allocations: buddy allocator with fallback direct map.
- Huge allocations: direct map with huge page support toggle.

Done criteria:
- Every size class path has unit and stress coverage.
- Fallback policy is deterministic and documented.

## 3. Fragmentation And Reclaim
- Internal fragmentation accounting per size class.
- External fragmentation accounting per arena.
- Background scavenger with decay-based release to OS.
- Empty slab retention policy tuned by workload profile.

Done criteria:
- RSS and fragmentation remain bounded in long-run churn tests.
- Reclaim behavior is stable under mixed-size workloads.

## 4. Deterministic Latency
- Bounded slow paths for alloc/free.
- Batch refill/drain paths for caches.
- Warm startup path for metadata and critical arenas.
- Explicit latency SLO tracking (p50/p95/p99/p99.9).

Done criteria:
- Latency SLO gates pass in CI for synthetic and mixed workloads.

## 5. Security Modes
- Hardened mode: canary/tag checks, quarantine, guard pages.
- Performance mode: low overhead with optional lightweight checks.
- Double-free and invalid free detection paths.
- Controlled memory poisoning options for debug mode.

Done criteria:
- Security tests catch expected misuse classes.
- Hardened mode overhead is measured and documented.

## 6. Observability
- Global and per-thread stats.
- Allocation histograms by size class.
- Lock contention counters.
- Export paths (JSON/CSV) and runtime snapshot API.

Done criteria:
- Metrics are stable under load and validated by tests.

## 7. Benchmark Lab And Regression Gates
- Benchmark matrix against jemalloc, tcmalloc, mimalloc, rpmalloc.
- Synthetic suites plus real-world traces.
- CI regression gates for throughput, latency, and RSS.
- Reproducible benchmark configs and pinned environments.

Done criteria:
- Performance regressions fail CI automatically.
- Benchmark methodology is reproducible from repo scripts.

## 8. Platform And Toolchain Parity
- Linux, Windows, macOS support for core paths.
- x86_64 and ARM64 support.
- Clang/GCC/MSVC build and test matrix.
- Sanitizer/fuzzer coverage on supported targets.

Done criteria:
- All supported platforms pass build, tests, and smoke performance checks.

## 9. Language Integration Stability
- Stable C ABI.
- C++ allocator adapters (including pmr).
- Rust allocator trait integration.
- Python extension integration and lifecycle safety.

Done criteria:
- ABI/API compatibility policy is documented and tested.

## 10. Reliability And Operations
- Long-run soak tests (hours to days).
- Fault injection for map failures and pressure scenarios.
- Crash diagnostics and allocator state dump tooling.
- Release process with versioning and compatibility guarantees.

Done criteria:
- Soak and fault-injection suites pass before release tags.

## Suggested Implementation Order
1. Concurrency model and allocator family completion.
2. Fragmentation/reclaim and deterministic latency controls.
3. Observability and benchmark regression gates.
4. Security modes and platform parity hardening.
5. Language integration finalization and release process.

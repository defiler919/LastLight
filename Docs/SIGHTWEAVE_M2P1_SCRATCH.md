# SightWeave M2P.1 reusable solver scratch

## Result

The production Optimized solver now has an allocation-stable `Into` API and bounded per-thread scratch. After one warm solve at the target high-water mark, all three measured samples for every solver workload, the point query, the 512-query batch, clean publication, and no-change update recorded zero allocation calls, zero reallocation calls, zero frees, zero allocated bytes, and zero peak temporary bytes.

| Warmed operation | Baseline alloc/realloc calls | Reusable-scratch alloc/realloc calls | Baseline allocated bytes | Reusable-scratch allocated bytes |
| --- | ---: | ---: | ---: | ---: |
| Optimized solver, 2×64 | 12 / 10 | 0 / 0 | 74,192 | 0 |
| Optimized solver, 8×64 | 48 / 40 | 0 / 0 | 296,768 | 0 |
| Optimized solver, 8×512 (4,096 total) | 48 / 40 | 0 / 0 | 1,845,056 | 0 |
| Optimized solver, 8×4,096 (4,096/source) | 48 / 40 | 0 / 0 | 14,231,360 | 0 |
| Authority point query | 1 / 0 | 0 / 0 | 32 | 0 |
| Authority batch query, 512 | 0 / 0 | 0 / 0 | 0 | 0 |
| Vision-source transform update | 169 / 17–18 | 162 / 12–13 | 189,720–190,344 | 148,096–148,720 |
| Dynamic door update | 416 / 67 | 389 / 45 | 376,468–376,804 | 218,812–219,148 |
| Clean publication | 0 / 0 | 0 / 0 | 0 | 0 |
| No-change source update | 0 / 0 | 0 / 0 | 0 | 0 |

Each row is three explicitly marked current-thread samples. The post-change raw evidence is `Saved/SightWeaveM2P1/AllocationProof/after-scratch-v3/after-scratch-v3.csv`. Capture and analysis each reported exactly one success with zero test warnings/errors. The trace is intentionally ignored because it is generated evidence; reproduce it with the checked-in script rather than committing a 417 MiB binary.

## Storage design

- `SolveOptimizedPolygonInto` resets logical lengths while preserving the caller-owned result arrays and strings. The returning `SolveOptimizedPolygon`/`SolvePolygon` APIs remain compatible wrappers.
- Temporary prepared segments, angular intervals, and active indices live in `FSightWeaveSolverFrame`. `TThreadSingleton` gives every actual executing thread its own storage, so worlds, sources, and workers never share mutable arrays.
- A lease increments a per-thread active depth. The first four nested solves use distinct retained frames. Deeper, abnormal recursion uses a frame owned by that stack lease, so it cannot alias any live frame. RAII releases the depth even on early return or task cancellation through normal C++ unwinding.
- A frame retains at most 8 MiB after a solve. If its three arrays exceed that combined high-water mark, all are emptied on lease release. This prevents an exceptional scene from being retained indefinitely while ordinary warmed workloads keep capacity.
- Scratch contains no `UObject`, world pointer, source pointer, task handle, or published result. World teardown/restart cannot leave a world reference behind; thread exit owns normal `TThreadSingleton` destruction.
- Reference and Verify continue to use their diagnostic result paths. Shipping compiles out the test-only reentrancy probe and still forces Optimized mode.
- Snapshot rebuild reuses the cached entry's output arrays by moving them into a solve result and back. Point query has a public caller-storage overload. These changes remove the output-allocation part of the measured hot path without changing the existing returning APIs.

## Safety and regression evidence

- `SightWeave.M2P1.Scratch.ConcurrentIsolation` runs eight parallel workers, each reusing its own result for sixteen 1,024-segment solves, and compares every vertex, candidate angle, hit distance, and boundary point to the deterministic expected result.
- `SightWeave.M2P1.Scratch.Reentrancy` nests eight leases. This covers the four retained frames and four stack-owned overflow frames and proves no active frame address aliases another.
- Both tests passed with zero warnings/errors. The complete SightWeave regression after these additions passed 87/87 with zero warnings/errors. A fresh final complete regression after all later M2P.1 changes remains a final-validation item.
- `DarkwellEditor Win64 Development` passed after the scratch and tests were added. The only build warning was the existing local-toolchain warning: MSVC 14.51.36256 is newer than Epic's preferred 14.50.35717.

## Remaining work

The solver, point, batch, clean-publication, and no-change allocation gates are closed. Dynamic door and source-transform updates still allocate in their full synchronous rebuild/publication scopes; the dynamic public path is a later gate. The reusable storage removes allocator overhead but does not by itself close the 4,096/source CPU-time gate, which must be re-profiled next.

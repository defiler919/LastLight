# SightWeave M2P.1 final validation

## Final state

The final state is **PARTIAL**, not `COMPLETED` or `BLOCKED`.

The allocator-call, synchronous dynamic-door, correctness, regression, Editor, Lab, plugin-isolation, clean-host, dependency, Git, and LFS gates are closed. The strict 4,096-relevant-segments-per-source single-solve p99 is below 2 ms, but the latest median is 1.733 ms rather than below 1 ms. One of the latest six retained 512-query distributions also exceeded the 250 us p95/p99 gate during a sustained host scheduling/frequency band, so the batch evidence is not called uniformly passing.

The remaining extreme gap is now an architecture boundary rather than an untried local container or arithmetic optimization. Each exact solve reconstructs and orders approximately 24,704 radial-boundary and endpoint/-epsilon/zero/+epsilon events, computes exact directions and intersections, preserves stable-ID tie-breaks, and materializes the full authority boundary. The retained radix/cyclic merge, prepared segment cache, distance-ordered active set, exact radial fast path, allocation-stable scratch, and local topology guard have reduced median CPU by about 69%. Closing the remaining 42% median gap requires replacing per-solve flat event reconstruction with a prepared scene/source event index that can incrementally maintain exact sorted events and invalidation across static/dynamic geometry. That changes cache ownership, mutation invalidation, source lifecycle, and memory bounds across the subsystem and is the explicit architecture replacement that remains after this task. Intra-solve or multi-source parallelism alone would not satisfy the per-source CPU gate.

## Git identity

- Baseline branch: `codex/m2p-sightweave-authority-performance`
- Baseline SHA: `d0b90d2e5687105f1e25bf03476a07d6bb5337de`
- Final branch: `codex/m2p1-sightweave-final-performance-gate`
- Final SHA: this document's containing commit; resolve with `git log -1 --format=%H`
- No merge, rebase, force-push, or M3 work was performed.

Checkpoint commits before this document:

1. `397b9d6382f07b77980e585274a558fb26e1987b` — `docs: start SightWeave final performance gate`
2. `1284174d0dbc9ed096e4c7eb347a3c3a0ac788ce` — `test: instrument SightWeave hot-path allocations`
3. `141aedf172b73af225d9eeadab609c55674f5587` — `perf: add reusable SightWeave solver scratch`
4. `22cf4f63c5e8a629019429cb41162fd650922a57` — `perf: optimize SightWeave extreme authority workloads`
5. `6fc0deb1134520b3b03fed13c3962d10440dcb30` — `perf: harden dynamic occluder dispatch and publication`
6. `83633364fb1f3d1485cef84d875d00033fcfe7fb` — `test: expand SightWeave concurrency and parity coverage`
7. `docs: record SightWeave final performance validation` — this document's containing commit

## Allocator-call proof

The final reproducible proof is `Saved/SightWeaveM2P1/AllocationProof/final-validation`. It starts the capture process with UE's startup-safe `-trace=memory,sightweaveallocation`, emits current-thread begin/end markers around only the warmed target operation from the Editor-only Tests module, and analyzes the completed raw trace in a second process. It does not replace `GMalloc` at runtime. World creation, registration, warmup, logging, Automation, and other-thread events are outside the measured scope. Capture and analysis each passed 1/1 with zero test warnings/errors.

| warmed operation | baseline alloc / realloc | baseline bytes | final alloc / realloc | final bytes |
|---|---:|---:|---:|---:|
| solver 2x64 | 12 / 10 | 74,192 | 0 / 0 | 0 |
| solver 8x64 | 48 / 40 | 296,768 | 0 / 0 | 0 |
| solver 4,096-total | 48 / 40 | 1,845,056 | 0 / 0 | 0 |
| solver 4,096/source | 48 / 40 | 14,231,360 | 0 / 0 | 0 |
| point query | 1 / 0 | 32 | 0 / 0 | 0 |
| batch 512 | 0 / 0 | 0 | 0 / 0 | 0 |
| dynamic door | 416 / 67 | 376,468-376,804 | 0 / 0 | 0 |
| clean publication | 0 / 0 | 0 | 0 / 0 | 0 |
| no-change update | 0 / 0 | 0 | 0 / 0 | 0 |
| source transform | 169 / 17-18 | 189,720-190,344 | 75 / 11-13 | 36,516-48,156 |

All zero rows also report zero frees, allocated bytes, peak temporary bytes, and end temporary bytes for all three final samples. Source transform is explicitly not claimed zero-allocation; its separate latency gate passes.

Reusable Optimized storage uses caller-owned output plus `TThreadSingleton` scratch. Every executing thread has four retained reentrant frames; deeper recursion receives stack-owned overflow storage. RAII releases depth, no scratch owns a world or `UObject`, and frames over an 8 MiB combined high-water mark are emptied on release. Reference/Verify diagnostic storage does not enter the Shipping Optimized hot path. Eight concurrent workers and eight nested leases passed deterministic isolation and non-aliasing tests.

## Latest solver evidence

The final extended run is `Saved/SightWeaveM2P1/FinalValidation/ExtendedSolver`; it passed 1/1. Values are median/p95/p99/max in microseconds.

| workload | result |
|---|---:|
| typical 8x64 radial, all-source CPU | 325.497 / 403.900 / 428.606 / 428.606 |
| typical 8x64 radial, individual solve | 40.300 / 53.599 / 56.300 / 59.500 |
| 4,096-total, all-source CPU | 1,700.897 / 1,740.102 / 1,917.098 / 1,917.098 |
| 4,096-total, individual solve | 211.000 / 223.201 / 268.798 / 340.398 |
| 4,096/source, all-source cumulative CPU | 13,876.293 / 14,405.597 / 14,405.597 / 14,405.597 |
| 4,096/source, individual solve | 1,733.102 / 1,856.200 / 1,896.501 / 1,896.501 |
| 4,096/source, enclosing sequential wall | 13,877.500 / 14,407.199 / 14,407.199 / 14,407.199 |

The final 4,096/source eight-source aggregate stage distributions are:

| stage | median us | p95 us | p99 us | max us |
|---|---:|---:|---:|---:|
| radial boundary | 1.900 | 2.496 | 2.496 | 2.496 |
| candidate/range/height/endpoint preparation | 2,756.692 | 2,841.402 | 2,841.402 | 2,841.402 |
| exact event sort/merge and direction preparation | 3,636.204 | 3,760.800 | 3,760.800 | 3,760.800 |
| angular acceleration build | 778.303 | 798.408 | 798.408 | 798.408 |
| active sweep, exact nearest intersection, output | 5,535.100 | 5,727.004 | 5,727.004 | 5,727.004 |
| polygon postprocess | 0.294 | 0.402 | 0.402 | 0.402 |
| local topology parity guard | 1,181.096 | 1,288.299 | 1,288.299 | 1,288.299 |

The run contains 32,768 candidates, 197,632 rays, 156,641 retained vertices, and 197,765 exact segment tests across eight sources. No production parallel path was introduced: worker wall time is therefore not applicable, and main-thread time is the reported sequential wall rather than a hidden dispatch-only duration.

Retained solver iterations were exact double radix sorting, cached source-relative metadata, radial cyclic endpoint merge, exact base direction reuse with epsilon rotation only for side rays, distance-ordered active pruning, binary insertion and expiry gating, exact in-range fraction division avoidance, sequential result writes, and the proven `i`/`i+2` topology parity guard. Rejected reciprocal multiplication, normalized endpoint-vector direction, `FMath::SinCos`, and wider event records were removed after differential failures or measured regressions. Reference, epsilon, and fixed seeds were not changed.

## Dynamic authority and query evidence

The production policy remains strict synchronous Authority. The public `UpdateOccluder` call performs prepare, dirty discovery, affected vision/illumination solves, materialization, and immutable publication before returning. No worker, pending window, old snapshot, lower update rate, or skipped update exists.

The three final 101-sample dynamic distributions are 222.400/226.900/230.700/231.899 us, 221.301/230.897/233.699/252.098 us, and 223.100/229.903/234.298/236.101 us. All pass median and p99 below 250 us. Publication itself is 220.001/224.300/228.200/229.198 us, 218.801/228.401/231.098/249.602 us, and 220.500/227.202/230.897/233.501 us. Because the path is synchronous:

- public dispatch and main-thread time are the full dynamic totals above;
- worker time, in-flight count, cancellation, and stale-dropped count are not applicable/zero;
- publication has zero additional delay after return;
- maximum observed full-call latency is 252.098 us, while the maximum additional publication delay is 0 us;
- there is no pending authority leakage policy because callers always read the just-published revision.

Held-reader testing retained one immutable snapshot across 32 alternating door updates, proved every update published the current geometry and advanced revision, proved the held snapshot remained unchanged, and then proved it remained safe ordinary data after world teardown. A two-world test proved that updating world A did not change world B's revision or geometry.

The latest six retained batch-512 distributions have five passing p99 results. The three final runs are 233.799/239.700/243.198/243.600 us, 238.001/250.001/288.300/291.802 us, and 235.099/239.801/245.102/245.702 us; the middle tail failure is retained. The preceding three runs had medians 235.800/235.900/235.800 us and p99 243.802/242.900/245.102 us. Source transform is 52.1-52.8 us median and 56.0-58.8 us p99. No-change is 0.201-0.298 us median and 0.302 us p99 and does not advance revision. Typical 8x64 radial final p99 is 428.606 us versus the 766 us non-regression ceiling.

## Correctness and final validation matrix

- Reference/Optimized geometry differential passed all nine hand-authored adversarial cases, all 96 fixed-seed randomized cases, and the new fixed-seed 512-segment high-count case.
- Runtime authority/update differential passed after the final cache/spatial work.
- Shipping compiled the `#if UE_BUILD_SHIPPING` branch that unconditionally calls Optimized; Reference and Verify remain non-Shipping diagnostics, and Verify visibly falls back to Reference on mismatch.
- `DarkwellEditor Win64 Development`: succeeded, up to date after the preceding full rebuild.
- `SightWeave.M1`: 21/21 passed, 0 warning/error/not-run.
- `SightWeave.M2`: 69/69 passed, 0 warning/error/not-run.
- `SightWeave.M2P`: 13/13 passed, 0 warning/error/not-run.
- `SightWeave.M2P1`: 7/7 passed, 0 warning/error/not-run.
- Complete `SightWeave`: 90/90 passed, 0 warning/error/not-run.
- `Darkwell`: 24/24 passed, 0 warning/error/not-run.
- Dynamic-door smoke: 1/1 passed, 0 warning/error/not-run.
- Lab headless Game smoke loaded `/SightWeave/Maps/L_SightWeave_Lab` in 0.043648 seconds, entered play, and reported status 0 with authoritative/live/vision/bypass all 1, snapshot 58, and one attributed vision source.
- Extended solver benchmark: 1/1 passed, 0 warning/error/not-run.
- Allocation capture/analyze: 1/1 plus 1/1 passed, 0 warning/error/not-run.
- Repository-external BuildPlugin at `C:\Users\defiler\AppData\Local\Temp\SightWeaveM2P1Final-20260825-0204`: `BUILD SUCCESSFUL`, AutomationTool exit 0 in 1m49s. Clean HostProject UnrealEditor Win64 Development, UnrealGame Win64 Development, and UnrealGame Win64 Shipping all reported `Result: Succeeded`.

Runtime dependency isolation passed. The Runtime Build.cs lists only `Core`, `CoreUObject`, `Engine`, and `DeveloperSettings`; repository and packaged Runtime sources contain no `Darkwell`, `UnrealEd`, `SightWeaveEditor`, or `SightWeaveTests` references. The packaged `UnrealEditor-SightWeaveRuntime.dll` imports only those four UE DLLs plus Kernel32 and MSVC/CRT libraries.

Every final Automation JSON contains zero test warnings/errors. Final test, allocation, benchmark, and Lab logs contain zero ensure, assertion, fatal, critical error, unhandled exception, or AutomationController error. UE startup emits 13 pre-discovery `LogAutomationTest: Error: Condition failed` engine self-diagnostic lines per process; these are not attached to any queued test, and every exported report is clean. Win64 SDK validation is valid; startup also reports absent/invalid non-Win64 SDKs. Builds warn that local MSVC 14.51.36256 is newer than UE 5.8's preferred 14.50.35717, and clean-host compilation surfaces C4996 deprecations in UE 5.8 headers; no warning points to a SightWeave source line.

## Final repository gates and recovery

Before the final documentation commit, `git diff --check`, cached diff check, `git lfs status`, and `git lfs fsck` passed; LFS reports no pending object and `Git LFS fsck OK`. The UAT-generated untracked `Plugins/SightWeave/Config/FilterPlugin.ini` template was inspected and removed. After this document is committed, explicitly push the branch and its LFS objects, verify the local HEAD, upstream SHA, and `git ls-remote` SHA are identical, and require an empty worktree/index before shutdown.

Unverified/not applicable items are limited to async worker completion order, stale-task rejection, async pending policy, and multi-worker solve wall time because no async or parallel production path exists. The remaining risk is the architectural prepared-event-index work described above; it must preserve exact event ordering, tie-breaks, invalidation, lifecycle safety, bounded memory, and the Reference differential corpus. M3, GPU masks, post process, memory textures, persistence, DARKWELL gameplay integration, and main-branch merge remain outside scope.

Exact next recovery command:

```powershell
git switch codex/m2p1-sightweave-final-performance-gate; git pull --ff-only origin codex/m2p1-sightweave-final-performance-gate; git rev-parse HEAD; git lfs pull; git lfs status
```

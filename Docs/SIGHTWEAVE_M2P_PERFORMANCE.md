# SightWeave M2P CPU authority performance

## Status and acceptance budgets

This document records measured evidence for `SightWeave M2P — CPU Authority Performance Hardening`. A broad automation hang ceiling is not a performance pass. The unchanged CPU targets are:

- game/main-thread registration or dispatch `< 0.25 ms`;
- visibility solves median `< 1.0 ms`, p99 `< 2.0 ms`;
- up to 8 active sources and 4,096 relevant segments;
- 512-subject batch query median `< 0.25 ms`;
- steady-state visibility solve/query hot paths: `0` heap allocations;
- deterministic output with no correctness regression.

The requirements say 4,096 relevant segments **total** and the architecture says 4,096 relevant segments **across dirty sources**. The M2P task also requires the severe interpretation where every one of 8 sources receives 4,096 candidates. Results label `segments/source` and `relevant sum` separately.

## Measurement environment

- Machine: `DESKTOP-IBLNS3N`
- CPU: AMD Ryzen 9 3900X, 12 cores / 24 logical processors
- RAM: 32 GB
- GPU: NVIDIA GeForce RTX 2070 SUPER (not used by the CPU benchmark)
- OS reported by Automation: Windows 10 22H2, `10.0.19045.6466`
- Unreal Engine: 5.8.1, `D:\UE_5.8`
- Build: WindowsEditor Development, SM6, unattended `NullRHI`
- Compiler: MSVC 14.51.36256; Windows SDK 10.0.26100.0
- Fixed seed: `0x51A7E`; per-source deterministic offset `7919`
- Timing: `FPlatformTime::Seconds()` immediately around the named CPU stage or public operation
- Distribution: warmed first; nearest-rank median/p95/p99/max; no world creation, Actor registration, logging, debug drawing, JSON, or geometry hashing inside solver timed regions

The benchmark is reproducible through `SightWeave.M2P.Performance.Baseline`. Extended 256/1024/4096 workloads require the process switch `-SightWeaveExtendedBenchmarks`. Raw reports and logs live under ignored `Saved/` paths and are not committed.

## Reference Solver baseline — 2026-08-24

The table reports microseconds as `median / p95 / p99 / max`. `Relevant sum` is the sum of candidates presented to all source solves in one sample. The first two rows reproduce the original 2/8-source, 64-segment workload with a stronger distribution instead of a single sample.

| Workload | Relevant sum | Repeats | Candidates / rays / vertices | Total µs median / p95 / p99 / max |
| --- | ---: | ---: | ---: | ---: |
| Typical radial, 2 sources × 64 segments/source | 128 | 11 | 128 / 1,024 / 898 | 5,663.604 / 5,701.598 / 5,701.598 / 5,701.598 |
| Typical radial, 8 × 64 | 512 | 11 | 512 / 4,096 / 3,635 | 22,773.497 / 22,872.295 / 22,872.295 / 22,872.295 |
| Typical directional cone, 8 × 64 | 512 | 11 | 512 / 1,448 / 1,359 | 6,243.899 / 6,282.203 / 6,282.203 / 6,282.203 |
| Typical radial, 8 × 256 | 2,048 | 7 | 2,048 / 13,312 / 8,838 | 232,822.005 / 233,253.695 / 233,253.695 / 233,253.695 |
| Dense radial, 8 × 1,024 | 8,192 | 5 | 8,192 / 50,176 / 46,520 | 3,390,991.498 / 3,466,146.991 / 3,466,146.991 / 3,466,146.991 |
| Dense radial, **4,096 total** = 8 × 512 | 4,096 | 5 | 4,096 / 25,600 / 24,784 | 917,852.398 / 920,467.503 / 920,467.503 / 920,467.503 |
| Dense radial, **4,096 per source** = 8 × 4,096 | 32,768 | 3 | 32,768 / 197,632 / 156,641 | 46,951,673.295 / 46,987,216.204 / 46,987,216.204 / 46,987,216.204 |

### Stage medians and hotspot shares

| Workload | Boundary | Candidate/filter | Sort/dedup | Raycast | Post | Topology | Dominant share |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 2 × 64 radial | 1.699 | 14.201 | 24.803 | 3,328.200 | 0.101 | 2,293.598 | raycast 58.8%, topology 40.5% |
| 8 × 64 radial | 7.194 | 57.999 | 126.496 | 13,249.502 | 0.201 | 9,337.991 | raycast 58.2%, topology 41.0% |
| 8 × 64 cone | 1.803 | 55.403 | 40.501 | 4,731.398 | 0.197 | 1,414.597 | raycast 75.8%, topology 22.7% |
| 8 × 256 radial | 10.200 | 224.996 | 502.400 | 174,731.106 | 0.302 | 57,449.203 | raycast 75.0%, topology 24.7% |
| 8 × 1,024 radial | 23.603 | 783.902 | 1,800.403 | 1,926,429.905 | 1.404 | 1,462,906.200 | raycast 56.8%, topology 43.1% |
| 8 × 512 / 4,096 total | 20.891 | 401.497 | 880.808 | 494,356.293 | 0.805 | 421,034.001 | raycast 53.9%, topology 45.9% |
| 8 × 4,096 / 32,768 sum | 24.494 | 3,087.699 | 8,220.896 | 30,508,919.999 | 1.702 | 16,431,442.093 | raycast 65.0%, topology 35.0% |

The profile confirms two production blockers rather than framework overhead:

1. Every candidate ray scans every candidate segment, so the dominant work grows as `rays × segments`.
2. `IsSimplePolygon` compares non-adjacent polygon edges quadratically and consumes 23–46% of measured time. It remains necessary in Reference/Verify, but cannot remain in the optimized production hot path.

Fixed 128-step radial boundaries, endpoint event generation, sorting, deduplication, and polygon post-processing collectively remain below 1% in the representative 8×64 workload. They are not the first optimization target, though their temporary storage still matters for zero-allocation steady state.

The Reference owned-array working capacity was 135,168 bytes at 8×64, 851,968 bytes at 4,096-total, and 6,586,368 bytes at 4,096-per-source. This is allocated capacity, not yet a claim about allocator call counts.

## Runtime pipeline baseline

The runtime fixture contains 64 static occluder segments, radial and camera-cone vision, bypass and requires-illumination policies, Visible/Infrared multi-capability compatibility, two legal lights, and a dynamic door. Times are microseconds.

| Operation | Median | p95 | p99 | Max | Notes |
| --- | ---: | ---: | ---: | ---: | --- |
| Spatial candidate query, 64 | 8.099 | 9.000 | 9.801 | 11.601 | 64 returned; output capacity 7,872 bytes |
| Snapshot publish/copy, 4 vision + 2 light + 64 segments | 6.702 | 7.000 | 11.101 | 11.101 | no dirty solve in timed call |
| Public snapshot value copy | 5.201 | 5.599 | 6.303 | 6.303 | copied ordinary immutable data |
| Authority point query | 18.001 | 30.801 | 32.902 | 42.301 | polygon containment and attribution |
| 512-request batch | 11,630.598 | 11,905.201 | 11,973.500 | 11,973.500 | output 57,344 outer + 16,512 inner bytes |
| Dynamic-door local update + affected solve + publish | 15,892.502 | 16,100.802 | 16,153.101 | 16,153.101 | local cells, but synchronous dirty solves dominate |
| Source transform update + solve + publish | 2,924.901 | 2,952.699 | 2,959.598 | 2,959.598 | current call is not a sub-0.25 ms dispatch |

The spatial lookup and clean snapshot copy are already small. Batch query is approximately 46.5× over its 0.25 ms median budget; 8×64 radial p99 is 11.4× over the 2 ms solve budget; 4,096-total p99 is 460× over. Source update p99 is 11.8× over the dispatch budget because the public mutation performs solve and publication synchronously.

## Baseline run history

- First instrumentation build: passed, UBT success, 21.00 seconds; only the existing non-preferred MSVC warning.
- First extended benchmark: `1 succeeded / 1 failed`. The random dense generator produced crossing, non-normalized authored segments, so Reference correctly returned non-simple polygon failures at 1,024 and 4,096 segments. Its valid 64–256 measurements agreed with the final baseline. This result remains in `Saved/AutomationReports/SightWeaveM2P_Baseline_20260824`.
- The fixture was corrected to deterministic, non-intersecting concentric short tangents without changing source/candidate/event scale.
- Corrected rebuild: passed, UBT success, 5.85 seconds.
- Corrected extended benchmark: **2 discovered, 2 run, 2 passed, 0 failed/warning/not-run**, process exit code 0, duration 170.033 seconds. Report: `Saved/AutomationReports/SightWeaveM2P_Baseline_ValidDense_20260824/index.json`.

## Optimized solver — final candidate 2026-08-24

Profile evidence led to a deterministic angular-interval candidate sweep rather than the initially prototyped BVH. The optimized solver deliberately keeps the Reference event set and exact intersection/tie semantics:

1. filter the same floor/height candidates and emit the same fixed boundary plus endpoint/±epsilon rays;
2. prepare cache-local segment data and conservative angular intervals expanded for the existing inclusive endpoint tolerance;
3. sort intervals deterministically, then sweep the sorted candidate rays while maintaining only intervals that cover the current ray;
4. run the same analytic intersection and stable-ID nearest-hit rule on that reduced set;
5. construct by sorted angle and omit quadratic topology validation only in Optimized production mode.

Reference remains callable as the Oracle. Non-Shipping Verify runs both and visibly falls back to Reference on a detected mismatch. Shipping always executes Optimized and contains no mode that runs Reference/Verify.

Times below are microseconds as `median / p95 / p99 / max`. A sample includes all named source solves, matching the Reference table.

| Workload | Candidates / rays / vertices | Optimized total µs median / p95 / p99 / max | Median speedup |
| --- | ---: | ---: | ---: |
| Typical radial, 2 × 64 | 128 / 1,024 / 898 | 117.801 / 120.398 / 126.000 / 126.000 | 48.1× |
| Typical radial, 8 × 64 | 512 / 4,096 / 3,635 | 485.197 / 492.286 / 495.002 / 495.002 | 46.9× |
| Typical cone, 8 × 64 | 512 / 1,448 / 1,359 | 251.401 / 256.103 / 259.001 / 259.001 | 24.8× |
| Typical radial, 8 × 256 | 2,048 / 13,312 / 8,838 | 2,330.702 / 2,346.803 / 2,358.802 / 2,358.802 | 99.9× |
| Dense radial, 8 × 1,024 | 8,192 / 50,176 / 46,520 | 5,960.889 / 6,031.394 / 6,142.505 / 6,142.505 | 568.9× |
| Dense radial, **4,096 total** = 8 × 512 | 4,096 / 25,600 / 24,784 | 2,828.199 / 2,864.107 / 2,870.202 / 2,870.202 | 324.5× |
| Dense radial, **4,096/source** = 8 × 4,096 | 32,768 / 197,632 / 156,641 | 31,627.502 / 31,659.801 / 31,659.801 / 31,659.801 | 1,484.5× |

At the documented 4,096-total scale, the eight-solve sample is approximately 0.354 ms median and 0.359 ms p99 per solve, below the 1/2 ms worker thresholds. The intentionally harsher 4,096-per-source interpretation is approximately 3.953/3.957 ms per solve and therefore fails that stress interpretation; it is not relabeled as a pass.

The 8×64 all-source warm p99 is 0.495 ms, comfortably below its explicit 2 ms gate. The interval sweep tests 8,334 exact segment intersections instead of the Reference path's 2,097,152 potential ray/segment pairs in that sample. At 4,096 total it tests 34,520 instead of 13,107,200 potential pairs. Output candidate, ray, and vertex counts match Reference in every row.

Raw final candidate report: `Saved/AutomationReports/SightWeaveM2P_OptimizedFinalCandidate_20260824/index.json` (ignored, not committed). Remaining runtime, allocator-call, and differential evidence is recorded in later sections/checkpoints; solver performance alone is not a final M2P completion claim.

## Hardened runtime pipeline — final candidate 2026-08-24

The query path uses immutable polar boundary points plus a 1,024-bin exact-refinement lookup, with conservative fallback to the original polygon classifier near any boundary. Compatibility is resolved to direct immutable snapshot indices, duplicate illumination containment is cached per query, batch output arrays retain their capacity, and one snapshot/floor context is shared across an anchor batch. Clean publication and identical normalized updates return without a new snapshot or revision.

| Operation | Median | p95 | p99 | Max | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| Spatial candidate query, 64 | 7.801 | 8.099 | 8.300 | 11.601 | stable |
| Clean snapshot publish | 0.000 | 0.101 | 5.800 | 5.800 | no-copy fast path |
| Public snapshot value copy | 11.001 | 11.999 | 12.100 | 12.100 | explicit caller copy |
| Authority point query | 0.697 | 0.902 | 3.502 | 3.900 | 25.8× median speedup |
| 512-request batch | **244.800** | 252.001 | 268.500 | 268.500 | median passes 0.25 ms; tail does not |
| Dynamic door local update + solve + publish | 503.898 | 510.599 | 511.102 | 511.102 | 31.5× median speedup |
| Source transform update + solve + publish | **83.100** | 89.601 | 90.502 | 90.502 | dispatch gate passes |
| Identical source update | 0.298 | 0.302 | 0.302 | 0.302 | no revision change |

The warmed 512-result outer and attribution-array capacities grew by **0 bytes** over the timed distribution. This proves no `TArray` capacity growth in that fixture, not global allocator call count. The optimized solver still owns per-call result/work arrays, so the strict “0 heap allocations” gate is not claimed without allocator-hook evidence and reusable solver scratch.

Report: `Saved/AutomationReports/SightWeaveM2_RuntimeHardened_20260824/index.json`; **59 discovered/run/passed, 0 failed/warning/not-run**. The 512-batch median is 47.5× faster than baseline. Its p95/p99 remain 2.001/18.500 µs over 0.25 ms, and are explicitly retained as a remaining tail risk.

## Differential correctness and post-parity final measurements — 2026-08-24

The completed differential suite compares the Reference and Optimized solver at geometry and gameplay-query levels. It includes 9 manual adversarial cases, 96 fixed-seed valid randomized cases, dense point/boundary classification, and a paired runtime trace covering vision, illumination, bypass, suppression, attribution/rejection, isolation, batch/sample rules, dynamic-door transitions, revision/determinism, and stale-snapshot rejection.

An initial randomized generator was tightened to keep every segment strictly inside a disjoint polar sector, avoiding the same invalid crossing-authored-geometry problem already recorded in the baseline history. With valid geometry, four CameraCone near-awareness cases still produced a real semantic difference: Reference rejected a near-collinear local boundary-event cluster as non-simple while Optimized accepted the identical boundary. The minimum failures were always edge `i` against `i+2`. Optimized now applies an O(vertices) constant-neighborhood topology-degeneracy guard using the same inclusive topology tolerance. General O(vertices²) `IsSimplePolygon` remains out of Optimized and Shipping.

Final differential report: `Saved/AutomationReports/SightWeaveM2P_Differential_Pass_20260824/index.json`; **3/3 passed**, zero failed/warning/not-run. The subsequent full `SightWeave.M2` prefix passed **62/62**, zero failed/warning/not-run.

The parity guard adds measured linear work, so the table below supersedes the earlier optimized timing table for final acceptance. Times remain microseconds as `median / p95 / p99 / max` for all eight solves unless stated otherwise.

| Workload | Final optimized total µs median / p95 / p99 / max | Approx. per-solve median / p99 | Gate |
| --- | ---: | ---: | --- |
| Typical radial, 8 × 64 | 749.797 / 762.004 / 765.700 / 765.700 | 93.7 / 95.7 | pass |
| Typical directional cone, 8 × 64 | 351.306 / 358.496 / 358.794 / 358.794 | 43.9 / 44.8 | pass |
| Typical radial, 8 × 256 | 3,080.703 / 3,105.994 / 3,153.399 / 3,153.399 | 385.1 / 394.2 | pass |
| Dense radial, 8 × 1,024 | 9,322.096 / 10,863.200 / 11,238.996 / 11,238.996 | 1,165.3 / 1,404.9 | pass |
| Dense radial, **4,096 total** = 8 × 512 | 4,489.996 / 4,578.594 / 4,606.094 / 4,606.094 | **561.2 / 575.8** | pass |
| Dense radial, **4,096/source** = 8 × 4,096 | 44,404.898 / 44,786.401 / 44,786.401 / 44,786.401 | **5,550.6 / 5,598.3** | fail severe interpretation |

The post-differential runtime run measured:

| Operation | Median | p95 | p99 | Max | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| Authority point query | 0.499 | 0.700 | 0.801 | 1.099 | pass |
| 512-request batch | **206.102** | **210.300** | **211.101** | **211.101** | pass this distribution |
| Dynamic door update + solve + publish | 652.701 | 675.201 | 694.402 | 694.402 | local affected solve only |
| Source transform update + solve + publish | **115.298** | 121.802 | 122.700 | 122.700 | dispatch gate passes |
| Identical source update | 0.298 | 0.302 | 0.302 | 0.302 | no revision change |

Raw reports: `Saved/AutomationReports/SightWeaveM2P_OptimizedAfterDifferential_20260824/index.json` and `Saved/AutomationReports/SightWeaveM2_AfterDifferential_20260824/index.json`. Candidate/ray/vertex counts continue to match Reference. Zero warmed `TArray` capacity growth is retained as evidence, but actual allocator-call count remains uninstrumented; therefore the strict zero-heap gate is not claimed.

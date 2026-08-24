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

## Optimization direction selected from evidence

The production path should keep the exact endpoint-event semantics but replace all-segments-per-ray scans with deterministic spatial acceleration (a local 2D segment BVH or an equivalently exact angular structure). This is lower semantic risk than immediately introducing a fragile active-order angular sweep because it can reuse the same analytic intersection and stable-ID tie-break. Optimized construction must guarantee topology and omit O(vertices²) validation from the production hot path; Reference and Verify retain it. Scratch buffers and batch output must be reused, compatibility lookup must be pre-resolved, and no-change mutations must avoid new revisions/publications.

No performance gate is passed yet. These are optimization-before measurements.

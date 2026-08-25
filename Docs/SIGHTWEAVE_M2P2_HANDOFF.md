# SightWeave M2P.2 — Motion Hot Path & Prepared Event Index handoff

## Status

- State: **IN_PROGRESS**
- Baseline branch: `codex/m2p1-sightweave-final-performance-gate`
- Baseline SHA: `69ac8d50019ef7674c2aed58d2c0c931ee8fa874`
- Working branch: `codex/m2p2-sightweave-motion-event-index`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current phase: warmed transform allocation, the bounded exact prepared-origin index, shared origin preparation, and 512-query headroom are implemented; expanded lifecycle/concurrency coverage and the final validation/package matrix are next
- Latest safe commit: `0ebf849a2e2487c634fbedf38a631a50b76a52ae` (`perf: add bounded SightWeave prepared event index`); the tested 512-query checkpoint described below is pending commit/push
- Next recovery command: `git switch codex/m2p2-sightweave-motion-event-index; git pull --ff-only origin codex/m2p2-sightweave-motion-event-index; git rev-parse HEAD; git lfs pull; git lfs status`

## Objective and exclusions

M2P.2 must eliminate warmed source-transform allocations, establish a bounded and precisely invalidated prepared-event index for static/dynamic geometry and active sources, distinguish rotation/translation/teleport/profile/dynamic-door update strategies, and safely share only compatible observer-origin geometry preparation. The hard targets include source-transform p99 at or below 0.10 ms, 4,096-segments/source median below 1 ms and p99 below 2 ms, normal all-source and 4,096-total non-regression, and ten independent 512-query batch distributions at or below 0.15/0.18/0.20 ms median/p95/p99.

Reference remains the correctness oracle. Shipping remains Optimized-only. Immediate authority, exact events/tie-breaks, immutable held readers, floor/height/owner/capability isolation, and bounded memory are mandatory. GPU masks, post process, memory textures/tiles, persistence, DARKWELL gameplay integration, `/Game/Maps/L_Prototype`, M3, main merge, force-push, and tolerance/seed weakening are outside scope.

## Recovered baseline evidence

- Warm source transform: 75 allocations, 11–13 reallocations, and 36,516–48,156 allocated bytes. The latest M2P.1 latency was approximately 52.1–52.8 us median and 56.0–58.8 us p99.
- Warm solver, point query, 512 batch, dynamic door, clean publication, and no-change update: proven 0 allocation/reallocation/bytes through startup `memory,sightweaveallocation` tracing.
- 4,096/source individual solve: 1,733.102 / 1,856.200 / 1,896.501 / 1,896.501 us median/p95/p99/max. P99 passes; median fails the strict 1 ms gate.
- 4,096/source eight-source cumulative CPU: 13,876.293 / 14,405.597 / 14,405.597 / 14,405.597 us. Enclosing sequential wall is 13,877.500 / 14,407.199 / 14,407.199 / 14,407.199 us.
- Typical 8x64 radial all-source: 325.497 / 403.900 / 428.606 / 428.606 us.
- 4,096-total individual solve: 211.000 / 223.201 / 268.798 / 340.398 us.
- Latest six retained 512-batch distributions have five passing M2P.1 tail results, but the requested M2P.2 gates are substantially tighter. The final three were 233.799/239.700/243.198/243.600 us, 238.001/250.001/288.300/291.802 us, and 235.099/239.801/245.102/245.702 us.
- Strict synchronous dynamic door: final distributions were approximately 221–223 us median and 231–234 us p99, with no stale snapshot window.
- Final M2P.1 validation: SightWeave 90/90 and DARKWELL 24/24; Editor, Lab, allocation proof, extended benchmark, BuildPlugin, clean host, Shipping dependency isolation, Git, and LFS passed.

## Source-transform call chain and allocation problem

The production call chain is:

1. `USightWeaveVisionSourceComponent::OnUpdateTransform` calls `RefreshVisionSourceRegistration`.
2. `BuildWorldDescription` copies the full `FSightWeaveVisionSourceDescription`, including the compatibility `TArray<FName>`, then replaces its transform.
3. `USightWeaveWorldSubsystem::UpdateVisionSource` copies the full description again, normalizes capability storage, compares reflected structs, replaces the registry value, unconditionally removes cached solve segments and the shared-pointer prepared cache, marks the source dirty, advances revision, and synchronously publishes.
4. `RebuildVisionSnapshotEntry` moves retained output arrays into a temporary result, resets compatibility/LUT storage, deep-copies the description into the entry, re-queries candidate segments after the source cache removal, creates a new prepared cache/control block, reconstructs exact events/intervals/output, then moves results back.
5. `PublishSnapshot` deep-copies every cached vision and illumination entry into a reusable or new frame snapshot, materializes every enabled occluder segment and suppression, rebuilds all compatibility arrays, then atomically swaps the immutable shared pointer.

This proves the remaining allocations are not one isolated `TArray` growth. Likely sources include description/capability copying and normalization, `TMap` cache removal/recreation plus `TSharedPtr` control blocks, segment candidate re-query internals, entry description/output/LUT materialization, complete-frame entry copies, and compatibility arrays. Startup trace callstack/size evidence must identify the exact retained set before production changes.

## Prepared Event Index decision

The full decision and proof obligations are in `Docs/SIGHTWEAVE_M2P2_EVENT_INDEX_ARCHITECTURE.md`. Three candidate families were compared:

1. **Floor/static-scene prepared metadata:** retain stable-ID keyed world segment metadata and static/dynamic revision partitions. This should reduce repeated validation and static extraction but cannot alone reuse observer-relative angles after translation.
2. **Per-source persistent exact event index:** retain endpoint base events, sorted order, directions, interval acceleration, and output capacity for one source geometry key. Rotation-only can potentially preserve relative event order by changing the frame/orientation cut; translation needs exact endpoint-angle recomputation and order-crossing detection, with deterministic full rebuild fallback.
3. **Shared observer-origin occlusion preparation:** share only origin/floor/height/occluder-revision compatible geometry preparation across vision and illumination sources. Different cone/range/capability/owner/handle/revision must still produce isolated final results. Range sharing requires exact proof because candidate inclusion and nearest-hit truncation differ.

The accepted direction is a bounded world-owned hybrid: registered floor/static metadata, a 32-entry/64-MiB initial observer-origin preparation pool with exact keys and deterministic revision/ordinal eviction, and source-owned final views. A hit requires exact origin/floor/height/tolerance/static+dynamic revision/candidate-sequence equality. Source views retain range/cone/forward/owner/capability/policy/handle/revision isolation. Floor metadata alone is rejected because the measured 4,096/source residual is about 1.43 ms/source; retaining candidate preparation, event ordering, and angular acceleration leaves a measured approximately 0.83 ms/source residual. Final-polygon sharing, unbounded caches, and approximate small-motion order retention are rejected.

Pure-radial rotation reuses world geometry while still advancing public metadata/revision. Cone rotation recuts/sweeps exact absolute prepared events. Translation recomputes exact endpoint keys; a bounded kinetic reorder must validate the full deterministic order or fall back to radix rebuild. Teleport/profile/floor changes use exact lookup or full rebuild. Dynamic revisions invalidate intersecting entries synchronously. Capacity pressure falls back to the exact uncached solver instead of growing or returning stale data.

## Corrected motion/cache baseline

`SightWeave.M2P2.Performance.MotionTrace` now measures the update call only; published snapshot retrieval and value-copying are outside the timed region. No-change samples do not reuse stale rebuild/solve counters. Ordinary source samples attribute candidate/event/vertex and source CPU values to the target handle, while dynamic-occluder samples explicitly label their frame-wide snapshot count as an upper bound. Compound door+motion and four-source shared-origin workloads sample each synchronous publication separately and aggregate only their update-call durations.

The corrected headless run passed **1/1**, with 0 failed, 0 succeeded-with-warnings, and 0 not-run, in 0.08985 seconds. Report: `Saved/AutomationReports/SightWeaveM2P2_MotionBaselineV2/index.json`; log: `Saved/Logs/SightWeaveM2P2_MotionBaselineV2.log`. The engine's pre-worker `UnifiedErrorTest` self-diagnostic messages remain outside the exported test result and are not SightWeave failures.

Selected baseline distributions are median/p95/p99/max microseconds:

| Workload | Total update | Exact target source CPU | Revisions / rebuilds | Event observation |
|---|---:|---:|---:|---:|
| no change | 0.201 / 0.298 / 0.399 / 5.800 | 0 / 0 / 0 / 0 | 0 / 0 | 39,592 reused, 0 rebuilt |
| radial rotation 0.5 deg | 39.298 / 47.699 / 140.600 / 157.200 | 26.599 / 31.199 / 40.900 / 41.001 | 101 / 101 vision | 39,592 rebuilt |
| cone rotation 0.5 deg | 17.598 / 18.198 / 19.100 / 22.400 | 9.499 / 9.898 / 10.800 / 14.000 | 101 / 101 vision | 12,019 rebuilt |
| camera rotation 1 deg | 45.002 / 59.098 / 198.700 / 301.801 | 31.300 / 39.600 / 172.202 / 276.100 | 101 / 101 vision | 40,198 rebuilt |
| translation 1 cm | 18.898 / 21.100 / 31.903 / 32.697 | 10.800 / 12.100 / 12.599 / 14.201 | 101 / 101 vision | 14,443 rebuilt |
| translation 5 cm | 19.699 / 21.398 / 23.600 / 23.600 | 11.202 / 11.403 / 12.502 / 12.897 | 101 / 101 vision | 14,443 rebuilt |
| diagonal 5 cm | 30.603 / 38.400 / 42.398 / 43.999 | 16.801 / 22.199 / 26.003 / 29.199 | 101 / 101 vision | 14,443 rebuilt |
| endpoint-order crossing | 16.201 / 16.797 / 16.998 / 17.799 | 8.300 / 8.501 / 9.198 / 9.902 | 101 / 101 vision | 9,292 rebuilt |
| range change at scene origin | 14.000 / 19.200 / 19.901 / 21.301 | 7.100 / 10.602 / 11.697 / 11.701 | 101 / 101 vision | 10,489 rebuilt |
| dynamic door | 46.898 / 55.302 / 110.403 / 232.499 | 46.801 / 53.994 / 87.097 / 202.201 frame sum | 101 / 202 vision + 202 illumination | snapshot event upper bound 96,761 |
| door plus source motion | 66.400 / 70.501 / 99.801 / 148.501 | 57.209 / 59.597 / 73.202 / 108.995 mixed frame+target | 201 / 301 vision + 200 illumination | one intentional initial no-change update |
| four compatible-origin sources | 76.097 / 157.502 / 180.699 / 232.700 | 44.301 / 88.401 / 92.898 / 97.003 target sums | 404 / 202 vision + 202 illumination | 80,497 exact target events rebuilt |

The baseline's `strategy` names describe the semantic workload, not an implemented reuse path. Every changed source currently reports a cache miss and rebuilt events. Radial orientation still triggers a full rebuild even though the radial result is orientation-independent. The four-source workload advances four public revisions per iteration and independently rebuilds all four source results, which establishes the shared-origin opportunity without claiming that final output can be shared.

## Implemented transform hot path and prepared index checkpoint

The production implementation now includes:

- transform-only vision and illumination mutation APIs used by both source components, with exact no-change detection, invalid-input rejection, synchronous revision/publication, and preservation of all non-transform metadata;
- retained candidate-query keys and spatial-query scratch, including in-place segment assignment that preserves nested `SourceEdgeIndices` capacity;
- source output/snapshot buffers and prepared cache control blocks retained across ordinary updates;
- absolute endpoint angles, exact forward-independent segment preparation, retained canonical endpoint order/directions, and a validated-cache solver entry point that avoids a duplicate exact-key scan;
- one world-owned bounded prepared-origin index with 32 entries and 64 MiB defaults, clamped configuration, exact origin/floor/height/tolerance/full-segment-sequence comparison, source binding counts, deterministic unbound LRU/ordinal replacement, hard-cap exact fallback, retained-capacity byte accounting, and teardown/invalidation;
- sharing of plain origin preparation across vision and illumination sources while final descriptions, polygons, capability/policy fields, handles, source revisions, and attribution remain source-owned;
- public aggregate diagnostics for hits, misses, full rebuilds, evictions, capacity/oversized fallback, invalidations, live/high-water bytes, entries, and bindings.

The allocation trace first reduced transform work to 65 four-byte allocations. Captured stack `2256283` and symbolized runtime frames proved that `FSightWeaveFloorSpatialIndex::Query` copy-constructed every `FSightWeaveSegment2D::SourceEdgeIndices`. In-place assignment into retained live elements removed that cost. A later failed proof caught one zero-byte `ReallocShrink` on teleport when the retained absolute endpoint direction array was resized to zero; `EAllowShrinking::No` removed it. Both failed traces remain under ignored `Saved/SightWeaveM2P1/AllocationProof` evidence directories.

The final startup memory-trace proof is `Saved/SightWeaveM2P1/AllocationProof/M2P2PreparedIndexGatedFinal_20260825`. Capture and analysis each passed 1/1 with zero warnings/failures/not-run and exactly 60 CSV rows. All 24 strict warmed motion rows are exact **0 allocation calls / 0 reallocation calls / 0 allocated bytes**:

- ordinary source transform;
- radial rotation;
- cone rotation;
- 1, 5, and 20 cm translation;
- teleport;
- four-source shared-origin aggregate.

Range growth still reports 3 calls/12 bytes when candidate cardinality grows. Held-reader publication can allocate a new immutable frame buffer, and dynamic-door-plus-motion can allocate four-byte edge attribution when changed geometry introduces new nested content. These are reported separately and are outside the strict ordinary warmed-transform gate; held snapshots are never mutated.

The counter-corrected implementation run is `Saved/AutomationReports/SightWeaveM2P2_PreparedIndexCounters/index.json` and `Saved/Logs/SightWeaveM2P2_PreparedIndexCounters.log`. It passed 1/1 with no warnings/failures/not-run. Selected median/p95/p99/max values in microseconds are:

| Workload | Total update | Prepared hit/miss/full rebuild |
|---|---:|---:|
| no change | 0.000 / 0.101 / 0.101 / 0.101 | 0 / 0 / 0; no lookup or publication |
| radial rotation 0.5 deg | 17.300 / 17.699 / 20.299 / 23.499 | 101 / 0 / 0 |
| cone rotation 0.5 deg | 11.198 / 11.399 / 11.500 / 11.500 | 101 / 0 / 0 |
| camera rotation 1 deg | 26.200 / 45.799 / 65.099 / 91.903 | 101 / 0 / 0 |
| translation 1 cm | 21.301 / 24.099 / 27.999 / 31.799 | 101 / 0 / 0 resident exact origins |
| translation 5 cm | 21.502 / 25.000 / 32.697 / 35.103 | 101 / 0 / 0 resident exact origins |
| translation 20 cm | 20.698 / 26.401 / 29.299 / 85.998 | 101 / 0 / 0 resident exact origins |
| teleport | 5.599 / 6.802 / 7.499 / 9.201 | 0 / 101 / 101 exact empty-scene rebuilds |
| dynamic door plus motion | 56.498 / 59.601 / 63.803 / 70.300 | 398 / 103 / 103 |
| four compatible-origin sources | 57.798 / 66.299 / 117.201 / 180.297 aggregate | 303 / 101 / 101: exactly one rebuild then three shared hits per iteration |

The four-source exact target solve median fell from 44.301 to 35.703 us. Aggregate p99 contains publication/scheduler tail and is 117.201 us, or about 29.3 us/source; individual representative transform p99 values remain below 100 us.

The new 4,096-segment production cached sequence runs 101 warmed samples and hard-gates median below 1 ms and p99 below 2 ms. Two independent passing runs were:

- 947.401 / 1,465.101 / 1,564.801 / 1,722.701 us;
- 921.700 / 1,346.998 / 1,642.700 / 1,876.798 us.

The second run's candidate/event-merge/angular-acceleration medians were 65.699/160.202/92.201 us. Reports: `SightWeaveM2P2_Prepared4096_Initial2` and `SightWeaveM2P2_Prepared4096_Headroom1`.

Current clean automation evidence after index integration:

- full `DarkwellEditor Win64 Development` build passed; only the installed MSVC 14.51 versus preferred 14.50 warning remains;
- `SightWeave.M2.Runtime`: 9/9;
- `SightWeave.M2P.Differential`: 3/3 after absolute events, sharing, validated-cache fast path, and seam binary search;
- `SightWeave.M2P2.PreparedEventIndex`: 2/2 for exact cross-kind sharing, range/floor-height isolation, held-reader immutability, binding release, hard one-slot capacity fallback, reclaim, deterministic eviction, and byte cap;
- `SightWeave.M2P2.TransformAPI`: 1/1 for no-change revision preservation, NaN/invalid-handle rejection, valid revision advance, and non-transform metadata preservation.

## 512-query headroom checkpoint

The production query path now retains exact semantics while removing repeated work before final point evaluation:

- immutable polygon bounds reject points before polar `atan2`/LUT refinement;
- nominal-shape distance uses squared range checks and an equivalent cosine threshold instead of `acos`;
- each published source entry stores its normalized nominal forward and tolerance-adjusted minimum cosine;
- directional and camera cones use the invariant that the exact visibility polygon is contained by the nominal cone, so nominal-cone misses skip polar work;
- uniform batches of 256-512 valid anchor queries on one floor, with no suppressions, at most 16 vision and 64 illumination sources, and one exact shared owner/floor/Z, prefilter the immutable snapshot once into fixed stack storage and reuse that state for every point;
- mixed-owner/floor/Z, invalid, non-anchor, small, oversized, suppressed, or higher-source-count batches retain the existing exact serial fallback.

The uniform path is serial and therefore does not replace or hide the single-source CPU measurements required by the motion and 4,096/source gates. The performance test first compares all 512 reflected result fields against independent point queries, then measures ten separate warmed distributions of 101 samples each. Every distribution independently hard-gates median/p95/p99 at 150/180/200 us and checks zero result-capacity growth.

The pre-optimization ten-distribution run failed with worst 148.300/225.499/302.698 us median/p95/p99. AABB, cosine, and cone prefiltering reduced the stable serial median to approximately 90-95 us. A subsequent `ParallelFor` prototype passed the latency gate twice, but startup trace `M2P2BatchParallelFinalV2_20260825` proved that every batch incurred exactly 2 allocations / 0 reallocations / 912 allocated bytes (the UE task data and task array). It was rejected rather than weakening the zero-allocation requirement. The final fixed-stack uniform serial path passed twice in independent editor invocations:

| Run | Distribution | median / p95 / p99 / max us |
|---|---:|---:|
| `UniformSerial1` | 0 | 89.001 / 127.099 / 142.302 / 144.899 |
|  | 1 | 90.800 / 122.398 / 174.597 / 201.099 |
|  | 2 | 89.802 / 120.603 / 128.102 / 134.200 |
|  | 3 | 93.099 / 96.399 / 106.398 / 270.899 |
|  | 4 | 95.502 / 143.200 / 150.699 / 162.799 |
|  | 5 | 93.102 / 131.998 / 141.300 / 175.100 |
|  | 6 | 89.198 / 92.201 / 95.800 / 138.998 |
|  | 7 | 89.403 / 137.303 / 153.601 / 182.301 |
|  | 8 | 89.098 / 129.998 / 161.901 / 173.900 |
|  | 9 | 89.198 / 93.199 / 100.300 / 152.800 |
| `UniformSerial2` | 0 | 88.800 / 92.398 / 106.100 / 130.601 |
|  | 1 | 90.700 / 131.600 / 139.900 / 159.401 |
|  | 2 | 89.001 / 141.799 / 150.003 / 208.899 |
|  | 3 | 128.098 / 147.000 / 168.499 / 200.100 |
|  | 4 | 89.999 / 97.599 / 126.302 / 129.599 |
|  | 5 | 90.100 / 133.500 / 161.599 / 167.601 |
|  | 6 | 89.001 / 92.100 / 94.403 / 94.600 |
|  | 7 | 89.198 / 118.800 / 186.000 / 214.003 |
|  | 8 | 89.299 / 141.300 / 153.102 / 156.000 |
|  | 9 | 88.900 / 95.397 / 103.202 / 104.200 |

All 20/20 distributions pass. Worst median/p95/p99 are 128.098/147.000/186.000 us, with zero warmed capacity growth in every distribution. Reports: `Saved/AutomationReports/SightWeaveM2P2_Batch512Gate_UniformSerial1/index.json` and `Saved/AutomationReports/SightWeaveM2P2_Batch512Gate_UniformSerial2/index.json`. Startup trace `Saved/SightWeaveM2P1/AllocationProof/M2P2BatchUniformSerialFinalV3_20260825` passed capture and analysis 1/1; all three batch samples and all 24 strict warmed transform samples are exact 0 allocation calls / 0 reallocation calls / 0 allocated bytes. With the final evaluator refactor, `SightWeave.M2P.Differential` passed 3/3 and `SightWeave.M2.Query` passed 14/14 (`SightWeaveM2PDifferential_UniformSerialFinal`, `SightWeaveM2Query_UniformSerialFinal`).

## Planned phases

1. Add a reproducible motion trace benchmark covering no-change, radial/cone/camera rotation, 1/5/20 cm and diagonal/wall/endpoint-crossing translation, teleport/floor/profile/range/height changes, dynamic-door combinations, and shared-origin source groups. Record cold/warm cost, allocations, latency distributions, cache/event counters, publication cost, and bounded memory.
2. Write `Docs/SIGHTWEAVE_M2P2_EVENT_INDEX_ARCHITECTURE.md`, compare the three candidate families with prototype evidence, and retain rejected designs and failure reasons.
3. Use startup allocation tracing to attribute every source-transform allocation, then eliminate warmed allocation/reallocation/bytes without mutating held snapshots or suppressing valid revisions.
4. Implement the chosen bounded prepared-event index, exact invalidation/fallback, stats/debug visibility, lifecycle/high-water reclamation, and Shipping isolation.
5. Add safe shared-observer-origin preparation and prove owner/floor/height/capability/range/cone/bypass isolation.
6. Increase 512-batch headroom and add motion/cache/parity/lifecycle/concurrency coverage.
7. Run the complete Editor, Automation, performance, allocation, Lab, BuildPlugin/clean-host, Game Development/Shipping, dependency, Git, and LFS matrix; record the honest final status.

## Commands executed

```powershell
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight rev-parse HEAD
git -c safe.directory=D:/UE_projects/LastLight branch --show-current
git -c safe.directory=D:/UE_projects/LastLight remote -v
git -c safe.directory=D:/UE_projects/LastLight branch -vv
git -c safe.directory=D:/UE_projects/LastLight fetch origin --prune
git -c safe.directory=D:/UE_projects/LastLight pull --ff-only origin codex/m2p1-sightweave-final-performance-gate
git -c safe.directory=D:/UE_projects/LastLight lfs pull
git -c safe.directory=D:/UE_projects/LastLight lfs status
git -c safe.directory=D:/UE_projects/LastLight ls-remote --heads origin codex/m2p2-sightweave-motion-event-index
git -c safe.directory=D:/UE_projects/LastLight switch -c codex/m2p2-sightweave-motion-event-index
.\Scripts\BuildEditor.ps1 -EngineRoot D:\UE_5.8 -Configuration Development
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave.M2P2.Performance.MotionTrace' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM2P2_MotionBaselineV2' '-AbsLog=D:\UE_projects\LastLight\Saved\Logs\SightWeaveM2P2_MotionBaselineV2.log'
pwsh -NoProfile -File .\Scripts\RunSightWeaveAllocationProof.ps1 -Label M2P2PreparedIndexGatedFinal_20260825 -EngineRoot D:\UE_5.8
pwsh -NoProfile -File .\Scripts\RunSightWeaveAllocationProof.ps1 -Label M2P2BatchUniformSerialFinalV3_20260825 -EngineRoot D:\UE_5.8
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave.M2P.Differential' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM2PDifferential_IndexedValidated' '-AbsLog=D:\UE_projects\LastLight\Saved\Logs\SightWeaveM2PDifferential_IndexedValidated.log'
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave.M2P2.Performance.PreparedEventIndex4096' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM2P2_Prepared4096_Headroom1' '-AbsLog=D:\UE_projects\LastLight\Saved\Logs\SightWeaveM2P2_Prepared4096_Headroom1.log'
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave.M2P2.PreparedEventIndex' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM2P2_PreparedIndexCorrectnessFinal' '-AbsLog=D:\UE_projects\LastLight\Saved\Logs\SightWeaveM2P2_PreparedIndexCorrectnessFinal.log'
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave.M2P2.TransformAPI' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM2P2_TransformAPIFinal' '-AbsLog=D:\UE_projects\LastLight\Saved\Logs\SightWeaveM2P2_TransformAPIFinal.log'
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave.M2P2.Performance.Batch512Gate' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM2P2_Batch512Gate_UniformSerial1'
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave.M2P2.Performance.Batch512Gate' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM2P2_Batch512Gate_UniformSerial2'
```

The baseline and remote M2P.1 branch were identical at the required SHA. The target branch did not exist locally or remotely. The only pre-existing worktree change is the company-machine `Darkwell.uproject` EngineAssociation GUID; it is preserved locally and must never be staged.

Before source changes, the root guidance, requirements, architecture, migration plan, M2/M2P/M2P.1 handoffs and performance/allocation documents, plugin README/descriptor, build/allocation scripts, Geometry, OptimizedSolveCache, SpatialIndex, WorldSubsystem, Queries, Settings, Types, Components, and all M2P/M2P.1 test sources were read in full.

The corrected benchmark build succeeded in 11.89 seconds. The only build warning was the installed MSVC 14.51 toolchain being newer than Unreal's preferred 14.50 version. An earlier benchmark export was discarded after review because it attributed retained snapshot counters to no-change calls and ran range/height changes from the preceding teleport location; its numbers are not evidence.

## Unverified items and current risks

- The bounded exact-origin lookup is implemented, but the optional kinetic small-translation reorder is not. Continuous unique translations therefore rebuild exact origin preparation; repeated resident exact origins hit. Any future kinetic path must prove full deterministic order or fall back visibly.
- Absolute endpoint preparation/order is retained. Angular interval arrays are still rebuilt into thread scratch per solve; the current 4,096/source gate passes, but median headroom is only about 52–78 us and must be watched across final independent runs.
- Pure-radial rotation reuses origin preparation and canonical endpoint order but still recomputes final source arrays/polygon. A final-polygon shortcut remains deliberately disabled until canonical query-array reordering is proven across the seam.
- Dynamic changes are exact and synchronous, but dedicated multiple-door locality/counter tests and held-reader coverage across dynamic invalidation/source deletion/eviction are still required.
- Source/world teardown, restart, multiworld isolation, high-water reclaim under large entries, byte-pressure eviction, and concurrent independent solver/index scratch need broader coverage.
- The 512-query optimized path is a fixed-stack serial fast path for tightly bounded uniform anchor batches. Its exact independent-point parity, 20 performance distributions, and zero-allocation startup trace pass; broader repeated-world teardown and concurrent query/publication stress remain part of the lifecycle matrix.
- Normal 8x64/4,096-total non-regression, full SightWeave/DARKWELL suites, Lab, BuildPlugin/clean host, Game Development/Shipping, Shipping dependency scan, Git/LFS audit, and final remote verification remain pending.
- Held-reader publication correctly allocates a fresh immutable frame when both reusable buffers are owned. That permitted path is reported separately and is not part of the strict ordinary warmed-transform 0/0/0 claim.

# SightWeave M2P.2 — Motion Hot Path & Prepared Event Index handoff

## Status

- State: **IN_PROGRESS**
- Baseline branch: `codex/m2p1-sightweave-final-performance-gate`
- Baseline SHA: `69ac8d50019ef7674c2aed58d2c0c931ee8fa874`
- Working branch: `codex/m2p2-sightweave-motion-event-index`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current phase: corrected motion/cache baseline complete; prepared-event-index architecture comparison is next
- Latest safe commit: `15a30dfdc3ca3b39f6d52be324cc3e200c654ca3` (`docs: start SightWeave motion hot-path architecture`); benchmark checkpoint is pending
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

## Prepared Event Index hypotheses

Three candidate families will be benchmarked before selecting a production replacement:

1. **Floor/static-scene prepared metadata:** retain stable-ID keyed world segment metadata and static/dynamic revision partitions. This should reduce repeated validation and static extraction but cannot alone reuse observer-relative angles after translation.
2. **Per-source persistent exact event index:** retain endpoint base events, sorted order, directions, interval acceleration, and output capacity for one source geometry key. Rotation-only can potentially preserve relative event order by changing the frame/orientation cut; translation needs exact endpoint-angle recomputation and order-crossing detection, with deterministic full rebuild fallback.
3. **Shared observer-origin occlusion preparation:** share only origin/floor/height/occluder-revision compatible geometry preparation across vision and illumination sources. Different cone/range/capability/owner/handle/revision must still produce isolated final results. Range sharing requires exact proof because candidate inclusion and nearest-hit truncation differ.

The initial leading hypothesis is a bounded world-owned observer-origin cache containing immutable/plain prepared geometry plus source-owned exact output/event views, with a small deterministic capacity and revision-keyed invalidation. This is not yet a production decision. Cold cost, translation order changes, dynamic overlay behavior, memory high water, lifecycle, and Reference parity must be measured first.

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
```

The baseline and remote M2P.1 branch were identical at the required SHA. The target branch did not exist locally or remotely. The only pre-existing worktree change is the company-machine `Darkwell.uproject` EngineAssociation GUID; it is preserved locally and must never be staged.

Before source changes, the root guidance, requirements, architecture, migration plan, M2/M2P/M2P.1 handoffs and performance/allocation documents, plugin README/descriptor, build/allocation scripts, Geometry, OptimizedSolveCache, SpatialIndex, WorldSubsystem, Queries, Settings, Types, Components, and all M2P/M2P.1 test sources were read in full.

The corrected benchmark build succeeded in 11.89 seconds. The only build warning was the installed MSVC 14.51 toolchain being newer than Unreal's preferred 14.50 version. An earlier benchmark export was discarded after review because it attributed retained snapshot counters to no-change calls and ran range/height changes from the preceding teleport location; its numbers are not evidence.

## Unverified items and current risks

- The corrected timing/counter benchmark is complete, but the startup allocation capture, architecture prototype, and production implementation have not run yet.
- The exact source-transform allocation callstacks and size groups have not yet been extracted from a new M2P.2 startup trace.
- Rotation-only reuse, translation order-crossing detection, range sharing, dynamic overlay invalidation, and shared-origin memory benefit remain hypotheses.
- The existing spatial query itself uses temporary cells, sets, and sorted ID arrays, but source-local candidate caching normally bypasses it until source updates currently remove that cache.
- The existing prepared cache key includes exact origin and forward, so even radial rotation invalidates prepared slots despite radial geometry being orientation-independent.
- Snapshot double buffering reuses a buffer only when no held reader owns it; arbitrary held-reader duration can force new frame allocation. A zero-allocation source-update design therefore needs bounded immutable structural sharing or a publication slab policy rather than assuming the standby buffer is always reusable.
- Any small-translation incremental path must detect unsafe exact order changes and fall back visibly; approximate order maintenance is forbidden.

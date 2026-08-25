# SightWeave M2P.2 — Motion Hot Path & Prepared Event Index handoff

## Status

- State: **IN_PROGRESS**
- Baseline branch: `codex/m2p1-sightweave-final-performance-gate`
- Baseline SHA: `69ac8d50019ef7674c2aed58d2c0c931ee8fa874`
- Working branch: `codex/m2p2-sightweave-motion-event-index`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current phase: repository/context recovery complete; motion benchmark and event-index architecture work not started
- Latest safe commit: baseline `69ac8d50019ef7674c2aed58d2c0c931ee8fa874`; this document is the first pending checkpoint
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
```

The baseline and remote M2P.1 branch were identical at the required SHA. The target branch did not exist locally or remotely. The only pre-existing worktree change is the company-machine `Darkwell.uproject` EngineAssociation GUID; it is preserved locally and must never be staged.

Before source changes, the root guidance, requirements, architecture, migration plan, M2/M2P/M2P.1 handoffs and performance/allocation documents, plugin README/descriptor, build/allocation scripts, Geometry, OptimizedSolveCache, SpatialIndex, WorldSubsystem, Queries, Settings, Types, Components, and all M2P/M2P.1 test sources were read in full.

## Unverified items and current risks

- No M2P.2 benchmark, allocation capture, architecture prototype, implementation, test, or build has run yet.
- The exact source-transform allocation callstacks and size groups have not yet been extracted from a new M2P.2 startup trace.
- Rotation-only reuse, translation order-crossing detection, range sharing, dynamic overlay invalidation, and shared-origin memory benefit remain hypotheses.
- The existing spatial query itself uses temporary cells, sets, and sorted ID arrays, but source-local candidate caching normally bypasses it until source updates currently remove that cache.
- The existing prepared cache key includes exact origin and forward, so even radial rotation invalidates prepared slots despite radial geometry being orientation-independent.
- Snapshot double buffering reuses a buffer only when no held reader owns it; arbitrary held-reader duration can force new frame allocation. A zero-allocation source-update design therefore needs bounded immutable structural sharing or a publication slab policy rather than assuming the standby buffer is always reusable.
- Any small-translation incremental path must detect unsafe exact order changes and fall back visibly; approximate order maintenance is forbidden.

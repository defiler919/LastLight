# SightWeave M2 handoff

## Status

- State: **IN PROGRESS**
- Baseline branch: `codex/m1-sightweave-skeleton-lab`
- Baseline SHA: `3ec080180b3de3c95258ce07d48fa8165d04701b`
- Working branch: `codex/m2-sightweave-2p5d-authority`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current checkpoint: Checkpoint 4 — debug data/draw, component-backed M2 lab, and performance automation complete
- Last completed checkpoint: Checkpoint 4 Editor build, M2 56-test suite, M1 regression, idempotent lab generation, and headless lab game smoke passed
- Next command: commit/push Checkpoint 4, then run combined SightWeave, DARKWELL, BuildPlugin, Git/LFS, and final validation gates

## M2 scope recovered from repository documentation

M2 implements CPU-authoritative explicit 2.5D visibility and legal illumination in the standalone SightWeave plugin. Vision polygons and legal-illumination polygons remain separate, retain their own source handles and revisions, and are combined only by source-specific compatibility during exact queries. Effective hard live coverage is the union of each gated vision source intersected with that source's compatible illumination polygons, plus illumination-bypass vision polygons, followed by the minimal hard `SuppressLiveVision` operation.

The correctness/reference endpoint-ray solver is the authority baseline. Floor/height isolation, authoring normalization, a deterministic floor-local spatial index, event-driven dirty updates, immutable snapshots, exact point/bounds/multi-sample/batch/source-specific queries, attribution, debug data, dynamic-door coverage, lab fixtures, automation, and performance samples are in scope.

GPU masks/RDG/render targets, fog and memory presentation, memory tiles, last-seen proxies, the full modifier system, persistence, DARKWELL adapters, `/Game/Maps/L_Prototype`, and legacy-fog removal are outside M2.

## Planned public API inventory

- `FSightWeaveKnowledgeOwnerId`
- `FSightWeaveFloorDefinition` and `USightWeaveFloorComponent`
- `FSightWeaveOccluderHandle`, `FSightWeaveSegment2D`, and `USightWeaveOccluderComponent`
- `USightWeaveVisionSourceComponent` and `USightWeaveIlluminationSourceComponent`
- `FSightWeavePolygon` and distinct `FSightWeaveIlluminationPolygon`
- `FSightWeaveFrameSnapshot`
- `FSightWeaveGeometryTolerances`
- `FSightWeaveSpatialIndexStats`
- `FSightWeaveIlluminationQueryResult`
- extended `FSightWeaveVisibilityQueryResult`
- `FSightWeaveQuerySampleSet`, `ESightWeaveSampleRule`, and query rejection flags
- minimal hard-live suppression handle/region API
- immutable snapshot access and point, bounds, multi-sample, batch, and source-specific query APIs

Checkpoint 3 implements this inventory in `SightWeaveQueries.h`, `SightWeaveComponents.h`, and `USightWeaveWorldSubsystem`. Registry mutations synchronously publish `TSharedPtr<const FSightWeaveFrameSnapshot>` data. Separate pending-dirty sets are consumed by the polygon cache while diagnostic dirty sets remain inspectable. Vision entries resolve only same-owner/same-floor legal-illumination handles with overlapping normalized capability sets; bypass entries discard and never resolve a compatibility key. Point queries evaluate per vision source, then apply M2 hard-live circle suppression after gated/bypass union. Sample, bounds, batch, and source-specific hard queries reuse the same point semantics and published revision.

M1 handles, registration entry points, result types, and all `SightWeave.M1` tests remain supported unless an explicitly documented compatibility adjustment is required.

## Commands executed

```powershell
git status --short --branch
git log -6 --oneline
git diff
git diff --cached
git lfs status
git fetch origin
git rev-parse HEAD
git rev-parse origin/codex/m1-sightweave-skeleton-lab
git show-ref --verify -- refs/remotes/origin/codex/m2-sightweave-2p5d-authority
git switch -c codex/m2-sightweave-2p5d-authority
git push -u origin codex/m2-sightweave-2p5d-authority
```

The required repository guidance, five vision design/audit/baseline documents, M1 handoff, decisions, plugin README/descriptor, all existing SightWeave module source, M1 tests, and lab-generation script were read in full before implementation.

## Build and test results

- Editor build: passed after Checkpoint 1, exit code 0, UBT `Result: Succeeded`, 8.52 seconds. The only build warning was the installed MSVC 14.51.36256 being newer than UE's preferred 14.50.35717.
- Independent Checkpoint 1 BuildPlugin compile: passed, exit code 0, including UnrealEditor Development, UnrealGame Development, and UnrealGame Shipping. Clean shared PCHs emitted UE-header deprecation warnings; no warning originated from a SightWeave source line.
- `SightWeave.M2.Geometry` first run: 21 discovered/run, 12 passed, 9 failed, 0 warnings, 0 not-run. The common defect was topology validation using the inclusive gameplay boundary epsilon, which classified short collinear endpoint-event runs as self intersections. The wall-motion assertion also initially included source/range-boundary vertices. The implementation now uses a tight topology-only intersection epsilon while retaining the documented inclusive gameplay boundary, and the assertion selects only genuinely clipped rays.
- `SightWeave.M2.Geometry` final run: **21 discovered, 21 run, 21 passed, 0 failed, 0 succeeded-with-warnings, 0 skipped/not-run, 0 in-process**; process exit code 0; report duration 0.1679 seconds. JSON: `Saved/AutomationReports/SightWeaveM2Geometry_20260824_Final/index.json`.
- Checkpoint 2 Editor build: passed, exit code 0, UBT `Result: Succeeded`, 12.60 seconds; only the existing MSVC preference warning.
- `SightWeave.M2.SpatialIndex` first run: 8 discovered/run, 7 passed and 1 failed because the test used strict `FBox2D::IsInside` on a boundary point. The implementation preserved the exact old bounds; the assertion now compares Min/Max directly.
- `SightWeave.M2.SpatialIndex` final run: **8/8 passed**, zero warnings/errors/not-run, process exit code 0. JSON: `Saved/AutomationReports/SightWeaveM2SpatialIndex_20260824_Final/index.json`.
- `SightWeave.M2.Runtime` first run: 8 discovered/run, 7 passed and 1 failed. The failure revealed that UE only calls `USceneComponent::OnUpdateTransform` for ordinary transform changes when `bWantsOnUpdateTransform` is enabled. All four SightWeave scene components now enable it explicitly.
- `SightWeave.M2.Runtime` final run: **8/8 passed**, zero warnings/errors/not-run, process exit code 0. It covers the single-active-floor rule, handle/revision lifecycle, disabled occluders, local dynamic-door replacement, affected-source dirtying, component self-registration/destruction, explicit transform rejection, and component-driven door motion. JSON: `Saved/AutomationReports/SightWeaveM2Runtime_20260824_Final/index.json`.
- `SightWeave.M1` Checkpoint 2 regression: **21 discovered/run, 21 passed, 0 failed/warning/not-run**, process exit code 0. JSON: `Saved/AutomationReports/SightWeaveM1_M2Checkpoint2_20260824/index.json`.
- Checkpoint 3 initial compile after the snapshot/query API: passed, exit code 0, UBT `Result: Succeeded`, 12.47 seconds; only the MSVC preference warning.
- Initial `SightWeave.M2.Query` run before the final profile-update and suppression aggregation assertions: **13/13 passed**, zero test warnings/errors/not-run, process exit code 0. JSON: `Saved/AutomationReports/SightWeaveM2Query_20260824_Initial/index.json`. No query implementation failure occurred in this run.
- Checkpoint 3 final Editor build: passed, exit code 0, UBT `Result: Succeeded`, 12.82 seconds; only the installed MSVC 14.51.36256 preference warning.
- Full `SightWeave.M2` Checkpoint 3 run: **52 discovered, 52 run, 52 passed, 0 failed, 0 warning, 0 error, 0 skipped/not-run**, process exit code 0, report duration 0.4393 seconds. Category counts: Geometry 21, SpatialIndex 8, Runtime 9, Query 14. JSON: `Saved/AutomationReports/SightWeaveM2_Checkpoint3_Final_20260824/index.json`.
- `SightWeave.M1` Checkpoint 3 regression: **21 discovered/run, 21 passed, 0 failed/warning/error/not-run**, process exit code 0, report duration 0.2000 seconds. JSON: `Saved/AutomationReports/SightWeaveM1_Checkpoint3_Final_20260824/index.json`.
- Debug API first build emitted an IWYU diagnostic because `SightWeaveDebug.cpp` did not include `SightWeaveDebug.h` first. UBT still reported success, but the include order was corrected and the immediate rebuild passed in 6.12 seconds with no SightWeave diagnostic.
- Initial `SightWeave.M2.Debug`: **3/3 passed**, zero warnings/errors/not-run. The deterministic `0x51A7E` sample measured two sources at 64 segments / 127 candidates / 1018 rays / 997 vertices / 6173.402 microseconds and eight sources at 64 / 509 / 4078 / 3868 / 23962.598 microseconds. JSON: `Saved/AutomationReports/SightWeaveM2Debug_20260824_Initial/index.json`.
- The first M2 lab generation stopped before saving when Python positional `Rotator` arguments produced Pitch instead of Yaw and the 2.5D occluder validator correctly rejected the diagonal actor. Named `roll/pitch/yaw` arguments fixed the authoring script. The next run and two subsequent idempotency runs succeeded and saved `/SightWeave/Maps/L_SightWeave_Lab` through Editor asset APIs.
- The first no-tick Debug Query component build failed, exit code 1 / UBT code 6, because `SightWeaveComponents.h` did not include the header declaring `FSightWeaveDebugDrawOptions`. Adding `SightWeaveDebug.h` fixed the dependency; the next build passed in 10.81 seconds.
- Checkpoint 4 final Editor build: passed, exit code 0, UBT `Result: Succeeded`, 5.66 seconds; only the installed MSVC preference warning.
- Full `SightWeave.M2` Checkpoint 4 run: **56 discovered, 56 run, 56 passed, 0 failed/warning/error/skipped/not-run**, process exit code 0, report duration 0.5599 seconds. Categories: Geometry 21, SpatialIndex 8, Runtime 9, Query 14, Debug 4. JSON: `Saved/AutomationReports/SightWeaveM2_Checkpoint4_Final_20260824/index.json`.
- Final recorded deterministic reference sample in that run: two sources = 64 segments / 127 candidates / 1018 rays / 997 vertices / 6197.903 microseconds; eight sources = 64 / 509 / 4078 / 3868 / 24202.801 microseconds. Repeated samples produced identical candidate/ray/vertex counts and quantized geometry hashes. Angular Sweep remains deliberately deferred: the reference path is correct and fast enough at this M2 sample scale, so no unproven boundary-risk optimization is enabled.
- `SightWeave.M1` Checkpoint 4 regression: **21/21 passed**, zero warnings/errors/not-run, process exit code 0, duration 0.2024 seconds. This includes map load and asset dependency isolation after the M2 lab rewrite. JSON: `Saved/AutomationReports/SightWeaveM1_Checkpoint4_Final_20260824/index.json`.
- M2 lab headless game smoke: passed, exit code 0. The no-tick BeginPlay marker reported `AuthoritativeResult`, `authoritative=1`, `live=1`, `vision=1`, `bypass=1`, snapshot 58, and one attributed vision source. No SightWeave Error/Ensure/Assert/Fatal occurred. Log: `Saved/Logs/SIGHTWEAVE_M2_LAB_GAME_SMOKE.log`.
- Combined `SightWeave`: not run yet.
- `Darkwell`: not run yet on the M2 branch.
- Lab load/game smoke: not run yet for M2.
- BuildPlugin: not run yet for M2.

## Commits

- `2d5b230374a03607679c6a4605ff67ccf13f03ae` — `docs: start SightWeave M2 authority implementation`
- `d5c24f9235ccf451c8475bf9e4b40b046f338f3c` — `feat: add SightWeave reference geometry solver`
- `82dbefe517387f905148ea805c56c1c43ec3d49d` — `feat: add SightWeave 2.5D authority components`
- `5aec48493eba7feb9de36a9de2c3c871c61bbe2c` — `feat: add SightWeave authoritative live queries`
- Checkpoint 4: `test: expand SightWeave M2 lab and geometry coverage` — pending in this document's containing commit.

## Blockers and unverified items

- No implementation blocker is known.
- Interactive visual inspection is not yet performed.
- Angular Sweep is deliberately deferred until the reference solver passes correctness tests and profiling demonstrates a need.
- Build, automation, lab, packaged plugin, and final Git/LFS checks remain pending.

# SightWeave M2 handoff

## Status

- State: **IN PROGRESS**
- Baseline branch: `codex/m1-sightweave-skeleton-lab`
- Baseline SHA: `3ec080180b3de3c95258ce07d48fa8165d04701b`
- Working branch: `codex/m2-sightweave-2p5d-authority`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current checkpoint: Checkpoint 1 — reference geometry solver complete
- Last completed checkpoint: Checkpoint 1 build and `SightWeave.M2.Geometry` automation passed
- Next command: implement floor-local spatial indexing and the four neutral M2 actor components

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
- `SightWeave.M1` Checkpoint 1 regression: **21 discovered/run, 21 passed, 0 failed/warning/not-run**, process exit code 0. JSON: `Saved/AutomationReports/SightWeaveM1_M2Checkpoint1_20260824/index.json`.
- Full `SightWeave.M2`: later categories are not implemented or run yet.
- Combined `SightWeave`: not run yet.
- `Darkwell`: not run yet on the M2 branch.
- Lab load/game smoke: not run yet for M2.
- BuildPlugin: not run yet for M2.

## Commits

- `2d5b230374a03607679c6a4605ff67ccf13f03ae` — `docs: start SightWeave M2 authority implementation`
- Checkpoint 1: `feat: add SightWeave reference geometry solver` — pending in this document's containing commit.

## Blockers and unverified items

- No implementation blocker is known.
- Interactive visual inspection is not yet performed.
- Angular Sweep is deliberately deferred until the reference solver passes correctness tests and profiling demonstrates a need.
- Build, automation, lab, packaged plugin, and final Git/LFS checks remain pending.

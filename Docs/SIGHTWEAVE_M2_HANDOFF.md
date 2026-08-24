# SightWeave M2 handoff

## Status

- State: **IN PROGRESS**
- Baseline branch: `codex/m1-sightweave-skeleton-lab`
- Baseline SHA: `3ec080180b3de3c95258ce07d48fa8165d04701b`
- Working branch: `codex/m2-sightweave-2p5d-authority`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current checkpoint: Checkpoint 0 — context restored and M2 branch published
- Last completed checkpoint: M1 validation at the baseline SHA
- Next command: implement the pure geometry, normalization, and reference polygon solver under `Plugins/SightWeave/Source/SightWeaveRuntime`

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

- Editor build: not run yet for M2.
- `SightWeave.M1`: not run yet on the M2 branch.
- `SightWeave.M2`: not implemented or run yet.
- Combined `SightWeave`: not run yet.
- `Darkwell`: not run yet on the M2 branch.
- Lab load/game smoke: not run yet for M2.
- BuildPlugin: not run yet for M2.

## Commits

- Checkpoint 0: `docs: start SightWeave M2 authority implementation` — pending in this document's initial commit.

## Blockers and unverified items

- No implementation blocker is known.
- Interactive visual inspection is not yet performed.
- Angular Sweep is deliberately deferred until the reference solver passes correctness tests and profiling demonstrates a need.
- Build, automation, lab, packaged plugin, and final Git/LFS checks remain pending.


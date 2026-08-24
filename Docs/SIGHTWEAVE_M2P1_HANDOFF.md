# SightWeave M2P.1 — Final Authority Performance Gate handoff

## Status

- State: **IN_PROGRESS**
- Baseline branch: `codex/m2p-sightweave-authority-performance`
- Baseline SHA: `d0b90d2e5687105f1e25bf03476a07d6bb5337de`
- Working branch: `codex/m2p1-sightweave-final-performance-gate`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current checkpoint: checkpoint 1 — context recovery and persistent handoff
- Last safe commit: `docs: start SightWeave final performance gate` (this document's containing commit; resolve with `git log -1 --format=%H`)
- Next recovery command: `git switch codex/m2p1-sightweave-final-performance-gate; git pull --ff-only origin codex/m2p1-sightweave-final-performance-gate`

## Objective and scope

Close the three remaining M2P CPU-authority performance gates without changing geometry semantics, hiding work behind stale snapshots, weakening differential correctness, or entering M3. The Reference solver remains the correctness oracle; Shipping remains Optimized-only. GPU masks, post process, memory tiles/textures, persistence, DARKWELL gameplay integration, `/Game/Maps/L_Prototype`, and legacy removal are outside this task.

## Baseline gates still failing

1. **Allocator proof:** warmed `TArray` capacity growth is zero in the existing fixture, but there is no allocator-call evidence. The optimized solver still constructs result/work arrays per call, so zero heap allocation cannot be claimed.
2. **4,096 segments per source:** final M2P single-solve estimates are about 5.551 ms median / 5.598 ms p99; all eight solves total about 44.4/44.8 ms. The required single-solve gates are median below 1 ms and p99 below 2 ms.
3. **Dynamic occluder public mutation:** the current synchronous door update, affected solve, and publication is about 0.653/0.694 ms median/p99, above the 0.25 ms main-thread public-dispatch gate.

## Recovered implementation facts and current hotspots

- `SolveOptimizedPolygon` currently owns per-call `TArray` storage for candidate angles, vertices, candidate distances, boundary points, prepared segment AoS data, angular intervals, and an active interval list. The result owns the published arrays, while the prepared/interval/active arrays are temporary.
- The optimized angular-interval sweep keeps the Reference boundary and endpoint/±epsilon event set, exact intersection math, inclusive tolerances, and stable-ID tie-break. Its active interval list is scanned for every ray; this and interval/event construction are the first extreme-workload candidates to re-profile.
- Optimized production topology validation is the linear constant-neighborhood parity guard. Reference retains the quadratic general topology oracle. Non-Shipping Verify executes both and falls back with a visible error on mismatch; Shipping unconditionally calls Optimized.
- `USightWeaveWorldSubsystem::UpdateOccluder` prepares and compares geometry, replaces the spatial-index entry, advances revisions, discovers affected vision/illumination sources, and then calls `PublishSnapshot` synchronously.
- `PublishSnapshot` synchronously rebuilds dirty solver entries, materializes a complete ordinary-data frame snapshot, resolves compatibility, and swaps a thread-safe immutable shared pointer. Queries read the published pointer and return that exact revision. No asynchronous solve/publication or stale-task guard exists yet because all mutation/solve/publication is on the current synchronous call stack.
- Clean publication already returns without creating a snapshot when revision and pending rebuild sets are unchanged. Normalized no-change source and occluder updates return without revision growth.
- The existing 512-query path reuses the caller result array, one immutable snapshot/floor context, and inner attribution capacities; the current evidence is capacity-only and must be replaced by actual allocator-call measurement.

## Planned checkpoints

1. Persist this IN_PROGRESS handoff and push the new branch.
2. Add safe test-only, current-thread, explicitly scoped allocator-call instrumentation and record allocation/free/reallocation calls, allocated bytes, and peak temporary bytes for all required workloads before optimization.
3. Add bounded, concurrency-safe reusable Optimized solver scratch and prove warm zero allocations/reallocations without contaminating Shipping with test hooks or Reference/Verify storage.
4. Re-profile and optimize the 4,096/source single-solve path until the median/p99 hard gates pass or a concrete correctness/engine/architecture limit is demonstrated. Report per-source CPU, cumulative CPU, worker wall time, and main-thread time separately.
5. Optimize strict synchronous dynamic occluder mutation first. If it cannot meet the gate, add an explicit, tested fast-dispatch/worker/deterministic-publication policy while retaining strict synchronous mode and documenting pending semantics.
6. Expand high-count differential, scratch concurrency/reentrancy, stale/out-of-order result, lifecycle, teardown, multiworld, rapid-door, Shipping-mode, determinism, and instrumentation-safety coverage.
7. Run the full Editor, Automation, performance, allocation, Lab, BuildPlugin/clean-host, dependency, Git, and LFS validation matrix; record the honest final status and evidence.

## Commands executed

```powershell
git status --short --branch
git fetch origin --prune
git switch codex/m2p-sightweave-authority-performance
git pull --ff-only origin codex/m2p-sightweave-authority-performance
git rev-parse HEAD
git lfs pull
git lfs status
git ls-remote --heads origin codex/m2p1-sightweave-final-performance-gate
git switch -c codex/m2p1-sightweave-final-performance-gate
```

The baseline was clean and exactly matched the required SHA. The target branch did not exist locally or remotely. Repository guidance, requirements, architecture, migration, M2/M2P handoffs, M2P performance evidence, plugin README, the relevant Runtime implementation, and all M1/M2/M2P tests and benchmarks were read before runtime source changes.

## Unverified items

- No real allocator-call instrumentation or fresh allocation baseline has been produced yet.
- The 4,096/source stage distribution has not yet been re-measured on this branch.
- Reusable scratch, concurrency/reentrancy behavior, bounded high-water reclamation, and worker isolation are not implemented.
- Dynamic occluder updates remain synchronous; no pending-window or stale-result policy exists.
- No M2P.1 C++ change has been built or tested yet.
- Final Editor/Automation/Lab/BuildPlugin/clean-host/dependency/Git/LFS validation is pending.

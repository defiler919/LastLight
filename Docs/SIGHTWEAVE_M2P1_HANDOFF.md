# SightWeave M2P.1 — Final Authority Performance Gate handoff

## Status

- State: **IN_PROGRESS**
- Baseline branch: `codex/m2p-sightweave-authority-performance`
- Baseline SHA: `d0b90d2e5687105f1e25bf03476a07d6bb5337de`
- Working branch: `codex/m2p1-sightweave-final-performance-gate`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current checkpoint: checkpoint 4 — exact extreme-authority optimization ready for commit
- Last safe commit: `perf: add reusable SightWeave solver scratch` (`141aedf172b73af225d9eeadab609c55674f5587`)
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
& .\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot 'D:\UE_5.8'
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe ... '-trace=memory,sightweaveallocation' '-ExecCmds=Automation RunTests SightWeave.M2P1.Allocation.Capture' ...
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe ... '-ExecCmds=Automation RunTests SightWeave.M2P1.Allocation.Analyze' ...
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe ... '-ExecCmds=Automation RunTests SightWeave.M2P1.Scratch' ...
& .\Scripts\RunSightWeaveAllocationProof.ps1 -Label 'after-scratch-v3' -EngineRoot 'D:\UE_5.8'
```

The baseline was clean and exactly matched the required SHA. The target branch did not exist locally or remotely. Repository guidance, requirements, architecture, migration, M2/M2P handoffs, M2P performance evidence, plugin README, the relevant Runtime implementation, and all M1/M2/M2P tests and benchmarks were read before runtime source changes.

The full `DarkwellEditor Win64 Development` build passed after adding the Editor-only allocation instrumentation. Capture and offline analysis each passed one Automation test with zero test warnings/errors. The exact baseline and method are in `Docs/SIGHTWEAVE_M2P1_ALLOCATION_BASELINE.md`. The key result is that each Optimized solve currently performs six allocations and five reallocations; 8×4,096 allocates 14,231,360 bytes across the eight-solve marked scope. Batch 512, clean publication, and no-change update are proven zero-allocation already.

Checkpoint 2 was committed and pushed as `1284174d0dbc9ed096e4c7eb347a3c3a0ac788ce`; local HEAD, upstream, and the remote branch were then identical. The current uncommitted checkpoint adds reusable caller-output and per-thread solver storage. Each thread owns four retained reentrant frames; deeper nesting receives stack-owned overflow storage, and any frame above an 8 MiB combined high-water mark is emptied at release. Scratch stores no world or `UObject` references. Snapshot entry rebuild and point-query callers now reuse output arrays.

Post-change allocator proof `after-scratch-v3` passed one capture and one analysis test with zero test warnings/errors. All solver sizes, point query, batch 512, clean publication, and no-change update measured exactly 0 allocations/reallocations/bytes in every sample. Door update dropped from 416/67 alloc/realloc and 376,468–376,804 bytes to 389/45 and 218,812–219,148 bytes; source update dropped from 169/17–18 and 189,720–190,344 bytes to 162/12–13 and 148,096–148,720 bytes. Exact evidence and design rationale are in `Docs/SIGHTWEAVE_M2P1_SCRATCH.md`.

The new eight-worker deterministic isolation test and eight-level reentrancy test both passed with zero warnings/errors. The complete SightWeave suite then passed 87/87 with zero warnings/errors. A full `DarkwellEditor Win64 Development` build also passed; the only build warning remains the local MSVC 14.51 versus Epic-preferred 14.50 toolchain notice. Home Screen is disabled before editor-module startup in the proof script, eliminating the unrelated `google.com/generate_204` timeout that contaminated one discarded analysis run.

Checkpoint 3 was committed and pushed as `141aedf172b73af225d9eeadab609c55674f5587`; local HEAD, upstream, and the remote branch were identical before checkpoint 4 work began.

The checkpoint 4 solver keeps the exact Reference event set and arithmetic while adding exact double radix sorting, a pure-radial cyclic endpoint-event merge, cached source-relative segment metadata, distance-ordered active pruning, binary active insertion with expiry gating, exact division avoidance only for provably in-range fractions, sequential result writes, exact base-direction reuse for ±epsilon events, and the proven local `i`/`i+2` topology parity guard. Rejected reciprocal and normalized-direction experiments were removed after differential failures.

The latest retained `ProfileTopologyOffset2` result is 1,706.500/1,729.101/1,747.902/1,747.902 us single-solve median/p95/p99/max for 4,096 segments per source. Eight-source cumulative CPU is 13,656.195 us median and sequential wall is 13,657.503 us median. The p99 gate now passes; the median remains above 1 ms and is not relabeled as a pass. The 4,096-total fixture is 213.500/222.202 us median/p99 per solve, and typical 8x64 radial all-source is 318.699/327.297 us median/p99, both comfortably within their non-regression gates. Full stage and iteration evidence is in `Docs/SIGHTWEAVE_M2P1_EXTREME_PERFORMANCE.md`.

The retained candidate passed both `SightWeave.M2P.Differential.Geometry` tests: all nine manual adversarial cases and all 96 fixed-seed randomized cases. The latest full Editor build passed with only the already documented MSVC preferred-version warning. The current synchronous dynamic-door measurement is 329.003/338.700/340.398/340.398 us median/p95/p99/max, improved from the M2P 653/694 us result but still above the strict 250 us gate. Source transform is 60.599/66.198 us median/p99; batch 512 is 237.498/249.803 us median/p99; no-change update is 0.298/0.302 us and preserves revision.

## Unverified items

- Real startup-safe allocator-call instrumentation, the 30-sample baseline, and the post-scratch 30-sample proof are complete. The solver/query allocation gate is closed; full dynamic updates remain nonzero.
- The 4,096/source stage distribution and exact per-solve distribution are recorded. Extreme p99 passes, but the 1 ms median gate remains open at 1.7065 ms.
- Reusable scratch, caller-output reuse, bounded high-water reclamation, eight-worker isolation, and eight-level reentrancy are implemented and tested. High-count Reference differential expansion remains pending.
- Dynamic occluder updates remain synchronous at 0.329/0.340 ms median/p99; no pending-window or stale-result policy exists. Synchronous publication/materialization is the next measured target before considering an explicit async policy.
- The allocation instrumentation and reusable-scratch C++ have passed full Editor builds plus capture/analyze and focused scratch Automation.
- Final Editor/Automation/Lab/BuildPlugin/clean-host/dependency/Git/LFS validation is pending.

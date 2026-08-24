# SightWeave M2P.1 — Final Authority Performance Gate handoff

## Status

- State: **PARTIAL**
- Baseline branch: `codex/m2p-sightweave-authority-performance`
- Baseline SHA: `d0b90d2e5687105f1e25bf03476a07d6bb5337de`
- Working branch: `codex/m2p1-sightweave-final-performance-gate`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current checkpoint: checkpoint 7 — final validation complete
- Last safe commit: `docs: record SightWeave final performance validation` (this document's containing commit; resolve with `git log -1 --format=%H`)
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
& .\Scripts\RunSightWeaveAllocationProof.ps1 -Label 'after-dynamic-cache-final' -EngineRoot 'D:\UE_5.8'
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe ... '-ExecCmds=Automation RunTests SightWeave.M2P.Performance.Baseline.RuntimePipeline' ...
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe ... '-ExecCmds=Automation RunTests SightWeave.M2P.Differential.Runtime.AuthorityAndUpdates' ...
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe ... '-ExecCmds=Automation RunTests SightWeave.M2P.Differential.Geometry' ...
```

The baseline was clean and exactly matched the required SHA. The target branch did not exist locally or remotely. Repository guidance, requirements, architecture, migration, M2/M2P handoffs, M2P performance evidence, plugin README, the relevant Runtime implementation, and all M1/M2/M2P tests and benchmarks were read before runtime source changes.

The full `DarkwellEditor Win64 Development` build passed after adding the Editor-only allocation instrumentation. Capture and offline analysis each passed one Automation test with zero test warnings/errors. The exact baseline and method are in `Docs/SIGHTWEAVE_M2P1_ALLOCATION_BASELINE.md`. The key result is that each Optimized solve currently performs six allocations and five reallocations; 8×4,096 allocates 14,231,360 bytes across the eight-solve marked scope. Batch 512, clean publication, and no-change update are proven zero-allocation already.

Checkpoint 2 was committed and pushed as `1284174d0dbc9ed096e4c7eb347a3c3a0ac788ce`; local HEAD, upstream, and the remote branch were then identical. The current uncommitted checkpoint adds reusable caller-output and per-thread solver storage. Each thread owns four retained reentrant frames; deeper nesting receives stack-owned overflow storage, and any frame above an 8 MiB combined high-water mark is emptied at release. Scratch stores no world or `UObject` references. Snapshot entry rebuild and point-query callers now reuse output arrays.

Post-change allocator proof `after-scratch-v3` passed one capture and one analysis test with zero test warnings/errors. All solver sizes, point query, batch 512, clean publication, and no-change update measured exactly 0 allocations/reallocations/bytes in every sample. Door update dropped from 416/67 alloc/realloc and 376,468–376,804 bytes to 389/45 and 218,812–219,148 bytes; source update dropped from 169/17–18 and 189,720–190,344 bytes to 162/12–13 and 148,096–148,720 bytes. Exact evidence and design rationale are in `Docs/SIGHTWEAVE_M2P1_SCRATCH.md`.

The new eight-worker deterministic isolation test and eight-level reentrancy test both passed with zero warnings/errors. The complete SightWeave suite then passed 87/87 with zero warnings/errors. A full `DarkwellEditor Win64 Development` build also passed; the only build warning remains the local MSVC 14.51 versus Epic-preferred 14.50 toolchain notice. Home Screen is disabled before editor-module startup in the proof script, eliminating the unrelated `google.com/generate_204` timeout that contaminated one discarded analysis run.

Checkpoint 3 was committed and pushed as `141aedf172b73af225d9eeadab609c55674f5587`; local HEAD, upstream, and the remote branch were identical before checkpoint 4 work began.

The checkpoint 4 solver keeps the exact Reference event set and arithmetic while adding exact double radix sorting, a pure-radial cyclic endpoint-event merge, cached source-relative segment metadata, distance-ordered active pruning, binary active insertion with expiry gating, exact division avoidance only for provably in-range fractions, sequential result writes, exact base-direction reuse for ±epsilon events, and the proven local `i`/`i+2` topology parity guard. Rejected reciprocal and normalized-direction experiments were removed after differential failures.

The latest retained `ProfileTopologyOffset2` result is 1,706.500/1,729.101/1,747.902/1,747.902 us single-solve median/p95/p99/max for 4,096 segments per source. Eight-source cumulative CPU is 13,656.195 us median and sequential wall is 13,657.503 us median. The p99 gate now passes; the median remains above 1 ms and is not relabeled as a pass. The 4,096-total fixture is 213.500/222.202 us median/p99 per solve, and typical 8x64 radial all-source is 318.699/327.297 us median/p99, both comfortably within their non-regression gates. Full stage and iteration evidence is in `Docs/SIGHTWEAVE_M2P1_EXTREME_PERFORMANCE.md`.

The retained checkpoint-4 candidate passed both `SightWeave.M2P.Differential.Geometry` tests: all nine manual adversarial cases and all 96 fixed-seed randomized cases. The latest full Editor build passed with only the already documented MSVC preferred-version warning. Before checkpoint 5, synchronous dynamic door measured 329.003/338.700/340.398/340.398 us median/p95/p99/max, improved from the M2P 653/694 us result but still above the strict 250 us gate.

Checkpoint 5 retains strict synchronous Authority. It adds stable-ID raw-segment patching; source-local prepared-segment and angular-padding caches; exact radial wrap-start and unnormalized-ray fast paths; safe immutable snapshot double buffering; retained publication/materialization scratch; a one-edge allocation-stable normalizer; and stable-ID spatial-index in-place updates with reusable nested arrays and a general remove/insert fallback. Source/occluder lifecycle and world reset invalidate the applicable caches. Shipping still forces Optimized; Reference and Verify semantics are unchanged.

The three latest 101-sample dynamic distributions are 222.400/226.900/230.700/231.899 us, 221.301/230.897/233.699/252.098 us, and 223.100/229.903/234.298/236.101 us median/p95/p99/max. All three pass the synchronous median and p99 below 250 us gate. No async work or stale publication window exists: public dispatch contains the full synchronous work and publication adds zero delay after return.

The corresponding batch 512 distributions are 233.799/239.700/243.198/243.600 us, 238.001/250.001/288.300/291.802 us, and 235.099/239.801/245.102/245.702 us. The middle run contains a retained host scheduling/frequency band and fails the tail gate. The immediately preceding three distributions all passed at 235.800/243.802, 235.900/242.900, and 235.800/245.102 us median/p99. Five of the latest six pass; the evidence is not relabeled as a uniform pass. Source transform is about 52.1–52.8/56.0–58.8 us median/p99. No-change is 0.201–0.298/0.302 us and preserves revision.

The final startup-memory trace proof is `Saved/SightWeaveM2P1/AllocationProof/after-dynamic-cache-final`. All three samples for every solver size, point, batch, dynamic door, clean publication, and no-change update are exactly 0 allocation/reallocation/free calls and 0 bytes. Dynamic door progressed from 416/67 allocations/reallocations and about 376 KB at baseline, through 389/45 and about 219 KB after solver scratch, to 0/0/0. Source transform remains 75 allocations, 11–13 reallocations, and 36,516–48,156 bytes; its required latency gate passes, but it is not called zero-allocation. The analyzer now records allocation-size/callstack-ID details and remains test-only.

The final checkpoint-5 runtime differential passed after spatial in-place updates. The full geometry differential again passed both tests (nine manual plus 96 fixed seed). Exact design and distribution evidence is in `Docs/SIGHTWEAVE_M2P1_DYNAMIC_PERFORMANCE.md`.

Checkpoint 6 adds a fixed-seed 512-segment Reference/Optimized geometry differential and publication-lifetime coverage without exposing test hooks in Shipping. A held immutable snapshot remained unchanged across 32 alternating synchronous door updates and remained safe ordinary data after world teardown. A two-world test proved that updating one world changes neither the other world's revision nor its geometry. Together with the existing scratch concurrency/reentrancy checks, the focused `SightWeave.M2P1` queue passed 7/7 with zero test warnings/errors after a full `DarkwellEditor Win64 Development` build. Because the runtime remains strictly synchronous, stale/out-of-order worker result tests remain not applicable rather than silently omitted.

Checkpoint 7 completed the full Editor, Automation, Lab, extended-performance, allocator-trace, BuildPlugin/clean-host, Shipping dependency, Git, and LFS matrix. Exact final distributions, accurate test counts, warnings, build/package results, repository gates, remaining architectural limit, and recovery command are recorded in `Docs/SIGHTWEAVE_M2P1_FINAL_VALIDATION.md`. The state is PARTIAL because the final 4,096/source individual solve is 1,733.102/1,856.200/1,896.501/1,896.501 us median/p95/p99/max: p99 passes, but median does not meet the strict 1 ms gate. The next meaningful CPU step is a prepared scene/source event-index architecture with incremental exact-order maintenance, not another local arithmetic/container change. One of the latest six batch-512 distributions also retains a host scheduling/frequency tail failure.

## Final unverified/not-applicable items

- Real startup-safe allocator-call instrumentation, baseline evidence, and final proof are complete. Solver, point, batch, dynamic door, clean publication, and no-change are zero-allocation/reallocation; source transform remains explicitly nonzero but passes its latency gate.
- The latest 4,096/source stage and exact per-solve distributions are recorded. Extreme p99 passes at 1.896501 ms, but the 1 ms median gate remains open at 1.733102 ms.
- Reusable scratch, caller-output reuse, bounded high-water reclamation, eight-worker isolation, eight-level reentrancy, and a fixed-seed 512-segment Reference differential are implemented and tested.
- Dynamic occluder updates remain synchronous and now pass the strict median/p99 gate in all three final distributions. Async pending/revision cases are not applicable because no async path was introduced; held-reader immutability, rapid updates, world teardown, and multiworld isolation are covered.
- The allocation instrumentation, solver scratch, dynamic prepared caches, spatial reuse, and snapshot double buffer passed the full final validation matrix.
- Async worker completion order, stale-task rejection, pending policy, and multi-worker wall time are not applicable because production remains strictly synchronous and sequential. No result is hidden behind a stale snapshot or dispatch-only timer.
- M3 and the prepared exact-event-index architecture remain future work. Do not merge main or begin M3 from this handoff.

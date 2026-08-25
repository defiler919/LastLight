# SightWeave M2P.2 final validation

## Final state

The final state is **PARTIAL**, not `COMPLETED` or `BLOCKED`.

The exact bounded prepared-event index, warmed motion allocation proof, source-transform latency, 4,096/source prepared solve, ordinary 8x64 and 4,096-total solves, geometry/query parity, lifecycle/isolation/reclamation, Editor build, DARKWELL regression, Lab smoke, clean-host Development/Shipping builds, Shipping isolation, Git, and LFS gates are closed.

Two performance gates are not uniformly closed:

1. The fixed-stack serial 512-query path passed three dedicated invocations, or 30/30 formal distributions, with the final dedicated worst median/p95/p99 at 92.600/144.299/173.099 us. Four later ordinary processes retained by the full/focused validation matrix each failed the hard 200 us p99 gate in one or more of their ten distributions. Their worst p99 values were 228.100, 229.802, 240.602, and 243.399 us. Median and p95 remained within 150/180 us and every distribution retained zero result-capacity growth, but the requested independent-distribution tail guarantee is not stable enough to call complete.
2. The broader 4-vision/2-illumination RuntimePipeline dynamic-door row produced p99 values of 261.299, 400.800, and 244.100 us in its latest three independent processes, so only one of three is below the inherited 250 us target. The dedicated M2P.2 motion test passes dynamic-door-only at 51.100 us p99 and door-plus-motion at 118.800 us p99, showing a host scheduling/frequency-sensitive broad-scene tail rather than an authority or correctness failure. The slow RuntimePipeline process also raised its source-transform row to 219.699 us p99 while the other two were 67.398 and 44.301 us and the dedicated motion test remained below 50 us.

No affinity, process priority, sample removal, relaxed threshold, changed seed, stale publication, reduced update rate, or async authority window was used. A source-major batch prototype was rejected because it improved the median by only approximately 1-2 us and still produced a 223.801 us p99. A `ParallelFor` prototype was rejected because startup tracing proved 2 allocations / 0 reallocations / 912 allocated bytes per batch.

## Git identity and checkpoints

- Baseline branch: `codex/m2p1-sightweave-final-performance-gate`
- Baseline SHA: `69ac8d50019ef7674c2aed58d2c0c931ee8fa874`
- Final branch: `codex/m2p2-sightweave-motion-event-index`
- Final SHA: this document's containing commit; resolve with `git log -1 --format=%H`
- No merge, rebase, force-push, main-branch mutation, or M3 work was performed.

Checkpoint commits before this document:

1. `15a30df` — initial M2P.2 handoff/checkpoint documentation
2. `4b3b5c71` — reproducible motion benchmark and corrected attribution
3. `30898ff5` — prepared-event-index architecture decision
4. `0ebf849a2e2487c634fbedf38a631a50b76a52ae` — exact bounded prepared-event index and transform hot path
5. `98db6d86090535f13e7ec68c64da9ff79f07a2a2` — 512-query headroom
6. `15ec06db8ade7ce4d85be5e9c8d5720555b5a459` — lifecycle, isolation, and reclamation hardening
7. `56cecde5417dcb17c679ab9a604eff89f5bd6966` — exact compatible cross-kind final-geometry reuse
8. `docs: record SightWeave M2P.2 final validation` — this document's containing commit

The company-machine `Darkwell.uproject` EngineAssociation GUID change predates task work, remains unstaged, and is not part of any checkpoint.

## Delivered architecture

Production now has transform-only vision and illumination update APIs, retained source and solver storage, absolute forward-independent endpoint preparation, and an exact world-owned prepared-origin index. The index is initially bounded to 32 entries and 64 MiB, uses exact origin/floor/height/tolerance/revision/segment-sequence keys, maintains source bindings and public hit/miss/rebuild/eviction/fallback/invalidation/byte statistics, reclaims physical retained storage on invalidation or eviction, and deterministically falls back to the exact uncached solver when capacity cannot admit a result.

Prepared origin geometry may be shared only when the geometry key is exact. Final source state remains isolated by handle, owner, capability, policy, public/source/polygon revision, range, cone, and attribution. Radial vision and illumination may reuse exact final geometry when transform, floor, height, shape, and range match and vision near-awareness is zero; radial half-angle is intentionally irrelevant, while non-radial half-angle must match. The destination retains its own arrays and metadata rather than aliasing another source's mutable state.

Pure radial rotation reuses canonical world preparation, cone rotation recuts exact absolute events, translation performs an exact prepared-origin lookup or rebuild, and teleport/profile/floor changes take the same exact lookup/fallback route. Dynamic revision changes invalidate affected entries synchronously before return. The optional kinetic small-translation reorder and final-polygon alias shortcut remain deliberately unimplemented because their full deterministic order/seam proof was not established; this does not weaken exactness because production uses the visible full-rebuild fallback.

The uniform 256-512 anchor-query fast path prefilters an immutable snapshot once into bounded stack storage, then preserves the existing exact point evaluator for final fields. Mixed owner/floor/Z, invalid/non-anchor, suppressed, small/oversized, or higher-source-count batches use the ordinary exact serial path.

## Allocation and motion proof

The final startup trace is `Saved/SightWeaveM2P1/AllocationProof/M2P2FinalCrossKind_20260825`. Capture and analysis each passed 1/1, emitted exactly 60 CSV rows, and reported no test warnings, failures, or not-run tests.

All warmed strict rows report exact **0 allocation calls / 0 reallocation calls / 0 allocated bytes**:

- solver 2x64, solver 8x64, solver 4,096-total, and solver 4,096/source;
- point query and batch 512;
- clean publication, dynamic door, and no-change update;
- radial rotation, cone rotation, 1/5/20 cm translation, teleport, shared-origin aggregate, and ordinary source transform.

Separate permitted rows remain explicit: range cardinality growth is 3 allocations/12 bytes, a held immutable reader can require a second frame allocation, and combined door-plus-motion can allocate when changed occluder geometry introduces new nested edge attribution. None is folded into the ordinary warmed source-transform claim.

`Saved/AutomationReports/SightWeaveM2P2_MotionCrossKindFinal/index.json` passed 1/1. Selected p99 values are:

| operation | p99 us |
|---|---:|
| no change | 0.101 |
| radial rotation | 44.998 |
| cone rotation | 31.799 |
| camera rotation | 25.202 |
| translation 1 cm | 21.901 |
| translation 5 cm | 49.800 |
| translation 20 cm | 27.500 |
| teleport | 6.899 |
| dynamic door only | 51.100 |
| door plus source motion, aggregate | 118.800 |
| four compatible-origin sources, aggregate | 116.602 |

Every pure transform row is below the required 100 us p99. Aggregate compound rows are labeled as aggregates and are not presented as single-source latency.

## Solver and query performance

Two independent prepared 4,096-segments/source sequences pass the required median below 1 ms and p99 below 2 ms:

| report | median us | p99 us |
|---|---:|---:|
| `SightWeaveM2P2_Prepared4096_Final1` | 897.899 | 1,505.598 |
| `SightWeaveM2P2_Prepared4096_Final2` | 891.902 | 1,461.998 |

`SightWeaveM2P2_OptimizedExtendedFinal` passed 1/1. Typical 8x64 individual p99 is 33.703 us and 4,096-total individual p99 is 262.801 us, both below the 500 us non-regression ceiling. The generic non-prepared 4,096/source row is 1,141.999/1,717.903 us median/p99; the acceptance sequence is the prepared production path above.

Three dedicated batch invocations passed all ten independent distributions in each process:

| invocation | worst median / p95 / p99 us | distributions |
|---|---:|---:|
| `UniformSerial1` | 95.502 / 143.200 / 174.597 | 10/10 |
| `UniformSerial2` | 128.098 / 147.000 / 186.000 | 10/10 |
| `CrossKindFinal` | 92.600 / 144.299 / 173.099 | 10/10 |

Later retained full/focused runs demonstrate that this is not a stable close on the requested p99 gate:

| report | passing tests | failing test | worst median / p95 / p99 us | failed distributions |
|---|---:|---|---:|---|
| `SightWeaveM2P2_FullSightWeaveFinal` | 100 | Batch512Gate | 93.501 / 151.601 / 228.100 | 3 |
| `SightWeaveM2P2_FocusedM2Final` | 79 | Batch512Gate | 92.801 / 167.798 / 229.802 | 2, 9 |
| `SightWeaveM2P2_FocusedM2PFinal` | 23 | Batch512Gate | 93.602 / 144.903 / 240.602 | 3, 4, 5, 6 |
| `SightWeaveM2P2_FocusedM2P2Final` | 10 | Batch512Gate | 132.300 / 163.797 / 243.399 | 3, 4, 5 |

All ten distributions in all four failing processes still report zero steady result-capacity growth. `SightWeaveM2P2_FocusedM1Final` passed 21/21 and `SightWeaveM2P2_FocusedM2P1Final` passed 7/7. Filter counts overlap by test-path prefix and must not be summed.

## Correctness, lifecycle, and regression

- `SightWeave.M2P2.PreparedEventIndex`: 7/7 in `SightWeaveM2P2_PreparedIndexRadialShareFinal`.
- `SightWeave.M2P.Differential`: 3/3 in `SightWeaveM2PDifferential_RadialShareFinal`.
- `SightWeave.M2.Query`: 14/14 in `SightWeaveM2Query_RadialShareFinal`.
- `Darkwell`: 24/24 in `DarkwellRegression_SightWeaveM2P2Final`.
- Dynamic-door deterministic smoke: 1/1 in `SightWeaveM2P2_DynamicDoorSmokeFinal`.
- Complete `SightWeave`: 100 passed / 1 failed / 0 warning / 0 not-run; the only failure is Batch512Gate described above.

Prepared-index coverage includes exact cross-kind sharing and metadata isolation; owner/capability/range/cone/floor/height isolation; same-input cold/warm and repeated-trace bit determinism; two-door locality and rapid changes; door-plus-motion; held immutable readers; source and occluder deletion; deterministic entry/byte pressure and oversized fallback with physical reclamation; three world restarts; simultaneous multiworld isolation; and eight concurrent independent index/scratch instances. The mutable world index remains game-thread-only by design; concurrent readers consume plain immutable published snapshots.

No geometry, result-field, tie-break, immediate-authority, lifecycle, or determinism failure was observed. The batch failures are hard timing assertions, not parity failures.

## Build, Lab, package, and dependency matrix

- Full `DarkwellEditor Win64 Development` after the final source state: succeeded. A later source-major experiment was reverted and the reverted source was rebuilt successfully.
- Lab headless Game smoke: exit 0; `SightWeaveRuntime` loaded, `/SightWeave/Maps/L_SightWeave_Lab` entered play, and `LogSightWeaveDebugQuery` reported `status=0 authoritative=1 live=1 vision=1 light=0 bypass=1 snapshot=58 visionSources=1`. Log: `Saved/Logs/SIGHTWEAVE_M2P2_LAB_FINAL.log`.
- Repository-external BuildPlugin: `BUILD SUCCESSFUL`, UAT exit 0 in 2m25s. Package: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM2P2Final-20260825-112457`.
- Clean HostProject `UnrealEditor Win64 Development`: `Result: Succeeded`.
- Clean HostProject `UnrealGame Win64 Development`: `Result: Succeeded`.
- Clean HostProject `UnrealGame Win64 Shipping`: `Result: Succeeded`.

Runtime dependency isolation passed. `SightWeaveRuntime.Build.cs` lists only `Core`, `CoreUObject`, `Engine`, and `DeveloperSettings`. Repository and packaged Runtime sources contain no `Darkwell`, `UnrealEd`, `SightWeaveEditor`, `SightWeaveTests`, or `AutomationTest` reference. The packaged `UnrealEditor-SightWeaveRuntime.dll` imports only those four UE DLLs plus Kernel32 and MSVC/CRT libraries. Packaged Development and Shipping Runtime objects/precompiled manifests exist; `.uplugin` keeps Runtime as `Runtime` and Editor/Tests as `Editor`.

The Lab log contains 13 UE 5.8 pre-engine-init `LogAutomationTest: Error: Condition failed` diagnostics immediately after the engine's `UnifiedErrorTest` sample messages. They are not attached to a queued SightWeave test; the map/status evidence and process exit are clean. Win64 SDK validation is valid. Startup also reports unavailable non-Win64 SDKs. Builds warn that installed MSVC 14.51.36256 is newer than UE 5.8's preferred 14.50.35717, and clean-host compilation surfaces C4996 deprecations in UE headers; no warning points to SightWeave source.

## Repository and LFS gates

- `git diff --check`: no whitespace error; only line-ending conversion notices for the local `.uproject` and one tracked source file.
- No tracked path exists under `Binaries`, `DerivedDataCache`, `Intermediate`, or `Saved`.
- `git lfs status`: no object to push or commit; only the unstaged local `.uproject` content difference.
- `git lfs fsck`: `Git LFS fsck OK`.
- All `.uasset`, `.umap`, and configured source-media extensions remain covered by `.gitattributes`; the Lab map is an LFS pointer.
- The BuildPlugin-generated untracked `Plugins/SightWeave/Config/FilterPlugin.ini` empty template was inspected and removed. It was not staged or committed.
- Before this documentation commit, local HEAD and `origin/codex/m2p2-sightweave-motion-event-index` both resolved to `56cecde5417dcb17c679ab9a604eff89f5bd6966`.

After committing this document, push the branch and verify local HEAD, upstream, and `git ls-remote` are identical. The expected final worktree is not literally empty because the pre-existing `Darkwell.uproject` EngineAssociation change must remain unstaged; no M2P.2 source or documentation delta should remain.

## Remaining work and recovery

Completion requires a production-safe way to make every ordinary 512-query distribution meet 150/180/200 us median/p95/p99 and to close the broad RuntimePipeline 250 us dynamic-door tail without affinity/priority manipulation or weakening exact synchronous authority. The retained evidence suggests host/scheduler tail sensitivity; it does not authorize relabeling the misses. Any next optimization must preserve the startup-traced 0/0/0 batch property, independent point parity, fixed seeds, exact output, and the current lifecycle and Shipping matrix.

Async worker completion order, stale-task cancellation/rejection, pending-authority policy, and multi-worker wall time are not applicable because production remains synchronous and serial. GPU masks, post process, memory textures/tiles, persistence, DARKWELL gameplay integration, `/Game/Maps/L_Prototype`, M3, and main merge remain outside scope.

Exact recovery command:

```powershell
git switch codex/m2p2-sightweave-motion-event-index; git pull --ff-only origin codex/m2p2-sightweave-motion-event-index; git rev-parse HEAD; git lfs pull; git lfs status
```

Key final commands:

```powershell
.\Scripts\BuildEditor.ps1 -EngineRoot D:\UE_5.8 -Configuration Development
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM2P2_FullSightWeaveFinal'
pwsh -NoProfile -File .\Scripts\RunSightWeaveAllocationProof.ps1 -Label M2P2FinalCrossKind_20260825 -EngineRoot D:\UE_5.8
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject /SightWeave/Maps/L_SightWeave_Lab -game -unattended -nop4 -nosplash -NullRHI -NoSound -benchmark -seconds=2 -fps=30
& D:\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin=D:\UE_projects\LastLight\Plugins\SightWeave\SightWeave.uplugin -Package=C:\Users\defiler919\AppData\Local\Temp\SightWeaveM2P2Final-20260825-112457 -TargetPlatforms=Win64 -Rocket
```

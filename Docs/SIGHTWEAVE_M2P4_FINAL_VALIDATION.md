# SightWeave M2P.4 Final Performance Validation

## Verdict

**Status: `BLOCKED`**

The production optimization, correctness coverage, allocation proof, clean
NVIDIA gate, three-process performance sampling, 36,000-frame NullRHI and
D3D12 soaks, full regression, Editor build, clean-host package matrix, Lab
smokes, dependency audit, and remote-ready Git state are complete. The final
acceptance cannot be labelled `COMPLETED` because the required post-change
administrator ContextSwitch/CSwitch trace was not acquired. Three explicit
`Start-Process -Verb RunAs` attempts returned the Windows "operation canceled"
result at the UAC boundary. No elevation bypass was attempted and no partial
trace was treated as evidence.

Consequently, this report keeps wall time and thread-cycle evidence separate
and does **not** relabel either as intrinsic running CPU microseconds. The
post-change intrinsic CPU verdict, exact running/wait/preemption intervals, and
authoritative classification of the remaining slow samples are unverified.

## Scope and checkpoints

- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`.
- Branch: `codex/m2p4-sightweave-etw-dynamic-sector`.
- Resumed-validation baseline: `68d19c6fa479f7a64cbbe76e493cc2acbbfffd63`.
- NVIDIA stability checkpoint: `28943d7613afa7fb87c7659d5885a33a7e4cabdd`.
- Evidence-driven capacity fix: `2e50de8dbff5d606126bce2aa344b048b1394ae8`.
- This document and the final handoff are committed as the final branch
  checkpoint; its exact SHA is the final local/upstream/`ls-remote` SHA.
- The local `Darkwell.uproject` `EngineAssociation` GUID difference was never
  restored, overwritten, staged, or committed.

## NVIDIA and host gate

Windows booted at `2026-08-25 17:35:46 +08:00`. The required 30-minute gate
was sampled at `18:06:14 +08:00`, after 30.473 minutes of uptime:

- `NvContainerLocalSystem`: Running, Auto, exit code 0, PID 5988;
- process creation: `17:35:57`, with no service transition or recovery loop;
- Application NVIDIA/nvcontainer 1000/1001 since boot: 0;
- System NVIDIA Service Control Manager 7023/7031 since boot: 0;
- new NVIDIA/nvcontainer/LiveKernelEvent WER directories since boot: 0;
- `nvidia-smi`: RTX 4060, Studio/KMD 610.88, WDDM, 36 C, P8;
- high-performance Windows power scheme selected;
- Chrome, Edge, Baidu user processes, OneDrive, Widgets, Settings, and other
  nonessential user applications were closed. Codex, NVIDIA Container,
  Defender, security services, Windows core processes, and required networking
  were preserved.

The service remained Running with the same PID 5988 after D3D12 soak, D3D12
Lab, and packaging. From the rendered-test baseline onward there were zero new
NVIDIA Application events, zero `nvlddmkm`/Display System events, zero NVIDIA
WER reports, and no `Device Removed`, DXGI, GPU crash, or TDR failure in the
Unreal logs.

## Elevated ETW acceptance blocker

The required final ETW workflow was prepared to run the calibration, ten
ordinary attribution processes, and allocation proof in one elevated child.
Windows canceled all three visible UAC attempts before the child started. No
post-change ETL, context-switch timeline, or elevated run log was created.

The older administrator ETW set under
`Saved/SightWeaveM2P4/EtwAttribution/admin-uac-formal01` predates the dynamic
sector implementation and the capacity-retention fix. It remains useful
historical evidence but is not substituted for post-change acceptance. The
ordinary-process timing sampler also showed that Windows `GetThreadTimes` has
insufficient resolution at these microsecond intervals; its zero-valued fields
are not used as CPU time.

## Allocation proof and the retained-capacity fix

The expanded startup Unreal Insights proof captures 23 workloads with three
marked samples each, 69 scopes total. Twenty formal warmed workloads are hard
asserted at exact zero allocator activity:

- optimized solver 2x64, 8x64, 8x256, 8x1024, 4096 total, and 4096/source;
- point query, Batch512, source transform, narrow dynamic door, broad 4v2l
  dynamic door, clean publication, and no-change update;
- radial rotation, cone rotation, 1/5/20 cm translation, teleport, and the
  shared-origin four-source path.

The first expanded proof correctly failed because the broad 4v2l door path
allocated one 4-byte nested `SourceEdgeIndices` array in samples 0 and 2. PDB
symbolization located `TArray<int>::CopyToEmpty` in
`USightWeaveWorldSubsystem::UpdateOccluder`: removing and reinserting a cached
illumination solve segment discarded the nested array's capacity. Production
code now recycles that capacity in the same bounded pattern already used by
the spatial index. Geometry, revision, publication, and synchronous authority
semantics are unchanged.

Three independent post-fix proof processes passed:

- `M2P4FinalRecycleCandidate_01_20260825`;
- `M2P4FinalRecycleCandidate_02_20260825`;
- `M2P4FinalRecycleCandidate_03_20260825`.

Every formal row is `0 allocation calls / 0 reallocation calls / 0 allocated
bytes`; no marked container capacity can grow without allocator activity in
these scopes. Evidence-only range-change, held-snapshot, and combined-motion
setup paths remain reported separately and are not hidden inside the formal
zero set. The final proof files are:

| File | Bytes | SHA-256 |
|---|---:|---|
| `Saved/SightWeaveM2P1/AllocationProof/M2P4FinalRecycleCandidate_03_20260825/M2P4FinalRecycleCandidate_03_20260825.utrace` | 433,029,537 | `9870B883519682D91C065B986231200F19A7FC75575E996292E2821A9FDA5D74` |
| adjacent `.csv` | 22,859 | `54F7FFBDAAB349835DBEA7C30D6E903F3C6DB806E55E5C78409706A06BAE4E89` |

The raw trace and CSV remain ignored and are not committed.

## Ordinary-process attribution

`final-post-nvidia-wall-20260825` ran ten serial ordinary processes without
affinity or priority changes. Every CSV sample records process/run, automation
thread ID, start/end processor, migration, frequency, page faults, thread
cycles, wall time, and SightWeave stage metrics. These classifications are
diagnostic only because CSwitch ETW is missing.

| Workload | Processes / samples | wall p50 / p95 / p99 / p99.9 / max us | Provisional slow classification |
|---|---:|---:|---|
| Batch512 | 10 / 10,100 | 91.5 / 138.0 / 156.6 / 431.9 / 1554.0 | 23 migration/frequency, 5 scheduler, 2 plugin, 3 unknown |
| broad dynamic door 4v2l | 10 / 1,010 | 161.5 / 252.9 / 329.7 / 433.3 / 470.6 | 41 plugin, 2 migration/frequency, 9 unknown |
| dedicated narrow door | 10 / 1,010 | 35.5 / 58.7 / 87.3 / 1366.6 / 1387.4 | 1 scheduler, 1 plugin, 1 unknown |
| held-reader door diagnostic | 10 / 110 | 164.3 / 266.3 / 284.0 / 318.1 / 318.1 | 2 plugin, 4 unknown |
| door plus motion | 10 / 1,010 | 236.1 / 334.9 / 445.3 / 767.0 / 809.1 | 1 scheduler, 29 plugin, 7 migration/frequency, 171 unknown |

Batch's two failing wall-time distributions were run-02/distribution-0 p99
201.6 us and run-06/distribution-9 p99 209.9 us. All four samples responsible
for those two p99 failures migrated cores, and had zero page faults. Across all
10,100 rows only 2 slow rows were provisionally classed as SightWeave CPU
overruns. This is specific evidence, but not an intrinsic-CPU waiver.

Raw combined evidence:

| File | Bytes | SHA-256 |
|---|---:|---|
| `Saved/SightWeaveM2P3/Attribution/final-post-nvidia-wall-20260825/batch-all.csv` | 5,312,576 | `5AFEC46B06AB70FF0A8554FCF51F217074EC244F68CB2205385FD3C2A160157F` |
| adjacent `door-all.csv` | 1,755,967 | `C9D593AD1B84B6B30B274C3D4FF96242D6A7D41F8A9AE216DDE7EF1007F27A07` |

## Three-process performance contracts

Each extended benchmark used a fresh `UnrealEditor-Cmd -NullRHI
-DisablePython -SightWeaveExtendedBenchmarks` process. All three processes ran
one automation worker/game thread and completed 3/3 tests. Solver tuples below
are `p50 / p95 / p99 / p99.9 / max` microseconds. Because every per-process
distribution has fewer than 1,000 samples, empirical nearest-rank p99.9 equals
the observed maximum; it is shown explicitly rather than invented by
interpolation.

Reference is the development/test oracle and is never a Shipping authority
path.

| Reference workload (samples; warmup) | process 1 | process 2 | process 3 |
|---|---:|---:|---:|
| 2x64 (11; 2) | 3600.903 / 3730.703 / 3730.703 / 3730.703 / 3730.703 | 2639.599 / 2670.102 / 2670.102 / 2670.102 / 2670.102 | 2646.498 / 2915.200 / 2915.200 / 2915.200 / 2915.200 |
| 8x64 (11; 2) | 10652.304 / 10914.400 / 10914.400 / 10914.400 / 10914.400 | 10725.699 / 10785.401 / 10785.401 / 10785.401 / 10785.401 | 10698.698 / 11342.093 / 11342.093 / 11342.093 / 11342.093 |
| 8x256 (7; 1) | 131153.800 / 132169.504 / 132169.504 / 132169.504 / 132169.504 | 130326.893 / 132161.096 / 132161.096 / 132161.096 / 132161.096 | 130439.695 / 145010.903 / 145010.903 / 145010.903 / 145010.903 |
| 8x1024 (5; 1) | 1611924.104 / 1620325.696 / 1620325.696 / 1620325.696 / 1620325.696 | 1629183.296 / 1663896.799 / 1663896.799 / 1663896.799 / 1663896.799 | 1611625.902 / 1620269.600 / 1620269.600 / 1620269.600 / 1620269.600 |
| 4096 total (5; 1) | 424980.707 / 427816.704 / 427816.704 / 427816.704 / 427816.704 | 425504.997 / 428397.808 / 428397.808 / 428397.808 / 428397.808 | 425527.707 / 427118.402 / 427118.402 / 427118.402 / 427118.402 |
| 4096/source (3; 0) | 23549873.896 / 23572795.693 / 23572795.693 / 23572795.693 / 23572795.693 | 23557222.798 / 23564189.296 / 23564189.296 / 23564189.296 / 23564189.296 | 23517032.903 / 23535098.597 / 23535098.597 / 23535098.597 / 23535098.597 |

| Optimized workload (samples; warmup) | process 1 | process 2 | process 3 |
|---|---:|---:|---:|
| 2x64 (31; 4) | 40.300 / 48.999 / 54.598 / 54.598 / 54.598 | 40.300 / 44.301 / 82.899 / 82.899 / 82.899 | 40.699 / 44.998 / 50.500 / 50.500 / 50.500 |
| 8x64 (31; 4) | 199.694 / 221.498 / 234.704 / 234.704 / 234.704 | 198.096 / 208.605 / 214.100 / 214.100 / 214.100 | 198.495 / 205.494 / 225.399 / 225.399 / 225.399 |
| 8x256 (21; 2) | 684.809 / 716.906 / 733.197 / 733.197 / 733.197 | 688.102 / 721.302 / 727.803 / 727.803 / 727.803 | 685.900 / 715.502 / 759.207 / 759.207 / 759.207 |
| 8x1024 (21; 2) | 2030.201 / 2927.702 / 3254.700 / 3254.700 / 3254.700 | 2113.499 / 2870.101 / 2882.212 / 2882.212 / 2882.212 | 1998.000 / 2047.397 / 2237.402 / 2237.402 / 2237.402 |
| 4096 total (21; 2) | 1021.001 / 1072.399 / 1097.295 / 1097.295 / 1097.295 | 1061.901 / 1078.799 / 1117.200 / 1117.200 / 1117.200 | 1019.403 / 1088.604 / 1639.102 / 1639.102 / 1639.102 |
| 4096/source (11; 1) | 9074.900 / 10831.803 / 10831.803 / 10831.803 / 10831.803 | 9232.193 / 9720.199 / 9720.199 / 9720.199 / 9720.199 | 9063.601 / 9663.403 / 9663.403 / 9663.403 / 9663.403 |

Runtime pipeline tuples use the same five-number order:

| Workload (samples; warmup) | process 1 | process 2 | process 3 |
|---|---:|---:|---:|
| snapshot publish (31; 3) | 0.000 / 0.101 / 0.101 / 0.101 / 0.101 | same | same |
| public snapshot value copy (31; 3) | 6.802 / 9.302 / 11.198 / 11.198 / 11.198 | 7.197 / 11.601 / 12.603 / 12.603 / 12.603 | 6.802 / 7.503 / 8.203 / 8.203 / 8.203 |
| point query (501; 20) | 0.298 / 0.402 / 2.697 / 2.801 / 2.801 | 0.298 / 0.402 / 2.500 / 2.801 / 2.801 | 0.201 / 0.402 / 2.600 / 2.697 / 2.697 |
| Batch512 (101; 10) | 87.101 / 89.500 / 99.998 / 121.001 / 121.001 | 87.000 / 89.299 / 115.301 / 116.598 / 116.598 | 87.101 / 89.701 / 93.203 / 117.302 / 117.302 |
| broad door solve/publish (101; 10) | 133.600 / 146.400 / 173.498 / 191.800 / 191.800 | 134.300 / 143.997 / 169.400 / 182.301 / 182.301 | 141.699 / 149.596 / 170.801 / 185.400 / 185.400 |
| source transform (101; 10) | 29.001 / 29.601 / 31.598 / 32.000 / 32.000 | 29.102 / 32.000 / 38.799 / 39.000 / 39.000 | 29.098 / 34.001 / 45.799 / 46.600 / 46.600 |
| no-change update (101; 5) | 0.101 / 0.201 / 0.201 / 0.399 / 0.399 | 0.101 / 0.201 / 0.201 / 0.298 / 0.298 | 0.101 / 0.201 / 0.201 / 0.201 / 0.201 |

All three RuntimePipeline Batch rows retained exact zero steady capacity
growth. No-change preserved the snapshot revision.

Prepared Event Index 4096/source was then run in three additional fresh
processes, with 3 warmups and 101 measured rotations per process:

| Process | p50 / p95 / p99 / p99.9 / max us | Contract |
|---:|---:|---|
| 1 | 891.898 / 1407.903 / 1437.500 / 1442.399 / 1442.399 | pass: p50 <1 ms, p99 <2 ms |
| 2 | 873.402 / 954.900 / 1336.299 / 1432.300 / 1432.300 | pass |
| 3 | 878.602 / 942.897 / 1125.202 / 1421.001 / 1421.001 | pass |

One retained M2-prefix process measured 1027.599 / 1170.799 / 1199.700 /
1447.901 us and failed only the median contract; another independent M2P2
process measured 880.901 / 1019.400 / 1500.301 / 1899.999 us and passed.
Neither result is discarded.

## Batch512 original gate matrix

The original ten-distribution assertions remain unchanged: p50 <=150 us, p95
<=180 us, p99 <=200 us, with exact zero warmed capacity growth. The test was
not skipped, split, or relaxed.

Four fresh complete `SightWeave.M2P` processes provide the required full-suite
matrix:

| Process | Suite result | worst p50 / p95 / p99 / max us |
|---:|---|---:|
| 1 | 35/35 pass | 89.899 / 144.899 / 184.402 / 1414.597 |
| 2 | 35/35 pass | 88.900 / 139.102 / 194.199 / 575.200 |
| 3 | 34 pass / 1 fail | 89.001 / 140.201 / 329.800 / 593.200 |
| 4 | 34 pass / 1 fail | 88.699 / 98.098 / 382.099 / 454.701 |

The first three isolated clean processes all passed, with worst p99
158.198/149.399/142.198 us. A larger isolated control then repeated only
Batch512 ten times in one process: 6 passes and 4 failures, worst p99 471.398
us and worst max 817.101 us. Therefore the earlier full-suite-fails / isolated-
passes pattern was a small-sample coincidence, not a deterministic suite state
pollution. Code review also found no leaked worker: the preceding scratch
`ParallelFor` is a synchronous barrier and all workers complete before return.

The ordinary 10-process matrix independently produced 98/100 passing
distributions, with the two failures entirely composed of core-migration
samples as described above. This narrows the cause but cannot replace the
missing ContextSwitch proof. Batch final acceptance is therefore `BLOCKED`,
not waived and not converted into a production rewrite without authoritative
on-CPU evidence.

## Soaks

Both tests used 2,400 warmup frames, 36,000 measured frames, an exact
60-fps/600-second simulated interval, one automation thread, no affinity or
priority modification, and zero correctness failures. Capacity growth was
zero. Allocation calls were not sampled inline for all 36,000 frames; the
corresponding warmed operations are covered by the separate three-run Insights
allocator proof above.

| Mode | wall p50 / p95 / p99 / p99.9 / max us | >1 ms and streaks | Correctness / capacity |
|---|---:|---|---|
| NullRHI | 83.0 / 131.5 / 251.8 / 298.5 / 1523.8 | 2 samples >1 ms; longest >p99 1, >1 ms 1 | 0 failures / 0 bytes |
| D3D12 SM6 rendered, RTX 4060 | 103.2 / 213.4 / 294.5 / 457.5 / 1480.4 | 2 samples >1 ms; longest >p99 2, >1 ms 1 | 0 failures / 0 bytes |

The rendered log proves Default RHI D3D12, SM6, NVIDIA adapter 0, feature level
12_2, shader model 6.7, and driver 610.88. Raw frame files:

| File | Bytes | SHA-256 |
|---|---:|---|
| `Saved/SightWeaveM2P3/Soak/m2p4-final-nullrhi-36000-20260825/Raw/frames.csv` | 5,504,231 | `689F5FD6298523E6FF20997A70C99B47BF2956EBD039111613B254EACA2441FA` |
| `Saved/SightWeaveM2P3/Soak/m2p4-final-d3d12-36000-20260825/Raw/frames.csv` | 5,522,491 | `BE559285F49211BB32CE6499544790411BE95D26B1B3A68F6B6DBEA0E2FEF165` |

## Correctness and regression

Focused post-fix coverage passed: dynamic sector 4/4, Runtime differential
3/3, Prepared Event Index lifecycle 7/7, and Transform API 1/1. The final M2P4
prefix passed 7/7, including exact single-segment incremental authority,
`-PI/+PI` cyclic seam handling, multi-change full-solve fallback, missing-
prepared-index fallback, synthetic QPC correlation, and ETW analyzer tests.
Together with M1/M2 coverage, the full suite exercises dynamic door before and
after authority, revision/snapshot lifetime, stale held snapshots,
compatibility, illumination, bypass, suppression, fixed-seed Reference versus
Optimized differential, concurrency, and reentrancy.

Final serial regression reports:

| Prefix | Passed | Failed | Result |
|---|---:|---:|---|
| `SightWeave.M1` | 21 | 0 | pass |
| `SightWeave.M2` | 90 | 1 | retained Prepared median failure |
| `SightWeave.M2P` | 34 | 1 | retained Batch p99 failure |
| `SightWeave.M2P1` | 7 | 0 | pass |
| `SightWeave.M2P2` | 10 | 1 | retained Batch p99 202.700 us |
| `SightWeave.M2P3` | 4 | 0 | pass |
| `SightWeave.M2P4` | 7 | 0 | pass |
| full `SightWeave` | 112 | 0 | pass in that independent process |
| `Darkwell` | 24 | 0 | pass |

The full-suite pass does not erase the retained performance failures.

## Builds, package, Shipping isolation, and Lab

- Final `Scripts/BuildEditor.ps1`: `DarkwellEditor Win64 Development`
  succeeded; target up to date.
- Fresh BuildPlugin:
  `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM2P4Final-20260825-1837`.
- Clean host `UnrealEditor Win64 Development`: pass.
- Clean host `UnrealGame Win64 Development`: pass.
- Clean host `UnrealGame Win64 Shipping`: pass.
- Runtime dependencies remain exactly `Core`, `CoreUObject`, `Engine`, and
  `DeveloperSettings`.
- Game Development and Shipping outputs contain Runtime products and no
  SightWeaveTests or SightWeaveEditor products.
- Packaged Runtime source contains no `Darkwell`, `UnrealEd`,
  `SightWeaveEditor`, `SightWeaveTests`, `AutomationTest`, or Windows ETW
  dependency.
- `dumpbin /dependents` for `UnrealEditor-SightWeaveRuntime.dll` lists only
  the four expected UE DLLs, KERNEL32, and VC/CRT libraries.
- UAT's generated untracked `Plugins/SightWeave/Config/FilterPlugin.ini` was
  inspected and removed; it is not committed.
- Lab NullRHI and D3D12/SM6 smokes both exited 0, loaded
  `/SightWeave/Maps/L_SightWeave_Lab`, and reported `status=0`,
  `authoritative=1`, `live=1`, `vision=1`, `bypass=1`, snapshot 58, and one
  vision source.

## Warning and severe-error audit

- No ensure failure, assertion failure, fatal/critical error, unhandled
  exception, device removal, DXGI error, or GPU crash appeared in the final
  logs.
- UE 5.8 emits 13 pre-discovery `LogAutomationTest: Error: Condition failed`
  messages per automation process as part of its own UnifiedError diagnostic;
  they precede the selected test and are not SightWeave assertions.
- The retained controller errors are exactly the Batch and Prepared hard-gate
  failures listed above.
- Build warnings are the installed MSVC 14.51 versus UE-preferred 14.50 notice
  and UE header C4996 deprecations. No SightWeave compile error occurred.
- Other nonfatal engine/host warnings are ordinary headless scalability,
  navigation, stylus-driver, and editor-data messages.

## Unverified items and remaining risk

1. Post-change elevated ContextSwitch/CSwitch capture and intrinsic running CPU
   distributions are not available.
2. Exact ETW-backed running/wait/preemption, GPU/driver, and Unknown labels for
   remaining Batch/broad-door tails are not closed.
3. Batch has retained failures both in full-suite and expanded isolated
   sampling; its strict final performance gate is not green.
4. Broad-door ordinary wall p95/p99 exceed 250 us in the aggregate, but no
   intrinsic verdict is made without ETW.
5. The 36k soaks record capacity but not per-frame allocator calls; strict
   allocation calls are proven in the separate marked Insights workload set.

These gaps are why the outcome is `BLOCKED`, even though the NVIDIA, runtime,
correctness, allocation, soak, build, package, dependency, Lab, and Git work is
otherwise complete.

## Exact recovery commands

```powershell
cd D:\UE_projects\LastLight
git switch codex/m2p4-sightweave-etw-dynamic-sector
git pull --ff-only origin codex/m2p4-sightweave-etw-dynamic-sector
git lfs pull
git status --short --branch
git rev-parse HEAD
git rev-parse '@{upstream}'
git ls-remote origin refs/heads/codex/m2p4-sightweave-etw-dynamic-sector
```

The expected worktree after recovery has only the workstation-local modified
`Darkwell.uproject`; no generated output or raw trace is tracked.

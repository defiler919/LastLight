# SightWeave M2P.5 Final Validation

Status: **IN PROGRESS — SAFE VALIDATION COMPLETE; TWO ELEVATED MATRICES PENDING**

Last updated: 2026-08-26 (Asia/Shanghai)

## Authority and scope

- Branch: `codex/m2p5-sightweave-vision-solve-tail-closure`
- Immutable baseline: `c3a3323edadb648058fd33c4c1e57806eeac8536`
- Validated production/test code SHA: `c8971a4ac1781b4a945e7a7bb3a4c415837027e9`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Local `Darkwell.uproject` `EngineAssociation` difference remains unstaged and was not restored, overwritten, staged, or committed.
- No M3, DARKWELL gameplay, GPU mask, post-process, memory layer, or map work was performed.
- No affinity, process-priority, Defender, or security-service change was made.

The user is away from the computer. Per the explicit pause instruction, no UAC,
administrator ETW, `Start-Process -Verb RunAs`, or secure-desktop action was
started after that instruction. The pending matrices are unattempted, not
failed, and are not counted as performance failures.

## Pre-change fine-grained authority

The fail-closed ten-process broad-door capture is preserved at:

`D:\UE_projects\LastLight\Saved\SightWeaveM2P5\VisionTailAttribution\preproduction-detailed-formal-20260826`

| Item | Result |
|---|---:|
| independent PIDs | 10 |
| broad-door samples | 1,010 |
| source/substage rows | 65,650 |
| event loss / buffer loss | 0 / 0 |
| ownership conflicts / unclosed timelines | 0 / 0 |
| Unknown | 0 |
| on-CPU p50 / p95 / p99 / max | 154.3 / 272.5 / 305.3 / 388.8 us |
| ready p99 / blocked p99 | 54.3 / 0 us |
| preemptions / migrations | 45 / 34 |
| classification | 872 Within, 129 Plugin, 9 Scheduler, 0 GPU, 0 Unknown |

The unchanged `<250 us` intrinsic p99 contract failed. Source-exact evidence
identified two production CPU amplifications:

1. The vision source sharing compatible illumination geometry rejected an
   exact alternating Prepared cache as `prepared_index_replaced` in
   1,010/1,010 samples, forcing a full ray solve.
2. The other three sources rebuilt only 38/49/52 dirty rays but advanced the
   angular active set before the reuse decision for all 518/523 rays.

Five independent marker control/detailed pairs found no positive p50/p95/p99
perturbation. No diagnostic overhead was subtracted. Full classification is in
`Docs/SIGHTWEAVE_M2P5_VISION_TAIL_CLASSIFICATION.md`.

## Production changes

Only the two evidence-authorized paths changed:

- Incremental solve now accepts the previous Prepared cache for old-state and
  dirty-context validation and the target Prepared cache for the new solve.
  Pointer identity is no longer required when the previous binding and
  revisions are valid. All stale, missing, replaced, capacity, seam, topology,
  and revision failures retain synchronous fail-closed fallback.
- Angular active-set advance now occurs inside the non-reused-ray path. Reused
  rays still require exact angle, finite old distance/point, and dirty-sector
  exclusion; rebuilt rays advance directly to their ordered angle.

The change does not alter candidate precision, stable-ID ties, inclusive
boundaries, 2.5D height/floor semantics, Visible/IR isolation, illumination or
suppression order, snapshot immutability, revision semantics, or Shipping
oracle behavior.

The later clean-host include-closure correction adds only
`Containers/StaticArray.h` to the two public Runtime headers that declare
`TStaticArray`. It changes no runtime behavior.

## Correctness and fallback validation

Post-include focused M2P.5 report:

`Saved\AutomationReports\M2P5_PostInclude_Focused_20260826`

- 4/4 passed: shared Prepared cache dynamic-sector exactness, broad 4V/2L
  dynamic-door exactness, 1/5/20 cm and narrow/wide/teleport/rotation movement
  matrix, and fixed-seed incremental differential determinism.
- The broad test holds an old snapshot, checks per-source fallback and
  rebuilt/reused counts, and compares every update with a fresh full Optimized
  solve.
- Fixed-seed warm/replay checks preserve bitwise polygons, deterministic
  counts, and fixed warmed capacities.
- Post-change functional smoke retained 101 broad updates and 404 vision-source
  rows with zero fallback. Average rebuilt/reused rays were source 1 `74/444`,
  source 2 `49/474`, source 3 `38/480`, and source 4 `52/471`.

Final serial reports after the include fix:

| Filter | Passed | Failed | Classification |
|---|---:|---:|---|
| `SightWeave.M1` | 21 | 0 | clean |
| `SightWeave.M2` | 94 | 1 | Batch512 wall-time performance assertion |
| `SightWeave.M2P` | 38 | 1 | Batch512 wall-time performance assertion |
| `SightWeave.M2P1` | 7 | 0 | clean |
| `SightWeave.M2P2` | 11 | 0 | clean |
| `SightWeave.M2P3` | 4 | 0 | clean |
| `SightWeave.M2P4` | 7 | 0 | clean |
| `SightWeave.M2P5` | 4 | 0 | clean |
| full `SightWeave` | 116 | 0 | clean |
| `Darkwell` | 24 | 0 | clean |

The two retained Batch failures are not hidden:

- M2 filter: distribution 6 wall p99 `242.401 us`, limit `<=200 us`;
  worst median/p95/max `89.601/139.903/601.001 us`.
- M2P filter: distribution 0 wall p99 `247.300 us` and distribution 2
  `239.797 us`; worst median/p95/max `93.803/137.202/274.699 us`.
- The later full SightWeave run passed all ten distributions with worst wall
  median/p95/p99/max `88.900/128.102/153.501/165.701 us`.

These are wall-time gates and are preserved as performance failures. They do
not become intrinsic CPU measurements and will be adjudicated only by the
pending ContextSwitch/CSwitch matrices.

Prepared Event Index 4096 passed in all three final serial scopes. The full
SightWeave run recorded median/p95/p99/max
`974.301/1147.199/1536.500/1575.299 us`, satisfying median `<1 ms` and p99
`<2 ms`.

Three independent extended performance processes passed 3/3 each:

| Run | broad-door wall p99 | Batch512 wall p99 | source-transform p99 | point-query p99 | no-change p99 |
|---|---:|---:|---:|---:|---:|
| 1 | 241.201 us | 134.997 us | 35.603 us | 2.600 us | 0.201 us |
| 2 | 144.701 us | 90.700 us | 77.803 us | 2.600 us | 0.201 us |
| 3 | 233.401 us | 105.500 us | 75.500 us | 2.600 us | 0.302 us |

All three reported Batch steady capacity growth `0`.

## Allocation proof

Final post-include proof:

`D:\UE_projects\LastLight\Saved\SightWeaveM2P1\AllocationProof\M2P5PostIncludeFinal_20260826`

- allocator-hook trace and analysis reports: 1/1 capture and 1/1 analysis;
- 69 marked rows, 23 workloads, three samples per workload;
- all 20 formal warmed workloads: zero allocation calls, reallocation calls,
  and allocated bytes;
- broad 4V/2L dynamic door is included in the strict-zero set;
- the two lifecycle controls (`motion_range_change` and held-snapshot
  transform) intentionally allocate their new public values; neither
  reallocates and neither is part of the formal warmed-zero contract;
- CSV SHA-256:
  `C64C47B44FC1FD839BC649FED9321E260F8E1F1CC9E77FEFFFEA2FB4AA502363`.

## Long soaks

| Mode | Evidence root | wall p50 / p95 / p99 / p99.9 / max (us) | >1 ms | max consecutive >p99 | correctness / capacity / Unknown |
|---|---|---:|---:|---:|---:|
| NullRHI | `Saved\SightWeaveM2P3\Soak\m2p5-post-include-final-nullrhi-36000-20260826` | 92.0 / 211.9 / 280.2 / 451.7 / 1005.4 | 1 | 2 | 0 / 0 / 0 |
| D3D12 SM6 | `Saved\SightWeaveM2P3\Soak\m2p5-post-include-final-d3d12-36000-20260826` | 89.3 / 218.6 / 286.3 / 465.5 / 1077.3 | 1 | 4 | 0 / 0 / 0 |

Each run used 2,400 warmup frames, 36,000 measured frames, and 600 simulated
seconds without affinity or priority modification. Frame CSV SHA-256 values:

- NullRHI:
  `B03C4C76B931938297B88DF0BD42459009AD76D12FE56971C47B3A44D0EA62AE`
- D3D12:
  `3DD5A7FFE3CD660D146D313FB41C66D676081273AD09A3066E1EB1D5FBEDEB01`

## Build, package, Shipping, and Lab

- Final `Scripts/BuildEditor.ps1`: `DarkwellEditor Win64 Development` passed,
  target up to date.
- A first retained package at
  `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM2P5PostVisionTail-20260826-1115`
  exposed missing direct `TStaticArray` public-header includes during clean-host
  UnrealGame Development. It is a build failure, not a performance result.
- Fresh corrected BuildPlugin package:
  `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM2P5PostInclude-20260826-1121`.
- Clean-host UnrealEditor Development, UnrealGame Development, and UnrealGame
  Shipping all passed; UAT ended `BUILD SUCCESSFUL`, exit 0.
- Runtime dependencies remain exactly `Core`, `CoreUObject`, `Engine`, and
  `DeveloperSettings`.
- Game Development and Shipping intermediate products contain only
  `SightWeaveRuntime`; no Tests or Editor target directory exists.
- Packaged Runtime source and Shipping object symbols contain no `Darkwell`,
  `UnrealEd`, `SightWeaveEditor`, `SightWeaveTests`, `AutomationTest`, or direct
  Windows ETW import.
- `dumpbin /dependents` for the packaged Runtime DLL lists the four expected UE
  DLLs, `KERNEL32`, and VC/CRT libraries only.
- The generated untracked `Plugins/SightWeave/Config/FilterPlugin.ini` was
  inspected and removed and is not committed.
- Final Lab NullRHI and D3D12/SM6 smokes both exited 0, loaded
  `/SightWeave/Maps/L_SightWeave_Lab`, and reported `status=0`, authoritative,
  live, vision, bypass, snapshot 58, and one vision source. D3D12 selected
  adapter 0, RTX 4060, SM6, driver 610.88.

## NVIDIA and severe-log audit

Post-validation read-only gate on the current boot:

- boot: `2026-08-26T08:33:41.5000000+08:00`;
- `NvContainerLocalSystem`: Running, Auto, PID 5584, exit 0; the PID matches the
  earlier post-soak gate;
- Application NVIDIA/nvcontainer 1000/1001: 0;
- System NVIDIA/nvcontainer 7023/7031: 0;
- System Display/nvlddmkm/TDR: 0;
- Application Device Removed/DXGI/GPU-crash/TDR: 0;
- new NVIDIA/nvcontainer/LiveKernelEvent/DXGI/GPU WER directories: 0;
- `nvidia-smi`: NVIDIA GeForce RTX 4060, Studio/KMD 610.88, WDDM, P8.

Across final automation, extended performance, allocation, soak, and Lab logs,
the severe scan found zero ensure failures, assertions, fatals, critical errors,
unhandled exceptions, device removals, DXGI errors, or GPU crashes. UE 5.8
emits 13 pre-discovery `LogAutomationTest: Error: Condition failed` UnifiedError
self-diagnostics per process. The only other final controller errors are the
retained Batch wall-time failures listed above. Build warnings are the installed
MSVC 14.51 versus preferred 14.50 notice and UE-header C4996 deprecations.

## Pending elevated acceptance gate

The thin orchestration wrapper is:

`Scripts\RunSightWeaveM2P5FinalEtwMatrices.ps1`

It has passed PowerShell AST parsing but has deliberately not been executed.
It performs one calibration followed by matrix A and matrix B in the same
elevated PowerShell process. Each matrix calls the existing M2P.4 fail-closed
workflow unchanged with ten independent processes, retaining Batch512 10,100
and broad-door 1,010 samples and its existing warmup, nearest-rank, and
threshold rules.

After the user replies `我回来了`, announce `现在请点击UAC的是`, launch exactly
one visible UAC request, and wait for the elevated child. Required capability
evidence is Administrator role `true`, `High Mandatory Level`, `fltmc` exit 0,
and present WPR/WPAExporter. Do not proceed from a failed capability gate.

M2P.5 may become **COMPLETED** only if both matrices have zero event/buffer
loss, ownership conflicts, unclosed timelines, and Unknown; Batch retains its
existing gates; and broad-door authoritative on-CPU p99 is independently
`<250 us` in both. Report `<225 us` engineering-headroom status separately.

Until those results exist, ready/blocked/preemption/migration and final slow
sample classifications are unverified post-change. No wall/cycle result in
this document substitutes for them.

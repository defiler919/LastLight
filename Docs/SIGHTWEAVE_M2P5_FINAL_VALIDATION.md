# SightWeave M2P.5 Final Validation

Status: **COMPLETED**

Last updated: 2026-08-26 (Asia/Shanghai)

## Authority and scope

- Branch: `codex/m2p5-sightweave-vision-solve-tail-closure`
- Immutable baseline: `c3a3323edadb648058fd33c4c1e57806eeac8536`
- Exact-result production checkpoint: `9a2daa0`
- Final warmed-allocation test checkpoint: `5855af9dee04e1fd686694c631ebc8e10bbe0c20`
- Final elevated-wrapper reliability checkpoint: `23cabf30548938ea9adc1297aeedb4e00f9496cf`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Local `Darkwell.uproject` `EngineAssociation` difference remained unstaged and
  was never restored, overwritten, staged, or committed.
- No M3, DARKWELL gameplay, GPU mask, post-process, memory layer, or map change
  was made. No merge, rebase, force-push, affinity, priority, Defender, or
  security-service change was made.

M2P.5 is complete because two independent final ten-process administrator
ContextSwitch/CSwitch matrices both satisfy the unchanged Batch512 gates and
report broad dynamic-door authoritative on-CPU p99 below the hard `250 us`
contract. Both broad results also satisfy the `225 us` engineering target and
the `200 us` ideal target. Every authoritative trace has zero event/buffer loss,
valid PID/TID/QPC ownership, a fully closed marker timeline, and zero Unknown.

Wall time and raw cycle counts in the retained ordinary tests are supporting
diagnostics only. They are not used as intrinsic CPU microseconds.

## Pre-change authority and authorized changes

The fail-closed Phase 1 capture is preserved at:

`D:\UE_projects\LastLight\Saved\SightWeaveM2P5\VisionTailAttribution\preproduction-detailed-formal-20260826`

It contains 10 independent PIDs, 1,010 broad samples, 65,650 source/substage
rows, zero loss/conflict/unclosed/Unknown, and broad on-CPU
`154.3/272.5/305.3/388.8 us` at p50/p95/p99/max. Classification was 872
Within, 129 Plugin CPU, and 9 Scheduler. Exact source evidence authorized two
initial changes:

1. validate the old state with the previous Prepared cache while solving the
   new state with the exact target Prepared cache;
2. skip angular active-set advancement for reused rays while advancing rebuilt
   rays directly to their ordered angle.

The first post-change ten-process matrices remained above contract. A clean
detailed administrator ETW at
`Saved\SightWeaveM2P5\VisionTailAttribution\postchange-detailed-formal-20260826-1215`
then proved that every exact `vision_solve` stage was broadly amplified rather
than exposing one correctable microstage. It retained 10 PIDs, 1,010 broad
samples, 65,650 detail rows, zero loss/conflict/unclosed/Unknown, broad on-CPU
`150.4/272.9/309.8/377.7 us`, and classification 135 Plugin / 8 Scheduler.

That authority opened one further bounded production change: exact-result
memoization in the already bounded Prepared Event Index.

## Final production implementation

`FSightWeaveOptimizedSolveCache` now retains an exact successful vision result
under a complete fail-closed key containing origin XYZ, raw forward vector,
shape, range, half-angle, near-awareness, floor, height band, every tolerance,
and the exact prepared segment count and keys. It retains vertices, candidate
angles/distances/boundary points, candidate segment count, and ray count.

`FSightWeavePreparedEventIndex`:

- reuses a result only when the complete key and result cardinalities match;
- invalidates an old exact result before any normal cached solve mutation;
- stores only successful, internally consistent results;
- accounts result storage in live and hard-maximum bytes;
- prechecks capacity and records an exact-result capacity fallback rather than
  exceeding the bound;
- retains bounded resident unbound states so alternating broad-door states can
  hit exact results without weakening LRU eviction or hard caps;
- exposes exact hit/miss/store/capacity-fallback diagnostics;
- copies results into caller-owned snapshot arrays and does not publish shared
  mutable storage.

Vision rebuilds attempt exact reuse before the existing incremental/full solve
path and store only successful results. Illumination behavior is unchanged.
Shipping gains no Windows, ETW, test, or Editor dependency. Candidate precision,
stable-ID ties, inclusive boundaries, 2.5D floor/height semantics, Visible/IR
isolation, illumination/suppression order, snapshot immutability, revision
semantics, and the Shipping oracle are unchanged.

## Correctness, differential, and regression

The new `SightWeave.M2P5.VisionTail.PreparedExactResultKey` test proves bitwise
equality with a fresh Optimized solve and fail-closed misses for mutations of
origin X/Y/Z, forward, shape, range, half-angle, near-awareness, floor, height
min/max, every tolerance, segment endpoints/floor/height/stable ID, and segment
count. A non-semantic `bDynamic` change is explicitly allowed and still checked
for identical output.

Broad 4V/2L, shared-cache, single-segment, fixed-seed 24-state, held-snapshot,
and lifecycle tests distinguish cold incremental solves from exact-result hits
and continue comparing every state with a fresh full Optimized solve.

Final serial report root:

`Saved\AutomationReports\M2P5_PostExact_Final_20260826`

| Filter | Passed | Failed | Result |
|---|---:|---:|---|
| `SightWeave.M1` | 21 | 0 | clean |
| `SightWeave.M2` | 96 | 0 | clean |
| `SightWeave.M2P` | 40 | 0 | clean |
| `SightWeave.M2P1` | 7 | 0 | clean |
| `SightWeave.M2P2` | 11 | 0 | clean |
| `SightWeave.M2P3` | 4 | 0 | clean |
| `SightWeave.M2P4` | 7 | 0 | clean |
| `SightWeave.M2P5` | 5 | 0 | clean |
| full `SightWeave` | 116 | 1 | Batch512 wall-only p99 failure |
| `Darkwell` | 24 | 0 | clean |

The retained first full run failed one Batch distribution with wall p99
`265.103 us`. A later full retry at
`Saved\AutomationReports\M2P5_PostExact_FullR1_20260826` is retained as 115/117:

- Batch distribution 5 wall p99 `247.501 us` (`<=200 us` wall assertion);
- Prepared4096 wall median `1011.100 us` (`<1 ms` wall assertion), with p95/p99/max
  `1605.697/1718.301/1790.099 us`.

These failures were not removed, split, or relaxed. Prepared4096 passed the
earlier final serial execution at `961.497/1225.103/1661.398/1739.301 us`.
Batch512 passes both final authoritative on-CPU matrices below. The full-run
wall failures therefore remain real environment-sensitive wall failures, not
evidence of an intrinsic Runtime tail.

Three independent ordinary processes at
`Saved\SightWeaveM2P5\OrdinaryAttribution\post-exact-result-ordinary-20260826`
produced 303 aggregate broad samples at wall
`61.3/112.8/144.2/178.6 us` and 3,030 aggregate Batch samples at wall
`91.9/138.4/181.4/642.1 us`. They are nonauthoritative wall/cycle evidence.

## Allocation proof

The first proof is intentionally preserved at:

`Saved\SightWeaveM2P1\AllocationProof\M2P5PostExactResultFinal_20260826`

It failed `motion_teleport` samples 0 and 1 with 5 allocations / 7,648 bytes
each because the capture had not warmed the newly alternating exact results.
This was a real test setup defect, not tool noise.

The test-only capture then added two unmarked alternating teleport warmups. The
corrected proof is:

`Saved\SightWeaveM2P1\AllocationProof\M2P5PostExactResultFinalR1_20260826`

- capture and analysis: 1/1 and 1/1;
- 69 rows, 23 workloads, three samples per workload;
- all 20 formal warmed workloads: zero allocation calls, reallocations, and
  bytes, including teleport and broad 4V/2L dynamic door;
- three excluded lifecycle controls remain explicit:
  `motion_range_change`, `motion_held_snapshot_transform`, and
  `motion_dynamic_door_plus_motion`;
- CSV SHA-256:
  `F29A0C43CBF6A943DD9F8E33D44CD6B653423775433912D16938A37E7ADAD30B`.

## Long soaks

Every run used 2,400 warmup frames, 36,000 measured frames, 600 simulated
seconds, and unchanged priority/affinity. Slow and failed runs remain present.

| Mode | Evidence root | wall p50/p95/p99/p99.9/max (us) | Classification | correctness / capacity / Unknown | CSV SHA-256 |
|---|---|---:|---|---:|---|
| NullRHI retained | `m2p5-post-exact-result-nullrhi-36000-20260826` | 87.9/218.6/283.2/494.5/1275.9 | 35,997 Within, 2 migration, 1 Unknown | 0/0/1 | `C4288D0F926B6F4E6645253D8E5A479D315DE6467C5D0C6C352F3F21A5E63FDC` |
| NullRHI retained | `m2p5-post-exact-result-nullrhi-36000-r1-20260826` | 89.7/224.3/292.2/529.7/1530.3 | 35,996 Within, 3 migration, 1 Unknown | 0/0/1 | `F9BF858291DCB8BBD935EC3807F5FBBBEEC3A7AC6C4BEA2F2D97D8A1D8019173` |
| NullRHI final | `m2p5-post-exact-result-nullrhi-36000-r2-20260826` | 92.4/241.8/322.3/519.8/943.2 | 36,000 Within | 0/0/0 | `379DDC670B9859453532DBD99B974085E356605519BBEB5D6F2B5CBB82DBEA97` |
| D3D12 retained | `m2p5-post-exact-result-d3d12-36000-20260826` | 101.2/268.9/370.6/574.6/2182.8 | 35,995 Within, 1 migration, 4 Unknown | 0/0/4 | `52CAC00C3109CE8579E32D85D5961CD84889DEF2EECD98705D42C38925098244` |
| D3D12 final | `m2p5-post-exact-result-d3d12-36000-r1-20260826` | 102.0/258.1/347.5/543.9/1507.5 | 35,999 Within, 1 migration | 0/0/0 | `829C22C1930F1B48094D92F8E437078D117341D26448DF7D32F476EC0A9985D3` |

The retained Unknown rows are not guessed from overlapping wall/cycle fields.
The final D3D12 run selected D3D12 SM6, adapter 0, RTX 4060, Studio 610.88 and
had a clean NVIDIA pre/post gate.

## Build, package, Shipping, and Lab

- Final repository `DarkwellEditor Win64 Development`: `Result: Succeeded`,
  target up to date.
- Fresh BuildPlugin package:
  `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM2P5PostExact-20260826-1340`;
  UAT `BUILD SUCCESSFUL`, exit 0.
- Source-only clean host:
  `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM2P5CleanHost-20260826-1341`.
- Clean-host UnrealEditor Development, UnrealGame Development, and UnrealGame
  Shipping all returned `Result: Succeeded`.
- Runtime dependencies are exactly `Core`, `CoreUObject`, `Engine`, and
  `DeveloperSettings`.
- Game Development and Shipping contain only `SightWeaveRuntime`; no Tests or
  Editor target module exists.
- Runtime source has no Darkwell/UnrealEd/Editor/Tests/Automation/ETW reference;
  COFF `/symbols` finds none in Shipping objects.
- `dumpbin /dependents` lists the four expected UE DLLs, KERNEL32, and VC/CRT
  libraries only.
- UAT's untracked empty `Plugins/SightWeave/Config/FilterPlugin.ini` template
  was inspected and removed.
- Final Lab NullRHI and D3D12/SM6 smokes exited 0, loaded
  `/SightWeave/Maps/L_SightWeave_Lab`, and reported `status=0`, authoritative,
  live, vision, bypass, snapshot 58, and one vision source. D3D12 selected
  adapter 0, RTX 4060, driver 610.88.

## Elevated ETW execution history

All failed orchestration artifacts remain present and are not performance
failures:

- one UAC was cancelled before any child or artifact existed;
- `Final\post-exact-result-final-20260826-1308` created an empty root because
  Windows PowerShell 5.1 does not support the wrapper's `utf8NoBOM` encoding;
- `...-r1` passed high-integrity capability and calibration, then its visible
  console entered QuickEdit selection mode during Matrix A run 3 and paused
  WPR stop. The stuck Event/System collectors were preserved at 199,229,440
  and 301,989,888 bytes with SHA-256
  `29D6B99953ED1756AF438AFA7F90E4FEE98E9896C6A84DDAAE1C1B7C0CAA838B`
  and `977281E667B071D1B17DCACC4F0EE5C8B6C3BB08109C7753BDABCF83FC8A4C5B`;
- `...-r2` and `...-r3` fail-closed before calibration because the interrupted
  WPR profile was still internally active; r3 preserved and stopped the exact
  system collector;
- wrapper commit `23cabf3` disables QuickEdit for only the current elevated
  console, verifies it is disabled, and records before/after modes. No UAC was
  hidden or automated;
- the successful r4 administrator child first ran elevated `wpr -cancel`
  (exit 0), verified `WPR is not recording`, and then completed calibration and
  both matrices in the same high-integrity process.

Successful orchestration root:

`D:\UE_projects\LastLight\Saved\SightWeaveM2P5\Final\post-exact-result-final-20260826-1308-r4`

Capability proves Administrator `true`, High Mandatory Level, `fltmc` exit 0,
WPR/WPAExporter present, and QuickEdit mode `503 -> 439`. Calibration root:

`Saved\SightWeaveM2P5\EtwCalibration\post-exact-result-final-20260826-1308-r4-calibration`

Calibration has 188/188 closed markers, QPC authority, event/buffer loss 0/0,
Unknown 0, and stage-probe on-CPU `2.6/4.2/4.4/4.4 us` at p50/p95/p99/max.
Compute, memory, sleep, loaded yield, scheduler, and migration assertions all
passed.

## Final authoritative matrices

| Matrix / workload | Samples | on-CPU p50/p95/p99/max (us) | ready p99/max (us) | blocked p99/max (us) | context switches / preemptions / migrations | Classification | Verdict |
|---|---:|---:|---:|---:|---:|---|---|
| A Batch512 | 10,100 | 95.9/150.2/189.0/437.3 | 29.4/406.3 | 0/3.8 | 434/433/338 | 9,953 Within, 67 Plugin, 80 Scheduler | PASS |
| A broad door | 1,010 | 65.5/127.6/169.6/245.4 | 10.3/161.5 | 0/0 | 30/30/22 | 1,007 Within, 0 Plugin, 3 Scheduler | PASS `<200` |
| B Batch512 | 10,100 | 95.6/150.0/183.3/476.2 | 20.4/450.9 | 0/2.6 | 362/360/286 | 9,979 Within, 39 Plugin, 82 Scheduler | PASS |
| B broad door | 1,010 | 65.8/128.7/170.9/247.7 | 4.4/76.2 | 0/0 | 24/24/20 | 1,007 Within, 0 Plugin, 3 Scheduler | PASS `<200` |

Each matrix has 10 independent PIDs and 105,120 marker rows. Every one of the
20 run manifests closes 10,512/10,512 markers, records event/buffer loss 0/0,
and passes PID/TID/QPC lifecycle ownership without conflict. ETW Unknown is 0
in both aggregate summaries. Raw ordinary-capture Unknown classifications are
retained but are superseded only for authority by the successful ETW
correlation, not by wall/cycle inference.

The broad `vision_solve` stage itself records 4,040 invocations per matrix and
on-CPU p50/p95/p99/max of `3.3/7.4/12.0/41.9 us` in A and
`3.3/7.6/11.8/30.9 us` in B.

Summary SHA-256:

- final capability: `C5813772779725293529E6D882271B7DC8D3027B20DA8D22656C957A6663BE06`;
- final completion: `97733D61D4A535EDD3D2827C7E8F96BC95F99D73D3C5E7AB6F76C58F6C4DC84C`;
- calibration: `819BC2567D79E3C157E442869D909CA00025907E792163CCBB76F8E3C163AE5C`;
- Matrix A: `47AB49C9F4312B17F4C9B71E77E45395047A6C5EB353C3CF1AF07FC498728603`;
- Matrix B: `CBED03D66BD801761334136F178AB6A667F2DA106DA1369158237900E7972E68`.

## NVIDIA and severe-log audit

Final gate on boot `2026-08-26T08:33:41.5000000+08:00`:

- `NvContainerLocalSystem`: Running, Auto, PID 5584; created 08:33:52 and
  unchanged through all sampling;
- `nvidia-smi`: NVIDIA GeForce RTX 4060, Studio/KMD 610.88, WDDM;
- relevant Application 1000/1001, System 7023/7031, Display/nvlddmkm/TDR,
  Device Removed/DXGI/GPU crash, and new WER/LiveKernel directories: all 0.

The unified post-exact severe scan found zero ensures, assertions, fatals,
critical errors, unhandled exceptions, device removals, DXGI errors, or GPU
crashes. UE 5.8's 13 pre-discovery `LogAutomationTest: Error: Condition failed`
self-diagnostics are present in the full process. The only controller failures
are the two retained wall-only assertions described above. Build warnings are
the installed MSVC 14.51 versus preferred 14.50 warning and UE-header C4996
deprecations.

## Final verdict and remaining scope

M2P.5 is **COMPLETED**. There is no authoritative remaining fixable production
Plugin CPU tail, so no further Runtime change is justified. No M2P.5 contract
item remains unverified. Optional human visual/PIE inspection and all M3 work
remain outside this milestone and require a separate task. The computer was
left on for user inspection.

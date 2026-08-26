# SightWeave M2P.5 Handoff

Status: **COMPLETED**

Last updated: 2026-08-26 (Asia/Shanghai)

## Objective and result

M2P.5 closes the intrinsic CPU tail of the unchanged authoritative broad
dynamic-door workload. The hard gate is administrator ContextSwitch/CSwitch
ETW on-CPU p99 `<250 us`; the engineering target is `<225 us` and the ideal
target is `<200 us`.

Both final independent ten-process matrices pass all three thresholds:

| Matrix | broad on-CPU p50/p95/p99/max (us) | Samples | Plugin / Scheduler / Unknown | Result |
|---|---:|---:|---:|---|
| A | 65.5/127.6/169.6/245.4 | 1,010 | 0/3/0 | PASS `<200` |
| B | 65.8/128.7/170.9/247.7 | 1,010 | 0/3/0 | PASS `<200` |

Batch512 also passes unchanged gates in both matrices at on-CPU p99
`189.0 us` and `183.3 us`. Each matrix contains 10 independent PIDs, 105,120
markers, zero event/buffer loss, valid PID/TID/QPC ownership, fully closed
timelines, and zero ETW Unknown. M2P.5 is therefore **COMPLETED**.

## Branch, baseline, and checkpoints

- Working branch: `codex/m2p5-sightweave-vision-solve-tail-closure`
- Baseline branch: `codex/m2p4-sightweave-etw-dynamic-sector`
- Immutable baseline: `c3a3323edadb648058fd33c4c1e57806eeac8536`
- Exact-result production commit: `9a2daa0`
- Warmed allocation-test commit: `5855af9dee04e1fd686694c631ebc8e10bbe0c20`
- QuickEdit-safe elevated wrapper commit:
  `23cabf30548938ea9adc1297aeedb4e00f9496cf`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`

The local-only `Darkwell.uproject` EngineAssociation difference remains
unstaged and was excluded from every commit. No main merge/rebase/force-push,
M3, DARKWELL gameplay, GPU mask, post-processing, memory-layer, or map change
was made.

## Authority chain

M2P.4 started with broad on-CPU p99 `300.2 us`; 42 of 43 Plugin CPU slow
samples were in `vision_solve`.

M2P.5 Phase 1 authority:

`Saved\SightWeaveM2P5\VisionTailAttribution\preproduction-detailed-formal-20260826`

It recorded broad on-CPU `154.3/272.5/305.3/388.8 us`, 10 PIDs, 1,010 samples,
65,650 source/stage rows, zero loss/conflict/unclosed/Unknown, and identified:

1. alternating exact Prepared states rejected as `prepared_index_replaced`;
2. active-set advancement performed for reused rays.

The authorized Prepared-cache and reused-ray changes were correct but did not
close the aggregate tail. A second detailed administrator trace at
`Saved\SightWeaveM2P5\VisionTailAttribution\postchange-detailed-formal-20260826-1215`
recorded broad on-CPU p99 `309.8 us` and proved broad amplification across every
exact solve stage rather than one fixable microstage.

This opened the final bounded optimization: exact successful vision-result
memoization inside the already capped Prepared Event Index. The full exact key
includes source geometry/profile/tolerance and exact prepared segment identity;
reuse and storage fail closed; result bytes count against hard memory bounds;
bounded resident unbound states support alternating broad-door states; caller
snapshots own their arrays. Illumination and Shipping dependencies are
unchanged.

## Final ETW evidence

Successful high-integrity root:

`D:\UE_projects\LastLight\Saved\SightWeaveM2P5\Final\post-exact-result-final-20260826-1308-r4`

It proves Administrator `true`, High Mandatory Level, `fltmc` exit 0, WPR and
WPAExporter present, and current-console QuickEdit mode `503 -> 439`.

- calibration:
  `Saved\SightWeaveM2P5\EtwCalibration\post-exact-result-final-20260826-1308-r4-calibration`
- Matrix A:
  `Saved\SightWeaveM2P5\EtwAttribution\post-exact-result-final-20260826-1308-r4-matrix-a`
- Matrix B:
  `Saved\SightWeaveM2P5\EtwAttribution\post-exact-result-final-20260826-1308-r4-matrix-b`

Calibration has 188/188 closed markers, loss 0/0, Unknown 0, QPC authority,
and stage-probe on-CPU p99 `4.4 us`. The wrapper completed calibration and both
10-process matrices in one administrator child without affinity or priority
changes.

| Matrix / workload | on-CPU p50/p95/p99/max (us) | ready p99/max | blocked p99/max | CS / preemptions / migrations | Classification |
|---|---:|---:|---:|---:|---|
| A Batch512 | 95.9/150.2/189.0/437.3 | 29.4/406.3 | 0/3.8 | 434/433/338 | 9,953 Within, 67 Plugin, 80 Scheduler |
| A broad | 65.5/127.6/169.6/245.4 | 10.3/161.5 | 0/0 | 30/30/22 | 1,007 Within, 0 Plugin, 3 Scheduler |
| B Batch512 | 95.6/150.0/183.3/476.2 | 20.4/450.9 | 0/2.6 | 362/360/286 | 9,979 Within, 39 Plugin, 82 Scheduler |
| B broad | 65.8/128.7/170.9/247.7 | 4.4/76.2 | 0/0 | 24/24/20 | 1,007 Within, 0 Plugin, 3 Scheduler |

Raw ordinary wall/cycle classifications are retained but do not replace ETW
authority. No diagnostic overhead was subtracted.

## Retained orchestration failures

The initial UAC cancellation created no child or performance artifact. Later
failed roots remain present:

- the base label used Windows PowerShell 5.1 and failed before ETW because
  `utf8NoBOM` requires PowerShell Core;
- r1 passed capability/calibration but the visible console entered QuickEdit
  selection mode during Matrix A run 3 and paused WPR stop; both stalled ETLs
  were preserved and hashed;
- r2/r3 correctly failed WPR start while the interrupted elevated profile
  remained internally active;
- r4 ran elevated `wpr -cancel` exit 0, verified not recording, disabled and
  verified QuickEdit for its own console, and completed exit 0.

These are orchestration failures, not performance samples. No UAC was hidden,
automated, or bypassed.

## Validation closure

- Exact-key/fallback/differential/lifecycle coverage: final M2P.5 5/5,
  M2P4 7/7, M2P2 11/11, M2 96/96, Darkwell 24/24.
- Full post-exact serial root retains 116/117 due one Batch wall-only failure.
- A later full retry retains 115/117 due Batch wall p99 `247.501 us` and
  Prepared4096 wall median `1011.100 us`. These were not relaxed or deleted;
  the final ETW matrices adjudicate Batch intrinsic CPU separately and pass.
- Corrected allocation proof:
  `Saved\SightWeaveM2P1\AllocationProof\M2P5PostExactResultFinalR1_20260826`;
  all 20 warmed formal workloads are zero allocation/reallocation/bytes; CSV
  SHA-256 `F29A0C43CBF6A943DD9F8E33D44CD6B653423775433912D16938A37E7ADAD30B`.
- The first allocation failure without teleport warmup remains preserved.
- Final NullRHI 36k:
  `m2p5-post-exact-result-nullrhi-36000-r2-20260826`, 36,000 Within,
  correctness/capacity/Unknown `0/0/0`.
- Final D3D12 36k:
  `m2p5-post-exact-result-d3d12-36000-r1-20260826`, 35,999 Within + 1
  migration, correctness/capacity/Unknown `0/0/0`.
- Two earlier NullRHI Unknown runs and the first D3D12 4-Unknown run remain
  preserved and unguessed.
- Fresh BuildPlugin:
  `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM2P5PostExact-20260826-1340`,
  UAT `BUILD SUCCESSFUL`.
- Source-only clean host:
  `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM2P5CleanHost-20260826-1341`;
  Editor Development, Game Development, and Game Shipping all succeeded.
- Runtime dependencies remain exactly Core/CoreUObject/Engine/DeveloperSettings;
  Game targets contain only Runtime; source, COFF symbols, and import table
  contain no Darkwell/Editor/Tests/Automation/ETW dependency.
- Final Editor build succeeded. NullRHI and D3D12 Lab smokes exited 0 and
  reported authoritative/live/vision/bypass with snapshot 58.
- Final severe scan: zero ensure/assert/fatal/critical/unhandled/device-removed/
  DXGI/GPU-crash hits.
- NVIDIA final gate: NvContainerLocalSystem Running/Auto, PID 5584 unchanged;
  RTX 4060 Studio 610.88; relevant service/application/TDR/DXGI/WER counts 0.

Full paths, metrics, hashes, failed-run detail, and warning classification are
in `Docs/SIGHTWEAVE_M2P5_FINAL_VALIDATION.md`.

## Handoff state and next work

There is no remaining M2P.5 Runtime action and no unverified M2P.5 contract
item. Do not reopen Runtime optimization from ordinary wall/cycle noise unless
a new fail-closed administrator ETW proves a regression. M3 and optional human
visual/PIE inspection remain separate, explicitly out-of-scope work.

For a future task, first fetch the branch, verify HEAD/upstream/remote and LFS,
and preserve the local `Darkwell.uproject` EngineAssociation difference. Do not
merge main, rebase, or force-push as part of this completed handoff.

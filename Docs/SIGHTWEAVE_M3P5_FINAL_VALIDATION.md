# SightWeave M3.5 final validation

## 1. Status

**IN_PROGRESS**

CPU authority, modifiers, GPU mirrors, static-environment compositing, focused safety automation, Lab automation, and precision/performance selection are implemented and measured. Packaging, clean-host, full regression closure, and user-operated PIE remain open, so the milestone is not complete.

## 2. Branch, baseline, and validation machine

- Branch: `codex/m3p5-sightweave-static-environment-memory`
- Frozen M3.4 baseline: `22f55b1e744cea37ad5d3c7beb618be0509fbf99`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- GPU: NVIDIA GeForce RTX 4060, driver 610.88, 8188 MiB

## 3. Initial repository evidence

- local HEAD, upstream, and remote M3.4 repair SHA matched the frozen baseline;
- `git diff --check` passed;
- `git lfs status` reported no pending objects;
- `git lfs fsck` passed;
- `Config/DefaultGame.ini` had no local override;
- the only retained local difference is the approved `Darkwell.uproject` EngineAssociation GUID and it must remain uncommitted.

## 4. Evidence policy

This document will record exact commands, counts, timings, bytes, failures, warnings, screenshot paths, agent image inspection, and user-operated PIE status as evidence is produced. Cold and warmed data, intrinsic timings and wall time, and results from different GPUs will remain separate. The two frozen M2P2 wall-time failures will be retained verbatim.

## 5. Completion gate

The milestone can become `COMPLETED` only when the CPU authority, modifiers, GPU mirror, static eligibility, three-state composite, precision comparison, lifecycle/isolation, D3D12/SM6 and NullRHI automation, packaging/clean-host/Shipping isolation, performance/memory budgets, Lab evidence, user-operated PIE check, and final remote closure all pass. Otherwise the final state is `PARTIAL` or `BLOCKED` with exact missing evidence.

## 6. Implemented checkpoints

- packed CPU HardMemory authority and immutable revisioned packets;
- ClearMemory, BlockMemoryWrites, and SuppressMemoryPresentation;
- persistent sparse R8 Memory mirror with fail-black invalidation;
- explicit authored static-environment eligibility and neutral attribute mirror;
- exact HardLive > remembered static environment > black composite;
- CPU/GPU parity, scope collision, lifecycle, invalidation, leakage, and Width=0 compatibility coverage;
- isolated M3.5 Lab area and automated PIE capture.

## 7. Precision and performance evidence

The frozen four-tier experiment selected Coarse 25 cm/texel. Final combined precision automation was 4/4 with zero warnings; only Coarse passed every CPU, GT, RT, GPU, and memory selection gate. The dedicated resident/dirty scale was 3/3 with zero warnings. The frozen Width=50 live presentation matrix was 9/9 with zero warnings and met its 1080p/1440p and 32-source pressure budgets. Exact cold/warmed values and report paths are in `SIGHTWEAVE_M3P5_PERFORMANCE.md`.

## 8. Lab evidence

The selected Coarse default rerun at 1920x1080 completed `SightWeave.M3P5.Visual.LabCapture` 1/1 with zero report warnings. The authority gate logged:

```text
remembered:1 live:1 clear:0 block:0 suppressed:1/1 unknown:0
```

The agent directly inspected `Saved/Screenshots/M3P5_PIE_Overview.png`: live white, remembered neutral structure, and strict-black unknown/suppressed areas were distinct; no dynamic-object state or current-lighting leakage was visible. This is automated capture plus agent inspection, not user-operated PIE.

## 9. Preserved engine-startup diagnostics

D3D12 commandlet logs contain 13 pre-test `LogAutomationTest: Error: Condition failed` startup lines and engine navigation/scalability warnings. Exported focused reports contain zero test errors/warnings. These lines predate individual test execution and are retained for final severe-log classification rather than hidden.

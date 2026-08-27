# SightWeave M3.5 final validation

## 1. Status

**IN_PROGRESS**

No M3.5 implementation, precision-selection, performance, packaging, screenshot, or user-operated PIE claim is recorded at this initial checkpoint.

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

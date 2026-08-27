# SightWeave M4P1 subject policy and Last-Seen proxy foundation handoff

Status: **IN_PROGRESS**

Branch: `codex/m4p1-sightweave-subject-policy-last-seen`

Frozen M3.5 baseline: `941dcb75fe9fcda05bcb972a4d2d9651c6d521be`

Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`

Primary RHI: D3D12 / SM6

## Scope

M4P1 establishes the generic SightWeave subject-memory policy contract, immutable revisioned CPU Last-Seen descriptors, falling-edge capture, conservative render-only opaque static-mesh proxies, a versioned host-provider boundary for complex subjects, and fail-closed lifecycle isolation across world, knowledge owner, floor, canonical profile, precision, revision, and instance generation.

Supported policy values are `NeverRemember`, `VisibleOnly`, `StaticEnvironment`, `LastSeenSnapshot`, and `Custom`. M3.5 remains the only `StaticEnvironment` memory implementation. This milestone does not integrate DARKWELL gameplay, modify `/Game/Maps/L_Prototype`, add persistence/save migration, capture scene buffers, or support skeletal/VFX/light/audio snapshots.

## Baseline audit

The task started from the exact M3.5 baseline. Local HEAD, upstream, and `origin/codex/m3p5-sightweave-static-environment-memory` all resolved to `941dcb75fe9fcda05bcb972a4d2d9651c6d521be`. The working tree was clean, `Darkwell.uproject` had no local diff, `git diff --check` passed, `git lfs status` was empty, and `git lfs fsck` passed. `D:\UE_5.8\Engine\Build\Build.version` reported 5.8.1, while project configuration retained D3D12 and `PCD3D_SM6`.

## Current checkpoint

The initial documentation-only checkpoint records the authorized scope before product-code work. The implementation contract is frozen in `Docs/SIGHTWEAVE_M4P1_SUBJECT_MEMORY_CONTRACT.md`; it defines exact identity, immutable descriptors, falling-edge capture, deterministic presentation priority, render-only proxy isolation, and versioned Custom-provider fail-closed behavior. No M4P1 implementation or validation result is claimed yet.

## Resume

```powershell
git fetch origin
git switch codex/m4p1-sightweave-subject-policy-last-seen
git pull --ff-only
```

# SightWeave M4P1 final validation

## 1. Status

**IN_PROGRESS**

This file begins the M4P1 evidence record. No implementation, build, automation, render, Lab, packaging, or manual PIE result is represented as complete at this checkpoint.

## 2. Identity and frozen baseline

- Branch: `codex/m4p1-sightweave-subject-policy-last-seen`
- Frozen M3.5 baseline: `941dcb75fe9fcda05bcb972a4d2d9651c6d521be`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Primary rendering configuration: D3D12 / SM6

The pre-branch baseline audit confirmed identical local, upstream, and remote SHAs; a clean working tree; no `Darkwell.uproject` difference; a clean Git diff; and healthy Git LFS status and object verification. M3.5 handoff and validation documentation both reported `COMPLETED`.

## 3. Authorized M4P1 boundary

M4P1 covers only subject-memory policy, immutable CPU Last-Seen descriptors, legal live-to-non-live capture, non-live remembered render-only proxies, reacquire/clear/suppression/unknown lifecycle, a basic opaque static-mesh descriptor, a versioned custom host-provider interface, plugin Lab fixtures, targeted automation, and strict world/scope/revision/generation isolation.

Deferred work includes save persistence and migration, cross-level persistence, reveal overrides, enemies and skeletal snapshots, animation, SceneCapture or texture snapshots, live lighting/shadow/VFX memory, DARKWELL adapters and production content, `/Game/Maps/L_Prototype`, final art tuning, non-D3D12 RHIs, and Fab packaging.

## 4. Planned evidence

Reliable checkpoints will record exact builds, targeted test counts, NullRHI and D3D12/SM6 results, M3.4/M3.5 regressions, severe-log scans, Lab evidence, preserved warnings/failures, repository/LFS closure, and any intentionally unverified work. Until those gates run, status remains `IN_PROGRESS`.

## 5. Frozen contract

`Docs/SIGHTWEAVE_M4P1_SUBJECT_MEMORY_CONTRACT.md` records the M4P1 policy matrix, immutable descriptor fields, exact non-hash scope equality, legal falling-edge capture, deterministic state priority, clear/suppression/unknown behavior, render-only proxy constraints, and versioned Custom-provider fail-closed boundary. This is design evidence only; implementation results will be added after build and targeted automation.

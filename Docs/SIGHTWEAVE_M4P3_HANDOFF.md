# SightWeave M4P3 handoff

## Current state

The branch contains a complete M4P3 persistence/atomic-restore implementation. Focused correctness, rollback, determinism, actual D3D12 rebuild/readback, BuildPlugin, clean-host builds, and Shipping boundary scans pass. Overall validation is `PARTIAL` only for the two open gates in the final validation document.

## Public integration path

Use `FSightWeavePersistence::Capture` and `FSightWeavePersistence::Restore` with explicit `FSightWeavePersistenceScopeBinding` entries and a call-local `FSightWeavePersistenceProviderRegistry`. The blob is an opaque bounded V1 value; callers own slot/file I/O. Do not persist Runtime handles or attempt to interpret Render/GPU state.

World integration should go through `USightWeaveWorldSubsystem` so lifetime/revision validation and full derived publication are preserved. M4P3 deliberately does not bind DARKWELL SaveGame slots.

## Invariants for follow-up work

- keep canonical ordering and BLAKE3 coverage byte-for-byte deterministic;
- retain validate-before-commit atomicity and no-publication rollback;
- keep missing-provider fallback scoped and fail black;
- never serialize UObject, World, Actor, Render, RDG, RHI, or GPU resources;
- do not weaken the 64 MiB limits or old performance thresholds without explicit approval;
- keep Runtime free of Editor/Tests dependencies;
- use Unreal APIs for all `.uasset`/`.umap` operations.

## Recommended closure work

1. Re-run the frozen M2P2 batch gate on an accepted performance host or diagnose the host-level 1–6% miss without changing the 150 us threshold.
2. Add Development-only measurement coverage for small, typical, and maximum fixtures, reporting p50/p95/p99/max separately for capture, prepare, validate, commit, and derived publication.
3. Add direct before/after counters for provider registrations, UObject population, Render resources, and GPU resident resources across 100 no-change/success/failure cycles.
4. Re-run the exact full NullRHI and forced-resolution D3D12 suites, then change the verdict to `COMPLETED` only if every required gate passes.

## Retained external artifacts

- BuildPlugin: `C:\Users\defiler\AppData\Local\Temp\SightWeaveM4P3_BuildPlugin_Closure_ac893da_20260828_2317`
- Clean host: `C:\Users\defiler\AppData\Local\Temp\SightWeaveM4P3_CleanHost_Closure_ac893da_20260828_2324_Retry1`
- Failed V6 clean host: `C:\Users\defiler\AppData\Local\Temp\SightWeaveM4P3_CleanHost_Closure_ac893da_20260828_2322`
- Retained accidental host copy: `C:\Users\defiler\AppData\Local\Temp\SightWeaveM4P3_AccidentalHost_Retained_20260828_2329`

These paths are local evidence, not repository inputs.

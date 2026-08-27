# SightWeave M4P1 final validation

## 1. Status

**PARTIAL**

The code foundation and automated P0/P1 gates completed successfully. `COMPLETED` is not claimed because M4P1-specific visual Lab capture and user-operated PIE inspection were not performed, and the extended package/clean-host matrix was intentionally skipped under the quota-first protocol.

## 2. Identity and baseline

- Branch: `codex/m4p1-sightweave-subject-policy-last-seen`
- Frozen M3.5 baseline: `941dcb75fe9fcda05bcb972a4d2d9651c6d521be`
- Last implementation checkpoint: `8ecfbbd4399901632cfd12862c08ac277f6324ba`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- RHI: NullRHI plus NVIDIA D3D12 / SM6

The baseline audit established identical local/upstream/remote M3.5 SHAs, a clean tree, no `Darkwell.uproject` difference, clean `git diff --check`, healthy LFS status/fsck, M3.5 `COMPLETED`, and unchanged D3D12/SM6 configuration before the M4P1 branch was created.

## 3. Delivered source

- `SightWeaveSubjectMemory.h/.cpp`: policy enum, generation-safe handle and identity, registration, basic static-mesh candidate, immutable descriptor, observation/result contracts, deterministic CPU authority, exact-scope clear, and fail-closed presentation evaluation.
- `ISightWeaveSubjectSnapshotProvider`: synchronous non-owning host boundary invoked only on a Custom falling edge, with exact provider name/version matching and distinct missing/mismatch/reject/invalid failures.
- `SightWeaveLastSeenProxyComponent.h/.cpp`: transient render-only opaque static-mesh proxy and presentation bridge.
- `SightWeaveM4P1SubjectPolicyTests.cpp`, `SightWeaveM4P1ProxyTests.cpp`, and `SightWeaveM4P1TransitionFixtureTests.cpp`: policy, transition, isolation, provider, proxy, teardown, and generated Lab-fixture automation.
- `SIGHTWEAVE_M4P1_SUBJECT_MEMORY_CONTRACT.md`: frozen CPU/render authority and lifecycle contract.

No product asset, map, `Darkwell.uproject`, Shader, DARKWELL gameplay source, or generated directory was modified or committed.

## 4. Five-policy behavior

| Policy | Live | Non-live | Snapshot/proxy |
| --- | --- | --- | --- |
| `NeverRemember` | live presentation | hidden | never created |
| `VisibleOnly` | live presentation | hidden | never created |
| `StaticEnvironment` | M3.5 delegated | M3.5 delegated | no M4P1 duplicate |
| `LastSeenSnapshot` | live, proxy hidden | proxy only when remembered/exact/unsuppressed | one descriptor per legal falling edge |
| `Custom` | live | fail black without accepted exact provider | accepted stable opaque descriptor only |

## 5. Snapshot and transition rules

The descriptor freezes stable identity, instance generation, world lifetime identity/generation, owner, floor, origin/plane, precision, complete canonical profiles, policy, snapshot/eligibility/source-live revisions, transform/bounds, mesh/material asset paths, visual variant, capture reason, transition identity, and validity flags. Equality never relies on hash alone.

A snapshot is created only when the previous accepted observation was HardLive, the new accepted observation is non-live, the previous live state was eligible for memory writes, the transition identity is new, and the built-in candidate or Custom provider result is fully valid. Remaining live and remaining non-live do not increment the revision. Reacquire selects live immediately; a later falling edge advances the snapshot revision exactly once.

## 6. Presentation lifecycle and fail-closed priority

Invalid handle/identity/scope/world/generation/revision, unknown memory, missing descriptor, failed provider, invalid resource, or presentation failure hides both real presentation and proxy. HardLive selects the real presentation and clears the proxy. Non-live remembered exact state selects the proxy. Suppression/block hides and clears the instantiated proxy but retains the CPU descriptor; exact state can recreate it after suppression lifts. Clear removes the CPU descriptor and leaves strict black until a future legal observation and falling edge.

The proxy constructor, registration, presentation, hide, and teardown enforce no collision/overlap, physics, navigation, tick, decals, dynamic shadow, or dynamic indirect/distance-field lighting. A `UStaticMeshComponent` introduces no audio, VFX, light, AI, interaction, damage, targeting, or persistence authority. Invalid asset/material or revision input clears any stale mesh before returning false.

## 7. Automated tests

Final M4P1 test count is 8:

1. `SightWeave.M4P1.Lab.GeneratedFixture.FullTransition`
2. `SightWeave.M4P1.Proxy.RenderOnlyLifecycle`
3. `SightWeave.M4P1.Subject.Custom.ProviderRoute`
4. `SightWeave.M4P1.Subject.Isolation.WorldScopeGenerationRevision`
5. `SightWeave.M4P1.Subject.Policy.Matrix`
6. `SightWeave.M4P1.Subject.Snapshot.ClearSuppressUnknown`
7. `SightWeave.M4P1.Subject.Snapshot.FallingEdgeReacquireRevision`
8. `SightWeave.M4P1.Subject.Unsupported.FailClosed`

Coverage includes all five policies, falling-edge single capture, no non-live churn, immediate reacquire, second revision, clear, suppression retain/hide/restore, unknown black, stale revisions, exact world/owner/floor/precision/profile comparisons including forced hash collision, generation reuse, teardown/reset, unsupported complex input, provider failure modes, render-only invariants, and integrated live/proxy/black switching.

## 8. Exact gate results

| Gate | RHI | Report | Result |
| --- | --- | --- | ---: |
| Final Editor Development build | build | UBT local log | succeeded, 4 actions |
| M4P1 transition suite | NullRHI | `Saved/AutomationReports/M4P1_Final_NullRHI` | 8 succeeded, 0 warning, 0 failed |
| M4P1 transition suite | D3D12/SM6 | `Saved/AutomationReports/M4P1_Final_D3D12_Retry` | 8 succeeded, 0 warning, 0 failed |
| Frozen M3.4 presentation | D3D12/SM6 | `Saved/AutomationReports/M4P1_M3P4Presentation_D3D12` | 3 succeeded, 0 warning, 0 failed |
| Complete M3.5 prefix | D3D12/SM6 | `Saved/AutomationReports/M4P1_M3P5Regression_D3D12` | 26 succeeded, 0 warning, 0 failed |

The D3D12 log records `Using Highest Feature Level of D3D12: SM6`, shader model 6.7 support on the selected adapter, and creation of the D3D12 RHI at SM6.

## 9. Severe scans and preserved diagnostics

The final M4P1 NullRHI, M4P1 D3D12, M3.4 D3D12, and M3.5 D3D12 logs each produced zero matches for Shader compiler failure, ShaderCompileWorker failure, Renderer/RenderCore error, RDG error/warning, D3D12/RHI error, GPU crash/hang/device removal, fatal error, assertion, ensure, or failed automation result.

Preserved facts:

- M3.5 Lab startup emitted expected resource-prepublication `bindingFailure=13`, then recovered to `memoryReady=1 staticEnvironmentReady=1 memoryPresentationAvailable=1 memoryScopeMatchesBinding=1 memoryScopeMismatchMask=0x00 memoryProfiles=1 liveProfiles=1` and `submitted-feather bindingFailure=0`.
- UE startup retained missing optional profiler DLL notices and localization/network/telemetry-style diagnostics unrelated to SightWeave composite correctness.
- MSVC 14.51 is newer than UE 5.8's preferred 14.50; all final builds succeeded.
- The first proxy compilation failed on `TObjectPtr` test deduction and a shadowed member name. The first transition build exposed unity namespace leakage. Both were repaired without weakening tests, and final builds passed.
- The first push of `e0d21c9` failed with Git LFS locking endpoint `unexpected EOF`; unchanged ordinary retry succeeded.
- M3.5 Lab regression startup logged cleanup of default `L_Prototype` before loading `/SightWeave/Maps/L_SightWeave_Lab`. No PIE, save, asset operation, or source change targeted `L_Prototype`.

## 10. Lab and visual evidence

`SightWeave.M4P1.Lab.GeneratedFixture.FullTransition` is a generated transient fixture, not an authored map change. It validates live, captured proxy, suppression black, exact restore, reacquire, clear black, and render-only invariants. It passed under NullRHI and D3D12/SM6.

The M3.5 regression independently loaded only the allowed `/SightWeave/Maps/L_SightWeave_Lab` for PIE and passed its five-camera capture. Those screenshots prove the frozen M3.5 composite, not M4P1 proxy visuals, and remain uncommitted under `Saved`.

No M4P1 agent visual inspection or user manual PIE occurred. Page/tile seam, yaw 45, and viewport appearance of an actual Last-Seen proxy remain unverified.

## 11. Build, packaging, and shipping boundary

Repository-standard `Scripts/BuildEditor.ps1` passed after final code changes. BuildPlugin, clean-host builds, Game Development, Game Shipping, full SightWeave, DARKWELL suite, Shipping symbol scan, and expanded performance matrix were not run. No shader source changed, and no shader parameter binding error appeared in any final log.

## 12. Git/LFS and recovery

Every reliable implementation checkpoint through `8ecfbbd` reached the normal remote branch. No rescue branch was created or required. Generated reports, logs, screenshots, `Binaries`, `Intermediate`, and `Saved` are untracked/ignored and not committed. `Darkwell.uproject` remained unchanged.

Final closure must record the documentation commit SHA and confirm local HEAD, upstream, and remote equality, clean `git diff --check`, clean LFS status, and successful `git lfs fsck` after this document is committed and pushed.

## 13. Remaining work and resume command

Remaining M4P1 completion work is bounded to real visual Lab integration and clearly separated user PIE acceptance, followed by any desired P2 package/clean-host matrix. Formal gameplay integration, persistence, skeletal/VFX/light/audio capture, SceneCapture, reveal override, and production content remain deferred beyond M4P1.

```powershell
git fetch origin
git switch codex/m4p1-sightweave-subject-policy-last-seen
git pull --ff-only
& 'D:\UE_pro\Darkwell\Scripts\BuildEditor.ps1' -EngineRoot 'D:\UE_5.8'
```

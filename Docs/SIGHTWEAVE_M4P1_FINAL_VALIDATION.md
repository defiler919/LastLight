# SightWeave M4P1 visual closure validation

## 1. Status

**PARTIAL**

All scoped implementation, standard build, NullRHI, D3D12/SM6, real Lab screenshot, exact pixel, agent image inspection, lifecycle, M3.4/M3.5 regression, severe-log, and remote checkpoint gates passed. The sole remaining item is user-operated interactive PIE; this document does not label automated screenshot inspection as manual acceptance.

## 2. Identity and baseline

- Branch: `codex/m4p1-sightweave-subject-policy-last-seen`
- Continuation baseline: `d2b27136288ecf11d96b6e09d7a2b2fea6dbaa05`
- Frozen M3.5 baseline: `941dcb75fe9fcda05bcb972a4d2d9651c6d521be`
- Last source/test checkpoint before this documentation commit: `e22d0a7`
- Final SHA: authoritative post-push `git rev-parse HEAD` recorded in the final task report; a commit cannot embed its own SHA.
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Hardware/RHI: NVIDIA GeForce RTX 4060, NullRHI plus D3D12 / SM6

At continuation start local, upstream, and remote were all `d2b2713`. The only permitted local difference was the existing `Darkwell.uproject` EngineAssociation GUID. That file remained unstaged and uncommitted throughout; final Git/LFS closure is recorded after the documentation push.

## 3. Delivered source

- `SightWeaveSubjectMemory.h/.cpp`: policy enum, generation-safe handle and identity, registration, basic static-mesh candidate, immutable descriptor, observation/result contracts, deterministic CPU authority, exact-scope clear, and fail-closed presentation evaluation.
- `ISightWeaveSubjectSnapshotProvider`: synchronous non-owning host boundary invoked only on a Custom falling edge, with exact provider name/version matching and distinct missing/mismatch/reject/invalid failures.
- `SightWeaveLastSeenProxyComponent.h/.cpp`: transient opaque static-mesh proxy restricted to CustomDepth/stencil 246, with no SceneColor/main-pass/depth participation and fixed neutral intensity 0.46.
- `SightWeaveSingleTile.usf`, tile shader parameters, and sparse-atlas render state: HardLive remains first; a legal proxy is depth-occluded against SceneDepth and emitted as deterministic RGB 117 before the M3.5 remembered-environment fallback.
- `SightWeaveM4P1LabFixture.h/.cpp` and editor-module controls: runtime-generated real-PIE fixture, five cameras, six states, deterministic teardown, and explicit log binding.
- `SightWeaveM4P1SubjectPolicyTests.cpp`, `SightWeaveM4P1ProxyTests.cpp`, `SightWeaveM4P1TransitionFixtureTests.cpp`, and `SightWeaveM4P1VisualLabTests.cpp`: policy, transition, isolation, provider, proxy, lifecycle, mode, real screenshot, and exact pixel automation.
- `SIGHTWEAVE_M4P1_SUBJECT_MEMORY_CONTRACT.md`: frozen CPU/render authority and lifecycle contract.

No product asset, `.umap`, `Darkwell.uproject`, DARKWELL gameplay source, or generated directory was committed. The only shader change is the bounded Last-Seen presentation path; it never captures or stores Scene Color/current lighting.

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

## 7. Automated tests and lifecycle

Final M4P1 test inventory is 10; NullRHI executes the 9 non-visual tests and D3D12/SM6 executes all 10:

1. `SightWeave.M4P1.Lab.GeneratedFixture.FullTransition`
2. `SightWeave.M4P1.Proxy.RenderOnlyLifecycle`
3. `SightWeave.M4P1.Subject.Custom.ProviderRoute`
4. `SightWeave.M4P1.Subject.Isolation.WorldScopeGenerationRevision`
5. `SightWeave.M4P1.Subject.Policy.Matrix`
6. `SightWeave.M4P1.Subject.Snapshot.ClearSuppressUnknown`
7. `SightWeave.M4P1.Subject.Snapshot.FallingEdgeReacquireRevision`
8. `SightWeave.M4P1.Subject.Unsupported.FailClosed`
9. `SightWeave.M4P1.Lab.ModeContract`
10. `SightWeave.M4P1.Visual.LastSeenLab`

Coverage includes all five policies, falling-edge single capture, no non-live churn, immediate reacquire, second revision, clear/rebuild, suppression retain/hide/restore, unknown black, stale descriptor/presentation revisions, exact world/owner/floor/precision/profile comparisons including forced hash collision, generation reuse, teardown/reset, unsupported complex input, provider failure modes, render-only invariants, and integrated live/proxy/black switching. The visual test starts PIE, rebuilds the transient fixture across eight state/camera captures, stops PIE, and logs world cleanup; repeated dedicated/full runs established consistent restart behavior with no duplicate actors, components, delegate, ticker, or pending-render-command failure.

## 8. Exact gate results

| Gate | RHI | Report | Result |
| --- | --- | --- | ---: |
| Final Editor Development build | build | standard `Scripts/BuildEditor.ps1` | succeeded, 9 actions |
| M4P1 full | NullRHI | `Saved/AutomationReports/M4P1_Full_Null_ExactPixels` | 9 succeeded, 0 warning, 0 failed |
| M4P1 full | D3D12/SM6 | `Saved/AutomationReports/M4P1_Full_D3D12_ExactPixels` | 10 succeeded, 0 warning, 0 failed; 28.93 s |
| Dedicated visual/Camera | D3D12/SM6 | `Saved/AutomationReports/M4P1_Visual_D3D12_ExactPixels` | 1 succeeded, 0 warning, 0 failed; 28.73 s |
| M4P1 Lab prefix | NullRHI | `Saved/AutomationReports/M4P1_Lab_Final_NullRHI` | 2 succeeded, 0 warning, 0 failed |
| M4P1 Lab prefix | D3D12/SM6 | `Saved/AutomationReports/M4P1_Lab_Final_D3D12` | 2 succeeded, 0 warning, 0 failed |
| Frozen M3.4 presentation | D3D12/SM6 | `Saved/AutomationReports/M4P1_Visual_M3P4Presentation_D3D12` | 3 succeeded, 0 warning, 0 failed |
| Complete M3.5 prefix | D3D12/SM6 | `Saved/AutomationReports/M4P1_M3P5Regression_Final_D3D12` | 26 succeeded, 0 warning, 0 failed; 16.26 s |
| Repaired M3.5 packaging case | D3D12/SM6 | `Saved/AutomationReports/M4P1_M3P5PackagingFix_D3D12` | 1 succeeded, 0 warning, 0 failed |

The D3D12 log records `Using Highest Feature Level of D3D12: SM6`, shader model 6.7 support on the selected adapter, and creation of the D3D12 RHI at SM6.

## 9. Severe scans and preserved diagnostics

The seven final logs for M4P1 NullRHI, M4P1 D3D12, dedicated visual D3D12, M3.4 D3D12, M3.5 D3D12, Lab NullRHI, and Lab D3D12 each produced zero matches for Shader compiler/ShaderCompileWorker failure, Renderer/RenderCore error, RDG error/warning, D3D12/RHI error, GPU crash/hang/device removal, fatal error, assertion, ensure, or failed automation result.

Preserved facts:

- MSVC 14.51 is newer than UE 5.8's preferred 14.50; all final standard builds succeeded.
- A sandboxed build attempt failed before compile because UBT could not rotate `%LOCALAPPDATA%\UnrealBuildTool\Log.txt`. Process/log inspection identified the filesystem restriction; the independently launched standard build outside the sandbox succeeded.
- The first D3D visual report `M4P1_Visual_D3D12_First` failed only Camera 1 Reacquired because root-component installation reset the actor transform. Explicit post-registration placement fixed it; all later visual reports passed and the failure remains preserved.
- The first final M3.5 report `M4P1_Visual_M3P5Regression_D3D12` retained 25 successes and one packaging source-string failure. The old test forbade any render-state `LastSeen` token and matched obsolete shader syntax. Its authority exclusions were kept; a bounded assertion now proves the M4P1 stencil constants are presentation-only and no subject authority/descriptor enters RenderState. Isolated 1/1 and final 26/26 passed.
- A first non-escalated M3.5 launch exited before creating a report/log after platform validation. No Unreal/UBT process remained; the authorized retry created full evidence. This is retained as a host sandbox launch failure, not a test result.
- In the standalone M4P1 visual PIE session, the delegated StaticEnvironment control logged `memoryReady=0` / packet-identity mismatch and correctly failed black. It did not affect Last-Seen assertions. The independent frozen M3.5 D3D12 prefix passed 26/26, so this remains a fixture-session diagnostic rather than evidence of an M3.5 contract regression.
- UE startup retains optional profiler/localization/network diagnostics unrelated to the tested renderer. No final report records them as test warnings.
- Unreal can clean up the default `L_Prototype` world before loading `/SightWeave/Maps/L_SightWeave_Lab`; no PIE, save, asset operation, or source change targeted `/Game/Maps/L_Prototype`.

## 10. Lab and visual evidence

The M4P1 fixture is runtime-generated in the real PIE copy of `/SightWeave/Maps/L_SightWeave_Lab`; it is not a CPU-only simulation and does not modify the `.umap`. It creates real world actors, real static-mesh live/proxy components, real camera views, and the production M3 composite. Camera bindings and every state rebuild are logged. PIE teardown logs `BeginTearingDown`, `CleanupWorld`, and subsystem shutdown after all eight captures.

| Screenshot | Black | Nonblack | Neutral | Exact RGB117 proxy | Center nonblack | Nonfinite | Agent inspection |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `M4P1_Camera0_Overview.png` | 834366 | 1882 | 639 | 559 | 182 | 0 | live subject visible; neutral remembered controls; negative fixtures black |
| `M4P1_Camera1_LastSeen.png` | 831418 | 4830 | 4830 | 4830 | 4830 | 0 | centered fixed-neutral Last-Seen proxy |
| `M4P1_Camera1_Reacquired.png` | 828765 | 7483 | 71 | 0 | 7483 | 0 | live square restored; proxy absent, no double image |
| `M4P1_Camera2_NoDynamicLeak.png` | 836248 | 0 | 0 | 0 | 0 | 0 | strict black; NeverRemember, VisibleOnly, moving mesh and current light do not leak |
| `M4P1_Camera3_PageBoundary.png` | 832344 | 3904 | 3904 | 3904 | 3904 | 0 | continuous neutral proxy across page boundary; no seam break |
| `M4P1_Camera3_ClearSuppression.png` | 832222 | 4026 | 4026 | 4026 | 4026 | 0 | control proxy retained; clear/suppression/block ROIs strict black |
| `M4P1_Camera4_Yaw45.png` | 835029 | 1219 | 1219 | 1219 | 1219 | 0 | rotated proxy aligned with yaw-45 view |
| `M4P1_Camera4_IdentityReuse.png` | 835029 | 1219 | 1219 | 1219 | 1219 | 0 | control remains; reused generation and mismatch subjects remain black |

All images are 1526x548 under `Saved/Screenshots/M4P1/` and remain uncommitted. The automated validator additionally checks exact black ROIs for unknown, NeverRemember, VisibleOnly, invalid Custom, clear, suppression/block, identity reuse, scope/revision mismatch, moving mesh, and current light. Camera 0 contains thin cyan authored/live-region Lab markers; Camera 2's exact-zero frame proves no negative-fixture leakage. The agent opened and inspected all eight final images. User interactive inspection has not yet occurred.

## 11. Build, packaging, and shipping boundary

Repository-standard `Scripts/BuildEditor.ps1` passed after the last source/test change with 9 actions. A separate standard up-to-date invocation had also succeeded before the final test-only compatibility update. The shader change compiled and ran under D3D12/SM6 with no binding, compilation, RDG, RHI, or GPU error.

BuildPlugin, clean-host builds, Game Development, Game Shipping, full SightWeave, DARKWELL, Shipping dependency/import/string/COFF scans, and the expanded performance matrix were explicitly not required for this bounded M4P1 visual continuation. They are P2 packaging closure and do not block M4P1 manual-acceptance readiness.

The M3.5 regression still exercised its current performance gates. Selected Coarse 25 cm/texel passed with CPU dirty p95 82.102 us, no-change p95 0.101 us with `nochange_work=0`, RT total p95 62.302 us, GPU dirty p95 53 us, CPU snapshot 7,688 B, GPU persistent 4,194,320 B, and worst plugin runtime 36,462,592 B. Scale cases passed at resident/dirty 1/1, 8/8, and 128/32; CPU bytes were 7,688 / 61,504 / 984,064 and GPU mirror p95 was 86 / 192 / 2,453 us respectively. These are regression evidence, not a rerun of the complete M3.5 performance matrix.

## 12. Commits and Git/LFS closure

M4P1 commits from the frozen M3.5 baseline through the last source/test checkpoint are:

```text
eb7f70e docs: start SightWeave M4P1 subject policy foundation
b344453 docs: define SightWeave subject memory contract
96b05a1 feat: add SightWeave subject memory policies
04cb024 feat: capture SightWeave last-seen snapshots
ee4d594 feat: render SightWeave last-seen proxies
e0d21c9 test: validate SightWeave subject memory transitions
8ecfbbd test: complete SightWeave M4P1 isolation coverage
d2b2713 docs: record SightWeave M4P1 validation
1596807 feat: add SightWeave M4P1 visual Lab fixture
1e848b8 test: validate SightWeave Last-Seen presentation
e22d0a7 test: preserve M3P5 boundaries through Last-Seen rendering
```

The final documentation commit containing this file is added to the post-push task report. Every reliable checkpoint is pushed to the normal branch; no rescue branch, merge, rebase, force push, reset, or clean was used. Generated reports/logs/screenshots, `Binaries`, `Intermediate`, and `Saved` are ignored and uncommitted. `Darkwell.uproject` retains only the known local EngineAssociation GUID difference and is never staged. Final HEAD/upstream/remote, LFS, and object-database results are the closure commands run after the documentation push.

## 13. User interactive PIE acceptance

The real visual Lab integration is complete. The only remaining M4P1 work is this user-operated check:

1. Open `/SightWeave/Maps/L_SightWeave_Lab` and start PIE.
2. Run `SightWeave.Lab.Mode 4`.
3. Run `SightWeave.Lab.Camera 0`, then `SightWeave.Lab.M4P1.State 0`; confirm the Overview contains live, remembered, and black-negative controls.
4. Run Camera 1 with State 0, then 1, then 2. Confirm Live -> fixed-neutral Remembered -> immediate Reacquired, with no live/proxy double image.
5. Run Camera 2 with State 1. Confirm NeverRemember, VisibleOnly, moving mesh, and current-light sentinels remain black.
6. Run Camera 3 with State 1, then 3, then 1, then 4. Confirm page-boundary continuity, suppression/block black, exact restore after suppression lifts, and clear remains black without stale reuse.
7. Run Camera 4 with State 1, then 5. Confirm yaw-45 alignment and that the reused identity, stale revision, and scope mismatch do not inherit an old proxy.
8. Copy the final `M4P1 Lab state=...` and `Lab camera bound mode=M4P1 actor=...` health lines from the Output Log, stop PIE, and confirm no teardown error.

State mapping is `0=Live`, `1=Remembered`, `2=Reacquired`, `3=SuppressedBlocked`, `4=Cleared`, `5=IdentityReuse`. Camera actors are `SW_M4P1_Camera0_Overview`, `SW_M4P1_Camera1_Transition`, `SW_M4P1_Camera2_PolicyMatrix`, `SW_M4P1_Camera3_PageBoundary`, and `SW_M4P1_Camera4_Rotated45`.

Formal gameplay integration, persistence, skeletal/VFX/light/audio capture, SceneCapture, reveal override, and production content remain deferred beyond M4P1.

## 14. Home recovery

```powershell
git fetch origin
git switch codex/m4p1-sightweave-subject-policy-last-seen
git pull --ff-only
```

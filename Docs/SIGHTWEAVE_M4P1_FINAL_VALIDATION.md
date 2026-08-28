# SightWeave M4P1 visual closure validation

## 1. Status

**COMPLETED**

All scoped implementation, standard build, NullRHI, D3D12/SM6, real Lab screenshot, exact pixel/ROI, agent image inspection, lifecycle, M3.4/M3.5 regression, severe-log, remote checkpoint, and user-operated Camera 0–4 PIE gates passed. M4P1 has no remaining acceptance item.

## 2. Identity and baseline

- Branch: `codex/m4p1-sightweave-subject-policy-last-seen`
- Continuation baseline: `d2b27136288ecf11d96b6e09d7a2b2fea6dbaa05`
- Timing-repair continuation baseline: `4e0b5e1ec93391467846028f1bf46fdf92e67ed3`
- Camera 3/4 observability continuation baseline: `598e1798cc649b32b2ed244eba9fcec915b67205`
- Frozen M3.5 baseline: `941dcb75fe9fcda05bcb972a4d2d9651c6d521be`
- Last source/test checkpoint before this documentation commit: `e330b1f5e025ae9d7c4f348fc6a787787f7c362d`
- Final SHA: authoritative post-push `git rev-parse HEAD` recorded in the final task report; a commit cannot embed its own SHA.
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Hardware/RHI: NVIDIA GeForce RTX 4060, NullRHI plus D3D12 / SM6

At continuation start local, upstream, and remote were all `d2b2713`. The only permitted local difference was the existing `Darkwell.uproject` EngineAssociation GUID. That file remained unstaged and uncommitted throughout; final Git/LFS closure is recorded after the documentation push.

## 3. Timing repair and delivered source

The manual Camera 1 failure had two independent causes. The Last-Seen stencil was composited after TAA while UE's CustomDepth pass still carried temporal projection jitter, so a frozen proxy appeared to move every running frame and stopped when PIE paused. Reacquire rebuilt all fixture actors/components/lights and introduced an abrupt live presentation into incompatible temporal history, producing the transient white ring. The state rebuild also made sparse residency accidentally depend on fresh source registration.

The final repair disables temporal jitter only for CustomDepth (`r.CustomDepthTemporalAAJitter=0`), leaves TAA/Width=50 feather/bloom/quality intact, makes repeated identical snapshot presentation idempotent, orders proxy hide before live show, rejects incompatible temporal history at the legal reacquire edge, preserves monotonic residency generations, and changes State 0-5 transitions to update the primary subject in place. Snapshot revision, descriptor, mesh/material, world transform, and proxy presentation stay frozen throughout State 1.

The later Camera 3/4 manual failure was a distinct Lab-only observability defect. The changing `SW_M4P1_PrimaryTransition` was outside both camera frusta; Camera 3's local suppression/clear fixtures were permanent negative samples, and Camera 4's local identity fixture was `NeverRemember`. Thus log counts correctly changed while each viewport showed only one static control. Product SubjectMemory, proxy, renderer, descriptor, revision/generation, and presentation-command logic required no change.

The Lab now uses an array of three independent in-place transition targets. `SW_M4P1_PageBoundaryTransition` is separated to the right of the page-boundary control and follows State `1 -> 3 -> 1 -> 4`; `SW_M4P1_IdentityReuseTransition` is separated to the left of the yaw45 control and follows State `1 -> 5`. Their own legal falling edges, frozen descriptors, suppression, clear regions, and generation update drive the visual change. The static controls use separate authority records and never disappear.

The final state logs match the expanded fixture: State 0/2 report `live=3 proxies=4`; State 1 reports `live=0 proxies=7 retainedSnapshots=13`; State 3 reports `live=0 proxies=4 retainedSnapshots=13`; State 4/5 report `live=0 proxies=4 retainedSnapshots=10`. Camera commands continue to bind the exact actors `SW_M4P1_Camera3_PageBoundary` and `SW_M4P1_Camera4_Rotated45`.

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

Final M4P1 test inventory is 12; NullRHI executes the 9 non-visual tests and D3D12/SM6 executes all 12:

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
11. `SightWeave.M4P1.Visual.ContinuousTransition`
12. `SightWeave.M4P1.Visual.Camera34Observability`

Coverage includes all five policies, falling-edge single capture, no non-live churn, immediate reacquire, second revision, clear/rebuild, suppression retain/hide/restore, unknown black, stale descriptor/presentation revisions, exact world/owner/floor/precision/profile comparisons including forced hash collision, generation reuse, teardown/reset, unsupported complex input, provider failure modes, render-only invariants, and integrated live/proxy/black switching. The visual tests start PIE, capture the eight full-Lab views plus six dedicated Camera 3/4 state frames, stop PIE, and log world cleanup; repeated dedicated/full runs established consistent restart behavior with no duplicate actors, components, delegate, ticker, or pending-render-command failure.

## 8. Exact gate results

| Gate | RHI | Report | Result |
| --- | --- | --- | ---: |
| Final Editor Development build | build | standard `Scripts/BuildEditor.ps1` | succeeded; 8 actions, 6.67 s; both Lab fixture and ROI test compiled independently |
| M4P1 full | NullRHI | `Saved/AutomationReports/M4P1_Camera34_Full_NullRHI` | 9 succeeded, 0 warning, 0 failed; 0.074 s |
| M4P1 full | D3D12/SM6 | `Saved/AutomationReports/M4P1_Camera34_Full_D3D12` | 12 succeeded, 0 warning, 0 failed; 65.82 s |
| Camera 3/4 ROI | D3D12/SM6 | `Saved/AutomationReports/M4P1_Camera34_ROI_Iteration1` | 1 succeeded, 0 warning, 0 failed; six captures inspected |
| Continuous transition | D3D12/SM6 | `Saved/AutomationReports/M4P1_Continuous_Final_D3D12` | 1 succeeded, 0 warning, 0 failed; 13.46 s |
| Dedicated visual/Camera | D3D12/SM6 | `Saved/AutomationReports/M4P1_Visual_D3D12_ExactPixels` | 1 succeeded, 0 warning, 0 failed; 28.73 s |
| M4P1 Lab prefix | NullRHI | `Saved/AutomationReports/M4P1_Lab_Final_NullRHI` | 2 succeeded, 0 warning, 0 failed |
| M4P1 Lab prefix | D3D12/SM6 | `Saved/AutomationReports/M4P1_Lab_Final_D3D12` | 2 succeeded, 0 warning, 0 failed |
| Frozen M3.4 presentation | D3D12/SM6 | `Saved/AutomationReports/M4P1_Camera34_M3P4_D3D12` | 3 succeeded, 0 warning, 0 failed |
| Complete M3.5 prefix | D3D12/SM6 | `Saved/AutomationReports/M4P1_Camera34_M3P5_D3D12` | 26 succeeded, 0 warning, 0 failed; 16.16 s |
| Repaired M3.5 packaging case | D3D12/SM6 | `Saved/AutomationReports/M4P1_M3P5PackagingFix_D3D12` | 1 succeeded, 0 warning, 0 failed |

The D3D12 log records `Using Highest Feature Level of D3D12: SM6`, shader model 6.7 support on the selected adapter, and creation of the D3D12 RHI at SM6.

## 9. Severe scans and preserved diagnostics

The final Camera 3/4 logs for M4P1 NullRHI, M4P1 D3D12, M3.4 D3D12, and M3.5 D3D12 produced zero severe matches for Shader compiler/ShaderCompileWorker failure, Renderer/RenderCore error, RDG error/warning, D3D12/RHI error, GPU crash/hang/device removal, fatal error, assertion, ensure, or failed automation result.

Preserved facts:

- MSVC 14.51 is newer than UE 5.8's preferred 14.50; all final standard builds succeeded.
- Two restricted launches waited before UBT acquired its global mutex or created a new log. They were stopped only after process/log inspection; no compilation occurred. The single outside-sandbox standard build then succeeded 15/15.
- Two sandboxed build attempts failed before compile because UBT could not rotate `%LOCALAPPDATA%\UnrealBuildTool\Log.txt`; the second reproduced the same `UnauthorizedAccessException` after process/log inspection. The standard build outside the filesystem sandbox then compiled all 8 actions successfully. Neither sandbox failure was a source failure.
- The first D3D visual report `M4P1_Visual_D3D12_First` failed only Camera 1 Reacquired because root-component installation reset the actor transform. Explicit post-registration placement fixed it; all later visual reports passed and the failure remains preserved.
- The first final M3.5 report `M4P1_Visual_M3P5Regression_D3D12` retained 25 successes and one packaging source-string failure. The old test forbade any render-state `LastSeen` token and matched obsolete shader syntax. Its authority exclusions were kept; a bounded assertion now proves the M4P1 stencil constants are presentation-only and no subject authority/descriptor enters RenderState. Isolated 1/1 and final 26/26 passed.
- A first non-escalated M3.5 launch exited before creating a report/log after platform validation. No Unreal/UBT process remained; the authorized retry created full evidence. This is retained as a host sandbox launch failure, not a test result.
- In the standalone M4P1 visual PIE session, the delegated StaticEnvironment control logged `memoryReady=0` / packet-identity mismatch and correctly failed black. It did not affect Last-Seen assertions. The independent frozen M3.5 D3D12 prefix passed 26/26, so this remains a fixture-session diagnostic rather than evidence of an M3.5 contract regression.
- UE startup retains optional profiler/localization/network diagnostics unrelated to the tested renderer. No final report records them as test warnings.
- Unreal can clean up the default `L_Prototype` world before loading `/SightWeave/Maps/L_SightWeave_Lab`; no PIE, save, asset operation, or source change targeted `/Game/Maps/L_Prototype`.
- User Camera 0–4 PIE passed. UE's Lumen `CachedLightingPreExposure=-8.5` red exposure hint was observed but is unrelated to SightWeave behavior and did not affect any acceptance state.
- A thin blue old-Lab strip appeared briefly only after PIE startup and before explicitly running `SightWeave.Lab.Mode 4`. It disappeared on formal M4P1 entry and did not recur in any tested state; this is retained as a startup-transition observation, not an M4P1 failure.

## 10. Lab and visual evidence

The M4P1 fixture is runtime-generated in the real PIE copy of `/SightWeave/Maps/L_SightWeave_Lab`; it is not a CPU-only simulation and does not modify the `.umap`. It creates real world actors, real static-mesh live/proxy components, real camera views, and the production M3 composite. Camera bindings and each in-place state transaction are logged. PIE teardown logs `BeginTearingDown`, `CleanupWorld`, and subsystem shutdown after capture.

The final continuous sequence contains 152 screenshots at 1009x340. All 120 remembered frames have exactly 1936 RGB117 pixels, identical bounds `(483,148)-(527,192)`, identical centroid, no non-proxy RGB, and no descriptor/presentation churn. All 32 immediate reacquire frames classify as live; none classify as proxy or black handoff, none exceed the final live bounds by more than 2 px or its pixel count by more than 10%, and the final full-prefix run records zero proxy-color collision. The final live bounds are `(475,141)-(534,200)`. The agent opened representative remembered/reacquire frames and found no visible jitter, black hole, double image, residue, or ring.

| Screenshot | Black | Nonblack | Neutral | Exact RGB117 proxy | Center nonblack | Nonfinite | Agent inspection |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `M4P1_Camera0_Overview.png` | 342280 | 780 | 235 | 183 | 72 | 0 | live subject visible; neutral remembered controls; negative fixtures black |
| `M4P1_Camera1_LastSeen.png` | 341124 | 1936 | 1936 | 1936 | 1936 | 0 | centered fixed-neutral Last-Seen proxy |
| `M4P1_Camera1_Reacquired.png` | 340048 | 3012 | 172 | 0 | 3012 | 0 | live square restored; proxy absent, no double image |
| `M4P1_Camera2_NoDynamicLeak.png` | 343060 | 0 | 0 | 0 | 0 | 0 | strict black; NeverRemember, VisibleOnly, moving mesh and current light do not leak |
| `M4P1_Camera3_PageBoundary.png` | 341612 | 1448 | 1448 | 1448 | 928 | 0 | page-boundary control and separated transition target both visible |
| `M4P1_Camera3_ClearSuppression.png` | 342132 | 928 | 928 | 928 | 928 | 0 | control retained; separated transition target strict black |
| `M4P1_Camera4_Yaw45.png` | 342546 | 514 | 514 | 514 | 264 | 0 | yaw45 control and separated old-generation target both visible |
| `M4P1_Camera4_IdentityReuse.png` | 342796 | 264 | 264 | 264 | 264 | 0 | yaw45 control remains; old-generation target strict black |

All current images are 1009x340 under `Saved/Screenshots/M4P1/` and remain uncommitted. The dedicated ROI sequence records Camera 3 control exact pixels `[928,928,928,928]`, transition nonblack `[300,0,300,0]`, changed pixels `[300,300,300]`, and absolute RGB errors `[105300,105300,105300]`. Camera 4 records yaw45 control `[264,264]`, old-generation nonblack `[250,0]`, 250 changed pixels, and absolute RGB error 87750. The agent opened all six dedicated frames and confirmed the separated objects and state changes visually. The user subsequently completed and passed Camera 0–4 PIE.

## 11. Build, packaging, and shipping boundary

Repository-standard `Scripts/BuildEditor.ps1` passed the independent runtime/source build with 15 actions. Later serial incremental builds passed, including the final post-test 4-action build. The renderer/config/runtime changes compiled and ran under D3D12/SM6 with no binding, compilation, RDG, RHI, or GPU error.

BuildPlugin, clean-host builds, Game Development, Game Shipping, full SightWeave, DARKWELL, Shipping dependency/import/string/COFF scans, and the expanded performance matrix were explicitly not required for this bounded M4P1 visual continuation. They are P2 packaging closure and do not block M4P1 manual-acceptance readiness.

The M3.5 regression still exercised its current performance gates. Selected Coarse 25 cm/texel passed with CPU dirty p95 55.302 us, no-change p95 0.101 us with `nochange_work=0`, RT total p95 76.599 us, GPU dirty p95 55 us, CPU snapshot 7,688 B, GPU persistent 4,194,320 B, and worst plugin runtime 36,462,592 B. Scale cases passed at resident/dirty 1/1, 8/8, and 128/32; CPU bytes were 7,688 / 61,504 / 984,064 and GPU mirror p95 was 149 / 190 / 1,381 us respectively. These are regression evidence, not a rerun of the complete M3.5 performance matrix.

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
3e0ed45 fix: stabilize SightWeave last-seen transitions
598e179 docs: record M4P1 timing repair validation
e330b1f fix: expose M4P1 Camera 3 and 4 transitions
```

The final documentation commit containing this file is added to the post-push task report. Every reliable checkpoint is pushed to the normal branch; no rescue branch, merge, rebase, force push, reset, or clean was used. Generated reports/logs/screenshots, `Binaries`, `Intermediate`, and `Saved` are ignored and uncommitted. `Darkwell.uproject` retains only the known local EngineAssociation GUID difference and is never staged. Final HEAD/upstream/remote, LFS, and object-database results are the closure commands run after the documentation push.

## 13. User interactive PIE acceptance

The user completed the authoritative Camera 0–4 pass through `SightWeave.Lab.Mode 4`:

1. Camera 0 Overview passed: live, remembered, and black unknown controls were visible; presentation was healthy and `bindingFailure=0`.
2. Camera 1 Transition passed: State 0 live, State 1 frozen Last-Seen proxy, and State 2 reacquired live were stable, with no jitter, white ring, double display, residual frame, or black handoff.
3. Camera 2 PolicyMatrix passed: State 1 was strictly black; NeverRemember, VisibleOnly, dynamic content, and invalid Custom provider output did not leak.
4. Camera 3 PageBoundary passed: State `1 -> 3 -> 1 -> 4` showed two objects, hid only the right target, restored it at the same position, then cleared it while retaining the horizontal control. No seam, offset, jitter, white flash, or double display occurred.
5. Camera 4 Rotated45 passed: State `1 -> 5` removed only the left old-generation diamond while retaining the vertical yaw45 control. No stale descriptor inheritance, offset, duplicate, jitter, or white flash occurred.

The Lumen exposure hint and pre-Mode-4 thin blue startup strip described in Section 9 are retained observations and did not occur as failures in the formal acceptance states.

State mapping is `0=Live`, `1=Remembered`, `2=Reacquired`, `3=SuppressedBlocked`, `4=Cleared`, `5=IdentityReuse`. Camera actors are `SW_M4P1_Camera0_Overview`, `SW_M4P1_Camera1_Transition`, `SW_M4P1_Camera2_PolicyMatrix`, `SW_M4P1_Camera3_PageBoundary`, and `SW_M4P1_Camera4_Rotated45`.

Formal gameplay integration, persistence, skeletal/VFX/light/audio capture, SceneCapture, reveal override, and production content remain deferred beyond M4P1.

## 14. Home recovery

```powershell
git fetch origin
git switch codex/m4p1-sightweave-subject-policy-last-seen
git pull --ff-only
```

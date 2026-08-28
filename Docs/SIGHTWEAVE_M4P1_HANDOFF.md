# SightWeave M4P1 visual acceptance handoff

Status: **COMPLETED — automated and user Camera 0–4 PIE acceptance passed**

- Branch: `codex/m4p1-sightweave-subject-policy-last-seen`
- Continuation baseline: `d2b27136288ecf11d96b6e09d7a2b2fea6dbaa05`
- Frozen M3.5 baseline: `941dcb75fe9fcda05bcb972a4d2d9651c6d521be`
- Timing-repair continuation baseline: `4e0b5e1ec93391467846028f1bf46fdf92e67ed3`
- Camera 3/4 observability continuation baseline: `598e1798cc649b32b2ed244eba9fcec915b67205`
- Last source/test checkpoint before this documentation commit: `e330b1f5e025ae9d7c4f348fc6a787787f7c362d`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- GPU/RHI: NVIDIA GeForce RTX 4060, D3D12 / SM6; NullRHI also validated

## Outcome

M4P1 now has a real runtime-generated Lab fixture inside the PIE world, five stable cameras, six controllable subject states, CustomDepth/stencil-only Last-Seen proxies, D3D12/SM6 screenshots, exact pixel validation, agent image inspection, lifecycle coverage, and green M3.4/M3.5 regressions. No map asset was modified: the fixture is created and destroyed by the editor module around `/SightWeave/Maps/L_SightWeave_Lab` PIE.

The user completed and passed Camera 0–4 interactive PIE after both visual repairs. Automated implementation, build, Lab, continuous-frame image, ROI, regression, severe-log, and remote-checkpoint gates are also green. M4P1 has no remaining acceptance item. BuildPlugin, clean-host, Game Shipping, full SightWeave, DARKWELL, and the full performance matrix were explicitly outside this bounded M4P1 continuation and do not block completion.

## Timing-defect repair

Manual Camera 1 PIE exposed two defects that stable single screenshots could not see. State 1 jitter came from the post-tonemap proxy composite reading CustomDepth with temporal projection jitter. State 2's white ring came from rebuilding the entire fixture on every state change and then carrying incompatible temporal post-process history into the reacquired live frame. The old rebuild also relied on a transient source-registration side effect to keep the presentation page resident.

The repair keeps TAA, Width=50 feather, bloom, and quality enabled. `r.CustomDepthTemporalAAJitter=0` removes jitter only from the CustomDepth pass consumed after TAA. A presented immutable descriptor is now idempotent: the same revision, mesh/material paths, transform, and descriptor do not clear/reload/recreate the scene proxy. Reacquire hides the proxy before showing live, rejects old temporal history with UE's camera-cut flag, and retains monotonic source/residency generation ordering. Lab state transitions mutate only the primary authority/presentation objects; the comparison matrix, lights, descriptors, and transforms remain in place.

The new `SightWeave.M4P1.Visual.ContinuousTransition` D3D12/SM6 test captures 120 immediate State 1 frames plus 32 immediate State 1-to-2 frames. Final evidence: State 1 `count_min=count_max=1936`, bounds `(483,148)-(527,192)`, centroid max delta `0`; reacquire `proxy_only=0`, `black_handoff=0`, `live=32`, maximum proxy-color collision `0`, final live bounds `(475,141)-(534,200)`. The agent opened representative remembered and reacquire frames; no jitter, black hole, proxy residue, double image, or white ring was present.

## Camera 3/4 observability repair

Manual Camera 3/4 PIE then exposed a Lab-only composition defect. The CPU counts changed because `SW_M4P1_PrimaryTransition` changed off-camera, while Camera 3's local suppression/clear sentinels were permanently hidden from fixture construction and Camera 4's local identity sentinel used `NeverRemember`. Each camera therefore contained only its unchanged page-boundary or yaw45 control. This was not a SubjectMemory, proxy, renderer, descriptor, or command-ordering defect.

The fixture now owns three independent in-place transition targets: the original Camera 1 target, `SW_M4P1_PageBoundaryTransition`, and `SW_M4P1_IdentityReuseTransition`. Camera 3 places its state target to the right of a smaller horizontal page-boundary control; Camera 4 places its old-generation target to the left of a smaller vertical yaw45 control. State 3 suppresses the Camera 3 target, returning to State 1 restores the same frozen descriptor at the same transform, State 4 clears it, and State 5 invalidates the Camera 4 target by generation update. Controls retain separate authority records and remain visible.

Accurate expanded-fixture logs are State 0/2 `live=3 proxies=4`, State 1 `live=0 proxies=7 retainedSnapshots=13`, State 3 `live=0 proxies=4 retainedSnapshots=13`, and State 4/5 `live=0 proxies=4 retainedSnapshots=10`. Camera binding names remain unchanged.

`SightWeave.M4P1.Visual.Camera34Observability` captures six D3D12/SM6 frames and validates separated ROIs. At 1009x340, Camera 3 control RGB117 pixels are `[928,928,928,928]`; transition nonblack pixels are `[300,0,300,0]` for State 1/3/restored 1/4, with changed pixels `[300,300,300]` and absolute RGB errors `[105300,105300,105300]`. Camera 4 yaw45 control pixels are `[264,264]`; old-generation nonblack pixels are `[250,0]`, with 250 changed pixels and absolute RGB error 87750. The agent opened all six frames and confirmed both targets are spatially separated, black only in their required states, and the controls remain visible.

## Final user PIE acceptance

The user completed the authoritative Camera 0–4 pass through `SightWeave.Lab.Mode 4`:

- Camera 0 Overview: live, remembered, and black unknown controls were visible; presentation was healthy and `bindingFailure=0`.
- Camera 1 Transition: State 0 live, State 1 Last-Seen proxy, and State 2 reacquired live were stable. There was no jitter, white ring, live/proxy double display, residual frame, or black handoff.
- Camera 2 PolicyMatrix: State 1 remained strictly black. NeverRemember, VisibleOnly, dynamic content, and invalid Custom provider output did not leak.
- Camera 3 PageBoundary: State 1 showed the horizontal control plus the right target; State 3 hid only the target; returning to State 1 restored it at the same position; State 4 hid it again while retaining the control. There was no seam, offset, jitter, white flash, or double display.
- Camera 4 Rotated45: State 1 showed the vertical yaw45 control plus the left old-generation diamond; State 5 removed only the old-generation target. There was no stale descriptor inheritance, offset, duplicate, jitter, or white flash.

Two observations are retained but do not block acceptance: UE emitted the unrelated Lumen `CachedLightingPreExposure=-8.5` red exposure hint; and immediately after PIE startup, before explicitly selecting Mode 4, an old-Lab thin blue strip briefly appeared. It disappeared upon entering the formal M4P1 Lab and did not recur in any tested state.

## Reliable remote checkpoints

All listed commits were pushed to `origin/codex/m4p1-sightweave-subject-policy-last-seen`:

1. `eb7f70e` — `docs: start SightWeave M4P1 subject policy foundation`
2. `b344453` — `docs: define SightWeave subject memory contract`
3. `96b05a1` — `feat: add SightWeave subject memory policies`
4. `04cb024` — `feat: capture SightWeave last-seen snapshots`
5. `ee4d594` — `feat: render SightWeave last-seen proxies`
6. `e0d21c9` — `test: validate SightWeave subject memory transitions`
7. `8ecfbbd` — `test: complete SightWeave M4P1 isolation coverage`
8. `d2b2713` — `docs: record SightWeave M4P1 validation`
9. `1596807` — `feat: add SightWeave M4P1 visual Lab fixture`
10. `1e848b8` — `test: validate SightWeave Last-Seen presentation`
11. `e22d0a7` — `test: preserve M3P5 boundaries through Last-Seen rendering`
12. `3e0ed45` — `fix: stabilize SightWeave last-seen transitions`
13. `598e179` — `docs: record M4P1 timing repair validation`
14. `e330b1f` — `fix: expose M4P1 Camera 3 and 4 transitions`

The final documentation checkpoint is the commit containing this file; the authoritative final SHA is the post-push `git rev-parse HEAD` value in the final Git closure and user handoff message. This avoids pretending a Git commit can contain its own hash.

The first push attempt for `e0d21c9` received an `unexpected EOF` from the unsupported Git LFS locking endpoint. An unchanged ordinary `git push` retry succeeded; no force push or Git configuration change was used.

## Implemented contract and behavior

- `NeverRemember`: live is permitted only for legal HardLive; falling edges never create a snapshot or proxy.
- `VisibleOnly`: live-only presentation; non-live is hidden with no remembered state.
- `StaticEnvironment`: delegates to the completed M3.5 path; M4P1 creates no duplicate static-memory store or proxy.
- `LastSeenSnapshot`: a legal HardLive-to-non-live edge captures one immutable descriptor. Remaining non-live does not churn revisions; reacquire immediately selects live; a later edge creates the next revision.
- `Custom`: exact provider name and nonzero schema version are required. Missing, mismatched, rejected, or invalid provider output fails closed. No DARKWELL gameplay provider was added.

The descriptor carries stable ID and instance generation, complete non-hash memory scope, policy, snapshot/eligibility/source-live revisions, transform, bounds, stable mesh/material paths, visual variant, capture reason/transition, and validity flags. Exact scope includes world identity/generation, owner, floor, floor origin/plane, precision, and the complete canonical profile sequence.

Clear deletes intersecting exact-scope descriptors. Suppression and block hide the proxy while retaining the descriptor. Unknown, stale revision, scope mismatch, world mismatch, or generation reuse selects black with no stale fallback. Authority reset removes subjects and snapshots for teardown/world restart.

The proxy is a transient `UStaticMeshComponent` with no collision, overlap, physics, navigation relevance, tick, decals, dynamic shadow, or dynamic indirect/distance-field lighting. It loads only stable mesh/material asset paths, requires opaque material overrides, preserves an exactly matching immutable presentation without churn, and clears stale resources on invalid, hidden, or changed decisions. The bridge mutates presentation visibility only; it does not alter gameplay, AI, damage, interaction, save, audio, VFX, or light state.

The final presentation path additionally disables main-pass and ordinary depth rendering. A reserved CustomDepth stencil value of `246` identifies legal proxies; the M3 hard-mask/inward-feather composite checks proxy depth against SceneDepth and emits the fixed neutral intensity `0.46` (8-bit RGB `117`). Hidden proxies disable CustomDepth. This preserves M3.5 HardLive-first ordering and never stores Scene Color, current lighting, shadow, VFX, skeletal, or dynamic-material output.

## Lab and controls

The runtime-generated fixture includes LastSeen, NeverRemember, VisibleOnly, delegated StaticEnvironment, valid and invalid Custom, clear, suppression, block, unknown, stale revision, scope mismatch, identity-generation reuse, page-boundary, yaw-45, moving-mesh, and current-light sentinels. It creates real actors, real `UStaticMeshComponent` proxies, a real PIE view, and the production M3 composite, then destroys all transient actors/components at teardown.

Open `/SightWeave/Maps/L_SightWeave_Lab`, start PIE, and use:

```text
SightWeave.Lab.Mode 4
SightWeave.Lab.Camera 0
SightWeave.Lab.Camera 1
SightWeave.Lab.Camera 2
SightWeave.Lab.Camera 3
SightWeave.Lab.Camera 4

SightWeave.Lab.M4P1.State 0   // Live
SightWeave.Lab.M4P1.State 1   // Remembered
SightWeave.Lab.M4P1.State 2   // Reacquired
SightWeave.Lab.M4P1.State 3   // Suppressed/Blocked
SightWeave.Lab.M4P1.State 4   // Cleared
SightWeave.Lab.M4P1.State 5   // Identity reuse
```

Camera actors logged by the binding command are, in order, `SW_M4P1_Camera0_Overview`, `SW_M4P1_Camera1_Transition`, `SW_M4P1_Camera2_PolicyMatrix`, `SW_M4P1_Camera3_PageBoundary`, and `SW_M4P1_Camera4_Rotated45`.

## Validation summary

- Final standard `DarkwellEditor Win64 Development`: independent runtime/source build succeeded with 15 actions; the final post-test build succeeded with 4 actions. MSVC 14.51 remains newer than UE 5.8's preferred 14.50.
- M4P1 full NullRHI: 9/9, 0 warning, 0 failed.
- M4P1 full D3D12/SM6: 12/12, 0 warning, 0 failed; this includes stable-frame, continuous-frame, and Camera 3/4 ROI visual tests.
- Dedicated Camera 3/4 ROI D3D12/SM6: 1/1, 0 warning, 0 failed; six state captures with strict-black target ROIs and pixel-stable controls.
- Dedicated continuous transition D3D12/SM6: 1/1, 0 warning, 0 failed; 120 remembered + 32 reacquire frames.
- Dedicated M4P1 visual D3D12/SM6: 1/1, 0 warning, 0 failed, 28.73 s.
- M4P1 Lab: NullRHI 2/2 and D3D12/SM6 2/2, both with zero warnings/failures.
- M3.4 presentation D3D12/SM6: 3/3, 0 warning, 0 failed.
- M3.5 complete D3D12/SM6 after compatibility repair: 26/26, 0 warning, 0 failed.
- Five timing-repair final gate logs: zero severe Shader compiler, ShaderCompileWorker, Renderer/RenderCore, RDG, D3D12/RHI, GPU crash/hang/device-removal, fatal, assert, ensure, or failed-test matches.
- Eight full-Lab screenshots plus six Camera 3/4 ROI screenshots: `nonfinite=0`; exact neutral proxy pixels use RGB 117; every negative-region assertion reports zero leakage.

Canonical evidence paths are listed in `Docs/SIGHTWEAVE_M4P1_FINAL_VALIDATION.md`. All reports, logs, and screenshots live under `Saved` and are intentionally uncommitted.

## Preserved limitations and warnings

- User-operated Camera 0–4 interactive PIE passed; no M4P1 acceptance work remains.
- UE's Lumen `CachedLightingPreExposure=-8.5` red exposure hint is unrelated to SightWeave behavior and did not affect acceptance.
- A thin blue old-Lab strip was briefly visible only during PIE startup before `SightWeave.Lab.Mode 4`; it disappeared on formal entry and never appeared in a tested M4P1 state.
- Two restricted build launches waited before UBT log creation/global-mutex acquisition and were stopped only after process/log inspection. The single authorized standard build then completed 15/15 actions; later serial incremental/final builds also succeeded.
- No authored `.umap` was changed. Unreal may log cleanup of the project-default `L_Prototype` before loading the allowed Lab, but no PIE, save, asset operation, or source change targeted `/Game/Maps/L_Prototype`.
- No WorldSubsystem/gameplay adapter, production subject, persistence, skeletal/VFX/light/audio snapshot, SceneCapture, or reveal override was added.
- Full SightWeave, DARKWELL, BuildPlugin, clean-host, Game/Shipping, and expanded performance/package matrices were not required or run in this bounded visual continuation.
- The first D3D visual run retained one failure because Camera 1 Reacquired was black; installing a root component had reset the actor location. Explicit post-registration placement repaired it, and all later dedicated/full visual runs passed.
- One sandboxed standard build failed before compilation because UBT could not rotate `%LOCALAPPDATA%\UnrealBuildTool\Log.txt`; the same standard build outside the filesystem sandbox succeeded. This was not a source failure.
- The first final M3.5 attempt retained 25 successes and one old packaging source-string failure. The M3.5 authority exclusions remain frozen; the test now recognizes only the bounded M4P1 stencil presentation extension. The isolated repair passed 1/1 and the final full prefix passed 26/26.
- The standalone M4P1 visual session's delegated StaticEnvironment control logged `memoryReady=0` / packet-identity mismatch and correctly failed black. The independent frozen M3.5 D3D12 prefix passed 26/26; treat this as a retained fixture-session diagnostic, not a Last-Seen or M3.5 regression.
- No rescue branch was required because every retained implementation checkpoint compiled, tested, committed, and reached the normal remote branch.

## Resume

```powershell
git fetch origin
git switch codex/m4p1-sightweave-subject-policy-last-seen
git pull --ff-only
& 'D:\UE_projects\LastLight\Scripts\BuildEditor.ps1' -EngineRoot 'D:\UE_5.8'
```

M4P1 is complete after recovery; no acceptance rerun is required unless later work changes its product or Lab paths. Begin the next milestone only from an explicit new task. Do not modify `/Game/Maps/L_Prototype` or the local `Darkwell.uproject` association while recovering this checkpoint.

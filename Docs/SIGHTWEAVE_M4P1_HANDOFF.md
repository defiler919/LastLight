# SightWeave M4P1 visual acceptance handoff

Status: **PARTIAL — automated closure complete; user interactive PIE pending**

- Branch: `codex/m4p1-sightweave-subject-policy-last-seen`
- Continuation baseline: `d2b27136288ecf11d96b6e09d7a2b2fea6dbaa05`
- Frozen M3.5 baseline: `941dcb75fe9fcda05bcb972a4d2d9651c6d521be`
- Timing-repair continuation baseline: `4e0b5e1ec93391467846028f1bf46fdf92e67ed3`
- Last source/test checkpoint before this documentation commit: `3e0ed45`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- GPU/RHI: NVIDIA GeForce RTX 4060, D3D12 / SM6; NullRHI also validated

## Outcome

M4P1 now has a real runtime-generated Lab fixture inside the PIE world, five stable cameras, six controllable subject states, CustomDepth/stencil-only Last-Seen proxies, D3D12/SM6 screenshots, exact pixel validation, agent image inspection, lifecycle coverage, and green M3.4/M3.5 regressions. No map asset was modified: the fixture is created and destroyed by the editor module around `/SightWeave/Maps/L_SightWeave_Lab` PIE.

The only remaining acceptance item is the user's interactive PIE inspection, which must be repeated for Camera 0-4 after the timing repair. Automated implementation, build, Lab, continuous-frame image, regression, severe-log, and remote-checkpoint gates are green. BuildPlugin, clean-host, Game Shipping, full SightWeave, DARKWELL, and the full performance matrix were explicitly outside this bounded M4P1 timing continuation and do not block this status.

## Timing-defect repair

Manual Camera 1 PIE exposed two defects that stable single screenshots could not see. State 1 jitter came from the post-tonemap proxy composite reading CustomDepth with temporal projection jitter. State 2's white ring came from rebuilding the entire fixture on every state change and then carrying incompatible temporal post-process history into the reacquired live frame. The old rebuild also relied on a transient source-registration side effect to keep the presentation page resident.

The repair keeps TAA, Width=50 feather, bloom, and quality enabled. `r.CustomDepthTemporalAAJitter=0` removes jitter only from the CustomDepth pass consumed after TAA. A presented immutable descriptor is now idempotent: the same revision, mesh/material paths, transform, and descriptor do not clear/reload/recreate the scene proxy. Reacquire hides the proxy before showing live, rejects old temporal history with UE's camera-cut flag, and retains monotonic source/residency generation ordering. Lab state transitions mutate only the primary authority/presentation objects; the comparison matrix, lights, descriptors, and transforms remain in place.

The new `SightWeave.M4P1.Visual.ContinuousTransition` D3D12/SM6 test captures 120 immediate State 1 frames plus 32 immediate State 1-to-2 frames. Final evidence: State 1 `count_min=count_max=1936`, bounds `(483,148)-(527,192)`, centroid max delta `0`; reacquire `proxy_only=0`, `black_handoff=0`, `live=32`, maximum proxy-color collision `0`, final live bounds `(475,141)-(535,200)`. The agent opened five remembered and four reacquire representative frames; no jitter, black hole, proxy residue, double image, or white ring was present.

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
- M4P1 full D3D12/SM6: 11/11, 0 warning, 0 failed; this includes the stable-frame and continuous-frame visual tests.
- Dedicated continuous transition D3D12/SM6: 1/1, 0 warning, 0 failed; 120 remembered + 32 reacquire frames.
- Dedicated M4P1 visual D3D12/SM6: 1/1, 0 warning, 0 failed, 28.73 s.
- M4P1 Lab: NullRHI 2/2 and D3D12/SM6 2/2, both with zero warnings/failures.
- M3.4 presentation D3D12/SM6: 3/3, 0 warning, 0 failed.
- M3.5 complete D3D12/SM6 after compatibility repair: 26/26, 0 warning, 0 failed.
- Five timing-repair final gate logs: zero severe Shader compiler, ShaderCompileWorker, Renderer/RenderCore, RDG, D3D12/RHI, GPU crash/hang/device-removal, fatal, assert, ensure, or failed-test matches.
- Eight final screenshots: `nonfinite=0`; exact neutral proxy pixels use RGB 117; every negative-region assertion reports zero leakage.

Canonical evidence paths are listed in `Docs/SIGHTWEAVE_M4P1_FINAL_VALIDATION.md`. All reports, logs, and screenshots live under `Saved` and are intentionally uncommitted.

## Preserved limitations and warnings

- User-operated Camera 0-4 interactive PIE rerun remains pending; this is the sole reason for `PARTIAL`.
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

After recovery, perform only the user PIE checklist above. Confirm Camera 0–4, states 0–5, no live/proxy double image on reacquire, black negative subjects, continuous page-boundary proxy, yaw-45 alignment, and no identity-reuse inheritance. Copy the final `M4P1 Lab state=...` and `Lab camera bound...` lines. Do not modify `/Game/Maps/L_Prototype` or `Darkwell.uproject`.

# SightWeave M3.3 hard-mask presentation handoff

Status: **IN_PROGRESS**

Branch: `codex/m3p3-sightweave-hard-mask-composite`

Frozen M3.2 baseline: `ccb4c02c7a0bcbd9295847f16da17985bd8fd39c`

Engine: Unreal Engine 5.8.1, changelist `56057345`, at `D:\UE_5.8`

Home validation GPU: NVIDIA GeForce RTX 2070 SUPER, Turing, 8192 MiB, Studio Driver 610.88

## Admission

Before branch creation, local HEAD, upstream, and `git ls-remote` all matched the frozen M3.2 baseline. The worktree was clean, Git LFS had no pending objects, `git lfs fsck` and `git diff --check` passed, and `Darkwell.uproject` was unchanged. M3.2 remains `COMPLETED`.

## Frozen M3.3 boundary

M3.3 closes only this presentation path:

`persistent sparse PF_G8 EffectiveLiveMask -> world-space view sampling -> post-tonemap hard composite -> live Scene Color / non-live black`

CPU geometry, compatibility, floor rules, source attribution, and suppression remain authoritative. The GPU mask is presentation-only and cannot become gameplay, AI, HUD, interaction, memory, Last-Seen, reveal, or save authority.

M3.3 does not implement gray memory, Last-Seen, subject proxies, damage reveal, formal feathering, noise/fog animation, final art grading, DARKWELL gameplay or `L_Prototype` integration, SceneCapture, GPU visibility solving, D3D11, Vulkan, or Fab final packaging.

## UE 5.8 post-process decision

The implementation will use the existing world-scoped `FWorldSceneViewExtension` and subscribe to `ISceneViewExtension::EPostProcessingPass::Tonemap`. In UE 5.8.1 `SceneViewExtension.h` documents this as the pass immediately preceding `BL_SceneColorAfterTonemapping`; `OpenColorIODisplayExtension` demonstrates the supported callback signature, `SceneColor` acquisition, and required `OverrideOutput` handling.

Scene depth comes from `FPostProcessMaterialInputs::SceneTextures`. World reconstruction uses the current `FSceneView` uniform buffer and UE's `SVPositionToTranslatedWorld` path, with explicit output-to-depth viewport mapping. Camera transform, projection, FOV/OrthoWidth, resolution, and screen-percentage changes execute only the composite and never rebuild the world atlas.

## Presentation contract

The implementation must create immutable revisioned bindings containing world lifetime, owner, floor, complete profile sequence or explicitly unioned effective scope, precision, resource/residency generation, packet/registry/snapshot revision, and presentation revision. Full canonical profile equality remains authoritative; hashes are accelerators only.

Missing, stale, contradictory, evicted, nonresident, wrong-generation, wrong-world, wrong-owner/floor, unsupported, unallocated, or teardown state fails the affected view black. No Scene Color passthrough, stale atlas, other scope, or SceneCapture fallback is permitted once SightWeave presentation is enabled.

## Planned checkpoints

1. `docs: start SightWeave M3P3 hard mask presentation`
2. `feat: bind SightWeave presentation scopes`
3. `feat: composite SightWeave hard masks`
4. `test: validate SightWeave screen-space presentation`
5. `test: add SightWeave M3P3 lab coverage`
6. `perf: measure SightWeave hard mask composite`
7. `test: validate SightWeave M3P3 packaging boundaries`
8. `docs: record SightWeave M3P3 validation`

Each reliable checkpoint is committed and pushed to the configured `origin`. No merge, rebase, force-push, ordinary-filesystem asset mutation, `Darkwell.uproject` change, generated-directory staging, prototype-map entry, SceneCapture path, or computer shutdown is allowed.

## Current verdict

**IN_PROGRESS**

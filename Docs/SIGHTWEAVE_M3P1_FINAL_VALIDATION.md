# SightWeave M3.1 Final Validation

Status: IN PROGRESS

Branch: `codex/m3p1-sightweave-render-single-tile`

Frozen baseline: `2cb3f82ab44e810a09f18bed036fa1e4d36db4aa`

## Scope

M3.1 is the first executable GPU presentation slice of the frozen M3 contract:

`CPU-authoritative polygons -> deterministic CPU triangles -> immutable packet -> SightWeaveRender -> RDG single-tile raster -> asynchronous test readback`

The implementation is restricted to one world, one knowledge owner, one floor, one compatibility profile, and one 256 x 256 physical tile (248 x 248 interior plus a four-texel gutter) at the Standard 10 cm/texel precision. The persistent result is a binary `PF_G8` EffectiveLive mask. D3D12 SM6 is the formal GPU path; NullRHI must remain safe and CPU Authority remains authoritative.

The following remain out of scope: complete sparse-atlas paging, multi-owner/floor/profile scheduling, final post-process presentation, feathering, memory, Last-Seen proxies, damage reveal presentation, DARKWELL gameplay/map integration, SceneCapture, and GPU visibility solving.

## Frozen formula

For the single compatibility profile:

- `Gated = Vision intersection CompatibleIllumination`
- `EffectiveLive = Gated union Bypass`
- `Final = EffectiveLive minus Suppression`

Hard-mask values are exactly 0 or 255. Sampling is point-filtered. Missing, invalid, stale, or failed input must produce black and must never retain an older world or revision.

## Planned evidence

- Immutable packet validation, deterministic hashing, stale/duplicate classification, and fail-closed validation.
- Deterministic bounded triangulation for convex and concave simple polygons, with explicit invalid-input handling.
- `SightWeaveRender` Runtime module loaded at `PostConfigInit`, `/Plugin/SightWeave` shader mapping, and dependency audit.
- World-scoped render lifecycle, NullRHI behavior, no-change behavior, and teardown/restart isolation.
- D3D12 SM6 single-tile raster and test-only asynchronous readback for the complete M3.1 matrix.
- CPU/GPU texel-center differential coverage and explicit boundary coverage evidence.
- Preliminary packet/GT/RT/pass/readback timing and capacity evidence.
- Full Editor and automation regression, Lab NullRHI/D3D12 smoke, BuildPlugin, clean-host Editor/Game Development/Game Shipping, Shipping isolation, and severe-log audits.
- Git, upstream, remote, and Git LFS closure.

## Evidence log

### 2026-08-26 - Recovery and baseline

- Verified the exact M3.0 baseline commit and its remote tracking branch before creating M3.1.
- Created and pushed `codex/m3p1-sightweave-render-single-tile` from `2cb3f82ab44e810a09f18bed036fa1e4d36db4aa`.
- The only pre-existing worktree difference is the local `Darkwell.uproject` EngineAssociation change. It is intentionally excluded from every M3.1 commit.
- Git LFS integrity passed at recovery.
- Read the frozen M3 contract, architecture, validation plan, M3.0 handoff, M2P.5 closure, vision requirements/architecture, repository guidance, and existing plugin modules.
- Audited the local UE 5.8.1 source before implementation; API findings will be recorded with the relevant implementation checkpoint.

## Final disposition

Pending implementation and validation. M3.1 must not be marked COMPLETED until every frozen completion condition is supported by retained evidence.

# DARKWELL Project Fog Visual Rebuild Contract

Date frozen: 2026-08-30

Branch: `codex/darkwell-project-fog-visual-rebuild`

Starting SHA: `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb`

Deadline: 2026-09-05

Status: **PARTIAL — P0 CONTRACT FROZEN**

## 1. Scope and ownership

This contract authorizes a DARKWELL-project-only fog presentation rebuild. It is not another patch to the rejected SightWeave visual layer, a SightWeave authority rewrite, plugin generalization, Fab publication, packaging or marketplace work.

SightWeave remains the reliable CPU authority/backend for legal vision sources and illumination, occlusion inputs, subject policy, `NeverRemember`, static-memory ownership, snapshots, persistence/restore, world lifecycle and gameplay tests. The new presentation layer belongs to the `Darkwell` module and consumes only the smallest durable authority interface it needs.

The following rejected visual paths remain in repository history but may not register or run in the formal candidate:

- post-TSR or pre-TSR SightWeave full-screen composite;
- CustomDepth/CustomStencil scene composition;
- discrete SurfaceMaterial presentation coverage;
- per-rendered-face wall sampling;
- the tile-mirror texture as the final Live edge.

No old source or asset is deleted merely to hide evidence.

## 2. First visual mode

The only implemented initial-knowledge policy in this phase is `RememberedFromStart`. The project-facing enum must reserve:

```text
EInitialKnowledgePolicy
    UnknownUntilExplored
    RememberedFromStart
    FullyLive
```

For the formal candidate in this phase:

```text
KnownCoverage = 1 everywhere in the presentation scope
UnknownWeight = 0
LiveWeight = LiveCoverage
RememberedWeight = 1 - LiveCoverage
```

There is no black Unknown, exploration accumulation, exploration persistence, locked black region or restored full three-state presentation in this phase. Those are independent future work and cannot be smuggled into this gate.

## 3. Continuous Live coverage

`LiveCoverage` must be a continuous world-space presentation field. It must not be generated from the CPU tile state, tri-state texture, dirty-tile rectangles, a bilinear upsample of discrete texels or the old visual tile mirror.

The field is derived from continuous body-circle and aim-cone geometry, legal illumination, and continuous occluder intersections. Static 2D wall/obstacle segments or polygons are cached. A valid visibility-polygon implementation may cast to each endpoint angle and its `+/- epsilon` neighbors, keep the nearest intersection, sort hits by angle, and clip by circle/cone. Multiple legal sources combine by GPU maximum/union. The formal path is GPU-only and performs no per-frame GPU readback.

Preferred coverage storage is linear `R16F`; `RG16F` is allowed when a second diagnostic channel is necessary. Samplers are clamped. Point sampling and forced mip zero are prohibited in the formal path. Derivatives, filtering and mip selection must match the actual presentation resolution and normal TSR path.

The edge must contain measured fractional coverage produced by real coverage evaluation such as MSAA resolve, analytic area coverage, controlled supersampling or a continuous distance field. Blur, a wider feather, threshold relaxation or interpolation of binary tile values is not proof.

P1 must prove sub-texel translations of `0.25`, `0.5` and `1.0` presentation texel horizontally, vertically and diagonally. Raw coverage must show intermediate values and a continuously moving edge, with no plateaus followed by jumps, world-grid periodic teeth or periodic flicker.

## 4. Occlusion and wall/object semantics

Free-space and rendered-surface visibility are different fields:

```text
FreeSpaceLiveCoverage
WallSurfaceLiveCoverage
```

Free space is evaluated against the continuous visibility polygon. Wall material coverage uses the wall's stable local geometry, never camera/ViewDirection and never a per-face outward decision. For a local wall point `P`, stable wall-local normal `N`, half thickness `H` and small exterior epsilon `E`:

```text
SideA = L(P + N * (H + E))
SideB = L(P - N * (H + E))
WallLive = max(SideA, SideB)
```

The maximum is legal only for that wall object's material classification. It must never brighten free space or floor behind the wall. If either side is legally observable, the entire local wall cross-section—top and both rendered sides—shares Live state. Coverage remains local along the wall tangent; seeing one segment may not light an arbitrarily long wall.

Boxes use their stable footprint, half extents and four outside directions to compute object-material coverage. The result applies to the object only. No surface rule depends on view direction, face winding, two-sided sign or north/south orientation.

The `L_VisionIntegration` proof fixture must include an axis-aligned wall, a rotated wall, a cube, a doorway, a wall end, a concave combination and a multi-wall intersection. Space behind each occluder must remain Remembered gray rather than Live.

## 5. Remembered appearance and information control

`RememberedFromStart` displays the real static scene BaseColor in a readable filtered form. It may be desaturated, darkened, reduced in contrast and have unstable high-frequency detail reduced, but walls, floor, doorways and large static objects must remain recognizable. It may not collapse to a flat gray or an invented replacement texture.

Formal Remembered input must not use current SceneColor, SceneCapture, dynamic-light/shadow history, enemy imagery, particles, time noise, random `frac`, or other live/dynamic history. Dynamic actors remain subject to SightWeave CPU authority and subject policy, are Live-only, and never leave a gray image. `NeverRemember` remains mandatory. Stalker and threat HUD use the same authoritative snapshot/revision; opacity is not a substitute for authority.

Supporting image-quality gates for a stable static region are correlation with the real BaseColor of at least `0.80` and standard-deviation/contrast retention of at least `35%`. These numbers assist inspection; human readability is controlling.

## 6. Temporal, resolution and rendering contract

The formal candidate runs with normal project TSR, D3D12/SM6, at 1920x1080 and 2560x1440. Disabling TSR is diagnostic only and cannot be the solution. Coverage, geometry and material inputs must use one declared world/resolution/temporal convention; no post-TSR/pre-TSR mixing or repeated jitter compensation is allowed.

The presentation must remain stable with a fixed player/camera, horizontal/vertical/diagonal translation, slow rotation, along-wall movement and visibility boundaries crossing static objects. It must not show tile stepping, periodic jaggies, periodic flicker, gray seams, black/gray offset, north/south asymmetry or opposite face states.

## 7. Phase gates and stop rules

- P0 freezes this contract, isolates the new branch and records the rejected candidate.
- P1 proves a high-contrast no-wall body circle and one cone with `KnownCoverage=1`, no occlusion, 1080p, D3D12/SM6 and normal TSR. Required evidence is 20 seconds static, 30 seconds horizontal, 30 seconds vertical, 30 seconds diagonal, 30 seconds rotation and raw sub-texel samples.
- P2 adds continuous occlusion and the complete proof fixture.
- P3 adds wall/object surface coverage and four-direction wall/doorway/box validation.
- P4 reconnects dynamic subjects, Stalker/HUD and Torch/Lantern/Torch without changing the continuous edge.
- P5 produces 1080p/1440p evidence and the user handoff only after P1–P4 pass.

If P1 exposes tile stepping or periodic jaggies, work stops before walls with `BLOCKED — CONTINUOUS LIVE COVERAGE PROOF FAILED`. P1 may receive at most two reliable implementation attempts. A second failure stops implementation, records evidence, and pushes the last reliable state.

P1 is due within 24 hours; the project vertical slice is due within 48 hours; the unconditional overall stop-loss date is 2026-09-05.

## 8. Acceptance authority

Agent automation, logs, screenshots, raw coverage values and frame metrics are engineering evidence only. The agent must open key frames, strips and contact sheets. Only the user's real dynamic PIE can accept visual usability.

The highest agent state after all gates is:

```text
PARTIAL — READY_FOR_USER_GRAY_LIVE_PIE
```

It is never `COMPLETED`. Unknown/exploration history is not part of this acceptance and may begin only as a separately authorized phase after the user accepts gray/live behavior.

## 9. Repository discipline

Builds and tests are serial. Do not run BuildPlugin, Fab checks, clean-host, the full historical SightWeave matrix, Cook, Package or a full performance matrix. Do not modify `Darkwell.uproject` or `L_Prototype`. `L_VisionIntegration` may be changed only through Unreal-safe asset tooling.

Never commit `Saved`, user videos, Binaries, Intermediate, DDC, AutomationReports or temporary diagnostics. Do not merge, rebase, reset, clean, force-push, delete unknown files or edit Unreal binary assets with ordinary filesystem tools. Keep the computer on.

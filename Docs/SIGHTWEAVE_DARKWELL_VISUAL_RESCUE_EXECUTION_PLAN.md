# SightWeave DARKWELL Visual Rescue Execution Plan

Date: 2026-08-29  
Branch: `codex/sightweave-darkwell-visual-rescue`  
Frozen starting SHA: `f364f780904c7ced5d649e7d582c3d91a7d43baf`  
48-hour dynamic prototype checkpoint: 2026-08-31  
Final stop-loss deadline: 2026-09-05  
Current status: **PARTIAL — USER_PIE VISUAL AND SEMANTIC ACCEPTANCE FAILED**

This plan is subordinate to `Docs/SIGHTWEAVE_DARKWELL_VISUAL_REQUIREMENTS.md`. It is a DARKWELL project rescue, not a general plugin, packaging, Fab, or publication milestone. The original automated evidence remains valid engineering evidence, but it cannot establish visual acceptance after the user's dynamic PIE rejection.

## 1. Evidence reviewed

The six images under ignored path `Saved/M6P1_GameViewEvidence` were opened at original resolution. They show large stair steps on diagonal and circular boundaries, an inward blur that preserves the underlying staircase, a uniform neutral-gray Remembered region without recognizable scene structure, discontinuities between Live and Remembered, and unknown black consuming visible wall surfaces. Static images cannot prove temporal stability; continuous dynamic evidence is required later.

The review also covered the frozen visual contract, the M6P1 execution/final-validation/handoff record, M3.4 formal-View/feather contracts, M3.5 static-environment-memory contracts, and the vision requirements and architecture documents. M3.4/M3.5 engineering evidence is retained, but the old visual-completion inference is superseded by the user PIE result and the new contract.

## 2. Root-cause attribution

### 2.1 The formal masks are too coarse for this camera

- `Source/Darkwell/Private/Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.cpp`, `UDarkwellSightWeaveWorldSubsystem::TryActivate`, explicitly configures both `ConfigureExplorationMemory` and `SetPresentationScope` as `ESightWeaveRenderPrecisionTier::Coarse`.
- `Plugins/SightWeave/Source/SightWeaveRuntime/Private/SightWeaveSparseAtlas.cpp`, `SightWeaveCentimetersPerTexel`, defines Coarse as 25 cm/texel, Standard as 10, Fine as 5, and Ultra as 2.5.
- `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveSparseAtlas.h` fixes a physical tile at 256x256, with a 248x248 interior and four-texel gutters. At Coarse, one interior tile covers 62 m. The observed screen-space staircase is the projected 25 cm hard cell boundary, not a missing post-process blur.
- `FSightWeaveSparseRenderPacketBuilder::WorldToLogicalTile` uses `floor((WorldPoint - FloorOrigin) / InteriorWorldSpan)`. CPU memory and static-environment rasterizers sample at `(row/column + 0.5) * CentimetersPerTexel`, so the half-texel convention is explicit, but the 25 cm quantization remains visible.

### 2.2 The feather operates on an already stair-stepped binary gate

- `Plugins/SightWeave/Shaders/Private/SightWeaveSingleTile.usf`, `SightWeaveIsHardLive` and `SightWeaveIsHardLiveLogicalTexel`, use integer page-table lookup and `Texture.Load`; there is no filtered visibility sample.
- The feather seed/jump/finalize entries reconstruct a distance from that hard logical mask. `SightWeaveVisualFeatherCompositePS` then hard-gates with `SightWeaveIsHardLive` before sampling `SightWeaveSampleVisualFeather`.
- The current 50 cm width in `USightWeaveSettings::VisualFeatherWidthCentimeters` is only two Coarse texels. It softens intensity inside the same binary staircase and cannot reconstruct a sub-cell boundary.
- The final expression is `SceneColor * VisualFeatherWeight`. On a Live-to-Remembered boundary it fades Live toward black and only then switches to the Remembered branch. That creates an avoidable dark seam even when the two authorities describe adjacent states.

### 2.3 Live, memory, and static eligibility are separate sampled authorities

- `SightWeaveIsHardLive` samples the live atlas with `TranslatedFloorOrigin` and `CentimetersPerTexel`; `SightWeaveSampleMemory` independently samples a memory atlas with `MemoryTranslatedFloorOrigin` and `MemoryCentimetersPerTexel`; `SightWeaveSampleStaticAttribute` samples a third page table/atlas.
- `Plugins/SightWeave/Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.cpp`, `MemoryScopeMismatchMask` and `MemoryScopeMatchesPresentationBinding`, require equal world identity, owner, floor, precision, origin, and profiles before memory presentation. That is a useful fail-closed guard, but the shader still makes separate state decisions and the final blend does not first resolve one mutually exclusive state.
- Black is not an authored state field. It is the fallback returned when neither independent branch succeeds. Thus rounding and the Live-to-black feather expression can expose black between otherwise adjacent Live and Remembered results.

### 2.4 Remembered is intentionally a flat 2D value, not a scene memory

- `Plugins/SightWeave/Source/SightWeaveRuntime/Private/SightWeaveStaticEnvironment.cpp`, `SightWeaveStaticEnvironmentPrivate::Rasterize`, rasterizes only `WorldFootprint` into one byte per XY texel and stores the maximum `Description.NeutralIntensity`. It contains no surface normal, material, depth, wall face, or captured static appearance.
- `Source/Darkwell/Private/Visibility/DarkwellVisionIntegrationFixture.cpp`, `ADarkwellVisionIntegrationFixture::BuildSightWeaveStaticSurfaces`, supplies only rectangular XY footprints and neutral intensities for the ground, two walls, and landmark.
- In `SightWeaveSingleTile.usf`, `SightWeaveIntersectFloorPlane` intersects the camera ray with `MemoryTranslatedFloorPlaneZ`, not the visible scene surface. `SightWeaveRememberedEnvironment` returns `float4(Attribute.xxx, 1)`. The pure-gray result is therefore the implemented data model, not a tuning accident.
- Dynamic objects are separately controlled through `ESightWeaveSubjectMemoryPolicy::NeverRemember`; the Stalker and threat HUD already consume the same hard-live snapshot/revision in `UDarkwellSightWeaveWorldSubsystem` and `ADarkwellHUD::DrawHUD`. This authority contract should be preserved while the scene presentation is replaced.

### 2.5 A floor-only XY lookup cannot classify visible wall faces

- `SightWeaveHardMaskCompositePS` and `SightWeaveVisualFeatherCompositePS` reconstruct the current visible position from `SceneDepthTexture.Load` and Unreal's `SvPositionToTranslatedWorld`, then reduce it to `.xy` for the live lookup.
- The fixture occluder is a zero-width 2D segment at the wall boundary. A wall top or player-facing vertical side is therefore classified only by its projected XY point relative to that segment. There is no ground/wall-top/wall-side/wall-behind classification.
- `SightWeaveRememberedEnvironment` discards the current scene depth and intersects the floor plane, so it cannot preserve wall side/top geometry. These are architectural surface-classification failures, not ordinary art parameters.

### 2.6 View and temporal handling are not the primary origin defect

- `FSightWeaveSceneViewExtension::SubscribeToPostProcessingPass` injects the supported after-Tonemap callback for the formal player View; no SceneCapture is used.
- `FSightWeaveSparseAtlasRenderState::AddHardMaskComposite_RenderThread` maps output, SceneColor, SceneDepth, and `ViewRectMinAndSize` separately, so screen-percentage/view-rect scaling is explicit.
- The floor origin comes from the registered floor bounds and is stable in world space; it is not snapped to camera tiles each frame. `PreViewTranslation` is applied before shader use, which is required for Unreal translated-world coordinates.
- `SvPositionToTranslatedWorld` uses `View.SVPositionToTranslatedWorld`, so the formal depth reconstruction follows the active temporal View. The project sets `r.CustomDepthTemporalAAJitter=0`, but the visibility masks themselves have no temporal reprojection or history. The observed crawl is primarily the high-contrast projection of a 25 cm point-sampled binary grid while the player/view moves. Temporal blurring of that grid would hide rather than solve the defect.

### 2.7 Legacy and SightWeave are mutually exclusive in the intended path

- `UDarkwellSightWeaveWorldSubsystem::TryActivate` calls `SetLegacyConsumersEnabled(false)` before registering SightWeave authority.
- `ADarkwellHUD::SetLegacyFogAuthorityEnabled(false)` sets the old blendable weight to zero, and `ADarkwellHUD::Tick` exits the legacy fog update when the authority is disabled.
- The rescue must preserve and test this exclusion. Re-enabling the legacy post-process is not an acceptable visual fix.

## 3. Minimal DARKWELL rescue design

### 3.1 Common state space

For the integration slice, both exploration memory and presentation will use `Ultra` (2.5 cm/texel) with the same registered floor origin, extent, 248-texel interior convention, four-texel gutters, and texel-center rule. The exact memory/presentation scope guard remains fail-closed.

The formal shader will resolve one state before composition:

```text
0 Unknown    = not Live and not eligible Remembered
1 Remembered = hard memory and explicit immutable static surface
2 Live       = hard live (highest precedence)
```

One helper will return this mutually exclusive state for the final visible surface. Unknown will be derived only from state 0, not sampled as a competing texture. Live, Remembered, and Unknown will share the same output/ViewRect/depth mapping and translated-world coordinate. Tests will assert precedence and that no input combination can produce a gap or overlap.

### 3.2 Edge reconstruction and stable sampling

- Raise the effective world-space mask resolution from 25 cm to 2.5 cm for this small DARKWELL slice. This changes the source representation rather than increasing blur.
- Preserve the stable world floor origin; do not introduce camera-relative or tile-snapped origins.
- Keep the existing page-table gutters and logical-neighbor-aware jump-flood distance reconstruction, but run it on the Ultra source field.
- Replace the Live-to-black expression with a transition from the resolved lower-priority state (`Remembered` scene when eligible, otherwise Unknown black) to Live SceneColor. The hard gameplay result remains authoritative; the continuous weight is presentation-only.
- Use the same resolved-state helper for the hard and feather paths. Any conservative boundary sample must use the same origin, centimeters-per-texel, and integer/half-texel convention.
- No history blur is planned for the first prototype. At 2.5 cm, world-space stabilization plus continuous distance reconstruction should remove the visible cell jumping without ghosting. If sequential D3D12 frames still crawl, stop and record the failure before considering reprojection.

Primary entries: `SightWeaveIsHardLiveLogicalTexel`, `SightWeaveIsHardLive`, `SightWeaveSampleMemory`, `SightWeaveSampleVisualFeather`, `SightWeaveHardMaskCompositePS`, and `SightWeaveVisualFeatherCompositePS` in `SightWeaveSingleTile.usf`; parameter binding remains in `FSightWeaveSparseAtlasRenderState::AddHardMaskComposite_RenderThread`.

### 3.3 Filtered static Remembered scene

The integration fixture's ground, wall components, and landmark are immutable during this slice. They will be explicitly classified through CustomDepth/Stencil; the Stalker, player, particles, and other dynamic objects will not receive this class. The existing project configuration already enables stencil (`r.CustomDepth=3`) and stable custom-depth sampling (`r.CustomDepthTemporalAAJitter=0`). Reserved stencil values will not overlap the existing LastSeen proxy stencil.

The formal player-View composite will compare classified CustomDepth with SceneDepth, reconstruct the actual static surface position, and synthesize a parameterized grayscale/low-contrast/low-information static rendering from immutable class, depth/geometry orientation, and stable world-space detail. It will not sample current full SceneColor for Remembered, so enemies, particles, dynamic shadows, real-time lights, animation, and unobserved dynamic changes cannot leak. The original static-attribute atlas remains an explicit immutable eligibility/intensity gate.

Parameters to add under `USightWeaveSettings` and bind through the render shader parameters:

- remembered brightness;
- remembered contrast;
- remembered detail strength and world scale;
- static-environment stencil and occluder-surface stencil;
- conservative visible-surface bias in centimeters.

This is deliberately project-specific. It preserves recognizable ground planes, wall tops/sides, and the landmark for `L_VisionIntegration`; it is not claimed as a general material snapshot system.

### 3.4 Wall surfaces and black-behind-wall rule

Ground/static-environment and occluder-surface stencils will be distinct. For a pixel whose current scene depth matches a classified occluder surface, the shader may conservatively test adjacent visibility texels within the configured small world bias. This dilation applies only to the wall surface classification, never to ground or the region behind it. Therefore the player-facing wall side/top can resolve Live (or Remembered after observation), while the ground immediately behind remains Unknown. Door openings and wall ends continue to use the actual 2D visibility polygon; they are not filled by a global dilation.

Primary project files: `Source/Darkwell/Private/Visibility/DarkwellVisionIntegrationFixture.cpp` and its header. No change is planned to `/Game/Maps/L_Prototype`; no binary map change is planned unless runtime component classification proves insufficient.

## 4. Minimal verification before user PIE

All UBT, Editor, shader compile, and automation processes will run serially.

1. Build `DarkwellEditor Win64 Development` with `Scripts/BuildEditor.ps1` after each reliable C++ checkpoint.
2. Run only focused state-resolution, coordinate/scope, dynamic NeverRemember, wall-classification, and Legacy-exclusion tests. Do not run the complete NullRHI/D3D12 history, BuildPlugin, Cook, Package, clean-host, or performance matrix.
3. Launch `L_VisionIntegration` with real D3D12/SM6 and the formal player View.
4. Capture 1080p and 1440p metadata, stills, and consecutive frames under ignored `Saved/SightWeaveVisualRescueEvidence` for wall translation, slow cone rotation, circle+cone movement, doorway crossing, wall approach, Live/Remembered transitions, torch toggle, Stalker/HUD agreement, and camera follow.
5. Open every key still and inspect consecutive frames. Static captures cannot establish no-jitter; if video cannot be inspected reliably, compare opened frame sequences and record that limitation.
6. Scan logs for fatal, assert, ensure, GPU crash, and device removal.

The visual handoff gate is: no large block staircase, no blurred staircase substitute, no obvious slow-motion grid jump/crawl, recognizable non-flat Remembered structure, no dynamic leakage, visible wall surfaces with black behind them, no black seam, aligned states, correct UI, no legacy double composite, and stable D3D12/SM6 execution. Passing this gate only permits **PARTIAL — READY_FOR_USER_DYNAMIC_PIE**.

## 5. Checkpoints and stop rules

Planned reliable checkpoints:

1. `docs: record SightWeave visual rescue root cause`
2. `render: unify DARKWELL fog state reconstruction`
3. `render: stabilize smooth SightWeave visibility edges`
4. `render: restore filtered static remembered scene`
5. `fix: classify visible occluder surfaces`
6. `test: prepare DARKWELL dynamic fog visual acceptance`

Each source checkpoint must compile, pass its focused check, contain no unrelated/generated/binary changes, and be pushed normally before the next risky step. `Darkwell.uproject`, `L_Prototype`, Saved/Binaries/Intermediate/DDC/AutomationReports, merge, rebase, reset, clean, and force-push remain prohibited.

If an obviously improved dynamic D3D12 prototype is not available by 2026-08-31, stop expanding implementation, retain honest evidence, push only reliable checkpoints, and mark the candidate BLOCKED or REJECTED_CANDIDATE. By 2026-09-05 the only final product decisions are `ACCEPTED — DARKWELL USABLE` or `REJECTED — ABANDON SIGHTWEAVE`; PARTIAL cannot be extended indefinitely.

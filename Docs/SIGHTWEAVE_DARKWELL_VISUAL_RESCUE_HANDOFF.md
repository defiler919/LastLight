# SightWeave DARKWELL Visual Rescue Retest Handoff

Date: 2026-08-29

Branch: `codex/sightweave-darkwell-visual-rescue`

User-rejected candidate baseline: `2439cfb0de843ab52b9c989439272f1e30727d1c`

Validated implementation SHA: `eb2a827`

Stop-loss deadline: 2026-09-05

Status: **PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST**

The user's first real dynamic PIE rejected the prior candidate for whole-game shaking/flicker and multiple thin gray lines. That verdict remains recorded. This handoff offers a new two-blocker candidate for the required second real dynamic PIE; it does not claim `COMPLETED` or visual acceptance.

## What was fixed

### Whole-View shaking and flicker

The formal post-process path mixed jittered SceneDepth reconstruction with unjittered CustomDepth/Stencil and stable post-TSR output coordinates. A fixed player and camera therefore still crossed surface and state boundaries as projection jitter changed.

The composite now maps through the exact current `ViewRect.Min` and ViewRect size, applies the current temporal projection jitter to SceneDepth sampling/reconstruction, and keeps CustomDepth/Stencil on the unjittered coordinate required by `r.CustomDepthTemporalAAJitter=0`. This corrected path is the default with normal TAA/TSR in Development, Test, and Shipping. The legacy coordinate path is available only as a Development/Editor diagnostic A/B.

### Gray lines and residual geometry leakage

The gray lines were false Remembered authority, not SceneColor, a wall-color problem, CustomDepth noise, Stencil leakage, wall conservative sampling, or incremental update churn. The memory scanline rasterizer clamped a wholly out-of-tile interval before testing intersection, collapsing the empty interval into one permanent tile-edge texel. Across tiles, those texels became long world-aligned lines.

The rasterizer now rejects non-intersecting intervals before clamping. A targeted concave cross-tile test freezes the exact failure. No black threshold, mask expansion, blur, color cover, or blanket wall suppression is part of the fix.

## Controlled A/B result

- Composite bypass: raw SceneColor is stable; the old gray line is absent.
- Old coordinates: settled adjacent-frame MAD median `0.1544` for the scene and `1.2319` in the gray-line band.
- Correct coordinates: `0.0134` and `0.0177` respectively.
- Remove Remembered: line disappears.
- Remove CustomDepth/Stencil: line remains.
- Unified state: line is Remembered.
- Raw memory: line present before the fix and absent after it.
- Raw static attributes, CustomDepth, and CustomStencil: no matching false line.
- Force full updates / disable wall bias: line remains before the memory fix.
- AA-off and startup mask-freeze were diagnostic only and are not accepted formal paths; the mask freeze occurred before valid authority and was treated as inconclusive.

## Preserved requirements

The candidate retains all accepted improvements:

- Ultra 2.5 cm/texel Live and Remembered scope;
- one mutually exclusive Unknown / Remembered / Live state;
- three-dimensional filtered Remembered scene;
- `NeverRemember` Stalker filtering and shared Stalker/HUD authority;
- wall-surface classification, readable player-facing wall surfaces, and black behind walls;
- black/gray coordinate alignment;
- Torch/Lantern revision continuity.

No `.uasset`, `.umap`, `L_VisionIntegration`, `L_Prototype`, configuration, plugin descriptor, or `Darkwell.uproject` change is part of this candidate.

## Verification completed

- `DarkwellEditor Win64 Development`: succeeded serially after the final source changes.
- `SightWeave.M3P5.Memory.Authority.ConcavePolygonDoesNotLeakAtTileEdge`: Success.
- `Darkwell.SightWeave.M6P1.Integration.VerticalSliceAuthority`: Success after the fixes, including Ultra scope, `NeverRemember`, Stalker/HUD shared authority, wall occlusion, Lantern transition, and Torch restoration.
- Formal dynamic runs: D3D12/SM6 with normal TSR at exact 1920x1080 and 2560x1440 ViewRects.
- Static five-second adjacent-frame MAD: 1080p median/p95 `0.0125/0.0197`; 1440p `0.0199/0.0245`.
- Old isolated-horizontal-line signature: `409–410` pixels per rejected frame at 640x360 analysis resolution.
- New startup/rotation/wall/tool-cycle evidence: 1,410 frames inspected; maximum `6` at 1080p and `2` at 1440p; zero frames at or above 300.
- No full-black frame in startup, rotation, wall, or Torch/Lantern/Torch captures.
- All formal logged frames use the corrected coordinate path and have `bindingFailure=0`.

UE 5.8 emits pre-existing experimental Toolset/Python startup errors before gameplay. They are retained in the logs. After initialization there is no SightWeave/Darkwell error, fatal, assert, ensure, GPU crash, device removal, DXGI device error, or D3D12/RHI error.

## Ignored evidence paths

All recordings and logs remain under ignored `Saved/SightWeaveVisualRescueEvidence`; none are committed.

- Original user failure: `UserDynamicPIEFailure/Darkwell - 虚幻编辑器 2026-08-29 18-35-37.mp4`
- 1080p first five gameplay seconds: `Dynamic/1080p_startup/startup.mp4`
- 1080p five-second static: `Dynamic/1080p_static/static.mp4`
- 1080p ten-second slow rotation: `Dynamic/1080p_rotate/rotate.mp4`
- 1080p ten-second controlled wall motion: `Dynamic/1080p_wall/wall.mp4`
- 1080p Torch -> Lantern -> Torch: `Dynamic/1080p_torch_cycle/torchcycle.mp4`
- 1440p five-second static: `Dynamic/1440p_static/static.mp4`
- 1440p ten-second slow rotation: `Dynamic/1440p_rotate/rotate.mp4`
- A/B captures and raw atlas views: `AB`
- Targeted test logs: `TargetedMemoryLeakTest.log`, `TargetedVerticalSliceTest.log`

## User retest setup

1. Restore the pushed branch:

   ```powershell
   git switch codex/sightweave-darkwell-visual-rescue
   git pull --ff-only
   ```

2. Open `D:\UE_pro\Darkwell\Darkwell.uproject` in Unreal Engine 5.8.1.
3. Open `/Game/Maps/L_VisionIntegration`.
4. Use the normal D3D12/SM6 editor configuration with TAA/TSR enabled.
5. Start normal PIE and choose `NEW GAME`.

## Required second dynamic acceptance sequence

1. Watch the first five seconds after gameplay appears. Confirm no horizontal gray lines appear and the whole game View is stable while the editor UI also remains stable.
2. Hold the player and camera still for at least five seconds. Check the circle, both cone edges, Remembered surfaces, walls, and doorway for flicker.
3. Slowly sweep aim left and right for at least ten seconds. Confirm no edge shaking, jumping, crawl, or flash.
4. Approach each wall head-on, then strafe along it. Confirm player-facing wall surfaces remain readable, Unknown begins behind them, and no ground line leaks through Unknown.
5. Move through the doorway and around wall ends. Confirm no black/gray seam returns.
6. Leave and re-enter explored areas. Confirm recognizable static Remembered structure and clean Remembered <-> Live transitions.
7. Tap and release `E` to select Lantern, then repeat to restore Torch. Confirm no full-screen black frame or stuck fog.
8. Aim toward and away from the Stalker. Confirm the Stalker and red threat HUD appear/disappear together and never leave a Remembered enemy image.
9. Continue normal movement and turning for three to five minutes. This subjective duration test cannot be replaced by agent evidence.

The user must explicitly confirm all of the following: no gray lines, no shaking, no flicker, acceptable circle/cone edges, and acceptable Remembered visuals. Until that confirmation, the status remains `PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST`. On rejection, remain within DARKWELL project usability and apply the 2026-09-05 stop-loss rule; do not pivot to plugin generalization or Fab work.

## Evidence and implementation record

- Frozen contract: `Docs/SIGHTWEAVE_DARKWELL_VISUAL_REQUIREMENTS.md`
- Detailed report: `Docs/SIGHTWEAVE_DARKWELL_VISUAL_RESCUE_REPORT.md`
- Failure capture checkpoint: `6d9e7db`
- Diagnostic checkpoint: `800264e`
- Temporal-coordinate fix: `37c5f0c`
- Memory-line fix: `eb2a827`

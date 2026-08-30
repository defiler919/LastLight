# SightWeave DARKWELL Visual Rescue Retest Handoff

Date: 2026-08-29

Branch: `codex/sightweave-darkwell-visual-rescue`

User-rejected candidate baseline: `2439cfb0de843ab52b9c989439272f1e30727d1c`

Second user-rejected candidate baseline: `2883cd5d9f68044c71da785eaaa90f03fff4193c`

Validated Remembered stabilization SHA: `bac0525`

Unattended B0/B1 continuation starting SHA: `e4d654b74e3557ebefa328986bb626dcbfde0301`

Validated temporal-coherent project candidate SHA before final documentation: `4e50e44`

Stop-loss deadline: 2026-09-05

Status: **PARTIAL — USER_DYNAMIC_PIE_RETEST_4_FAILED / SCREEN_SPACE COMPOSITION REJECTED FOR PRODUCTION**

## Fourth user dynamic PIE disposition (2026-08-30)

Retest 4 did not accept the screen-space production path. Static stability improved, but horizontal movement still shakes cube left/right edges and side surfaces, vertical movement still shakes cube top/bottom edges, and the visible failure follows motion direction. Remembered remains too gray and must instead preserve clearly recognizable real floor texture and the filtered appearance of the real static 3D scene.

The next and only authorized project-use prototype is stencil-free `SurfaceMaterial` on `L_VisionIntegration`. SightWeave authority, GPU mirror, memory, Adapter, `NeverRemember`, Stalker/HUD shared revision, wall rules, and all already-passed line/seam fixes remain frozen. Both post-TSR and pre-TSR full-screen production composites must be disabled for the candidate; CustomDepth/Stencil may remain diagnostic only and may not determine scene-object outlines.

Do not return to screen-space UV, stencil offset, blur, feather, threshold, or dilation repair. Success can only become `PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST_5`; prototype failure requires `BLOCKED — SURFACE MATERIAL VISUAL PROTOTYPE FAILED / SIGHTWEAVE ABANDONMENT REVIEW REQUIRED`.

## 2026-08-30 superseding interpretation

The prior proof's files, metrics, failures, and stop decision are retained. Its B path was specifically:

```text
BeforeDOF
    + jittered pre-TSR SceneColor / SceneDepth
    + unjittered point-sampled CustomDepth / CustomStencil
```

Accordingly, `PRE_TSR_ARCHITECTURE_PROOF_FAILED` is no longer the controlling general conclusion. The accurate state is:

```text
CURRENT PRE_TSR PROTOTYPE FAILED
PRE_TSR ARCHITECTURE NOT YET FALSIFIED
```

UE 5.8.1 source and project configuration explain the mismatch: the engine CVar defaults to temporal jitter enabled and is render-thread-safe, while both DARKWELL and the plugin currently set `r.CustomDepthTemporalAAJitter=0` for the older post-TAA consumer. With zero, CustomDepth and CustomStencil share an unjittered pass projection for both Nanite and non-Nanite paths. The continuation must first run B0 without any CustomStencil read, then run temporal-coherent B1 only if B0 is stable. No fourth user PIE candidate exists yet.

The user's first real dynamic PIE rejected the prior candidate for whole-game shaking/flicker and multiple thin gray lines. The second real dynamic PIE at `2883cd5d9f68044c71da785eaaa90f03fff4193c` confirms that those lines and the black/gray offset are gone, flow is acceptable, and the large staircase has not returned. It nevertheless rejects that candidate because the Remembered gray scene continuously shakes while Live and editor UI are relatively stable. That verdict remains recorded; the new candidate below is offered only for a third user retest and does not claim `COMPLETED` or visual acceptance.

## Third retest failure now in force

The third real dynamic PIE rejected candidate `acd1c5b8ae9950423aa9575c639e034b9ce21dd7`. The user confirmed that the gray horizontal-line repair, black/gray alignment, large-staircase improvement, three-dimensional Remembered scene, enemy filtering, and acceptable frame/input performance are preserved. However, wall edges still visibly shake when player and camera are completely still, and the same wall edges continue to shake in Remembered.

The third recording is failure evidence. The prior gray-interior ROI did not include the wall depth discontinuity and cannot establish visual acceptance. The formal post-TSR/post-Tonemap architecture is rejected: deleting `ddx`/`ddy`/`frac` and stabilizing the internal gray values did not stabilize the hard semantic boundary that SightWeave creates after TSR history has already completed.

The next and only authorized slice is `SightWeave pre-TSR composition architecture proof`. It must compare the rejected post-TSR path against a Development/Editor-controlled pre-TSR path in `/Game/Maps/L_VisionIntegration`, with D3D12/SM6, normal TSR and temporal jitter, at 1080p. The proof must measure the same Live and Remembered wall depth boundary rather than another gray-interior ROI. It may use a clearly isolated non-production fixed static gray input only to prove boundary stability; that input cannot satisfy the Remembered visual contract.

Success is capped at `PARTIAL — PRE_TSR_ARCHITECTURE_PROVEN / READY_FOR_USER_DYNAMIC_PIE_RETEST_4`. A pre-TSR path that still visibly jitters requires `BLOCKED — PRE_TSR_ARCHITECTURE_PROOF_FAILED`, identification of the earliest changing input, and a stop rather than another local tuning patch. No production migration, full regression, or `COMPLETED` claim is authorized by either outcome.

## Pre-TSR architecture proof failed

The B path moved the first final Unknown/Remembered/Live boundary from the Tonemap after-pass to the `BeforeDOF` scene-view-extension callback, before Unreal's normal `AddMainTemporalSuperResolutionPasses`. It was Development/Editor-only, limited to `L_VisionIntegration`, used normal D3D12/SM6 TSR, and forced a fixed neutral non-production Remembered input by default. It changed no CPU authority, Adapter, persistence, map, asset, configuration, plugin descriptor, or `Darkwell.uproject` contract.

At project-normal 1080p TSR, A consumed 1920x1080 post-TSR/post-Tonemap SceneColor while depth/stencil remained 1400x792. B wrote the semantic result into a 1400x788 active primary rect backed by 1400x792 SceneColor/depth/stencil textures, then allowed TSR to produce 1920x1080 and Tonemap normally. This verified the intended ordering, but B still failed the static wall gate.

The same left Live wall ROI measured MAD p95 `0.035442`, edge range `0 px`, and `0` one-pixel flips for A versus `0.540915`, `3 px`, and `12` flips for B. The right B wall recorded p95 `0.525111`, range `3 px`, and `34` flips. After priming the same wall into Remembered, A measured p95 `0.143064`, range `0 px`, and `0` flips; B measured p95 `1.119474`, range `2 px`, and `86` flips. These agent captures do not overrule the user's rejection of A; they prove only that B is not better and fails independently.

Fixed camera/player logs held Mask and resource authority constant: `stateRevision=8`, `featherRevision=8`, `staticClassVersion=4`, no tile submission/update, and `bindingFailure=0`. Raw CustomDepth had zero detected edge movement. The first changing categorical stage was CustomStencil/surface classification: raw CustomStencil had a one-pixel range and 46 flips, surface-classification alpha had one-pixel range and 2 flips, and the unified state output retained 32 flips. The evidence therefore points to the interface where an unjittered point-sampled categorical classification boundary is written into a jittered pre-TSR color buffer without the complete raster/velocity/rejection/reactive-history semantics of normal geometry. No alternate jitter-sign experiment or tuning patch was attempted.

Build and focused gates passed: two serial `DarkwellEditor Win64 Development` builds; pass-placement/temporal-space; three-state truth table; M6P1 wall, `NeverRemember`, Stalker/HUD and tool authority; and D3D12/SM6 three-state composite. Twelve dynamic proof logs had no severe error; the only text hit was `r.GPUCrashDebugging:0` configuration. Gray horizontal lines and black/gray offset were not observed in the opened proof contacts, and the targeted authority gate retains enemy filtering, but no fourth visual candidate is offered.

Evidence is ignored under `Saved/SightWeaveVisualRescueEvidence/Dynamic/PreTSRProof_*`; the critical A/B directories contain video, 300 consecutive PNG frames, logs, wall contact sheets/crops, and boundary JSON. No evidence or generated directory is committed.

The task stops here as required. There is no `READY_FOR_USER_DYNAMIC_PIE_RETEST_4` status and no fourth PIE checklist because no candidate passed the architecture proof. The next decision requires new user authorization beyond this stopped proof; it must not silently become another jitter-compensation, blur, threshold, or local patch cycle.

## Second retest failure now in force

- Recording: `Darkwell - 虚幻编辑器 2026-08-29 21-07-46.mp4`.
- Ignored evidence copy: `Saved/SightWeaveVisualRescueEvidence/UserDynamicPIERetest2Failure`.
- SHA-256: `974A5A879D693938BF4E5C52ECF279266A43DA00DB95BE2C53B2B60FB68BB989`.
- Agent frame inspection: stable editor chrome, comparatively stable Live wedge, and changing Remembered floor-grid/static-object interior values across consecutive frames.
- Preserved passes: no gray lines, no black/gray offset, no large staircase recurrence, acceptable flow, correct wall and enemy rules, and working Torch/Lantern/Torch recovery.

The composite is registered as a Tonemap after-pass. Unreal has already executed normal TSR before Tonemap, so SightWeave reads post-TSR/post-tonemap SceneColor while directly reading current primary-resolution SceneDepth and CustomDepth/Stencil. Controlled A/B selected stable immutable post-TSR Remembered shading rather than moving the formal pass. No overall performance work or unrelated feature work was performed.

## Remembered stabilization offered for third retest

The state Mask and static-surface classification were stable. The rejected internal gray crawl came from the old post-TSR Remembered shading: it recomputed a depth-derivative normal and a discontinuous `frac` world grid every frame after TSR history had already completed. Camera follow and slow aim rotation therefore changed internal gray values while the state/surface contour stayed fixed.

The formal pass remains post-TSR and post-tonemap. Remembered now uses only immutable static-attribute data, immutable stencil class, and a continuous cosine detail term anchored to the memory floor origin. It does not read current SceneColor, GBuffer lighting, dynamic shadows, particles, enemy data, or a SightWeave temporal history. No blur, mask expansion, resolution reduction, or TSR disable is part of the fix.

Development/Editor A/B established:

- unified state and surface-classification contours are stable;
- fixed gray removes the internal crawl but is rejected as an information-destroying solution;
- current SceneColor carries temporal/dynamic information and is rejected;
- freezing revisions does not explain the failure and startup freeze remains inconclusive;
- TSR-off is diagnostic only and restores unacceptable aliasing;
- pre-TSR composition changes exposure and introduces temporal grain;
- post-TSR stable immutable shading retains the accepted visual/information rules and is the chosen path.

Focused build/tests succeeded: `DarkwellEditor Win64 Development`, `SightWeave.M3P5.Packaging.RememberedTemporalSpace`, `SightWeave.M3P5.Composite.ThreeStateAndMemoryFailure.D3D12`, and `Darkwell.SightWeave.M6P1.Integration.VerticalSliceAuthority`. Formal D3D12/SM6 normal-TSR evidence was recorded at 1080p and 1440p. Static Remembered internal-material p95 adjacent-frame MAD is `0.001034` at 1080p and `0.001632` at 1440p. No severe log signature, gray horizontal line, black/gray offset, stuck-black tool transition, wall-rule regression, or enemy-filter regression was found.

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
- Second user failure: `UserDynamicPIERetest2Failure/Darkwell - 虚幻编辑器 2026-08-29 21-07-46.mp4`
- Third-retest 1080p static: `Dynamic/Retest3Formal_1080p_Static10/static.mp4`
- Third-retest 1080p rotation/floor boundary: `Dynamic/Retest3Formal_1080p_Rotate15/rotate.mp4` and `boundary_floor_10s.mp4`
- Third-retest 1080p wall translation/static-object boundary: `Dynamic/Retest3Formal_1080p_Wall15/wall.mp4` and `boundary_wall_static_10s.mp4`
- Third-retest 1080p Torch/Lantern/Torch: `Dynamic/Retest3Formal_1080p_TorchCycle/torchcycle.mp4`
- Third-retest 1440p static: `Dynamic/Retest3Formal_1440p_Static10/static.mp4`
- Third-retest 1440p rotation: `Dynamic/Retest3Formal_1440p_Rotate15/rotate.mp4`
- Remembered temporal-space and final authority logs: `Retest2RememberedTemporalSpaceTest.log`, `Retest2RememberedCompositeD3D12.log`, and `Retest3VerticalSliceAuthority.log`

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

## Required third dynamic acceptance sequence

1. Hold player and camera completely still for at least ten seconds. Compare editor UI, Live, Remembered floor detail, static-object outlines, and the Live/Remembered boundary. Confirm Remembered does not pulse or shake.
2. Translate for at least fifteen seconds with camera follow. Confirm Remembered remains world anchored and does not slide relative to its static-object contours.
3. Slowly rotate aim for at least fifteen seconds. Check the entire gray region, not only the boundary; confirm no internal crawl, jump, or flash.
4. Sweep the boundary across floor for at least ten seconds, then across wall surfaces and static objects for at least ten seconds. Confirm no gray line, seam, or residual geometry appears in Unknown.
5. Approach each wall head-on, then strafe along it. Confirm player-facing wall surfaces remain readable, Unknown begins behind them, and no ground line leaks through Unknown.
6. Move through the doorway and around wall ends. Confirm no black/gray seam returns and Remembered remains a recognizable filtered static scene rather than a flat gray fill.
7. Tap and release `E` to select Lantern, then repeat to restore Torch. Confirm no full-screen black frame or stuck fog.
8. Aim toward and away from the Stalker. Confirm the Stalker and red threat HUD appear/disappear together and never leave a Remembered enemy image.
9. Continue normal movement and turning for three to five minutes. This subjective duration test cannot be replaced by agent evidence.

The user must explicitly confirm all of the following: no gray lines, no black/gray offset, no Remembered shaking or flicker, acceptable circle/cone edges, acceptable recognizable Remembered visuals, preserved wall rule, and preserved enemy filtering. Until that confirmation, the status remains `PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST_3`. On rejection, remain within DARKWELL project usability and apply the 2026-09-05 stop-loss rule; do not pivot to plugin generalization or Fab work.

## Evidence and implementation record

- Frozen contract: `Docs/SIGHTWEAVE_DARKWELL_VISUAL_REQUIREMENTS.md`
- Detailed report: `Docs/SIGHTWEAVE_DARKWELL_VISUAL_RESCUE_REPORT.md`
- Failure capture checkpoint: `6d9e7db`
- Diagnostic checkpoint: `800264e`
- Temporal-coordinate fix: `37c5f0c`
- Memory-line fix: `eb2a827`
- Second failure capture: `434f21e`
- Remembered stabilization: `bac0525`
- Third user rejection record: `5c0049f`
- Pre-TSR architecture prototype: `12b1118`
- Pre-TSR diagnostic proof: `05d2f03`

## Fourth dynamic PIE handoff (2026-08-30)

### Candidate state

`PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST_4`

The selected project path is temporal-coherent B1 followed by formal pre-TSR migration. B0 first proved that `BeforeDOF + jittered SceneColor/SceneDepth + normal TSR` is stable without a Stencil read. B1 then enabled jittered CustomDepth/Stencil and showed a large improvement over the failed unjittered B. The stencil-free Phase D was not entered.

The formal `L_VisionIntegration` pass now runs at `BeforeDOF`. SceneColor, SceneDepth, CustomDepth, and CustomStencil share the jittered primary View space; normal TSR stabilizes the final result. Runtime evidence proves `r.CustomDepthTemporalAAJitter=1` and `bindingFailure=0`. B0 fixed gray and the rejected post-Tonemap path remain only behind Development/Editor diagnostics. Shipping has no diagnostic selection path.

### What the agent-side gate established

- 20-second static Remembered, 30-second slow rotation, and 30-second wall movement at both 1080p and 1440p under D3D12/SM6 and normal TSR.
- 20-second startup and Live wall controls, Torch/Lantern/Torch, and a verified 600-second continuous 1080p run.
- opened contact sheets and adjacent-frame crops show no Remembered whole-layer shake, horizontal gray line, black/gray offset, or stuck-black tool state.
- Remembered internal-material p95 adjacent-frame MAD is `0.127614/255` at 1080p and `0.127936/255` at 1440p; static-edge p95 is `0.165304/255` and `0.176164/255`.
- full DARKWELL is 29/29; final full SightWeave NullRHI is 198/198; full SightWeave D3D12 is 287/290 with three retained legacy M4P1 Lab pixel-baseline failures.
- Editor, Game Development, and Game Shipping builds succeeded.

This evidence does not replace user PIE and does not justify `COMPLETED`.

### Required user retest four

1. Restore the branch with `git switch codex/sightweave-darkwell-visual-rescue; git pull --ff-only`.
2. Open `D:\UE_pro\Darkwell\Darkwell.uproject` in UE 5.8.1 and load `/Game/Maps/L_VisionIntegration`.
3. Use normal D3D12/SM6 and TSR. Do not set any `r.SightWeave.Diagnostic.*` CVar.
4. Start normal PIE, select `NEW GAME`, and hold player and camera completely still for 20 seconds. Confirm Live walls, Remembered walls/floor, and their boundary do not shake or flash.
5. Move laterally along both walls for 30 seconds, then slowly rotate left/right for 30 seconds. Confirm gray detail stays attached to geometry and no one-pixel wall flashing is visible.
6. Cross the doorway and pass both wall ends. Confirm the wall surface remains readable, black begins behind it, and no gray line or black/gray seam appears.
7. Sweep Live to Remembered to Live across the floor, wall, and static landmark. Look for ghosting, gray residue in Unknown, or loss of recognizable static structure.
8. Switch Torch to Lantern and back to Torch. Confirm legal light disappears and returns without a stuck-black frame.
9. Aim toward and away from the Stalker. Confirm Stalker and threat HUD appear/disappear together and no enemy image remains in Remembered.
10. Repeat at 1080p and 1440p, then continue normal movement for at least five minutes.

Accept only if the user explicitly confirms: no gray line, no black/gray offset, no Remembered shaking/flicker, acceptable circle/cone edge, acceptable static Remembered information, preserved wall rule, preserved enemy filtering, and acceptable flow. Otherwise stop under the 2026-09-05 abandonment rule; do not begin a fifth post-process patch cycle.

### Evidence and known limits

Primary ignored evidence is under `Saved/SightWeaveVisualRescueEvidence/Dynamic/FormalJ1_*`; raw Stencil proof is under `Dynamic/B1RawStencil_*`; B0/B1/fog-off controls remain under their named directories. The detailed metrics and complete pass/fail list are in `Docs/SIGHTWEAVE_DARKWELL_VISUAL_RESCUE_REPORT.md`.

No separate Nanite wall existed in the integration fixture, and no independently scripted doorway capture was produced. Engine-source Nanite/non-Nanite View parity and the M6P1 doorway authority test are evidence, not substitutes for these visual checks. Three plugin-general M4P1 Lab D3D12 pixel-baseline failures remain deferred. These limits do not alter the project candidate's `PARTIAL` status.

### Reliable continuation commits

- `cc8ee8f` `docs: qualify DARKWELL pre-TSR proof failure`
- `2236940` `test: isolate temporal-coherent fog classification`
- `d10fd5f` `render: align DARKWELL custom stencil temporal space`
- `8965a01` `render: migrate DARKWELL fog before temporal resolve`
- `7b18882` `fix: let DARKWELL own custom depth jitter policy`
- `84c910d` `test: prepare DARKWELL visual retest four`
- `4e50e44` `test: align packaging contracts with selected pass`

Next recovery command:

```powershell
git switch codex/sightweave-darkwell-visual-rescue; git pull --ff-only
```

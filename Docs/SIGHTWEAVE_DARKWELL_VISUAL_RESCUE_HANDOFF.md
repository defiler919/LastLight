# SightWeave DARKWELL Visual Rescue Retest Handoff

Date: 2026-08-30

Branch: `codex/sightweave-darkwell-visual-rescue`

User-rejected candidate baseline: `2439cfb0de843ab52b9c989439272f1e30727d1c`

Second user-rejected candidate baseline: `2883cd5d9f68044c71da785eaaa90f03fff4193c`

Validated Remembered stabilization SHA: `bac0525`

Unattended B0/B1 continuation starting SHA: `e4d654b74e3557ebefa328986bb626dcbfde0301`

Validated temporal-coherent project candidate SHA before final documentation: `4e50e44`

SurfaceMaterial source candidate SHA before final documentation: `8daed357e26aa8fa41be0892d63e906bc153317e`

Final architecture proof starting SHA: `011fcbd3ce53704b14a89fd0d293995d52f2b427`

Final architecture source candidate SHA before final documentation: `96183a2258fa2b79e9991f140b76f28ffac6fdd8`

Stop-loss deadline: 2026-09-05

Status: **BLOCKED — USER_FINAL_ARCHITECTURE_PIE_FAILED / SIGHTWEAVE VISUAL PRESENTATION REJECTED**

## Final architecture PIE disposition (2026-08-30)

The user rejected final candidate `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb`. The visible Live boundary still exposed discrete world/tile-grid stepping, periodic jaggies and flicker. `RGBA16F` storage did not make coverage continuous because its source remained discrete tile state. Per-face outward-normal wall sampling produced opposite classifications on faces of the same wall and north/south asymmetry. Improved gray static texture was not sufficient for acceptance.

Every earlier readiness statement in this handoff is historical. The current candidate is not `COMPLETED`, not ready for another user PIE, and must not receive another blur, threshold, mask expansion, resolution reduction, TSR-disable or screen-space compensation cycle.

Reliable SightWeave CPU authority and gameplay rules remain available as a backend. Its rejected visual presentation paths remain in source history but must not register or run in the new candidate. Work continues on branch `codex/darkwell-project-fog-visual-rebuild` under `Docs/DARKWELL_PROJECT_FOG_VISUAL_REBUILD_CONTRACT.md`, beginning with a no-wall proof of genuinely continuous project-owned Live coverage. The new work does not generalize or republish SightWeave and does not rewrite its authority backend.

## Fifth user dynamic PIE disposition (2026-08-30)

Retest 5 rejected SurfaceMaterial candidate `011fcbd3ce53704b14a89fd0d293995d52f2b427`. The moving gray boundary still shakes; Remembered is too flat for the real floor texture and major static-scene blocks to remain plainly recognizable; and wall handling is directionally wrong, producing an all-black result from one direction and an all-bright result from the opposite direction. The current fixed CPD wall direction and maximum-of-both-sides sampling are rejected as a formal surface-visibility rule.

The gray horizontal-line repair, black/gray alignment, acceptable flow, unified three-state authority, Ultra 2.5 cm/texel, tile-edge empty-interval rejection, wall-behind black intent, `NeverRemember`, Stalker/HUD synchronization, Torch/Lantern/Torch recovery, CPU authority/persistence/tests, and the useful SurfaceMaterial progress remain frozen. They must not be traded away in the next proof, but they do not make the current candidate usable.

The only remaining authorized construction is a final, at-most-48-hour DARKWELL architecture proof in `L_VisionIntegration`: use the actual stable world geometric rendered-surface normal for wall-face sampling; preserve a recognizable filtered real static BaseColor; and derive continuous world-anchored Known/Live presentation coverage from the discrete authority without screen-space history or jitter compensation. Required controls are authority frozen with camera/player motion, camera frozen with visibility-source motion, and normal gameplay, followed by four-direction wall/cube/doorway checks and 1080p/1440p D3D12/SM6 normal-TSR evidence.

The highest possible handoff is `PARTIAL — READY_FOR_USER_FINAL_ARCHITECTURE_PIE`. Proof failure requires `BLOCKED — SURFACE MATERIAL ARCHITECTURE PROOF FAILED / SIGHTWEAVE VISUAL LAYER ABANDONED`. There is no current `READY_FOR_USER_DYNAMIC_PIE_RETEST_5` state and no automated result may replace the user's final visual verdict.

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

## Final architecture PIE handoff (2026-08-30)

### Candidate state

```text
PARTIAL — READY_FOR_USER_FINAL_ARCHITECTURE_PIE
```

This final section supersedes the earlier `BLOCKED — USER_DYNAMIC_PIE_RETEST_5_FAILED` as the current agent-side handoff state; it does not erase that fifth user rejection and does not claim `COMPLETED`. The source candidate before this documentation is `96183a2258fa2b79e9991f140b76f28ffac6fdd8`.

The final architecture removes the rejected primitive-wide `(+1,0)` CPD wall direction. Formal vertical-surface sampling now uses stable world geometric `VertexNormalWS`, corrected by `TwoSidedSign`, projected to XY and offset only toward that rendered face's outward free space. Floor/top faces sample their exact world XY. CPD `[0]` remains category and `[3]` remains sample distance; `[1]/[2]` no longer select formal wall direction. There is no ViewDirection choice and no maximum of both wall sides.

The final floor-specific root was `Normalize((0,0))`: a horizontal geometric normal has zero XY, producing a non-finite bias UV even after multiplication by a zero wall weight. Known-white texture, uniform-white RT, direct world-UV threshold, and RT half-plane A/B isolated this from RT binding and state generation. The material now divides normal XY by `max(length, 0.0001)`, explicitly truncates the post-WorldMin LWC local coordinate to float, and samples mip 0. The corrected runtime half-plane and authority views match the direct ground UV.

The persistent state target is linear `RTF_RGBA16f`, bilinear and clamped. R retains discrete authority for diagnostics; G is continuous world-anchored `LiveCoverage`; B is continuous `KnownCoverage`; A is scope validity. The formal weights are Live, Known-minus-Live Remembered, and one-minus-known Unknown, with Live precedence and memory eligibility. Real sampled BaseColor is preserved through the static Remembered desaturation/darkening/contrast filter. Unknown remains black. Dynamic SceneColor, lighting, shadow, particles, Stalker, temporal history and procedural time detail do not enter Remembered.

The formal path is ordinary primitive SurfaceMaterial -> depth/coverage/velocity -> normal project TSR. Neither old post-TSR nor pre-TSR full-screen composite is registered in SurfaceMaterial mode. No stencil classification, SceneCapture, blur increase, mask expansion, TSR disable, resolution reduction, `L_Prototype`, `Darkwell.uproject`, Fab, or plugin-generalization change is part of this candidate.

### Agent gate summary

- E1: 20-second fixed Live/Remembered samples and fog-off controls. Remembered/fog-off correlation is `0.947743` at 1080p and `0.963518` at 1440p. Standard-deviation retention is `68.1841%` and `68.4273%`; Remembered mean/std/RMS are `177.523990 / 5.744403 / 0.032358` and `177.883358 / 6.027118 / 0.033882`. Unknown MAD is exactly zero.
- E2: authority-frozen horizontal, vertical, rotation, along-wall and doorway runs are each 30 seconds / 900 frames with matched controls. Black-edge XOR/leading/trailing p95 is `0.00` in every run. Opened contacts/20-frame strips show no directional whole-edge inversion, gray line, seam or doorway discontinuity.
- E3: fixed-camera source translation and full slow rotation are each 30 seconds / 900 frames with controls. MAD p95 is `0.25`/`0.05`; black-edge XOR/leading/trailing p95 is `0.00`. Coverage sweeps continuously over the textured floor, walls, door and landmark.
- E4: normal gameplay at 1080p/1440p uses D3D12/SM6, project TSR and temporal jitter. Movement MAD p95 is `2.18`/`2.22`; Torch-cycle MAD p95 is `0.21`/`0.25`; black-edge metrics are zero. Both fog-on and fog-off soaks are exactly 600 seconds / 18,000 frames at 30 fps. Player Z remains `90.150` and both trajectories remain within the fixture.
- Build: serial `DarkwellEditor Win64 Development` Success.
- Automation: bounded `11/11` Success (SurfaceMaterial 3, M6P1 1, M3.4 3, M3.5 2, M4P1 1, D3D12/SM6 composite 1); complete DARKWELL `32/32` Success.
- Logs: zero candidate-path fatal/assert/ensure/GPU crash/device removal/binding failure/DARKWELL-or-SightWeave error. UE 5.8.1 startup retains its disclosed UnifiedError self-test, thirteen automation-condition self-test, and Experimental Toolsets Python import noise.

Evidence is ignored under `Saved/SightWeaveVisualRescueEvidence`: `Dynamic/E1_Final_*`, `E2_Final_*`, `E3_Final_*`, `E4_Final_*`, `Dynamic/FinalArchitecture*AB*`, and `FinalArchitectureProof/Tests`. User videos and all generated media remain uncommitted.

### Required final user PIE

1. Restore the pushed branch:

   ```powershell
   cd D:\UE_pro\Darkwell
   git fetch origin
   git switch codex/sightweave-darkwell-visual-rescue
   git pull --ff-only
   ```

2. Open UE 5.8.1, load `/Game/Maps/L_VisionIntegration`, use normal D3D12/SM6 and project TSR, and do not set any `r.Darkwell.SightWeave.Diagnostic.*` or `r.SightWeave.Diagnostic.*` CVar.
3. At 1920x1080, start normal PIE and choose `NEW GAME`. Hold player and camera still for 20 seconds. Confirm textured Remembered, wall faces, the Live boundary and Unknown remain stable with no gray line or seam.
4. Move horizontally and vertically for 30 seconds each while keeping cube/wall faces visible. Confirm no direction-correlated left/right or top/bottom shake.
5. Rotate aim slowly for 30 seconds, strafe along both walls, approach each face from both sides, and pass both wall ends. Confirm every face is consistent and black begins behind the wall.
6. Cross the doorway in both directions. Confirm the opening and wall sides remain continuous.
7. Sweep the floor and landmark through Live -> Remembered -> Live and Known -> Unknown. Confirm texture/large structure remains recognizable, Unknown leaks nothing, and no gray tile-edge line appears.
8. Switch Torch -> Lantern -> Torch. Aim toward and away from the Stalker. Confirm legal light restores, Stalker and red threat HUD share visibility, and no enemy image remains in Remembered.
9. Repeat the core static, horizontal, vertical, rotation, wall and doorway checks at 2560x1440, then play normally for at least ten minutes.

Accept only if the user explicitly confirms the complete visual contract. A rejection ends implementation with:

```text
BLOCKED — SURFACE MATERIAL ARCHITECTURE PROOF FAILED
SIGHTWEAVE VISUAL LAYER ABANDONED
```

No seventh visual patch, screen-space fallback, threshold relaxation, or evidence deletion is authorized after rejection. The 2026-09-05 stop-loss remains unconditional. Keep the computer on; no shutdown, sleep or restart is part of this handoff.

### Final architecture continuation commits before documentation

- `1303b4c12c8032ed4089e898023846fba4c28784` `docs: record fifth DARKWELL visual retest failure`
- `e4120cc4ecc82873c0d9d72967cc21079499d921` `render: stabilize DARKWELL surface architecture`
- `96183a2258fa2b79e9991f140b76f28ffac6fdd8` `render: stabilize DARKWELL horizontal surface sampling`

## Historical fifth dynamic PIE handoff — rejected SurfaceMaterial candidate (2026-08-30)

### Candidate state

```text
HISTORICAL — USER_DYNAMIC_PIE_RETEST_5_FAILED
```

This historical section records the candidate offered before the fifth user PIE. The user rejected it, and the authoritative current handoff is the final architecture section above plus the current-state footer below. It does not claim `COMPLETED`.

The pushed source candidate before this documentation is `8daed357e26aa8fa41be0892d63e906bc153317e`. `L_VisionIntegration` now uses a project-only stencil-free surface-material path: SightWeave's Ultra 2.5 cm/texel live/feather/memory GPU mirrors incrementally update a persistent RGBA8 world-state texture, and real primitives sample it with `AbsoluteWorldPosition.xy`. Their normal geometry coverage, depth, velocity, and normal TSR produce the visible edges. The old pre-TSR and post-TSR full-screen formal composites are not registered while the surface target is active; CustomDepth/Stencil does not determine the candidate's scene outlines.

The fixture's original floor/walls/landmark were non-Nanite Engine Cubes with `WorldGridMaterial` and no source textures or project master. The vertical slice therefore introduces one matched real sampled tile BaseColor baseline and preserves it through the Remembered filter. It does not claim to restore an absent project art texture. Added assets are:

- `/Game/Darkwell/Vision/Materials/MF_DarkwellSightWeaveSurface`;
- `/Game/Darkwell/Vision/Materials/M_DarkwellSightWeaveSurface`;
- `/Game/Darkwell/Vision/Materials/MI_DarkwellSightWeaveFloor`;
- `/Game/Darkwell/Vision/Materials/MI_DarkwellSightWeaveWall`;
- `/Game/Darkwell/Vision/Materials/MI_DarkwellSightWeaveStatic`.

All five are Git LFS objects. No map asset, `L_Prototype`, external asset, or `Darkwell.uproject` was modified. CPD indices are `[0]=category`, `[1]=wall direction X`, `[2]=wall direction Y`, `[3]=wall distance cm`; fixture walls use `(+1,0,27.5 cm)`.

### What the bounded agent gate established

- Editor Development serial build succeeded after the final source checkpoint.
- Bounded automation is 11/11 Success: SurfaceMaterial 3, M6P1 authority 1, M3.4 3, M3.5 2, M4P1 NeverRemember policy 1, D3D12/SM6 composite 1.
- 1080p/1440p horizontal and vertical candidate/fog-off pairs are each 30 seconds/900 frames with normal TSR. Candidate compensated bright-region MAD p95 is lower than fog-off in all four pairs. Worst extra black-edge XOR p95 is `0.000487`.
- Remembered/fog-off texture correlation is `0.830543` to `0.977992`; fixed Remembered ROI MAD p95 is `0.038452` at 1080p and `0.050425` at 1440p. Unknown ROI remains exactly black across 600 frames at both resolutions.
- Independent 1080p/1440p Static, Rotate, Wall, Doorway, and TorchCycle candidate and matched fog-off processes completed. A fixed 1080p Remembered soak completed 300 seconds/9000 frames and remained valid through Torch depletion.
- 31 formal dynamic logs and six bounded automation logs are severe-clean. The agent opened the required contacts and adjacent frames and observed no gray-line/black-gray-seam recurrence, no old directional whole-edge oscillation, recognizable Remembered floor texture, readable facing walls with black behind, continuous doorway coverage, and preserved HUD/tool behavior.

The ignored evidence root is `Saved/SightWeaveVisualRescueEvidence`. Primary candidate directories begin with `Dynamic/SurfaceFinal*`; matched controls begin with `Dynamic/SurfaceFogOff*`; bounded test logs are under `SurfaceFinalTests`. The `Dynamic/SurfaceFinal1080Continuous300` moving sample is explicitly invalid and excluded because the test input drove the actor out of the fixture and through the floor. The valid long sample is `Dynamic/SurfaceFinal1080StaticSoak300`.

### Required fifth user dynamic PIE sequence

1. Restore the branch:

   ```powershell
   git switch codex/sightweave-darkwell-visual-rescue
   git pull --ff-only
   ```

2. Open `D:\UE_pro\Darkwell\Darkwell.uproject` in Unreal Engine 5.8.1, load `/Game/Maps/L_VisionIntegration`, and use normal D3D12/SM6 with project-normal TSR. Do not set any `r.Darkwell.SightWeave.Diagnostic.*` or `r.SightWeave.Diagnostic.*` CVar.
3. Start normal PIE and select `NEW GAME`. Hold the player and camera completely still for 20 seconds. Confirm the textured Remembered floor, wall faces, Live boundary, and Unknown remain stable and there are no gray lines or black/gray seams.
4. Move horizontally for 30 seconds while keeping cube/wall left-right edges visible; then move vertically for 30 seconds while keeping top-bottom edges visible. Confirm there is no direction-correlated edge oscillation, side-surface shake, crawl, or trail beyond the native fog-off scene.
5. Slowly rotate aim for 30 seconds. Confirm the Remembered texture remains attached to the world and its internal detail does not slide or pulse.
6. Strafe along each wall for 30 seconds, approach it head-on, and pass both ends. Confirm the facing surface is readable and Unknown begins behind it rather than swallowing the wall or leaking gray geometry.
7. Cross the doorway in both directions. Confirm the opening, wall sides, and black-behind-wall rule stay continuous without a seam.
8. Sweep a static floor region and the landmark through Live -> Remembered -> Live. Confirm the tile texture and static silhouette remain recognizable, while no Stalker, dynamic shadow, or stale change enters Remembered.
9. Switch Torch -> Lantern -> Torch. Confirm the legal cone disappears/restores without a stuck-black state. Aim toward and away from the Stalker and confirm the actor and red threat HUD appear/disappear on the same revision with no remembered enemy image.
10. Repeat the core static/horizontal/vertical/rotation/wall/door checks at both 1920x1080 and 2560x1440, then play normally for at least five minutes.

Accept only if the user explicitly confirms: no directional shaking, no gray lines, no black/gray offset, acceptable circle/cone edges, recognizable filtered static Remembered texture, Unknown black, preserved wall/door rule, preserved enemy filtering, Torch/Lantern/Torch recovery, and acceptable game usability. On rejection, set `BLOCKED — SURFACE MATERIAL VISUAL PROTOTYPE FAILED / SIGHTWEAVE ABANDONMENT REVIEW REQUIRED` and apply the 2026-09-05 stop-loss. Do not resume screen-space patching.

### SurfaceMaterial continuation commits

- `ee04e10` `docs: record fourth DARKWELL visual retest failure`
- `cfb1543` `render: add DARKWELL surface fog vertical slice`
- `0bd1fb9` `test: add matched native surface fog control`
- `703eff4` `test: add directional DARKWELL fog motion gate`
- `8e3ccb1` `fix: preserve DARKWELL wall surface coverage`
- `8daed35` `test: add DARKWELL doorway motion gate`

Next recovery command:

```powershell
git switch codex/sightweave-darkwell-visual-rescue; git pull --ff-only
```

## Current-state footer

The authoritative end state of this document is `BLOCKED — USER_FINAL_ARCHITECTURE_PIE_FAILED / SIGHTWEAVE VISUAL PRESENTATION REJECTED`. Every earlier readiness section is retained only as historical engineering evidence. The final SurfaceMaterial candidate failed user PIE and must not be offered again. Continuation is the project-owned rebuild on `codex/darkwell-project-fog-visual-rebuild`; automated evidence cannot promote either effort to `COMPLETED`.

Recovery command:

```powershell
cd D:\UE_pro\Darkwell
git fetch origin
git switch codex/darkwell-project-fog-visual-rebuild
git pull --ff-only
```

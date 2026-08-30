# SightWeave DARKWELL Visual Rescue Prototype Report

Date: 2026-08-30

Branch: `codex/sightweave-darkwell-visual-rescue`

Frozen starting SHA: `f364f780904c7ced5d649e7d582c3d91a7d43baf`

User-rejected candidate / this rescue baseline: `2439cfb0de843ab52b9c989439272f1e30727d1c`

Second user-rejected candidate baseline: `2883cd5d9f68044c71da785eaaa90f03fff4193c`

Validated Remembered stabilization SHA: `bac0525`

Unattended B0/B1 continuation starting SHA: `e4d654b74e3557ebefa328986bb626dcbfde0301`

Validated temporal-coherent project candidate SHA before final documentation: `4e50e44`

SurfaceMaterial source candidate SHA before final documentation: `8daed357e26aa8fa41be0892d63e906bc153317e`

Final architecture proof starting SHA: `011fcbd3ce53704b14a89fd0d293995d52f2b427`

Final architecture source candidate SHA before final documentation: `96183a2258fa2b79e9991f140b76f28ffac6fdd8`

48-hour prototype checkpoint: 2026-08-31

Final stop-loss deadline: 2026-09-05

Status: **PARTIAL — READY_FOR_USER_FINAL_ARCHITECTURE_PIE**

This is a DARKWELL project-use rescue, not a plugin-generalization, Fab, packaging, or publication result. The user's first real dynamic PIE rejected the candidate at baseline `2439cfb0de843ab52b9c989439272f1e30727d1c`. The prior agent-side automation, screenshots, extracted frames, and D3D12/SM6 runs remain engineering evidence, but they do not establish visual acceptance and cannot be cited as proof that this candidate passed.

## 0H. Fifth user dynamic PIE rejection and final architecture-proof gate (2026-08-30)

The fifth user dynamic PIE rejected the SurfaceMaterial candidate at baseline `011fcbd3ce53704b14a89fd0d293995d52f2b427`. The controlling state is:

```text
BLOCKED — USER_DYNAMIC_PIE_RETEST_5_FAILED
```

The moving gray boundary still visibly shakes. Remembered is too flat and its real texture is almost invisible, so the current result neither meets the static-scene-memory information contract nor provides a trustworthy interior-flicker assessment. Wall classification is directionally incorrect: one view direction becomes wholly black while the opposite direction becomes wholly bright. The fixed wall custom-primitive-data direction and two-sided maximum sampling are therefore not an acceptable rendered-surface rule.

The user also confirmed that the gray horizontal lines and black/gray offset did not recur and that performance remains acceptable. Those passed results, together with unified three-state authority, Ultra 2.5 cm/texel, tile-edge empty-interval rejection, `NeverRemember`, Stalker/HUD synchronization, Torch/Lantern/Torch recovery, CPU authority/persistence, and the useful parts of the SurfaceMaterial direction, remain frozen. They do not establish visual acceptance.

The final authorized project-specific proof is limited to 48 hours and to `L_VisionIntegration`. It must replace fixed wall directions with the actual stable world geometric surface normal, retain recognizable real static BaseColor, and separate discrete Known/Live authority from continuous world-anchored presentation coverage. It must prove fixed-authority camera motion, fixed-camera visibility-source motion, four wall directions, doorway and cube faces, then normal 1080p/1440p D3D12/SM6 TSR gameplay. No screen-space composite revival, blur or mask expansion, resolution reduction, TSR disable, or plugin-generalization work is authorized.

Success is capped at `PARTIAL — READY_FOR_USER_FINAL_ARCHITECTURE_PIE`. If that controlled proof fails, the required stop state is:

```text
BLOCKED — SURFACE MATERIAL ARCHITECTURE PROOF FAILED
SIGHTWEAVE VISUAL LAYER ABANDONED
```

No prior `READY_FOR_USER_DYNAMIC_PIE_RETEST_5` statement remains current. Historical automated evidence is retained only as engineering evidence and cannot overrule this user verdict.

## 0G. Fourth user dynamic PIE rejection (2026-08-30)

The fourth user dynamic PIE rejected candidate baseline `f9dbf6f89046bbbe684e55889840f216422cb53c`. The authoritative disposition is:

```text
PARTIAL — USER_DYNAMIC_PIE_RETEST_4_FAILED
SCREEN_SPACE COMPOSITION REJECTED FOR PRODUCTION
```

The user confirmed a real improvement in static stability, but not project usability. Horizontal motion makes the cube's left/right edges and side surfaces shake; vertical motion makes its top/bottom edges shake; the failure is direction-correlated. Remembered is also too gray and retains too little information. The user requires the actual floor texture to remain plainly recognizable and the gray layer to be a filtered real static 3D scene, not a pure-gray fill or an artificial/generated replacement texture.

These observations supersede the prior retest-4 readiness state. Existing successful evidence for unified three-state authority, Ultra 2.5 cm/texel, gray-line removal, tile-edge empty-interval rejection, black/gray alignment, wall visibility, behind-wall black, `NeverRemember`, Stalker/HUD synchronization, Torch/Lantern/Torch recovery, and normal TSR remains frozen. It does not prove that screen-space composition is production-acceptable.

No further post-process UV, CustomStencil offset, blur, feather, threshold, or dilation tuning is authorized. The next vertical slice is project-first stencil-free surface fog in `L_VisionIntegration`: the state remains SightWeave-authored, while real scene primitives/materials, depth, coverage, velocity, and normal TSR produce the final scene edges. The formal screen-space composite must be disabled for that candidate. Its highest possible state is `PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST_5`.

## 0F. Qualification of the failed pre-TSR prototype (2026-08-30)

The raw data and artifact inventory in section 0E remain unchanged. The interpretation is narrowed because the failed B path was not temporally coherent:

```text
BeforeDOF
    + jittered pre-TSR SceneColor / SceneDepth
    + unjittered point-sampled CustomDepth / CustomStencil
```

The old wording `PRE_TSR_ARCHITECTURE_PROOF_FAILED` overgeneralized that evidence. The corrected conclusion is:

```text
CURRENT PRE_TSR PROTOTYPE FAILED
PRE_TSR ARCHITECTURE NOT YET FALSIFIED
```

UE 5.8.1 `CustomDepthRendering.cpp` defines `r.CustomDepthTemporalAAJitter` with engine default `1` and `ECVF_RenderThreadSafe`. DARKWELL `Config/DefaultEngine.ini` and `Plugins/SightWeave/Config/Engine.ini` both override it to `0`. At zero, `RenderCustomDepthPass` calls `CreateViewShaderParametersWithoutJitter`; the modified projection is used by the non-Nanite CustomDepth mesh pass and copied into the Nanite packed views. Depth and stencil share the same `PF_DepthStencil` target and therefore the same projection convention. This explains the observed unjittered categorical CustomStencil edge in the failed B path.

The next controlled gate is B0, which removes CustomStencil and dynamic surface classification entirely. Only a stable B0 permits B1, where runtime diagnostics must prove `r.CustomDepthTemporalAAJitter=1` and common primary ViewRect/extent/projection conventions. These gates preserve the original failure evidence and do not claim a new visual candidate.

## 0D. Third user dynamic PIE rejection (2026-08-29)

The third real dynamic PIE rejected candidate `acd1c5b8ae9950423aa9575c639e034b9ce21dd7`. The authoritative disposition is:

```text
PARTIAL — USER_DYNAMIC_PIE_RETEST_3_FAILED
POST_TSR COMPOSITION ARCHITECTURE REJECTED
```

The user confirmed that the gray horizontal lines remain gone, the black/gray offset remains gone, the large staircase is materially improved, Remembered is no longer a flat two-dimensional fill, enemy filtering remains correct, and frame rate/input responsiveness are acceptable. The blocking failure is narrower and architectural: wall edges still visibly shake with both the player and camera completely still, and the same wall edge still shakes after the wall enters Remembered.

The third user recording is failure evidence, not acceptance evidence. The earlier agent ROI results sampled the interior of the gray layer and did not cover the wall depth discontinuity. They remain supporting measurements for the old internal-shading hypothesis but cannot override the user's dynamic verdict or establish wall-edge stability.

The prior conclusion that immutable post-TSR Remembered shading was sufficient is therefore superseded. The formal callback still first creates the final Unknown/Remembered/Live screen boundary after normal TSR and after Tonemap. Normal scene geometry is temporally resolved inside TSR, but SightWeave then re-reads current-frame SceneDepth, CustomDepth/Stencil, and static attributes to create a new hard semantic boundary outside that history. The focused next step is a controlled pre-TSR architecture proof, not another blur, feather, threshold, dilation, resolution, or post-TSR jitter-compensation patch.

The only permitted success state for that proof is `PARTIAL — PRE_TSR_ARCHITECTURE_PROVEN / READY_FOR_USER_DYNAMIC_PIE_RETEST_4`. If the pre-TSR path still visibly jitters under normal TSR, the required state is `BLOCKED — PRE_TSR_ARCHITECTURE_PROOF_FAILED` and the earliest changing input must be identified before work stops. Neither result is `COMPLETED`, and no formal production migration or full regression is authorized by this proof.

## 0E. Temporal-incoherent pre-TSR composition prototype — failed

The controlled proof at source checkpoint `12b1118` and diagnostic checkpoint `05d2f03` disproved the narrow hypothesis that moving the existing temporally incoherent geometric semantic composite before TSR is sufficient by itself. The B path still changed at a completely static wall depth discontinuity under the project's normal D3D12/SM6 TSR configuration and was numerically worse than the rejected A control. Its historical disposition was:

```text
BLOCKED — CURRENT PRE_TSR PROTOTYPE FAILED
```

This is not a fifth tuning candidate and is not ready for user PIE retest 4. It does not falsify a temporal-coherent pre-TSR architecture. No blur, feather increase, mask dilation, threshold change, Screen Percentage reduction, TSR disable, camera snap, wall hiding, or current-SceneColor memory route was used.

### Exact current and proof render order

`FSightWeaveSceneViewExtension::SubscribeToPostProcessingPass` in `Plugins/SightWeave/Source/SightWeaveRender/Private/SightWeaveSceneViewExtension.cpp` selects the pass. `PostProcessComposite_RenderThread` calls `FSightWeaveSparseAtlasRenderState::AddHardMaskComposite_RenderThread`, which binds SceneColor, SceneDepth, CustomDepth/Stencil, the live/memory/static atlases, ViewRect data, and the temporal projection jitter. The screen boundary is first finalized in `SightWeaveHardMaskCompositePS` or `SightWeaveInwardFeatherCompositePS` in `Plugins/SightWeave/Shaders/Private/SightWeaveSingleTile.usf` after `SightWeaveResolvePresentationState`.

Unreal 5.8.1 executes the relevant stages in `D:/UE_5.8/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp` as follows:

```text
BeforeDOF scene-view-extension chain (lines 893-899)
    -> DOF / remaining pre-upscale processing
    -> AddMainTemporalSuperResolutionPasses (lines 1154-1160)
    -> full-resolution SceneColor
    -> AddTonemapPass (line 1629)
    -> Tonemap after-pass chain (line 1637)
```

A, the rejected control, remains `EPostProcessingPass::Tonemap`. In the project-normal 1080p run it receives post-TSR/post-Tonemap 1920x1080 SceneColor and output, while SceneDepth and unjittered CustomDepth/Stencil remain primary textures with 1400x792 extent. SightWeave therefore creates the final Unknown/Remembered/Live screen boundary after normal TSR history and after Tonemap.

B selects `EPostProcessingPass::BeforeDOF` only in Development/Editor and only when the World map name ends in `L_VisionIntegration`. It receives pre-Tonemap HDR SceneColor at primary resolution; the recorded SceneColor texture extent is 1400x792 and its active output/ViewRect is 1400x788. SceneDepth and CustomDepth/Stencil use the same 1400x792 texture extent. The semantic boundary is formed at that primary stage, passed through normal TSR to 1920x1080, and then Tonemapped. There is no SightWeave-owned previous-frame history. The proof's default Remembered input is diagnostic mode 13, a fixed neutral value; current SceneColor, lighting, dynamic shadow, particle, and enemy content are not used for Remembered.

`SightWeaveResolveDepthCoordinates` distinguishes the two temporal spaces. A starts with the stable secondary output pixel and moves only the SceneDepth coordinate into the current jittered primary buffer, leaving CustomDepth/Stencil in their unjittered convention. B starts from the current primary SceneDepth pixel and maps the point-sampled CustomDepth/Stencil coordinate back toward the unjittered convention. This is a private Shader parameter addition for the proof; no public Runtime/Adapter, CPU Mask, scope, revision, generation, persistence, `.uasset`, `.umap`, configuration, plugin descriptor, or `Darkwell.uproject` contract changed.

### Static wall A/B evidence

The authoritative comparison uses the project-normal TSR primary/secondary split above, not the initial native-resolution diagnostic where Screen Percentage was temporarily forced to 100. Values are adjacent-frame mean absolute differences in 8-bit channel-value units over the same narrow wall depth-discontinuity ROI. Each static sequence contains 300 consecutive 30 fps frames.

| State / path | wall ROI MAD p50 | p95 | max | detected edge range | one-pixel flips |
| --- | ---: | ---: | ---: | ---: | ---: |
| Live A — post-Tonemap control, left wall | 0.018682 | 0.035442 | 0.382422 | 0 px | 0 |
| Live B — BeforeDOF proof, left wall | 0.219052 | 0.540915 | 0.767966 | 3 px | 12 |
| Live B — BeforeDOF proof, right wall | 0.221721 | 0.525111 | 0.755563 | 3 px | 34 |
| Remembered A — post-Tonemap control, left wall | 0.074729 | 0.143064 | 0.530327 | 0 px | 0 |
| Remembered B — BeforeDOF proof, left wall | 0.494033 | 1.119474 | 1.291636 | 2 px | 86 |

These standalone A values do not overturn the user's embedded dynamic PIE rejection. They only establish that B is not clearly better than A in the controlled proof and fails the B success gate on its own. Contact sheets and adjacent-frame wall crops were opened directly; B shows the lower wall edge changing across otherwise fixed frames.

The broad black/Live diagonal remained position-stable in the Live static detector for both paths, but that does not rescue B: the contract explicitly requires the wall depth discontinuity and Remembered wall boundary, both of which failed. The Remembered black-boundary detector also had a moving/ambiguous multi-edge ROI after the priming translation and is not used as a pass claim.

### Earliest changing stage

The fixed-camera/player diagnostic runs retained `stateRevision=8`, `featherRevision=8`, `staticClassVersion=4`, `submittedTiles=0`, `update=none`, and `bindingFailure=0` throughout the sampled frames. Thus the CPU authority Mask, GPU atlas revision, incremental tile submission, static-attribute revision, player Transform, camera Transform, and resource binding were not the source of the B wall change.

The pre-TSR resource-isolation results for the same left-wall ROI were:

| B diagnostic output | MAD p50 | p95 | edge range | one-pixel flips |
| --- | ---: | ---: | ---: | ---: |
| raw CustomDepth | 0.052669 | 0.086447 | 0 px | 0 |
| raw CustomStencil | 0.289562 | 0.370186 | 1 px | 46 |
| static attribute | 0.079612 | 0.143349 | 0 px | 0 |
| remembered surface-classification alpha | 0.047752 | 0.071752 | 1 px | 2 |
| unified presentation state | 0.022015 | 0.042403 | 4 px | 32 |
| SceneDepth-reconstructed world-position diagnostic | 0.061813 | 0.129613 | 4 px | 50 |

The earliest visible instability is therefore at the pre-TSR categorical CustomStencil/surface-classification and world-position-to-state boundary, before fixed gray filtering. This does not prove that the underlying CustomStencil allocation mutates. The evidence supports the narrower inference that directly point-sampling an unjittered categorical stencil/classification boundary into a jittered pre-TSR color buffer does not give that new semantic edge the same raster, velocity, rejection/reactive-mask, and history semantics as normal scene geometry. Normal TSR alone did not stabilize it. Trying alternate jitter signs or adding another compensation is explicitly prohibited and was not attempted.

### Validation, evidence, and stop boundary

- `DarkwellEditor Win64 Development`: succeeded twice, first for the proof and again for explicit diagnostic isolation.
- `SightWeave.M3P5.Packaging.RememberedTemporalSpace`: Success after both source checkpoints.
- `Darkwell.SightWeave.VisualRescue.PresentationState.TruthTable`: Success.
- `Darkwell.SightWeave.M6P1.Integration.VerticalSliceAuthority`: Success, retaining the wall rule, `NeverRemember`, Stalker/HUD shared authority, and Torch/Lantern/Torch authority transitions.
- `SightWeave.M3P5.Composite.ThreeStateAndMemoryFailure.D3D12`: Success on D3D12/SM6.
- Twelve proof logs were scanned. The only text match was the benign configuration line `r.GPUCrashDebugging:0`; there was no fatal, assertion, ensure, GPU crash, device removal, DXGI, D3D12/RHI, SightWeave, or Darkwell severe error.

Ignored evidence is under `Saved/SightWeaveVisualRescueEvidence/Dynamic`:

- `PreTSRProof_A_PostTonemap_ProjectTSR_LiveStatic10`
- `PreTSRProof_B_BeforeDOF_ProjectTSR_LiveStatic10`
- `PreTSRProof_A_PostTonemap_ProjectTSR_RememberedStatic10`
- `PreTSRProof_B_BeforeDOF_ProjectTSR_RememberedStatic10`
- `PreTSRProof_B_Diag_StateMask`
- `PreTSRProof_B_Diag_WorldPosition`
- `PreTSRProof_B_Diag_CustomDepth`
- `PreTSRProof_B_Diag_CustomStencil`
- `PreTSRProof_B_Diag_StaticAttribute`
- `PreTSRProof_B_Diag_SurfaceClassification`

Each critical A/B directory contains the H.264 recording, game log, 300 lossless PNG frame extractions, wall contact sheets, adjacent-frame crops where applicable, and `boundary_metrics.json`. These files are ignored and not committed. The initial `ScreenPercentage=100` A/B directories are retained as explicitly non-authoritative diagnostic history.

No slow-rotation, along-wall translation, Torch cycle, or fourth user PIE candidate was produced after the static Live and Remembered B gates failed. Continuing that success matrix would contradict the explicit falsification stop rule. The previously passed gray-line, black/gray alignment, large-staircase, performance, wall, and enemy-authority work remains frozen, but this proof does not claim a new accepted visual candidate.

## 0B. Second user dynamic PIE rejection (2026-08-29)

User recording: `Darkwell - 虚幻编辑器 2026-08-29 21-07-46.mp4`.

The recording was copied to the ignored evidence directory `Saved/SightWeaveVisualRescueEvidence/UserDynamicPIERetest2Failure`. The source and ignored copy have SHA-256 `974A5A879D693938BF4E5C52ECF279266A43DA00DB95BE2C53B2B60FB68BB989`. The video, extracted frames, and analysis products are not repository artifacts and must not be committed.

The second real dynamic PIE did not accept baseline `2883cd5d9f68044c71da785eaaa90f03fff4193c`. The user confirmed that the gray horizontal lines and black/gray offset are gone, overall flow is acceptable, and the large staircase did not return. Live and the editor UI are relatively stable. The remaining explicit blocker is continuous temporal shaking inside the Remembered gray scene. The user did not request additional frame-rate work.

Agent inspection of 21 seconds of source video, a one-second overview, and consecutive six-frame-per-second crops agrees with that attribution: editor chrome is stable; the Live wedge is comparatively stable; the Remembered floor grid and static-object interior luminance change across consecutive frames. No horizontal gray-line signature or black/gray seam was found in the inspected frames. This is a new rejection, not a reopening of those passed fixes.

### Current-frame data path at the rejected baseline

The exact formal path at `2883cd5` is:

```text
pre-TSR primary-resolution SceneColor + SceneDepth + velocity
    -> AddMainTemporalSuperResolutionPasses (normal TSR/history)
    -> post-TSR full-resolution SceneColor
    -> AddTonemapPass
    -> EPostProcessingPass::Tonemap after-pass callback
    -> FSightWeaveSceneViewExtension::PostProcessPassAfterTonemap_RenderThread
    -> FSightWeaveSparseAtlasRenderState::AddHardMaskComposite_RenderThread
       inputs: post-TSR/post-tonemap SceneColor
               pre-TSR SceneDepth and pre-TSR CustomDepth/CustomStencil
               stable world-space state/memory/static-attribute atlases
    -> SightWeaveInwardFeatherCompositePS or SightWeaveHardMaskCompositePS
```

`FSightWeaveSceneViewExtension::SubscribeToPostProcessingPass` subscribes only to `EPostProcessingPass::Tonemap`. Unreal calls the delegate through `AddAfterPass(EPass::Tonemap, SceneColor)` after temporal upscaling and tonemapping. The SightWeave pass has no private temporal history, reports `historyValid=0`, and reconstructs Remembered surface position from the current frame's primary-resolution SceneDepth/CustomDepth while writing into the post-TSR output ViewRect. `SightWeaveRememberedSurfaceColor` then derives orientation with `ddx/ddy` and adds a high-frequency `frac` world-cell term. Thus state/memory may be stable while the visible Remembered content is regenerated after TSR from temporally jittered, lower-resolution geometry inputs. This mixed temporal/resolution space is the focused hypothesis for the second failure; it is not yet a completed root-cause claim until the required A/B matrix is recorded below.

The rescue now freezes the already-passed unified three-state result, Ultra 2.5 cm/texel, line-leak rejection, seam correction, wall rule, `NeverRemember`, Stalker/HUD coupling, Torch/Lantern/Torch continuity, normal TSR, and current performance. This slice may change only the Remembered temporal composition path.

## 0C. Remembered temporal rescue result

The second failure was isolated to the visible Remembered shading generated after TSR. It was not state-mask churn, memory revision churn, static-surface classification, player/camera transform instability, a return of the tile-edge line, or a new black/gray offset.

### Controlled A/B result

- A — unified three-state Mask: stable world-space Live/Remembered/Unknown contours in the fixed-camera and slow-rotation captures.
- B — Remembered surface-classification alpha: stable; the classified static silhouette does not reproduce the internal gray crawl.
- C — current SceneColor inside Remembered: inherits current-frame content and temporal variation and would leak dynamic information; diagnostic only and rejected.
- D — fixed pure-color static input: removes internal variation while retaining the state/classification contour; proves the grayscale arithmetic itself is not oscillating. It is not the formal solution because it destroys scene information.
- E — freeze state/memory revision: a startup freeze can occur before valid authority and remains inconclusive as a visual pass. Independent logs show stable bindings and synchronized state/feather revisions; the A/B result does not attribute the defect to revision updates.
- F — fixed player/camera: the standalone fixed-camera baseline is nearly static. The second user recording and the controlled rotation show that the objection is dominated by camera/aim movement, with the editor UI remaining stable.
- G — normal TSR: reproduces the rejected post-TSR shading behavior under movement.
- H — TSR off: reduces the temporal-sample interaction but restores unacceptable raster aliasing; diagnostic only and rejected.
- I — compose BeforeDOF/pre-TSR: lets TSR filter the gray result but changes its tone/exposure and adds temporal grain. It also broadens the risk of temporal history carrying dynamic content across information-state transitions. It was rejected as the formal path.
- J — post-TSR with stable immutable shading inputs: preserves the accepted state/classification, normal TSR, post-tonemap presentation value, static-scene silhouettes, wall rule, and enemy filtering while removing the unstable high-frequency generator. This is the selected formal path.

### Accurate root cause

`SightWeaveRememberedSurfaceColor` regenerated Remembered after TSR from the current frame's classified surface. Its tone included a normal inferred with `ddx/ddy(TranslatedSurfaceWorld)` and a discontinuous `frac` cell pattern at the configured world detail scale. Neither input had temporal history because the SightWeave callback runs after TSR. During camera follow or slow aim rotation, small current-depth/sample-footprint changes changed the derivative normal and crossed `frac` cusps, so internal floor/material values crawled relative to an otherwise stable world/state contour. Static frames could settle, but movement repeatedly reintroduced the error.

The fix does not move the formal pass and does not add temporal blur. The formal Remembered color now uses only:

- immutable static-attribute atlas value;
- immutable static/occluder stencil class;
- a continuous cosine detail term anchored to `MemoryTranslatedFloorOrigin` in world space;
- the existing brightness, contrast, and deliberately low detail strength.

The current-depth `ddx/ddy` normal and discontinuous `frac` pattern are removed. Actual three-dimensional SceneDepth/CustomDepth classification still supplies visible static-surface silhouettes, so walls, floor, doorway, and static objects remain recognizable. Remembered still never samples current SceneColor, lighting, dynamic shadow, particle, or enemy content.

### Final frame data path and spaces

The selected implementation is solution B and remains after TSR and after Tonemap:

```text
primary-resolution jittered SceneDepth
    + primary-resolution unjittered CustomDepth/CustomStencil
    -> ViewRect.Min/ViewRect-size mapped static-surface classification
    -> stable world position / Ultra state, memory, static-attribute atlases
    -> immutable stencil class + continuous world-anchored gray filtering

pre-TSR SceneColor + depth + velocity -> normal TSR/history
    -> secondary/full-resolution SceneColor -> Tonemap
    -> SightWeave Tonemap after-pass
       Live       = post-TSR/post-tonemap SceneColor
       Remembered = classified immutable stable inputs above (no SceneColor/history)
       Unknown    = black
```

SceneDepth alone receives the current temporal-jitter coordinate conversion; CustomDepth/Stencil retain their unjittered coordinate. Both use the current primary ViewRect including `ViewRect.Min`. Output and Live SceneColor use the post-TSR output/secondary ViewRect. The Remembered branch does not combine pre-TSR color/GBuffer with post-TSR color and does not read a previous-frame SightWeave history. The only current primary-resolution inputs retained in Remembered are the already A/B-proven stable static-surface identity/depth match; its visible information comes from one world-stable immutable space.

The Development/Editor-only `r.SightWeave.Diagnostic.CompositePass` and composite modes 11–14 retain the A/B paths. Test and Shipping always select the formal Tonemap after-pass and normal stable shading.

### Focused verification and evidence

- `DarkwellEditor Win64 Development`: succeeded serially after the formal source change.
- `SightWeave.M3P5.Packaging.RememberedTemporalSpace`: Success. It freezes the formal post-TSR default and forbids `ddx`, `ddy`, and `frac` in formal Remembered shading.
- `SightWeave.M3P5.Composite.ThreeStateAndMemoryFailure.D3D12`: Success on D3D12/SM6.
- `Darkwell.SightWeave.M6P1.Integration.VerticalSliceAuthority`: Success, including Ultra shared scope, `NeverRemember`, Stalker/HUD authority, wall rule, Lantern transition, and Torch restoration.
- No full historical regression, BuildPlugin, clean-host, Cook, Package, or performance matrix was run.

Formal normal-TSR evidence is ignored under `Saved/SightWeaveVisualRescueEvidence/Dynamic`:

- `Retest3Formal_1080p_Static10/static.mp4` — 10 seconds fixed camera/player.
- `Retest3Formal_1080p_Rotate15/rotate.mp4` — 15 seconds slow aim rotation; `boundary_floor_10s.mp4` is the ten-second floor-boundary review segment.
- `Retest3Formal_1080p_Wall15/wall.mp4` — 15 seconds translation/along-wall motion; `boundary_wall_static_10s.mp4` is the wall/static-object review segment.
- `Retest3Formal_1080p_TorchCycle/torchcycle.mp4` — Torch/Lantern/Torch.
- `Retest3Formal_1440p_Static10/static.mp4` — 10 seconds fixed camera/player.
- `Retest3Formal_1440p_Rotate15/rotate.mp4` — 15 seconds slow aim rotation.

All formal logs report D3D12/SM6, `diagnosticMode=0`, `stableDepthCoordinates=1`, `bindingFailure=0`, exact output ViewRects 1920x1080 or 2560x1440, and normal TSR primary buffers (1400x792 at the 1080p run and 1552x880 at the 1440p run). Contact sheets were opened for all six runs. They retain no gray horizontal line, no black/gray offset, wall surfaces with black behind, and no stuck-black tool transition. The severe scan found no fatal, assertion, ensure, GPU crash, device removal, DXGI, D3D12/RHI, SightWeave, or Darkwell error; the only pattern hit was the benign configuration line `r.GPUCrashDebugging:0`.

### Remembered-specific ROI data

Adjacent-frame MAD is reported in 8-bit channel-value units at 10 fps. It is supporting evidence, not visual acceptance:

| Resolution | ROI | median | p95 | max |
| --- | --- | ---: | ---: | ---: |
| 1080p | Remembered static gray | 0.000000 | 0.000028 | 0.090839 |
| 1080p | Remembered internal material | 0.000000 | 0.001034 | 0.095397 |
| 1080p | Remembered static-object/transition edge | 0.048412 | 0.078237 | 0.295581 |
| 1080p | Live control | 0.239873 | 0.375837 | 1.018358 |
| 1080p | HUD control | 0.000000 | 0.065925 | 0.152223 |
| 1440p | Remembered static gray | 0.000000 | 0.001262 | 0.064980 |
| 1440p | Remembered internal material | 0.000000 | 0.001632 | 0.070602 |
| 1440p | Remembered static-object/transition edge | 0.087935 | 0.104234 | 0.289404 |
| 1440p | Live control | 0.474410 | 0.538756 | 1.183153 |
| 1440p | HUD control | 0.000000 | 0.037798 | 0.085784 |

The second user recording's editor-toolbar control ROI, measured separately at six fps because the formal agent captures are standalone game windows, is median/p95/max `0.001525/0.039182/0.039583`. This confirms the user's observation that editor chrome was stable; it is not presented as post-fix editor-UI evidence. Agent judgment is based on the opened consecutive frames and contact sheets, not full-screen average MAD.

## 0. User dynamic PIE rejection (2026-08-29)

User recording: `Darkwell - 虚幻编辑器 2026-08-29 18-35-37.mp4`.

The recording was copied, without modification, to the ignored evidence directory `Saved/SightWeaveVisualRescueEvidence/UserDynamicPIEFailure`. The source and ignored copy both have SHA-256 `33298E98FB8B7AE52DA8C3F2F38794311338A5E44307FBEEEF2AA00DCC02CCD5`. The video is not a repository artifact and must not be committed.

User-confirmed result:

- the entire game image continuously shakes and flickers;
- multiple thin gray lines span the game view immediately after gameplay begins;
- the large edge staircase is materially smaller but residual aliasing remains;
- Remembered is now a three-dimensional scene instead of a flat gray 2D fill;
- enemy filtering is correct;
- the direction is materially improved, but the image remains unusable for the game.

Initial frame inspection established that the editor chrome remained stable while the embedded PIE game View changed, so the failure was inside the game View/render path rather than a whole-desktop capture displacement. Controlled A/B diagnostics have now resolved both causes; the result is recorded below. The rejected video and verdict remain authoritative history and are not overwritten by the new candidate.

This rescue slice was restricted to two blockers: (A) whole-view shaking/flicker and (B) gray-line/residual-geometry leakage. It preserves Ultra 2.5 cm/texel, the unified mutually exclusive three-state result, three-dimensional Remembered, `NeverRemember` enemy filtering, wall-surface classification, the black/gray alignment correction, and Torch/Lantern revision continuity.

## 0A. Two-blocker rescue result

Both blocker causes were reproduced and corrected without adding blur, expanding the mask, changing the black threshold, disabling TAA/TSR, or weakening static-surface classification.

### A/B attribution

- Composite bypass removed SightWeave-specific instability; the raw SceneColor path remained stable.
- With the rejected coordinate path, settled adjacent-frame mean absolute difference was `0.1544` for the scene and `1.2319` in the gray-line band. Jitter-compensated depth coordinates reduced those medians to `0.0134` and `0.0177` respectively.
- Player Transform, camera Transform, ViewRect, mask origin, state/feather revisions, resource binding, and submitted tile count were stable during the fixed-camera comparison. Therefore the static flicker was not gameplay motion, camera motion, revision churn, incremental submission, or resource rebinding.
- Removing Remembered removed the gray line. Removing CustomDepth/Stencil did not remove it. Removing wall-conservative sampling did not remove it. Forcing full mask rebuilds did not remove it.
- Unified-state output classified the line as Remembered, not Live SceneColor. Raw CustomDepth and CustomStencil did not contain a matching line. Raw memory did contain the full line; raw static attributes did not.
- Disabling AA was diagnostic only and was not accepted as a solution. Formal evidence below uses normal TSR. A startup mask freeze froze before valid authority was available and was explicitly treated as inconclusive rather than a pass.

### Exact causes and fixes

1. The post-process composite mapped stable output pixels to SceneDepth and CustomDepth as if both buffers shared the same temporal convention. SceneDepth reconstruction was jittered while CustomDepth is intentionally unjittered (`r.CustomDepthTemporalAAJitter=0`), so classification crossed mask and surface boundaries as the projection jitter changed. The formal path now maps through `ViewRect.Min` and ViewRect size, applies the current projection jitter only to SceneDepth sampling/reconstruction, and keeps CustomDepth/Stencil on their unjittered pixel coordinate. The corrected path is the default in Development, Test, and Shipping; the legacy path remains only as a Development/Editor A/B switch.
2. `FSightWeaveMemoryAuthority` clamped a scanline interval to `[0,247]` before testing whether that interval intersected the logical tile. An interval wholly outside a candidate tile was therefore collapsed into one false edge texel. Monotonic memory preserved those texels as world-aligned horizontal or vertical strips. The rasterizer now rejects non-intersecting intervals before clamping. A cross-tile concave-polygon regression test freezes the failure.

The old isolated-line detector reported `409–410` false horizontal pixels in every rejected settled frame at 640x360 analysis resolution. Across the new 1080p startup, rotation, wall, and Torch/Lantern/Torch sequences plus the 1440p rotation sequence (1,410 inspected frames), the maximum is `6` at 1080p and `2` at 1440p, with zero frames at or above 300.

## 1. Result

The first dynamic rescue candidate is materially different from the rejected M6P1 presentation:

- the 25 cm Coarse state field was replaced by the DARKWELL Ultra 2.5 cm field for both Live and Remembered;
- one mutually exclusive presentation state is resolved per visible surface: `0 Unknown`, `1 Remembered`, `2 Live`, with Live precedence;
- the Live feather transitions to the lower-priority resolved state instead of multiplying SceneColor toward black;
- Remembered is a classified, filtered static 3D scene representation rather than a neutral gray footprint fill;
- immutable ground/landmark and occluder wall surfaces are explicitly classified in the runtime fixture without changing a map asset;
- a small wall-only conservative bias exposes the player-facing wall surface while leaving the region behind Unknown;
- inactive compatible illumination no longer invalidates the entire render scope;
- unchanged resident atlas tiles now carry their valid content into the next packet revision, preventing incremental feather fail-black during Torch/Lantern cycling.

Before the user test, agent inspection of selected extracted frame sequences found no recurrence of the original large block staircase, blurred 25 cm staircase, Live/Remembered black seam, uniform gray fill, or whole-wall black consumption. That limited inspection missed the continuous whole-View instability and thin gray-line leakage visible in the user's real dynamic PIE. It is retained only as historical engineering evidence and is superseded for visual acceptance by the user recording and verdict above.

## 2. Root causes and replaced paths

The detailed source attribution is frozen in `Docs/SIGHTWEAVE_DARKWELL_VISUAL_RESCUE_EXECUTION_PLAN.md`. The implemented conclusions are:

1. `UDarkwellSightWeaveWorldSubsystem::TryActivate` forced both formal Live presentation and exploration memory to Coarse, which is 25 cm/texel. The rejected staircase was the projected authority grid, not an insufficient blur width.
2. `SightWeaveSingleTile.usf` reconstructed feather distance from a point-loaded binary mask and then multiplied Live SceneColor by the feather weight, producing a softened staircase and a Live-to-black seam.
3. Live, memory eligibility, and static eligibility were sampled separately and black was an implicit fallback. The final shader now resolves one exclusive presentation state before composition.
4. Remembered stored only a 2D footprint plus one intensity byte and intersected the floor plane. It could only produce flat gray and could not represent wall tops/sides or recognizable scene structure.
5. A ground-only XY lookup could not distinguish a visible wall surface from the ground behind the wall.
6. During tool cycling, Render treated a compatible but inactive illumination source as an invalid scope. A later incremental packet also advanced only dirty-tile revisions, causing stable resident tiles to fail feather residency checks with `FeatherUnavailable`.
7. The player-View composite mixed stable output coordinates, jittered SceneDepth reconstruction, and unjittered CustomDepth/Stencil without the required projection-jitter conversion. That made static surface and three-state classification change with the temporal sample.
8. The CPU memory scanline rasterizer clamped wholly out-of-tile intervals before rejecting them, turning empty intersections into permanent one-texel memory strips at logical tile edges.

The CPU gameplay authority, owner/floor/source declarations, subject `NeverRemember` policy, revision/snapshot contract, memory eligibility, Stalker/HUD shared authority, and strict Legacy/SightWeave exclusion were retained.

## 3. State, coordinate, and edge contract

The formal state is `ESightWeavePresentationState`:

```text
0 Unknown
1 Remembered
2 Live
```

`SightWeaveResolvePresentationState` gives Live precedence and otherwise selects Remembered only when the exact static-memory eligibility gate succeeds. Unknown is the sole fallback, not a competing black texture.

Live and Remembered use:

- the same world identity, owner, floor, floor origin, extent, and Ultra precision;
- 2.5 cm/texel in this integration slice;
- the existing 248x248 logical interior plus four-texel gutters;
- texel-center sampling at `index + 0.5` and the same stable floor origin;
- the same formal ViewRect, SceneDepth reconstruction, pre-view translation, and camera data;
- the same world-stable jump-flood feather reconstruction.

The rescue did not add temporal history blur or camera-snapped origins. Stability comes from the stable world origin, tenfold finer source field, logical-neighbor-aware distance reconstruction, correct carry-forward of unchanged atlas tiles, and explicit jittered-SceneDepth/unjittered-CustomDepth coordinate separation. ViewRect offsets, including embedded PIE viewports, are applied before either depth lookup. Feather remains presentation-only; gameplay queries retain the hard authority.

The former `SceneColor * VisualFeatherWeight` transition was replaced by a transition from the resolved lower state to Live SceneColor. This removes the extra black band between adjacent Live and Remembered regions.

## 4. Remembered and dynamic filtering

The DARKWELL integration fixture marks immutable visible surfaces through CustomDepth/Stencil:

- stencil 240: immutable ground and landmark;
- stencil 245: occluder wall surfaces;
- stencil 246 remains reserved for LastSeen proxies.

The formal player-View composite requires both a reserved static class and a SceneDepth/CustomDepth match. It reconstructs the current static 3D surface position but does not sample current SceneColor for Remembered. The output is synthesized from immutable class, geometry orientation, stable world-space detail, and the original static-eligibility atlas.

Default user-tunable parameters are:

- `r.SightWeave.RememberedBrightness=0.22`
- `r.SightWeave.RememberedContrast=0.42`
- `r.SightWeave.RememberedDetailStrength=0.055`
- `r.SightWeave.RememberedDetailWorldScale=160`
- `r.SightWeave.RememberedSurfaceDepthToleranceCm=8`
- `r.SightWeave.OccluderSurfaceBiasCm=7.5`

The Stalker, player, NPC-like dynamic subjects, particles, dynamic shadows, current lighting, animation, and dynamic material changes do not receive the immutable stencil and cannot enter the static Remembered result. The Stalker retains `NeverRemember`; the enemy and threat HUD consume the same hard-live snapshot/revision.

This is intentionally project-specific for the immutable `L_VisionIntegration` greybox slice. It is not a general captured-material or arbitrary-changing-world memory system.

## 5. Wall-surface rule

The shader uses actual classified scene depth for static surface identity. Only stencil-245 occluder surfaces receive a 7.5 cm conservative visibility/memory probe. This lets a wall top or player-facing side inherit the legal state at the hit boundary. Ground and the region behind the wall do not receive the probe, so Unknown begins behind the surface. Door openings and wall ends continue to follow the authority polygon.

No `.uasset`, `.umap`, `L_VisionIntegration`, `L_Prototype`, configuration, plugin descriptor, or `Darkwell.uproject` change was required.

## 6. Focused verification

All builds, Editor launches, shader compilation, and tests were run serially. No full historical matrix was run.

### Build

`Scripts/BuildEditor.ps1` completed successfully after each reliable C++ checkpoint, including the final two-blocker implementation at `eb2a827`. Target: `DarkwellEditor Win64 Development`.

### Focused NullRHI

- `Darkwell.SightWeave.VisualRescue.PresentationState.TruthTable`: Success.
- `Darkwell.SightWeave.M6P1.Integration.VerticalSliceAuthority`: rerun after the two fixes and succeeded, including Ultra shared scope, `NeverRemember`, shared Stalker/HUD authority, static classifications, tool-cycle no-fail-closed assertion, and restored valid render packet.
- `SightWeave.M3P4.Packaging.InwardFeatherShippingBoundaries`: Success.
- `SightWeave.M3P5.Packaging.StaticEnvironmentMemoryShippingBoundaries`: Success.
- `SightWeave.M3P5.Memory.Authority.ConcavePolygonDoesNotLeakAtTileEdge`: Success; this is the targeted regression for the false tile-edge memory strip.

One earlier command used an incorrect M3P5 test name and reported that no tests matched. It was not counted as a pass; the exact test above was rerun and succeeded.

### Focused real GPU

- `SightWeave.M3P5.Composite.ThreeStateAndMemoryFailure.D3D12`: Success after the final render-state change.
- RHI: D3D12; feature level/shader platform: SM6 / PCD3D_SM6.
- GPU: NVIDIA GeForce RTX 2070 SUPER.
- No fatal, assert, ensure, GPU crash, device removal, DXGI device error, or D3D12/RHI error was found in the focused final logs.

Unreal startup emits pre-existing UE 5.8 experimental Toolset/Python and automation-registration noise in these launches. The post-initialization scan found no SightWeave/Darkwell error, fatal, assert, ensure, GPU crash, device removal, DXGI device error, or D3D12/RHI error. The exact focused tests report `Result={Success}`; engine startup noise is recorded, not hidden or presented as a pass.

## 7. Dynamic formal-View evidence

All evidence is under ignored `Saved/SightWeaveVisualRescueEvidence`; it is not committed. Exact metadata and limitations are in `Saved/SightWeaveVisualRescueEvidence/METADATA.md`.

### Current two-blocker retest candidate (`eb2a827`)

The formal path uses D3D12/SM6 and normal TSR. No final result relies on AA-off, diagnostic color output, a frozen mask, or a still screenshot alone.

- 1080p startup, first five gameplay seconds: `Dynamic/1080p_startup/startup.mp4`
- 1080p completely static, five seconds: `Dynamic/1080p_static/static.mp4`
- 1080p slow aim rotation, ten seconds: `Dynamic/1080p_rotate/rotate.mp4`
- 1080p wall approach and controlled along-wall motion, ten seconds: `Dynamic/1080p_wall/wall.mp4`
- 1080p Torch -> Lantern -> Torch, twelve seconds: `Dynamic/1080p_torch_cycle/torchcycle.mp4`
- 1440p completely static, five seconds: `Dynamic/1440p_static/static.mp4`
- 1440p slow aim rotation, ten seconds: `Dynamic/1440p_rotate/rotate.mp4`

Every run has a sibling `game.log`; contact sheets and representative stills are stored beside each recording. Exact ViewRects are 1920x1080 and 2560x1440. All logged formal frames have `stableDepthCoordinates=1` and `bindingFailure=0`. The 1080p and 1440p static adjacent-frame scene MAD medians are `0.0125` and `0.0199`; p95 values are `0.0197` and `0.0245`. No full-black frame occurs in startup, rotation, wall, or tool-cycle captures. The wall run keeps player Z at `90.2` while moving between Y `-108.0` and `108.0`; invalid out-of-fixture experiments are not used as evidence.

The Torch-cycle HUD stills record Torch at t=1 s, Lantern `[BASE]` at t=4 s, and restored Torch at t=7 s and t=10 s. Render state/feather revisions advance together from 8 to 10 with no binding failure or stuck-black frame. The targeted VerticalSliceAuthority test separately confirms `NeverRemember`, Stalker/HUD shared authority, wall occlusion, Lantern removal of legal cone light, and Torch recovery.

The agent opened all contact sheets and representative frames, computed adjacent-frame stability for both static resolutions, and ran the isolated-horizontal-line detector across every frame of the five motion/transition recordings. This is evidence for offering a retest, not a substitute for the user's real dynamic PIE judgment.

### Historical first-candidate evidence

### 1080p

The 1920x1080 run used D3D12/SM6, native output, requested screen percentage 100, TSR (`r.AntiAliasingMethod=4`), and TemporalAA quality 2. Key evidence:

- `1080p/slow_aim_sweep.mp4`
- `1080p/controlled_motion.mp4`
- `1080p/sweep_adjacent_150_159.png`
- `1080p/sweep_contact_1.png`, `sweep_contact_2.png`
- `1080p/motion_contact_1.png`, `motion_contact_2.png`
- `1080p/game.log`

The controlled sequences cover slow cone rotation, circle+cone motion, wall/doorway traversal, Live/Remembered transitions, Stalker/HUD disappearance, and camera follow. The latter portion of an earlier `dynamic_sequence.mp4` left the fixture and became all black; it is explicitly invalid evidence and was replaced by controlled reruns.

### 1440p

The 2560x1440 run used the same RHI/AA settings. Key evidence:

- `1440p/controlled_dynamic.mp4`
- `1440p/adjacent_150_159.png`
- `1440p/contact_1.png`, `contact_2.png`, `contact_3.png`
- `1440p/torch_cycle_final.mp4`
- `1440p/torch_cycle_final_contact.png`
- `1440p/torch_final_before.png`, `torch_final_lantern.png`, `torch_final_restored.png`
- `1440p/game.log`
- `1440p/game_tool_cycle_final.log`

The final tool-cycle run shows Torch Live, Lantern with the cone retained only as filtered Remembered while the body radius remains Live, and restored Torch Live. The Stalker/threat HUD disappear and return with the same authority transition. The final log remains `submitted-feather` with `bindingFailure=0` before and after cycling.

The agent opened all key stills/contact sheets, two sheets of ten adjacent 30 fps frames, the 1080p extracted sweep/motion sequences, the 1440p extracted sequence contacts, and the final 28-frame tool-cycle contact sheet. The agent did not claim direct reliable real-time video playback; conclusions are limited to the opened frames and logs.

## 8. Retained limits

- Only the user can determine long-duration dynamic PIE usability. Agent-side extracted frames cannot prove the absence of every transient crawl or subjective objection.
- Pixel-scale raster/TSR grain remains at the transition. The rejected 25 cm block staircase is gone, but this is not a claim of mathematically analytic edges.
- Remembered is a DARKWELL-specific filtered static greybox representation, not a general material snapshot or mutable-world solution.
- The current agent evidence is sufficient only for `PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST_3`. It does not authorize `COMPLETED` or `ACCEPTED — DARKWELL USABLE`.
- No SaveGame wiring, damage reveal, Warden, production `L_Prototype` switch, multi-floor support, plugin public-API cleanup, BuildPlugin, Cook, Package, clean-host, full NullRHI/D3D12 history, or performance matrix was run.
- Earlier failed tool-cycle captures are retained as honest diagnostic evidence and are not final acceptance evidence.

## 9. Reliable commits

- `dfda1e7` `docs: record SightWeave visual rescue root cause`
- `c2c4d8f` `docs: normalize visual rescue plan formatting`
- `fa787ef` `render: unify DARKWELL fog state reconstruction`
- `278986f` `render: restore filtered static remembered scene`
- `ad603e5` `fix: preserve DARKWELL fog across tool cycling`
- `ce32d50` `fix: carry forward stable fog tile revisions`
- `6d9e7db` `test: capture DARKWELL fog temporal failure`
- `800264e` `test: add DARKWELL fog A/B diagnostics`
- `37c5f0c` `fix: align SightWeave depth classification with view jitter`
- `eb2a827` `fix: remove remembered surface line leakage`
- `434f21e` `test: isolate remembered temporal instability`
- `bac0525` `render: stabilize DARKWELL remembered composition`
- `5c0049f` `docs: record third DARKWELL visual retest failure`
- `12b1118` `render: prototype pre-TSR DARKWELL fog composition`
- `05d2f03` `test: prepare pre-TSR DARKWELL dynamic proof`

The first documentation commit contained Markdown trailing whitespace because a PowerShell command sequence did not short-circuit on `git diff --check`; `c2c4d8f` corrected it without rewriting history. No source checkpoint was affected.

## 10. Decision gate

The first and second candidates failed the user's dynamic PIE. The Remembered-only rescue now has controlled A/B attribution, normal TSR, targeted D3D12/SM6 dynamic evidence at 1080p and 1440p, Remembered-specific ROIs, and agent consecutive-frame inspection. Its highest permitted state is `PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST_3`; only the user's third real dynamic PIE can decide usability. No further implementation or deferred full matrix begins before that verdict. The final product decision remains exactly one of:

- `ACCEPTED — DARKWELL USABLE`
- `REJECTED — ABANDON SIGHTWEAVE`

`PARTIAL` cannot extend past the stop-loss decision.

## 11. Temporal-coherent pre-TSR rescue result (2026-08-30)

### 11.1 Root cause and corrected interpretation

The third candidate did not disprove pre-TSR composition. Its exact failed B path combined jittered pre-TSR SceneColor/SceneDepth with unjittered, point-sampled CustomDepth/Stencil. That categorical boundary changed relative to the primary color sample before TSR. The accurate root cause is temporal-space incoherence at the CustomStencil surface-classification boundary.

UE 5.8.1 defines `r.CustomDepthTemporalAAJitter` as render-thread-safe. With value zero, `RenderCustomDepthPass` builds View shader parameters without jitter; both CustomDepth and CustomStencil use the same depth-stencil target and projection. The adjusted View parameters are used by the non-Nanite mesh pass and Nanite packed views. No later DARKWELL code was found overriding the value. The plugin's own `Engine.ini` did override the host setting during the first formal recording attempt; that attempt is preserved as invalid configuration-authority evidence. The plugin override was removed, the project remains authoritative at value 1, and later runtime logs prove `customDepthTemporalAAJitter=1`, set-by project setting.

### 11.2 B0, B1, and fog-off comparison

All values below are adjacent-frame output-space measurements from ignored evidence. Raw pre-TSR jitter movement is not treated as final-output failure.

| 1080p static left wall | MAD p50 | MAD p95 | max | edge range | one-pixel flips |
|---|---:|---:|---:|---:|---:|
| fog off | 0.026006 | 0.059748 | 0.746291 | 3 px | 40 |
| B0 Live fixed class | 0.120449 | 0.204412 | 0.594505 | 0 px | 0 |
| B0 Remembered fixed class | 0.181084 | 0.321798 | 0.742636 | 0 px | 0 |
| B1 Live temporal-coherent class | 0.038682 | 0.064218 | 0.483562 | 2 px | 34 |
| B1 Remembered temporal-coherent class | 0.167580 | 0.265042 | 0.691145 | 1 px | 27 |

| 1440p static left wall | MAD p50 | MAD p95 | max | edge range | one-pixel flips |
|---|---:|---:|---:|---:|---:|
| fog off | 0.081396 | 0.154284 | 0.766694 | 2 px | 32 |
| B0 Live fixed class | 0.167583 | 0.251606 | 0.648831 | 0 px | 0 |
| B0 Remembered fixed class | 0.267981 | 0.392482 | 0.790909 | 0 px | 0 |
| B1 Live temporal-coherent class | 0.037762 | 0.069319 | 0.632489 | 2 px | 26 |
| B1 Remembered temporal-coherent class | 0.207119 | 0.325589 | 0.694089 | 3 px | 49 |

The rejected unjittered B measured Remembered p95 `1.119474` with 86 flips at 1080p. B1 is materially lower. Opened B0/B1 contacts and adjacent crops showed no gray-line or black/gray-seam recurrence. B0 passed, then B1 passed the project visual gate; the stencil-free Phase D was therefore not entered.

Raw mode-7 CustomStencil is expected to move with temporal jitter: at 1080p the two sampled wall boundaries had range 1 pixel with 162/160 flips; at 1440p they had range 1 pixel with 24/32 flips. The formal output does not use an unjittered categorical sample against jittered color. Final TSR, not raw primary input, is authoritative.

### 11.3 Formal frame data path

```text
L_VisionIntegration primary View
  -> jittered SceneColor + SceneDepth
  -> jittered CustomDepth/CustomStencil, same primary ViewRect/extent
  -> BeforeDOF SightWeave state/surface classification and filtered Remembered composite
  -> normal UE 5.8 TSR/history rejection
  -> Tonemap and final 1080p/1440p output
```

The selected pass is chosen in `FSightWeaveSceneViewExtension::SubscribeToPostProcessingPass`; the callback is `PostProcessComposite_RenderThread`; RDG binding and runtime diagnostics are in `FSightWeaveSparseAtlasRenderState::AddHardMaskComposite_RenderThread`; shader coordinate resolution and final state composition enter through `SightWeaveResolveDepthCoordinates` and the hard/feather composite entries in `SightWeaveSingleTile.usf`.

At 1080p, runtime logs record a 1920x1080 output View with 1400x788 active primary color/depth in 1400x792 extents. At 1440p, output is 2560x1440 with 1552x873 active primary color/depth in 1552x880 extents. CustomDepth extent matches SceneDepth in both cases. `ViewRect.Min=(0,0)` for the standalone evidence, diagnostic mode is 0, and all logged formal frames have `bindingFailure=0`.

The project does not combine post-TSR color with pre-TSR depth, does not apply a second unjitter operation, and does not sample a previous SightWeave history. Other plugin maps retain the older post-Tonemap callback and are explicitly outside this DARKWELL-first migration.

### 11.4 Formal dynamic evidence and ROI

Ignored evidence root: `Saved/SightWeaveVisualRescueEvidence`.

- 1080p: `Dynamic/FormalJ1_1080_Run1_RememberedStatic20`, `Run2_Rotate30`, `Run3_Wall30`, `Run4_Startup20`, `Run5_LiveStatic20`, `Run6_TorchCycle`, and `Run7_Soak600`.
- 1440p: `Dynamic/FormalJ1_1440_Run1_RememberedStatic20`, `Run2_Rotate30`, and `Run3_Wall30`.
- raw stencil: `Dynamic/B1RawStencil_1080_Static10` and `Dynamic/B1RawStencil_1440_Static10`.
- B0/B1/fog-off controls remain under their original `Dynamic/B0_*` and `Dynamic/B1_*` directories.

The agent opened all six main static/rotation/wall contact sheets, the startup/Live/tool contacts, the 10-minute contact-by-minute sheet, and 1080p/1440p ten-adjacent-frame Remembered crops. The agent did not claim to watch each video in real time. The 600-second recording is verified by `ffprobe`, reaches render frame 53,796, and closes naturally.

| formal static Remembered ROI | 1080p p95 | 1440p p95 |
|---|---:|---:|
| static gray interior | 0.118438 | 0.112972 |
| internal material detail | 0.127614 | 0.127936 |
| static-object edge | 0.165304 | 0.176164 |
| Live control | 0.335713 | 0.375879 |
| HUD control | 0.015497 | 0.014938 |

Values are RGB mean absolute difference in 8-bit code values. The Remembered interior and edge are less active than the Live control. Opened adjacent crops show the gray wall/floor detail and boundary fixed in place; no whole-gray-layer shaking is visible. Startup has no horizontal gray lines. Torch/Lantern/Torch loses and restores legal cone illumination without a stuck-black terminal state. Rotation and wall contacts preserve the black/gray seam repair and do not show a return of the former large block staircase.

### 11.5 Automation and builds

- `DarkwellEditor Win64 Development`: succeeded after every source/test checkpoint. Final relevant builds were 10/10, 4/4, 9/9, and 4/4 actions.
- temporal-space placement: 1/1 Success.
- D3D12 three-state composite: 1/1 Success.
- tile-edge regression: 1/1 Success.
- M3.4: first run 7 Success / 1 Fail due stale Tonemap-only source-string expectation; after selected-pass contract correction, 8/8 Success.
- M3.5 NullRHI group: 18/18 Success.
- M6P1 group: 4/4 Success, including wall, doorway target authority, `NeverRemember`, Stalker/HUD shared revision, Lantern loss of legal cone light, and Torch restoration.
- complete DARKWELL: 29/29 Success.
- complete SightWeave NullRHI: two preserved runs were 197/198 because two stale selected-pass source-string expectations were corrected in sequence; final run is 198/198 Success.
- complete SightWeave D3D12/SM6: 287 Success / 3 Fail out of 290. The failures are legacy M4P1 Lab screenshot/proxy pixel baselines (`Camera34Observability`, `ContinuousTransition`, `LastSeenLab`) on `L_SightWeave_Lab` after the host adopts jittered CustomDepth. They do not execute the DARKWELL pre-TSR map path. No threshold was relaxed and the failures remain recorded.
- `Darkwell Win64 Development`: succeeded, 5/5 actions.
- `Darkwell Win64 Shipping`: succeeded, 5/5 actions.

Build warnings are the non-preferred MSVC 14.51 toolchain notice and UE 5.8 deprecations in `Character.h` and `AISystem.h`. D3D12 automation also records Epic telemetry network warnings, an intentional corrupted-zlib negative-test warning, and an RHI reserved-virtual-size warning of 258 GB versus a 256 GB budget. No candidate dynamic log contains a fatal, assert, unhandled exception, GPU crash, device removal, or binding failure.

### 11.6 Preserved semantics and remaining limits

Preserved: Ultra 2.5 cm/texel, unified Unknown/Remembered/Live state, recognizable three-dimensional filtered static memory, tile-edge empty-interval rejection, black/gray alignment, readable wall surfaces with black behind, `NeverRemember`, shared Stalker/HUD authority, and Torch/Lantern/Torch recovery.

No user video, `Saved`, generated directory, asset, map, `L_Prototype`, or `Darkwell.uproject` was changed or committed. No SceneCapture, TSR-off formal path, blur increase, mask expansion, or resolution reduction was used.

The integration fixture uses ordinary BasicShapes meshes; a separate Nanite wall visual comparison was not available. Engine source establishes shared jittered View construction for Nanite/non-Nanite CustomDepth, but this is not a fixture-level Nanite pass claim. A separate scripted doorway traversal was not captured; wall-end movement and M6P1 doorway authority passed, while subjective doorway visuals remain in retest four. The project relies on normal TSR's local history handling after pre-TSR semantic composition; no bespoke SightWeave reactive texture was added. The three legacy plugin-Lab D3D12 failures remain deferred under the explicit DARKWELL-first priority.

The candidate is therefore `PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST_4`, not `COMPLETED`. User dynamic PIE remains the visual and semantic authority. Rejection triggers the 2026-09-05 stop-loss and SightWeave abandonment review; it does not authorize another local post-process compensation cycle.

## 12. Stencil-free SurfaceMaterial vertical slice (2026-08-30)

### 12.1 Superseding disposition and asset audit

The fourth user failure remains authoritative for every screen-space candidate. This section supersedes only the old retest-4 readiness statement and records a new architecture at source candidate `8daed357e26aa8fa41be0892d63e906bc153317e`:

```text
PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST_5
```

This is not `COMPLETED` and is not user visual acceptance.

The read-only Asset Registry audit found that the `L_VisionIntegration` fixture used `/Engine/BasicShapes/Cube` for ground, both walls, and the landmark; all were non-Nanite and used `/Engine/EngineMaterials/WorldGridMaterial`. No project master material, Material Layers stack, or BaseColor/Normal/Roughness/AO source texture was present. The fixture Stalker had no skeletal mesh assignment. Consequently, the vertical slice could not inherit a missing project floor texture. It establishes a matched real sampled texture baseline using `/Engine/Engine_MI_Shaders/T_Base_Tile_Diffuse`, then uses that same BaseColor source in Live, Remembered, and fog-off paths. This is not a `frac` grid, scanline, noise injection, SceneCapture, or SceneColor snapshot.

Five project assets were created through Unreal Editor Python and saved normally under `/Game/Darkwell/Vision/Materials`:

- `MF_DarkwellSightWeaveSurface`;
- `M_DarkwellSightWeaveSurface`;
- `MI_DarkwellSightWeaveFloor`;
- `MI_DarkwellSightWeaveWall`;
- `MI_DarkwellSightWeaveStatic`.

All five are Git LFS objects. Both walls share one material instance. No map asset needed modification because the existing integration fixture applies and restores the overrides at runtime. `L_VisionIntegration`, `L_Prototype`, external/Fab assets, and `Darkwell.uproject` remain unchanged.

### 12.2 Formal state-texture and frame path

`USightWeaveRenderWorldSubsystem::EnableSurfaceMaterialPresentation` owns a cross-frame-stable `UTextureRenderTarget2D` in `RTF_RGBA8`, nearest/clamp, with no mip generation. It is allocated for the active Ultra scope and reused until texture extent or world mapping changes. `FSightWeaveSceneViewExtension::ConfigureSurfaceMaterialTarget` transfers the texture RHI and mapping to the render thread. `FSightWeaveSparseAtlasRenderState::ProcessSurfaceMaterialState_RenderThread` registers the persistent target with RDG and updates only the dirty logical tiles, or performs the explicit full update required by a new resource/scope. It reads the existing live, feather, and exploration-memory GPU mirrors directly; there is no GPU-to-CPU readback. Static scene appearance comes from the bound surface material rather than the old neutral-intensity static atlas.

The pixel contract emitted by `SightWeaveSurfaceStatePS` in `SightWeaveSingleTile.usf` is:

```text
R = mutually exclusive state / 2        (Unknown=0, Remembered=0.5, Live=1)
G = Live visual feather coverage
B = Remembered validity
A = world/scope validity
```

Material UV comes only from `(AbsoluteWorldPosition.xy - WorldMin) / WorldExtent` using the Ultra 2.5 cm/texel mapping. Scope/resource replacement updates the texture and transform as one adapter operation; teardown clears the scene-view target and UObject reference.

The formal visual path is now:

```text
SightWeave CPU authority
  -> sparse live/feather/exploration-memory GPU mirrors
  -> persistent dirty-tile RGBA8 world-state texture
  -> real primitive rasterization + project surface material
  -> normal depth / coverage / velocity
  -> normal D3D12 SM6 TSR
  -> UI
```

When the surface target is active, `FSightWeaveSceneViewExtension::SubscribeToPostProcessingPass` does not register the old post-TSR or pre-TSR full-screen formal composite. CustomDepth/Stencil is not sampled by the new material and does not determine ground, wall, landmark, or doorway coverage. Development-only `r.Darkwell.SightWeave.Diagnostic.SurfaceFogOff` changes the same material graph to its matched original attributes for A/B evidence; it is not a Shipping product setting.

### 12.3 Material semantics and primitive categories

Live returns the material's original texture/attributes. Remembered derives from the original sampled BaseColor, retains recognizable luminance structure and a controlled amount of color, reduces saturation/contrast/brightness, raises roughness, suppresses metallic/specular response, and uses stable emissive information rather than current dynamic lighting. Unknown writes black with no emissive, metallic, or specular leakage. State priority remains `Live > Remembered > Unknown`; visual feather does not change gameplay authority.

Custom Primitive Data is frozen as:

| index | meaning |
|---:|---|
| 0 | surface category: 0 ground, 1 wall/side, 2 rememberable static, 3 excluded |
| 1 | stable wall sample direction X |
| 2 | stable wall sample direction Y |
| 3 | wall sample distance in cm |

The two 40 cm-thick fixture walls use direction `(+1,0)` and distance `27.5 cm`: 20 cm half-thickness plus the contract's 7.5 cm bias. Ground samples the exact surface point. Wall/side category takes the highest valid state from the center and stable bidirectional offset samples. Other non-ground static surfaces use their pixel normal with the 7.5 cm bias. This preserves the facing wall surface while keeping Unknown behind it; it does not dilate the global mask or hide all walls.

The M6P1 adapter remains authoritative for the `NeverRemember` Stalker and threat HUD. They consume the same hard-Live snapshot/revision and do not rely on a gray overlay or Stencil erase. Torch/Lantern/Torch resource revisions, tile-edge empty-interval rejection, unified three-state authority, Ultra precision, and black/gray alignment remain unchanged.

### 12.4 Directional, texture, static, and matched-control evidence

Ignored evidence root is `Saved/SightWeaveVisualRescueEvidence`. The formal horizontal/vertical candidate directories are `Dynamic/SurfaceFinal1080Horizontal`, `SurfaceFinal1080Vertical`, `SurfaceFinal1440Horizontal`, and `SurfaceFinal1440Vertical`; each contains an exact 30-second, 900-frame video, log, adjacent frames/contact sheet, and `metrics.json`. Their matched same-material fog-off controls are the corresponding `Dynamic/SurfaceFogOff*Horizontal` and `*Vertical` directories.

The four directional fog-off videos were recorded before the later wall-CPD conservative-sampling checkpoint. This does not affect their control output: `SurfaceFogOff` returns the same master graph's original material attributes before any state or wall sample is applied, and the CPD change only affects the fog-on wall-state lookup. Static/rotation/wall/doorway/tool fog-off controls were recorded after the final source checkpoint.

Motion-compensated half-resolution metrics over all 899 adjacent-frame pairs are:

| resolution / direction | Surface bright MAD p50 / p95 / max | fog-off p50 / p95 / max | Surface black-edge XOR p95 | fog-off XOR p95 | Surface minus fog-off XOR p95 |
|---|---:|---:|---:|---:|---:|
| 1080p horizontal | 0.688155 / 1.565711 / 1.865470 | 1.616708 / 2.821813 / 3.521747 | 0.001732 | 0.001245 | +0.000487 |
| 1080p vertical | 1.446203 / 1.989685 / 2.184379 | 1.903431 / 3.215450 / 3.451829 | 0.000805 | 0.000806 | -0.000001 |
| 1440p horizontal | 0.835623 / 1.933627 / 2.297000 | 2.387048 / 3.069116 / 3.609929 | 0.001335 | 0.000975 | +0.000360 |
| 1440p vertical | 1.984994 / 2.234265 / 2.485494 | 2.950415 / 3.300202 / 3.921712 | 0.000810 | 0.000620 | +0.000190 |

The worst additional black-edge occupancy is 0.000487, about 0.049% of the analyzed ROI. The compensated bright-region p95 is lower than fog-off by 1.065937 to 1.256102 code values. No one-pixel alignment correction was selected at 1080p; the 1440p candidate selected a one-pixel correction on 3/899 horizontal and 8/899 vertical pairs, versus 13/899 and 10/899 for fog-off. Leading/trailing p95 fractions remain below 0.001053. These measurements support, but do not replace, the opened adjacent frames or user PIE.

At the matched 10-second texture ROI, Remembered-to-fog-off luminance spatial correlation is `0.977992`/`0.869252` at 1080p horizontal/vertical and `0.830543`/`0.884501` at 1440p. Surface local variance is `3688.39`, `169.37`, `4081.62`, and `180.49`; edge energy is `12.006`, `12.793`, `11.349`, and `12.156`. The two lower-variance directional ROIs sample a comparatively uniform portion of the texture, but correlation remains high. No noise was added to inflate variance.

Dedicated fixed-frame static ROIs over 600 frames report:

| ROI | 1080p adjacent MAD p50 / p95 / max | 1440p p50 / p95 / max |
|---|---:|---:|
| Remembered floor | 0.016697 / 0.038452 / 0.370881 | 0.025502 / 0.050425 / 0.383874 |
| Live floor control | 0.023570 / 0.051438 / 0.423150 | 0.041574 / 0.067611 / 0.444861 |
| HUD control | 0 / 0.001083 / 0.115542 | 0.000372 / 0.002468 / 0.065517 |
| Unknown control | 0 / 0 / 0 | 0 / 0 / 0 |

Unknown mid-frame mean and variance are both exactly zero at both resolutions. Remembered mid-frame variance is `2504.59` at 1080p and `2697.69` at 1440p, which rules out a uniform gray fill for these ROIs.

Additional candidate evidence exists under `Dynamic/SurfaceFinal{1080,1440}{LiveStatic,RememberedStatic,Rotate,Wall,Doorway,TorchCycle}`. Matched fog-off controls exist for Static, Rotate, Wall, Doorway, and TorchCycle at both resolutions. The 1080p fixed `SurfaceFinal1080StaticSoak300` is exactly 300 seconds/9000 frames. At 150/180 seconds the HUD records Torch 25%/11%; by 270 seconds it records Torch 0% `[EMPTY]`. Player and camera transforms remain fixed. The expected Live-to-Remembered transition retains the sampled floor pattern instead of failing black. The earlier `SurfaceFinal1080Continuous300` is excluded: alternating input drove the fixture character outside the floor and down to approximately Z=-1,046,000 cm, making it a harness failure rather than valid render evidence.

### 12.5 Agent visual inspection

The agent opened the 1080p and 1440p Live and Remembered static sheets; rotation, wall, doorway, and tool sheets; matched fog-off rotation/wall/doorway sheets; 1080p and 1440p horizontal/vertical adjacent frames; full-resolution doorway frames; and the fixed-soak sheet plus 150/180/270-second frames. The opened evidence shows a recognizable sampled floor texture in Remembered, black Unknown, readable facing wall surfaces with black behind, continuous doorway coverage, and no recurrence of the horizontal gray line or black/gray offset. Directional adjacent frames do not show the old left/right or top/bottom whole-edge oscillation. The tool sheet also shows the wheel/UI after fog and the expected legal-light changes.

The agent did not claim to watch every video in real time and cannot substitute for the user's fifth dynamic PIE. Contact-sheet downscaling can make the 2.5 cm state boundary look toothier than the opened full-resolution frame; the full-resolution adjacent frames are the governing agent inspection evidence.

### 12.6 Minimal build, automation, logs, and exclusions

- `Scripts/BuildEditor.ps1`: `DarkwellEditor Win64 Development` succeeded after the doorway trajectory source change (5/5 actions; compile, module compile, library link, DLL link, metadata). Warnings are the non-preferred MSVC 14.51 toolchain and existing UE deprecations.
- Surface state truth/mapping/wall/CPD: 3/3 Success.
- M6P1 vertical-slice authority, including shared Stalker/HUD revision, `NeverRemember`, and Torch/Lantern/Torch authority: 1/1 Success.
- M3.4 presentation/feather: 3/3 Success.
- M3.5 static-environment memory: 2/2 Success.
- M4P1 `NeverRemember` subject policy matrix: 1/1 Success.
- D3D12/SM6 three-state composite smoke: 1/1 Success on RTX 2070 SUPER, forced SM6, shader model 6.7 support.
- Total bounded automation: 11/11 Success. Every automation log is severe-clean.
- Formal dynamic severe scan: 31/31 logs have zero fatal/assert/ensure/GPU-crash/device-removal/device-hung/critical-error hits.

No complete SightWeave regression, complete DARKWELL regression, BuildPlugin, clean-host, Cook, Package, Staged Shipping, performance matrix, Fab compatibility pass, game Development/Shipping build, or `L_Prototype` acceptance was run. The broad historical results in section 11 are retained as history and were not rerun for this vertical slice.

### 12.7 Remaining authority and stop rule

The project-side agent gate supports `PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST_5`; it does not establish subjective production usability. The fixture is intentionally small, non-Nanite, and lacks particle/dynamic-VFX consumers. The Stalker fixture has no skeletal mesh and is therefore validated as whole-actor authority/filtering, not per-pixel skeletal coverage. The texture baseline is a project prototype texture because the fixture had no original art texture. These are disclosed limits, not claims of plugin generality.

The user must retest normal PIE at 1080p and 1440p with D3D12/SM6 and normal TSR. If the user rejects motion stability, texture readability, wall/door coverage, enemy filtering, or tool recovery, the required state is `BLOCKED — SURFACE MATERIAL VISUAL PROTOTYPE FAILED / SIGHTWEAVE ABANDONMENT REVIEW REQUIRED`. The response must not return to a fifth screen-space patch. The 2026-09-05 stop-loss remains unconditional.

## 13. Final 48-hour SurfaceMaterial architecture proof (2026-08-30)

### 13.1 Disposition and source checkpoints

The fifth user dynamic PIE rejection at `011fcbd3ce53704b14a89fd0d293995d52f2b427` remains the controlling verdict on the previous candidate. It was recorded without weakening the visual contract in `1303b4c12c8032ed4089e898023846fba4c28784`. The final project-only architecture was introduced in `e4120cc4ecc82873c0d9d72967cc21079499d921`, and the horizontal-surface sampling correction was built, tested, and pushed in `96183a2258fa2b79e9991f140b76f28ffac6fdd8`.

The agent-side architecture gate now supports only:

```text
PARTIAL — READY_FOR_USER_FINAL_ARCHITECTURE_PIE
```

This does not supersede the user's fifth rejection with a success claim and is not `COMPLETED`. A user-operated final dynamic PIE is still mandatory. If that PIE rejects the candidate, the required terminal state remains:

```text
BLOCKED — SURFACE MATERIAL ARCHITECTURE PROOF FAILED
SIGHTWEAVE VISUAL LAYER ABANDONED
```

No post-TSR/pre-TSR full-screen patch, SceneCapture, SceneColor history, jitter compensation, enlarged blur, mask expansion, TSR disable, resolution reduction, `L_Prototype` change, or plugin/Fab generalization was introduced.

### 13.2 Confirmed roots and the new surface contract

The rejected contract used one primitive-wide `(+1,0)` wall direction in CPD `[1]/[2]`, then selected center/both-side samples. A cube or thick wall cannot use one direction for all rendered faces. The formal replacement uses the stable interpolated `VertexNormalWS`, corrected by `TwoSidedSign`, projects that geometric normal into world XY, and samples only the current rendered face's outward free space. It never uses `ViewDirection` and never takes the maximum of both wall sides. CPD `[0]` remains the surface category and CPD `[3]` remains the surface sampling distance; CPD `[1]/[2]` no longer decide formal wall direction. Horizontal floor/top faces use the exact `AbsoluteWorldPosition.xy`.

The last blocking floor failure had a separate, exact cause. `Normalize(GeometricNormalWS.xy)` was evaluated for every pixel before its wall-only distance was applied. A horizontal face supplies `(0,0)` in XY; the generated material produced a non-finite value, and multiplying that value by zero did not recover a valid UV. Controlled A/B established that a known-white texture and a uniform-white runtime RT both bound correctly, and that a direct computed world-UV threshold bisected the floor correctly, while an RT half-plane affected walls but not the floor. The fix computes `length(N.xy)`, divides by `max(length, 0.0001)`, and then applies the wall-only distance. The corrected RT half-plane exactly matched the direct UV threshold on the ground.

The same material now subtracts `SightWeaveWorldMin` in LWC space and explicitly truncates the small local coordinate to float before converting to UV. It samples the persistent state RT at explicit mip 0. The runtime RT is linear `RTF_RGBA16f`, bilinear, clamp-addressed, and reused across frames. These choices remove the prior RGBA8 hard-threshold presentation and prevent sampler/default-texture ambiguity.

The final state texture contract is:

```text
R = discrete Unknown / Remembered / Live authority divided by 2 (diagnostics only)
G = continuous world-anchored LiveCoverage
B = continuous world-anchored KnownCoverage
A = scope/resource validity
```

The material computes `LiveWeight = saturate(G) * A`, `KnownWeight = max(saturate(B), LiveWeight)`, `RememberedWeight = saturate(KnownWeight - LiveWeight) * memory eligibility * A`, and `UnknownWeight = saturate(1 - LiveWeight - RememberedWeight)`. Thus CPU gameplay authority remains discrete while the visible edge uses independent continuous coverage. `NeverRemember` remains a category/authority rule rather than a color erase.

Remembered is derived from the real sampled static BaseColor: luminance-preserving desaturation, contrast `0.90`, brightness `0.46`, saturation `0.10`, high roughness, and no live metallic/specular contribution. It does not sample current SceneColor, GBuffer lighting, dynamic shadow, particle, Stalker, SceneCapture, temporal history, `frac` grid, or time noise. Unknown has no BaseColor/emissive/specular leakage. The SurfaceMaterial is rendered with ordinary primitive depth/coverage/velocity before the project's normal D3D12/SM6 TSR. While the surface target is active, `FSightWeaveSceneViewExtension::SubscribeToPostProcessingPass` registers neither the post-TSR nor pre-TSR formal composite.

The concrete path is:

```text
CPU hard authority + exploration memory
  -> live / visual-feather / memory GPU mirrors
  -> FSightWeaveSparseAtlasRenderState::ProcessSurfaceMaterialState_RenderThread
  -> persistent RGBA16F KnownCoverage/LiveCoverage world texture
  -> ADarkwellVisionIntegrationFixture surface MIDs
  -> MF_DarkwellSightWeaveSurface / M_DarkwellSightWeaveSurface
  -> ordinary primitive depth, coverage and velocity
  -> normal project TSR
  -> HUD
```

Relevant implementation entries are `USightWeaveRenderWorldSubsystem::EnableSurfaceMaterialPresentation`, `FSightWeaveSceneViewExtension::ConfigureSurfaceMaterialTarget`, `FSightWeaveSceneViewExtension::PreRenderViewFamily_RenderThread`, `FSightWeaveSparseAtlasRenderState::ProcessSurfaceMaterialState_RenderThread`, shader entry `SightWeaveSurfaceStatePS`, and the project material generator `Content/Python/create_sightweave_surface_materials.py`.

### 13.3 E1 — fixed state and fixed camera

Formal 20-second Live and Remembered static samples and matched fog-off controls were recorded at 1080p; the Remembered pair was repeated at 1440p. A Development authority-color sample selected a fully Remembered, non-boundary floor ROI before measurement. The authoritative source and camera were fixed for all 600 frames.

| Output | Remembered mean | Remembered std | Remembered RMS contrast | fog-off mean / std / RMS | std retained | luminance correlation | Remembered adjacent MAD p95 | Unknown MAD p95 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1920x1080 | 177.523990 | 5.744403 | 0.032358 | 178.820707 / 8.424842 / 0.047113 | 68.1841% | 0.947743 | 0.044192 | 0 |
| 2560x1440 | 177.883358 | 6.027118 | 0.033882 | 178.988372 / 8.808064 / 0.049210 | 68.4273% | 0.963518 | 0.058503 | 0 |

Both resolutions exceed correlation `0.80` and contrast/std retention `35%`. The opened full-size frames show the real mid-frequency floor pattern at normal scale, readable wall/door/static-object structure, and truly black Unknown. The agent did not infer texture retention from correlation alone.

The native mapping test proves face samples at `+X/-X/+Y/-Y`, verifies that floor/top keep their exact world position, and freezes the 27.5 cm thick-wall sample distance. The authority view and the E2/E3 wall/door contacts visually cover both wall faces and wall ends. No primitive-wide fixed direction remains in the formal path.

### 13.4 E2 — authority frozen, camera/player moving

Development-only `SurfaceProofMode=1` froze authority while deterministic 15–20 cm/s trajectories moved the player and following camera. Each fog-on path had an equivalent same-material fog-off process. Horizontal, vertical, slow rotation, along-wall, and doorway runs are each exactly 30 seconds / 900 frames at 1920x1080, D3D12/SM6, normal TSR.

| E2 motion | aligned bright MAD p95 | black-edge XOR p95 | leading p95 | trailing p95 | one-pixel alignment count / 899 |
|---|---:|---:|---:|---:|---:|
| horizontal | 3.12 | 0.00 | 0.00 | 0.00 | 1 |
| vertical | 3.45 | 0.00 | 0.00 | 0.00 | 0 |
| slow rotation | 0.03 | 0.00 | 0.00 | 0.00 | 0 |
| along wall | 2.81 | 0.00 | 0.00 | 0.00 | 0 |
| doorway | 3.13 | 0.00 | 0.00 | 0.00 | 0 |

The agent opened all five ten-frame contacts and all five twenty-adjacent-frame strips. The fixed world boundary moves continuously with the camera; no old left/right or top/bottom whole-edge oscillation, gray line, black/gray seam, wall-direction black/bright inversion, or discontinuous doorway strip is visible.

### 13.5 E3 — camera frozen, source moving

Development-only `SurfaceProofMode=2` fixed the fixture proof camera while the visibility source translated at 20 cm/s and rotated through a full slow sweep. Each run and its fog-off control is 30 seconds / 900 frames at 1080p, D3D12/SM6, normal TSR. Translation covers new Live, Live-to-Remembered trail, and Known-to-Unknown boundary movement; the full source rotation sweeps previously known floor through Remembered-to-Live and Live-to-Remembered without view motion.

Translation aligned bright MAD p95 is `0.25`; rotation is `0.05`. Black-edge XOR, leading, and trailing p95 round to `0.00` in both. Opened contacts and adjacent strips show coverage moving across the textured floor, two thick walls, doorway, landmark, and NeverRemember actor without tile-edge lines or coordinate jumps.

### 13.6 E4 — normal gameplay, resolution, tools, and soak

`SurfaceProofMode=0` normal gameplay samples were recorded with updating authority, camera follow, presentation revisions, D3D12/SM6, normal TSR and temporal jitter. The 1080p and 1440p translation pairs are each 30 seconds / 900 frames. Their aligned bright MAD p95 is `2.18` and `2.22`; black-edge XOR/leading/trailing p95 is `0.00` for both. Torch-to-Lantern-to-Torch candidate/control pairs are 12 seconds / 360 frames at both resolutions; MAD p95 is `0.21` and `0.25`, with zero black-edge XOR/leading/trailing p95. The opened sheets show the legal cone being removed and restored without a stuck-black result. M6P1 authority independently retains same-revision Stalker/HUD visibility and `NeverRemember`.

The final normal-gameplay soak and matched fog-off control are each exactly `600.000` seconds, `18,000` frames, 1920x1080 and 30 fps. The fog-on player range was `X=-650`, `Y=-791.084..425.910`, `Z=90.150`; control range was `X=-650`, `Y=-207.091..833.598`, `Z=90.150`. Both remain on the 3000x2000 cm floor and close naturally. The agent opened every-minute contacts and mid-run twenty-adjacent-frame strips. The normal Torch depletion later in the run changes legal Live to textured Remembered; it does not fail black, introduce a line, or erase the static texture.

### 13.7 Automation, logs, evidence, and retained warnings

The serial `DarkwellEditor Win64 Development` build succeeded after the final material/source checkpoint. Post-proof automation is:

- SurfaceMaterial mapping/wall/category/state: 3/3 Success;
- M6P1 vertical-slice authority: 1/1 Success;
- M3.4 presentation/feather: 3/3 Success;
- M3.5 static environment memory: 2/2 Success;
- M4P1 NeverRemember policy: 1/1 Success;
- D3D12/SM6 three-state composite: 1/1 Success;
- bounded total: 11/11 Success;
- complete DARKWELL: 32/32 Success.

All seven final automation logs have zero test failure and zero fatal/assert/ensure/GPU-crash/device-removal/DARKWELL-or-SightWeave error hit. All 31 E1-E4 dynamic logs have zero candidate-path fatal, assert, GPU crash, device removal, binding failure, or DARKWELL/SightWeave error. They do retain known UE 5.8.1 startup noise in every process: three `UE::UnifiedErrorTest` example errors, thirteen `LogAutomationTest: Error: Condition failed` self-test lines, and Experimental Toolsets Python import errors for missing `AgentSkill`, `ToolsetDefinition`, and `PythonTestRunner`. These happen before map activation, reproduce in fog-on and fog-off, and are not hidden or counted as candidate-path success.

Primary ignored evidence is under `Saved/SightWeaveVisualRescueEvidence`:

- `Dynamic/E1_Final_*`: 1080p/1440p Live, Remembered, authority map, fog-off, frame and ROI data;
- `Dynamic/E2_Final_*`: authority-frozen motion/control videos, contacts, strips and metrics;
- `Dynamic/E3_Final_*`: camera-frozen source sweeps and controls;
- `Dynamic/E4_Final_*`: gameplay resolution/tool/control videos and both 600-second soaks;
- `FinalArchitectureProof/Tests`: final build/test logs and reports;
- `Dynamic/FinalArchitecture*AB*`: controlled binding, UV, RT half-plane, and safe-normal root-cause A/B evidence.

The old `FinalArchitectureRememberedReadabilityPilot1080` sample is excluded: physical input drove the actor off the fixture and into a fall. It was not deleted, and none of its frames or statistics are used as pass evidence. User recordings, `Saved`, Binaries, Intermediate, DDC and generated contacts/videos remain ignored and uncommitted.

### 13.8 Remaining authority and final user PIE

This proof is deliberately limited to `L_VisionIntegration`, its non-Nanite BasicShapes fixture, and project-owned materials. It does not prove Fab/plugin generality, Nanite parity on a project Nanite asset, `L_Prototype`, particle consumers, or final art migration. The fixture Stalker has no skeletal mesh, so its result is authority/HUD/filtering evidence rather than a per-pixel skeletal-material demonstration. Those limits do not permit another screen-space patch and do not weaken the user's final visual authority.

The user must now run the final architecture PIE at both 1920x1080 and 2560x1440, D3D12/SM6 and normal project TSR, with no diagnostic CVar. Acceptance requires: no directional shake, gray line, black/gray offset, black/bright wall inversion, or tile-edge leak; clearly recognizable Remembered floor/wall/static-object texture; black Unknown; stable circle/cone/doorway motion; preserved wall-facing/black-behind logic; `NeverRemember` plus Stalker/HUD synchronization; and Torch/Lantern/Torch recovery. Only the user can advance beyond `PARTIAL`.

# SightWeave DARKWELL Visual Rescue Prototype Report

Date: 2026-08-29

Branch: `codex/sightweave-darkwell-visual-rescue`

Frozen starting SHA: `f364f780904c7ced5d649e7d582c3d91a7d43baf`

Validated source SHA: `ce32d50`

48-hour prototype checkpoint: 2026-08-31

Final stop-loss deadline: 2026-09-05

Status: **PARTIAL — USER_DYNAMIC_PIE_FAILED, BUT DIRECTION VALID**

This is a DARKWELL project-use rescue, not a plugin-generalization, Fab, packaging, or publication result. The user's first real dynamic PIE rejected the candidate at baseline `2439cfb0de843ab52b9c989439272f1e30727d1c`. The prior agent-side automation, screenshots, extracted frames, and D3D12/SM6 runs remain engineering evidence, but they do not establish visual acceptance and cannot be cited as proof that this candidate passed.

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

Initial frame inspection establishes that the editor chrome remains stable while the embedded PIE game View changes, so the failure is inside the game View/render path rather than a whole-desktop capture displacement. The gray lines move in screen space with the camera while remaining correlated with world surfaces and wall/floor elevations. They are therefore being treated as surface-classification or depth-coordinate leakage until the controlled A/B diagnostics identify the exact source; they are not accepted as a color-tuning issue and may not be hidden with darker thresholds, mask expansion, or more blur.

The next rescue slice is restricted to two blockers: (A) whole-view shaking/flicker and (B) gray-line/residual-geometry leakage. It must preserve Ultra 2.5 cm/texel, the unified mutually exclusive three-state result, three-dimensional Remembered, `NeverRemember` enemy filtering, wall-surface classification, the black/gray alignment correction, and Torch/Lantern revision continuity.

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

The rescue did not add temporal history blur or camera-snapped origins. Stability comes from the stable world origin, tenfold finer source field, logical-neighbor-aware distance reconstruction, and correct carry-forward of unchanged atlas tiles. Feather remains presentation-only; gameplay queries retain the hard authority.

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

`Scripts/BuildEditor.ps1` completed successfully after each reliable C++ checkpoint, including the final `ce32d50` carry-forward fix. Target: `DarkwellEditor Win64 Development`.

### Focused NullRHI

- `Darkwell.SightWeave.VisualRescue.PresentationState.TruthTable`: Success.
- `Darkwell.SightWeave.M6P1.Integration.VerticalSliceAuthority`: Success, including Ultra shared scope, `NeverRemember`, shared Stalker/HUD authority, static classifications, tool-cycle no-fail-closed assertion, and restored valid render packet.
- `SightWeave.M3P4.Packaging.InwardFeatherShippingBoundaries`: Success.
- `SightWeave.M3P5.Packaging.StaticEnvironmentMemoryShippingBoundaries`: Success.

One earlier command used an incorrect M3P5 test name and reported that no tests matched. It was not counted as a pass; the exact test above was rerun and succeeded.

### Focused real GPU

- `SightWeave.M3P5.Composite.ThreeStateAndMemoryFailure.D3D12`: Success after the final render-state change.
- RHI: D3D12; feature level/shader platform: SM6 / PCD3D_SM6.
- GPU: NVIDIA GeForce RTX 2070 SUPER.
- No fatal, assert, ensure, GPU crash, device removal, DXGI device error, or D3D12/RHI error was found in the focused final logs.

Unreal startup emits pre-existing experimental Toolset/Python and automation-registration noise in some launches. The exact focused tests still report `Result={Success}`; that startup noise is not presented as a visual or test pass.

## 7. Dynamic formal-View evidence

All evidence is under ignored `Saved/SightWeaveVisualRescueEvidence`; it is not committed. Exact metadata and limitations are in `Saved/SightWeaveVisualRescueEvidence/METADATA.md`.

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
- The 1080p edge/motion run predates the final tool-cycle carry-forward fix. That fix changes packet revision continuity, not state geometry or Remembered shading; final tool cycling was rerun at 1440p on `ce32d50`.
- No SaveGame wiring, damage reveal, Warden, production `L_Prototype` switch, multi-floor support, plugin public-API cleanup, BuildPlugin, Cook, Package, clean-host, full NullRHI/D3D12 history, or performance matrix was run.
- Earlier failed tool-cycle captures are retained as honest diagnostic evidence and are not final acceptance evidence.

## 9. Reliable commits

- `dfda1e7` `docs: record SightWeave visual rescue root cause`
- `c2c4d8f` `docs: normalize visual rescue plan formatting`
- `fa787ef` `render: unify DARKWELL fog state reconstruction`
- `278986f` `render: restore filtered static remembered scene`
- `ad603e5` `fix: preserve DARKWELL fog across tool cycling`
- `ce32d50` `fix: carry forward stable fog tile revisions`

The first documentation commit contained Markdown trailing whitespace because a PowerShell command sequence did not short-circuit on `git diff --check`; `c2c4d8f` corrected it without rewriting history. No source checkpoint was affected.

## 10. Decision gate

The first candidate failed the user's dynamic PIE. Continue only the two-blocker DARKWELL project-usability rescue within the 2026-09-05 stop-loss deadline. A new candidate may be offered only after controlled A/B attribution, normal TAA/TSR restoration, targeted D3D12/SM6 dynamic evidence at 1080p and 1440p, and agent consecutive-frame inspection. Even then, the highest permitted state is `PARTIAL — READY_FOR_USER_DYNAMIC_PIE_RETEST`; only the user's second real dynamic PIE can decide usability. The final product decision remains exactly one of:

- `ACCEPTED — DARKWELL USABLE`
- `REJECTED — ABANDON SIGHTWEAVE`

`PARTIAL` cannot extend past the stop-loss decision.

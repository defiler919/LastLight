# DARKWELL Project Fog Visual Rebuild Report

Date: 2026-08-30

Branch: `codex/darkwell-project-fog-visual-rebuild`

Starting SHA: `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb`

Status: **PARTIAL — P4 DYNAMIC SUBJECT AUTHORITY PROVEN / P5 AUTHORIZED**

24-hour P1 gate: 2026-08-31

48-hour vertical-slice gate: 2026-09-01

Final stop-loss: 2026-09-05

## 1. Controlling user failure

The user's final dynamic PIE rejected the SightWeave SurfaceMaterial candidate at `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb`.

Confirmed failures:

- the Live edge follows the world/tile grid and shows periodic stepping, jaggies and flicker;
- `RGBA16F` storage did not provide continuous coverage because the input was still derived from discrete tile states;
- outward per-face wall sampling gave opposite states to faces of one wall and produced north/south asymmetry;
- the required wall rule is local cross-section visibility from either legal side without leaking Live into space behind;
- Remembered static texture improved, but the full visual architecture still failed user acceptance.

The prior automatic runs and image metrics remain engineering history. They did not predict or override the real PIE verdict.

## 2. Frozen retained backend

The new project presentation continues to consume SightWeave's reliable CPU authority, legal light/source decisions, occlusion inputs, subject policy, `NeverRemember`, static-memory ownership, snapshots, persistence/restore, world lifecycle and gameplay tests. These are not reimplemented.

The old post-/pre-TSR composite, CustomDepth/Stencil composite, discrete SurfaceMaterial coverage, per-face wall sampling and tile-mirror final edge are rejected presentation paths. They remain in history but will not register or run in the formal project candidate.

## 3. P0 result

- verified prior branch clean at the expected SHA locally and at upstream after fetch/pull;
- verified no unpushed Git or LFS work;
- confirmed the new branch did not exist remotely;
- created and pushed `codex/darkwell-project-fog-visual-rebuild` from the exact baseline;
- superseded the old readiness claim in the rescue report/handoff;
- froze the project-owned continuous coverage contract and gated execution plan;
- made no source, shader, asset, map, config or project-file change in P0.

## 4. Architecture and activation result

The smallest project seam keeps SightWeave as the CPU authority and replaces only presentation:

1. `UDarkwellSightWeaveWorldSubsystem::RequestSightWeaveAuthority` suppresses the rejected SightWeave renderer before the first floor publication and continues registering floors, legal sources, lights, occluders, static memory and subjects.
2. `UDarkwellSightWeaveWorldSubsystem::BuildFogVisualSourceSnapshot` converts the authoritative body/cone/light state into immutable continuous world geometry. It does not consume the tile-state texture.
3. `UDarkwellFogVisualSubsystem::UpdateSource` updates parameters only when the source snapshot changes; `DrawCoverage` renders analytic circle/cone coverage to the project-owned linear `R16F` target.
4. `ADarkwellVisionIntegrationFixture::EnableDarkwellProjectFogP1` binds that target and its world mapping to the DARKWELL floor material. The material computes `Known=1`, `Live=coverage`, `Remembered=1-coverage` and uses filtered real static BaseColor for Remembered.
5. `USightWeaveRenderWorldSubsystem::SetPresentationSuppressed` prevents the old scene-view composite, tile render packet and default presentation selection from registering/running while the project candidate is active. Rollback restores the prior subsystem state.

P1 deliberately hid walls, the landmark and the dynamic threat in the no-wall fixture only. P2 restored proof geometry and continuous free-space occlusion. P3 now classifies each static proof surface from stable object-local exterior samples while leaving ground/free-space coverage unchanged. Dynamic-subject restoration remains isolated to P4.

## 5. P1 continuous-coverage result

P1 passed its first reliable implementation attempt on 2026-08-30.

- coverage target: `1400 x 1000`, linear `R16F`, 2.5 cm/texel, bilinear/clamped, normal derivative/mip selection;
- formal rendering: 1920x1080, D3D12/SM6, normal TSR (`r.AntiAliasingMethod=4`);
- fixed evidence: 20 seconds;
- motion evidence: 30 seconds each horizontal, vertical, diagonal and slow rotation;
- direct adjacent-frame inspection: no periodic whole-texel plateau/jump, large stair-step recurrence, black Unknown, or blur-dependent edge was observed;
- old presentation registration: suppressed in every formal log;
- severe scan: 0 candidate errors.

GPU raw-coverage centroid displacement relative to the base sample, in coverage texels:

| Translation | Measured X | Measured Y |
| --- | ---: | ---: |
| X 0.25 | 0.248937 | -0.000017 |
| X 0.50 | 0.500743 | -0.000003 |
| X 1.00 | 1.000004 | -0.000001 |
| Y 0.25 | -0.000050 | 0.246789 |
| Y 0.50 | 0.000963 | 0.499993 |
| Y 1.00 | 0.000000 | 0.999998 |
| diagonal 0.25 | 0.250004 | 0.250179 |
| diagonal 0.50 | 0.498644 | 0.499995 |
| diagonal 1.00 | 1.000006 | 0.999997 |

Every raw target contained values strictly between 0 and 1 (1998–2050 intermediate texels). This proves that the edge is evaluated as fractional continuous coverage rather than a float-formatted copy of discrete state.

Targeted validation:

- `Darkwell.FogVisual.P1`: 3/3 Success;
- `Darkwell.SightWeave.M6P1.Integration.VerticalSliceAuthority`: 1/1 Success;
- `Scripts/BuildEditor.ps1 -Configuration Development`: Success, `DarkwellEditor Win64 Development` up to date;
- matrix: `P1_MATRIX_SUCCESS cases=15 severe=0`.

## 6. Evidence ledger

The ignored evidence root is `Saved/DarkwellProjectFogVisualRebuild/P1`.

- machine-readable index: `p1_evidence_index.json`;
- empty severe scan: `p1_severe_scan.txt`;
- targeted automation: `MatrixAutomation`;
- raw and formal D3D12 captures: `Dynamic`;
- each formal capture contains its source video, log, contact sheet and 20-frame adjacent strip.

The matrix preserves earlier invalid/failed samples under their original names, but the index references only the 15 validated cases. Nothing below `Saved` is committed.

## 7. P2 continuous-occlusion result

P2 passed on 2026-08-30 after one preserved shader-input failure and a corrected reliable run.

Implementation:

- the fixture publishes 11 stable continuous 2D segments covering the doorway pair, a rotated wall, four-sided cube, exposed wall ends, an L-shaped concave obstacle and a T-junction;
- those same world-space segments feed SightWeave gameplay occlusion and the DARKWELL project presentation;
- the material evaluates source-to-pixel ray/segment intersections directly on the GPU for body and cone independently, then unions their visible fractional coverage;
- `fwidth`-based analytic edge coverage stabilizes the continuous shadow boundary without blur, tile upsampling, forced mip zero or readback in the formal path;
- P2 obstacle meshes are displayed as forced-Remembered static surfaces. Their surface Live state is intentionally deferred to P3, while the ground uses `FreeSpaceLiveCoverage` now.

Raw D3D12/SM6 probe:

```text
extent=1400x1000 min=0.000000 max=1.000000 intermediate=5662
front=1.000000 behind=0.000000 doorway=1.000000
```

This directly proves that a point before the north wall is Live, a collinear point behind it is Remembered, and a point through the doorway remains Live. The raw target retains 5,662 fractional pixels.

Validation:

- `Darkwell.FogVisual.P2.ContinuousSegmentOcclusion`: 1/1 Success;
- updated vertical slice: 1/1 Success with 11 cached segments;
- 1080p normal-TSR captures: raw, 20-second fixed, 30-second along-wall, 30-second doorway and 30-second slow rotation;
- direct contact/adjacent-strip inspection: wall shadows remain anchored, the doorway stays open, wall-end wedges move continuously and behind-wall ground remains gray;
- severe scan: 0.

The first dynamic attempt is preserved in `Saved/DarkwellProjectFogVisualRebuild/P2/FailedAttempts`. It correctly failed because the Custom node received RGB (48 scalars) while declaring sixteen `float4` segments (64 scalars). The formal fix explicitly binds every segment parameter through its RGBA output; no visual threshold or filtering rule changed.

The validated P2 index is `Saved/DarkwellProjectFogVisualRebuild/P2/p2_evidence_index.json`.

## 8. P3 wall/object surface-coverage result

P3 passed its bounded matrix on 2026-08-30.

Implementation:

- ground continues to use only `FreeSpaceLiveCoverage`; surface classification never writes Live back into free space;
- every wall material reconstructs a stable object-local centerline and evaluates the maximum coverage of its two exterior-side samples;
- the cube uses the maximum of four stable exterior samples;
- tangent and normal data are derived from the owning native component transform, not the current camera or rendered face;
- the doorway pair, rotated wall, exposed ends, concave obstacle, T-junction and cube keep independent material instances and bindings;
- the formal material continues using derivative-selected, bilinear/clamped `R16F` sampling and normal TSR. No blur, forced mip zero, threshold expansion or screen-space face heuristic was introduced.

Validation:

- `Darkwell.FogVisual.P3.ObjectLocalSurfaceCoverage`: 1/1 Success;
- updated vertical slice: 1/1 Success with P3 presentation active;
- 7/7 D3D12/SM6/normal-TSR cases: raw, west, east, south, north, 30-second along-wall and 30-second doorway;
- raw coverage retained `min=0`, `max=1`, 5,662 fractional pixels and the P2 occlusion probe `front=1`, `behind=0`, `doorway=1`;
- all eight expected wall/box bindings were recorded with stable world origin, normal/tangent or four-side extent;
- direct inspection of four opposing contacts and every 20-frame strip found no face-state reversal, direction-dependent wall classification, contact-edge flicker, doorway closure or behind-wall ground leak;
- severe scan: 0.

The validated P3 index is `Saved/DarkwellProjectFogVisualRebuild/P3/p3_evidence_index.json`. All videos, contacts, frame strips, logs and automation reports remain ignored under `Saved`.

## 9. P4 dynamic-subject and legal-source result

P4 passed on 2026-08-30.

The failure was a phase-isolation override, not a SightWeave subject-policy failure: P1 still forced the Stalker hidden every frame and disabled its controller after the project presentation had already applied the correct authority result. P4 removes those overrides. `ApplySightWeaveVisibility` now controls Actor presentation directly from the authoritative `NeverRemember` snapshot, and the HUD continues to require the same stable ID, `bHardLive` state and exact authority revision.

The normal candidate leaves the Stalker controller enabled. Two Development-only diagnostics exist solely for bounded evidence: one holds the subject at a stable cone target and pauses only its controller; the other cycles the real loadout API through Torch/Lantern/Torch at six-second intervals. Neither diagnostic is compiled into Shipping/Test behavior or enabled by default.

Validation:

- `DarkwellEditor Win64 Development`: Success;
- updated `Darkwell.SightWeave.M6P1.Integration.VerticalSliceAuthority`: 1/1 Success, including visible, hidden, body-bypass, wall-occluded and revision-equality assertions;
- 36-second D3D12/SM6/normal-TSR Torch/Lantern/Torch evidence: Success;
- 30-second D3D12/SM6/normal-TSR normal-controller gameplay evidence: Success;
- tool sequence was observed three times in order; every Lantern transition produced `Hidden actorHidden=1`, every restored Torch transition produced `Live actorHidden=0`;
- every logged subject transition had `hudSharedRevision == authorityRevision` and `NeverRemember=1`;
- direct contact and adjacent-strip inspection confirmed the threat label disappears with the hidden subject and returns with the Live subject, without changing the proven gray/live edge or P3 wall behavior;
- severe scan: 0.

The validated P4 index is `Saved/DarkwellProjectFogVisualRebuild/P4/p4_evidence_index.json`.

## 10. Current limitations and next gate

P1 continuous coverage, P2 free-space occlusion and P3 object-local wall/object surface coverage are proven only as agent checkpoints. Restored Live-only dynamic subjects, `NeverRemember` runtime behavior, Stalker/HUD synchronization, Torch/Lantern/Torch transitions, 1440p and the final soak remain pending. The current state is not ready for user PIE.

P5 is now authorized. It must run only the bounded final matrix at 1080p and 1440p with normal D3D12/SM6/TSR, recheck the proven trajectories and walls, measure fog-off versus gray-layer texture correlation/contrast, include Torch/Lantern/Torch and Stalker/HUD, scan severe logs and complete the ten-minute soak. It may not change visual thresholds to satisfy evidence.

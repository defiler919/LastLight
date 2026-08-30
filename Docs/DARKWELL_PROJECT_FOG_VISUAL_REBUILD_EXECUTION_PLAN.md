# DARKWELL Project Fog Visual Rebuild Execution Plan

Date: 2026-08-30

Branch: `codex/darkwell-project-fog-visual-rebuild`

Starting SHA: `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb`

Deadline: 2026-09-05

## Operating rule

Each phase ends in a buildable, boundedly verified commit pushed immediately. A later phase may start only when the preceding visual gate passes. Evidence lives below ignored `Saved/DarkwellProjectFogVisualRebuild`; source videos and generated evidence are never committed.

## P0 — isolation and frozen contract

1. Verify the prior branch is clean and exactly at `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb` locally, upstream and remotely.
2. Create and push `codex/darkwell-project-fog-visual-rebuild` from that SHA.
3. Record the user's final rejection in the SightWeave rescue report/handoff.
4. Freeze the new contract, this plan, the new report and the new handoff.
5. Run document/Git checks, commit and push before source work.

P0 exit: four new documents exist, prior readiness is superseded, no source/asset/config change exists, and local/upstream/remote agree.

## Architecture audit before implementation

1. Trace SightWeave CPU authority inputs and identify the smallest read-only project interface for legal body/cone sources, source revisions, occluder geometry and subject decisions.
2. Trace all old visual registrations. Define one project-owned selector that prevents old composite and SurfaceMaterial paths from registering/running when the rebuild candidate is selected.
3. Draw the candidate frame path with concrete C++ functions, render-thread resource lifetime, texture extent, ViewRect and TSR location.
4. Inventory `L_VisionIntegration` geometry and required safe asset changes.

No authority semantics, persistence format or plugin-general public API is redesigned unless a demonstrably missing read-only seam blocks the project adapter.

## P1 — continuous no-wall proof (24-hour gate)

Implementation target: `Source/Darkwell/Public/VisionPresentation` and `Source/Darkwell/Private/VisionPresentation`, with project content under `/Game/Darkwell/Vision` only when Unreal-created assets are necessary.

Candidate responsibilities:

- project presentation subsystem/component owns lifecycle and policy;
- immutable render snapshot carries continuous source geometry and revision;
- render extension or project render hook builds a linear `R16F` coverage target;
- material-facing binding exposes world transform, texture, sampler and policy;
- Development/Editor diagnostics select raw coverage and controlled motion tests without changing Shipping defaults.

Initial proof scene contains only a high-contrast floor, body circle and one aim cone. `KnownCoverage=1`; no wall/occlusion/dynamic subject is allowed.

Required run order:

1. serial `DarkwellEditor Win64 Development` build;
2. targeted continuous-coverage automation;
3. 1080p D3D12/SM6 normal-TSR 20-second fixed run;
4. 30-second horizontal, vertical, diagonal and slow-rotation runs;
5. raw coverage captures at 0.25/0.5/1.0 texel moves in every required direction;
6. severe-log scan;
7. direct inspection of key frames, adjacent strips and contact sheets.

Pass requires measured fractional edge pixels, continuous centroid/edge displacement, no plateaus/jumps, no periodic world-grid teeth, no blur dependency and stable normal TSR. If it fails, record the exact failed invariant. After two failed reliable attempts, set `BLOCKED — CONTINUOUS LIVE COVERAGE PROOF FAILED`, push and stop. Do not enter P2.

## P2 — continuous occlusion

Cache stable 2D occluder segments/polygons. Build a continuous visibility polygon from nearest intersections at endpoint angles and `+/- epsilon`, clipped by the legal circle/cone. Union multiple legal sources on GPU.

Expand `L_VisionIntegration` through Unreal-safe tooling to include axis-aligned/rotated walls, cube, doorway, wall end, concave combination and multi-wall intersection. Validate raw free-space coverage before any wall material classification. Behind-wall free space must remain Remembered.

## P3 — surface coverage

Introduce separate `FreeSpaceLiveCoverage` and object-local `WallSurfaceLiveCoverage`. Walls sample both stable wall-local sides at half-thickness plus epsilon and apply the maximum only to that local wall material. Boxes evaluate four exterior directions. Tangent position limits coverage to the observable segment.

Validate every wall/box face from four directions, doorway traversal, wall ends, concave and multi-wall intersections. Reject any ViewDirection dependency, per-face disagreement, north/south asymmetry or Live leak behind geometry.

## P4 — dynamic subjects and legal source transitions

Reconnect dynamic actor rendering to existing SightWeave subject authority. Dynamic actors are Live-only; `NeverRemember` remains enforced. Stalker and threat HUD use one snapshot/revision. Validate Torch -> Lantern -> Torch and legal source removal/restoration without stuck gray/Live state.

## P5 — bounded formal evidence and handoff

Run only after P1–P4 pass:

- 1080p and 1440p, D3D12/SM6, normal TSR;
- 20 seconds fixed;
- 30 seconds each horizontal, vertical, diagonal, slow rotation and along-wall;
- wall/object four-direction and doorway sequence;
- body/cone raw-coverage sequence;
- Torch/Lantern/Torch and Stalker/HUD;
- ten-minute project soak;
- targeted tests and severe-log scan.

Open representative full frames, raw-coverage frames, adjacent strips and contacts. Report coverage histograms/edge motion plus Remembered correlation and contrast retention, but do not infer acceptance from averages alone.

The maximum handoff is `PARTIAL — READY_FOR_USER_GRAY_LIVE_PIE`. The user then verifies gray/live behavior in real dynamic PIE. No Unknown/exploration implementation begins automatically.

## Forbidden substitutions

Do not use blur, dilation, a wider feather, higher threshold, lower resolution, disabled TSR, point sampling, forced mip zero, a discrete tile upsample, temporal smearing, SceneColor memory or wall hiding to pass a gate. Do not run the excluded build/test matrices. Do not modify `Darkwell.uproject` or `L_Prototype`.

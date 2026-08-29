# SightWeave DARKWELL Visual Rescue Failure Handoff

Date: 2026-08-29

Branch: `codex/sightweave-darkwell-visual-rescue`

Frozen starting SHA: `f364f780904c7ced5d649e7d582c3d91a7d43baf`

Rejected candidate baseline: `2439cfb0de843ab52b9c989439272f1e30727d1c`

Status: **PARTIAL — USER_DYNAMIC_PIE_FAILED, BUT DIRECTION VALID**

The user's first real dynamic PIE rejected the candidate. The agent-side D3D12/SM6 runs, automation results, screenshots, and extracted frame sheets remain engineering evidence only; none of them may be used to claim visual acceptance. Do not run the deferred full regression while the two current visual blockers remain.

## Rejection evidence

- Recording: `Darkwell - 虚幻编辑器 2026-08-29 18-35-37.mp4`
- Ignored analysis copy: `Saved/SightWeaveVisualRescueEvidence/UserDynamicPIEFailure`
- SHA-256: `33298E98FB8B7AE52DA8C3F2F38794311338A5E44307FBEEEF2AA00DCC02CCD5`
- Result: whole-game image shaking/flicker and multiple thin gray lines immediately after gameplay begins make the image unusable.
- Direction retained: Ultra 2.5 cm/texel materially reduced the staircase; Remembered is three-dimensional; enemy filtering is correct.

Initial consecutive-frame inspection shows stable editor chrome around an unstable embedded PIE game View. The gray lines shift on screen with camera motion but remain aligned with world-surface/wall-floor elevations. Exact attribution remains subject to the required A/B depth, stencil, jitter, ViewRect, update, and wall-sampling diagnostics.

## Current work restriction

The next work is limited to:

1. remove whole-View shaking and flicker while restoring normal TAA/TSR for formal verification;
2. remove gray-line and residual-geometry leakage by correcting its depth/coordinate/classification cause, not by hiding it with thresholds, mask expansion, color cover, or blur.

The following gains are frozen and must not regress: Ultra 2.5 cm/texel, one mutually exclusive Unknown/Remembered/Live state, three-dimensional Remembered, `NeverRemember` enemy filtering, wall-surface classification, black/gray alignment, and Torch/Lantern revision continuity.

## Retest setup (only after a new candidate is documented)

1. Restore the pushed branch:

   ```powershell
   git switch codex/sightweave-darkwell-visual-rescue
   git pull --ff-only
   ```

2. Open `D:\UE_pro\Darkwell\Darkwell.uproject` in Unreal Engine 5.8.1.
3. Open `/Game/Maps/L_VisionIntegration`.
4. Use the normal D3D12/SM6 editor configuration with TSR/TAA enabled. Do not disable AA, reduce resolution, or hold the camera still for acceptance.
5. Start PIE through the normal player View and choose `NEW GAME` so the player begins at the integration fixture spawn.

## Required second dynamic acceptance sequence

Perform the following in one continuous PIE session:

1. At spawn, inspect the circular body radius and both diagonal Torch-cone edges at rest.
2. Move with `W/A/S/D` while slowly aiming left and right with the mouse. Look specifically for grid jumps, crawling, flashing, and a blurred underlying staircase.
3. Strafe along each wall and approach it head-on. Confirm the player-facing wall side/top remains visible and Unknown black begins behind the wall.
4. Move through the doorway and around both wall ends. Confirm openings and corners do not gain an extra black strip.
5. Leave a previously Live ground/wall area. Confirm it becomes a recognizable, dark, low-contrast static Remembered scene rather than a flat gray fill.
6. Re-enter that area. Confirm `Remembered -> Live` has no gap, overlap, or black seam.
7. Tap and release `E` once to cycle from Torch to Lantern. Confirm the Torch cone leaves Live, the body radius remains Live, and the former cone is only filtered static Remembered.
8. Tap and release `E` again to restore Torch. Confirm the Live cone returns immediately without a full-screen black frame or stuck fog.
9. Aim toward and then away from the Stalker. Confirm the Stalker and red threat HUD appear and disappear together, with no gray Stalker memory or current enemy-position leak.
10. Continue moving, turning, following walls, and crossing the doorway for at least three to five minutes. This long-duration motion stability judgment cannot be replaced by screenshots or agent frame sheets.

## Acceptance questions for the second user test

Please answer only after the continuous test:

- Is the moving edge visually usable at both the circle and cone?
- Is Remembered recognizable as static scene structure without leaking dynamic information?
- Are wall faces readable with black starting behind them?
- Are Unknown, Remembered, and Live aligned without extra black bands?
- Do the Stalker and threat HUD obey the same visibility transition?
- Does Torch -> Lantern -> Torch recover without fail-black?

If the result is usable, reply explicitly that the DARKWELL dynamic PIE visual is usable. That authorizes the deferred complete regression and final acceptance close; it does not retroactively make the rejected candidate complete.

If any item fails, report the exact motion, location, transition, resolution, and whether the failure is constant or intermittent. A second rejection keeps work limited to DARKWELL project usability until the 2026-09-05 stop-loss decision.

## Evidence and implementation record

- Frozen contract: `Docs/SIGHTWEAVE_DARKWELL_VISUAL_REQUIREMENTS.md`
- Root cause and plan: `Docs/SIGHTWEAVE_DARKWELL_VISUAL_RESCUE_EXECUTION_PLAN.md`
- Prototype report: `Docs/SIGHTWEAVE_DARKWELL_VISUAL_RESCUE_REPORT.md`
- Ignored local dynamic evidence: `Saved/SightWeaveVisualRescueEvidence`
- Evidence metadata: `Saved/SightWeaveVisualRescueEvidence/METADATA.md`

No `.uasset`, `.umap`, `L_VisionIntegration`, `L_Prototype`, configuration, plugin descriptor, or `Darkwell.uproject` modification is part of this handoff.

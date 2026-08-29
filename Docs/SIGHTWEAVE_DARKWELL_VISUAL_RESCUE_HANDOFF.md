# SightWeave DARKWELL Visual Rescue User PIE Handoff

Date: 2026-08-29

Branch: `codex/sightweave-darkwell-visual-rescue`

Frozen starting SHA: `f364f780904c7ced5d649e7d582c3d91a7d43baf`

Validated source SHA: `ce32d50`

Status: **PARTIAL — READY_FOR_USER_DYNAMIC_PIE**

This handoff does not declare visual completion. The agent-side D3D12/SM6 formal-player-View prototype is ready for the user's real dynamic PIE judgment. Do not run the deferred full regression until the user has seen and judged the moving result.

## User setup

1. Restore the pushed branch:

   ```powershell
   git switch codex/sightweave-darkwell-visual-rescue
   git pull --ff-only
   ```

2. Open `D:\UE_pro\Darkwell\Darkwell.uproject` in Unreal Engine 5.8.1.
3. Open `/Game/Maps/L_VisionIntegration`.
4. Use the normal D3D12/SM6 editor configuration with TSR/TAA enabled. Do not disable AA, reduce resolution, or hold the camera still for acceptance.
5. Start PIE through the normal player View and choose `NEW GAME` so the player begins at the integration fixture spawn.

## Required dynamic acceptance sequence

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

## Acceptance questions

Please answer only after the continuous test:

- Is the moving edge visually usable at both the circle and cone?
- Is Remembered recognizable as static scene structure without leaking dynamic information?
- Are wall faces readable with black starting behind them?
- Are Unknown, Remembered, and Live aligned without extra black bands?
- Do the Stalker and threat HUD obey the same visibility transition?
- Does Torch -> Lantern -> Torch recover without fail-black?

If the result is usable, reply explicitly that the DARKWELL dynamic PIE visual is usable. That authorizes the deferred complete regression and final acceptance close; it does not retroactively make the current report `COMPLETED`.

If any item fails, report the exact motion, location, transition, resolution, and whether the failure is constant or intermittent. A rejection keeps work limited to DARKWELL project usability until the 2026-09-05 stop-loss decision.

## Evidence and implementation record

- Frozen contract: `Docs/SIGHTWEAVE_DARKWELL_VISUAL_REQUIREMENTS.md`
- Root cause and plan: `Docs/SIGHTWEAVE_DARKWELL_VISUAL_RESCUE_EXECUTION_PLAN.md`
- Prototype report: `Docs/SIGHTWEAVE_DARKWELL_VISUAL_RESCUE_REPORT.md`
- Ignored local dynamic evidence: `Saved/SightWeaveVisualRescueEvidence`
- Evidence metadata: `Saved/SightWeaveVisualRescueEvidence/METADATA.md`

No `.uasset`, `.umap`, `L_VisionIntegration`, `L_Prototype`, configuration, plugin descriptor, or `Darkwell.uproject` modification is part of this handoff.

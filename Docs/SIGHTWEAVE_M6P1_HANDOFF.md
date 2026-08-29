# SightWeave M6P1 handoff

## Current state

**PARTIAL — READY_FOR_USER_PIE.** The implementation and all agent-owned gates are complete. The sole remaining gate is the user's dedicated-map PIE acceptance.

Branch: `codex/m6p1-sightweave-darkwell-adapter-vertical-slice`

Audit baseline: `25eb813a87cacd7dadb34f1b234c582df1467964`

Validated implementation/map head before closure documents: `868c89299f04012386dd1722a0c68c484c6bdb6d`

The exact remote-equal closure-document head is reported in the final task response because a commit cannot contain its own hash.

## What to inspect

- map: `/Game/Maps/L_VisionIntegration`;
- adapter: `UDarkwellSightWeaveWorldSubsystem`;
- authority type: `EDarkwellVisibilityAuthorityMode`;
- native map fixture: `ADarkwellVisionIntegrationFixture`;
- native map mode: `ADarkwellVisionIntegrationGameMode`;
- real subject: `ADarkwellStalkerCharacter`, stable ID `Enemy.Stalker.VisionIntegration`;
- generated evidence: `Saved/M6P1_GameViewEvidence`, `Saved/AutomationReports/M6P1_*`, and `Saved/Logs/M6P1_*`.

The vertical slice intentionally applies only to the dedicated map. `L_Prototype` remains on the existing Legacy path.

## Verified controls

These controls come from `ADarkwellCharacter::AddDefaultInputMapping` and `ADarkwellPlayerController::UpdateWeaponWheelInput`, not from assumption:

- `W`, `A`, `S`, `D`: movement;
- mouse cursor: aim and character/cone facing;
- press and release `E`: open/commit the right-hand wheel, cycling Torch ↔ Lantern;
- `Left Shift`: sprint, optional for this check;
- `Escape`: menu;
- bottom-HUD text also reflects the active input mapping.

For M6P1, Torch equipped/on is the only long-range legal SightWeave light. Cycling to Lantern is the functional torch-off test; the lantern is deliberately not a SightWeave legal-light source in this milestone.

## User PIE acceptance: 3–5 minutes

1. In Unreal Editor, open `/Game/Maps/L_VisionIntegration` and start PIE. If the normal DARKWELL startup menu appears, click `NEW GAME`; the same map reloads with gameplay input enabled.
2. Move with `W/A/S/D` and sweep the mouse cursor. Confirm the live cone follows character aim and the small close-range circle remains centered on the player.
3. Press and release `E` once to equip Lantern. Confirm the distant live area disappears and only the small body circle remains live; the lantern's rendered light must not make the SightWeave cone legal. Press/release `E` again to restore Torch.
4. Aim through the central doorway at the Stalker. Confirm the real Stalker and `THREAT STALKER HUNTING` HUD appear together. Aim away: both must disappear while previously explored static floor remains gray and contains no Stalker silhouette.
5. Strafe north/south of the doorway with `D/A` and aim across a wall segment. Confirm the wall truncates live coverage, the wall-behind region stays black unless previously explored, and both Stalker and threat HUD hide. Return to the doorway and confirm both reacquire together.
6. Inspect the boundary while turning and moving. Confirm there is one stable black/gray/live transition, not two fog layers, duplicate edges, or one-frame HUD/enemy disagreement.
7. Stop PIE and confirm the Editor does not crash or assert. Start PIE a second time, click `NEW GAME` if needed, and repeat a torch toggle plus doorway reacquisition; confirm there is no duplicate registration or stale memory from the previous World.

Report pass/fail for the checklist. A failure should include the exact step, screenshot if practical, and whether it reproduced on the second PIE session.

## Frozen boundaries

Do not extend this handoff into SaveGame/persistence-slot wiring, damage reveal, lantern authority, Warden, multi-floor, dynamic doors, broad `L_Prototype` migration, or new SightWeave public/plugin work. Do not modify the frozen plugin, `Darkwell.uproject`, or `L_Prototype` while accepting M6P1.

## Resume command

After the user reports PIE results, resume with:

`git fetch origin; git switch codex/m6p1-sightweave-darkwell-adapter-vertical-slice; git pull --ff-only; git status --short --branch; git rev-parse HEAD; git rev-parse '@{upstream}'`

If every PIE step passes, update M6P1 status to COMPLETED in a documentation-only checkpoint and push it. If a step fails, diagnose only that product integration defect without expanding milestone scope.

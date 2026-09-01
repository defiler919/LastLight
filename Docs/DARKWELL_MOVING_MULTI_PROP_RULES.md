# DARKWELL Moving and Multi-Prop Rule Validation

## Status

`PARTIAL — READY_FOR_USER_IN_WORLD_MOVING_PROP_RETEST`

This document records the qualification and implementation history for Step 1,
Moving and Multi-Prop Rule Validation. It does not change the frozen Mode 2
presentation baseline or integrate the experiment into a production map.

## Frozen inputs

- Branch: `codex/darkwell-prop-memory-gameplay-lab`
- Documentation starting point: `554630a69716c5cdd3f8e62dd287f716453fbe95`
- User-accepted runtime baseline: `39908cc67eb91d72a7d5ac35fe813367b41a7919`
- Stable tag: `darkwell-mode2-prop-memory-baseline-v1`
- Engine qualification: Unreal Engine `5.8.2`, changelist `56702186`

The user confirmed a real Mode 2 PIE smoke test after upgrading to UE 5.8.2.
The visual baseline remained accepted. This qualification did not repeat the
previous 298-image GPU evidence set.

## Authoritative moving-prop rule

The only moving-furniture memory rule is `SpatialEvidenceOnly`.

StableID is internal identity and is not player knowledge. Observing the same
StableID at a new position cannot invalidate an old spatial memory record. An old
record may only be revised by new legal SightWeave evidence covering that record's
world-space occupancy. `IdentityResolved` is not a candidate policy and must not
be implemented, exposed as a command, or retained as a test contract.

The target data model distinguishes actor StableID from one or more observation
epochs. Each spatial record owns its snapshot transform and residual spatial
memory. A record is erased independently by legal evidence at its own location
and may be released only after complete erasure or by an explicit resident-memory
policy that is not presented as empty-space verification.

## UE 5.8.2 qualification checkpoint

No `UnrealBuildTool`, `dotnet`, `UnrealEditor`, `UnrealEditor-Cmd`, or
`ShaderCompileWorker` process was running before qualification. No parallel asset
project was included in the evidence.

### Build

`Scripts/BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8`

- Result: Success
- Target: `DarkwellEditor Win64 Development`
- Actions: 26/26
- Build time: 91.14 seconds
- Log: `Saved/PropGameplayLab/MovingMulti/Qualification/BuildEditor_UE582.log`
- Preserved warnings: Visual Studio 14.51 is newer than UE's preferred 14.50;
  UE engine headers emit existing deprecation warnings.

### Frozen Mode 2 automation

Suite:

`Darkwell.PropLab.SpatialMemory+Darkwell.PropLab.ManualSwitch+Darkwell.FogVisual.RememberedProp+Darkwell.PropLab.Scope+Darkwell.FogVisual.GrayUnlit`

- Result: 16/16 Success
- Clean: 13
- Success with existing warnings: 3
- Failed or not run: 0
- Severe log matches: 0
- Report: `Saved/AutomationReports/MovingMultiQualificationUE582`
- Log: `Saved/PropGameplayLab/MovingMulti/Qualification/Automation16_UE582.log`

This checkpoint quotes the user's post-upgrade PIE smoke result and records a new
UE 5.8.2 build and NullRHI automation run. It makes no new GPU visual or
performance claim.

## Implemented SpatialEvidenceOnly history model

The lab now supports multiple independently verifiable spatial observations for
one real StableID. `FDarkwellSpatialObservationHistory` owns records containing:

- the actor StableID on the history container;
- a monotonically increasing observation epoch;
- the world-space snapshot transform and bounds;
- independent D/V/R state in `FDarkwellSpatialPropMemory`.

The real actor keeps its one internal StableID. A historical proxy has no
StableID, gameplay collision, navigation authority, shadow, GI, custom-depth, or
ray-tracing authority. It is only the rendering of one observation record. No
real actor identity is copied and an epoch is not presented as another item.

A newly observed legal location creates an epoch. Legally observed translation
or rotation rebases the current epoch, so a visible path does not create a trail.
Losing legal observation freezes the last record and later hidden transforms do
not modify it. Seeing the real actor at B or C creates a new current record but
does not invalidate A or B. Legal coverage over an old record advances only that
record's empty verification and Mode 2 fade. The record is released only after
its stale occupancy and opacity reach zero.

Each StableID has a resident limit of 64 records. At capacity, a new location is
rejected fail closed with an explicit warning; no existing record is silently
cleared and the rejection is not reported as verified empty. This is a bounded
lab memory policy, not a final whole-project streaming policy.

## Isolated moving and multi-prop room

The existing map asset is unchanged. The room is spawned at runtime only when
the Lab world has the `MoveRules` URL option or the editor is launched with
`-PropLabMovingControls`. The latter is the user-facing path. It uses Engine
basic geometry and contains no saved map changes:

- three-part cabinet;
- three-part low bed;
- tabletop and four separate legs;
- three-part lamp with a real gap around the tabletop;
- one thin panel;
- two geometrically and materially identical cabinets with different StableIDs.

The table and lamp aggregate bounds overlap, while their primitive bounds do not
intersect or share coplanar exterior faces. The room has eight explicit SightWeave
occluder segments, no enemy, no timer, and no imported content. The existing
manual stale room and default Lab path remain separate.

The Mode 2 historical proxies use the frozen accumulated-memory material, 4x4
conservative presentation sampling, bilinear filtering, `.20/.18` timing, and
the existing opaque, unlit, two-sided `#343A40` cap material. They do not change
the frozen static-prop path.

### In-world F-key mechanisms

The manual room now has seven green, labeled interactables that use the existing
`UDarkwellInteractionComponent` and character F binding. They do not open the
console or invoke the legacy `scenario` / `advance` command path:

- **VISIBLE TRANSLATE**: one-second hold, then a four-second continuous A to B
  translation; the result remains at B.
- **VISIBLE ROTATE**: one-second hold, then a four-second continuous 0 to 180
  degree rotation; the result remains at 180 degrees.
- **OFFSCREEN A TO B**: arms the rear orange pressure plate. The plate starts a
  four-second move only while the cabinet has zero legal coverage.
- **COVERAGE EDGE**: one-second hold, then an eight-second continuous passage
  across the legal view boundary.
- **A TO B TO C**: arms the pressure plate twice. B and C are each reached by a
  four-second hidden move; seeing B or C does not clear older records.
- **MULTI PROP**: a blue high cabinet and orange low cabinet move continuously
  for four seconds, a red small box becomes absent, and a green long table stays
  fixed. Shapes, dimensions, colors, StableIDs, and spatial records are distinct.
- **RESET CURRENT EXPERIMENT**: recreates only the selected experiment's real
  actor(s) and record state. It does not clear evidence in other experiment
  zones. Starts, motion completion, focus changes, and walking away never reset.

HUD telemetry always shows Mode, rule, enemy count, Scenario, Phase, Motion
`STOPPED/RUNNING/FINISHED`, object position `A/B/C/TRANSIT`, current interaction,
identity count, spatial record count, and multi-prop count. The room spawns no
Stalker and reports `ENEMY 0`.

## Deterministic scenarios and results

1. Visible translation stays inside legal view and retains one epoch with no
   historical proxy.
2. Visible 90- and 180-degree rotation retains one final observed pose.
3. Hidden A to B retains A; B remains undisclosed until it is legally viewed.
4. A-first verification erases A independently, then B appears independently.
5. B-first observation creates B while retaining A; later A verification erases
   A only.
6. The player leaves legal view, the cabinet moves continuously behind the
   opaque divider, the last legal pose freezes, and explicit re-entry creates a
   new epoch without a hidden interpolation path.
7. A to B to C produces three records for one StableID. Viewing A removes only
   A; viewing B removes only B; C remains the current real location.

The 2, 8, and 32-item runs preserve unique StableIDs and independent record
state. Their deterministic mutation moves item 00 and makes item 01 absent;
neighbors remain unchanged. A duplicate real StableID is rejected with a
warning. The counts are correctness evidence, not a performance matrix.

## Shortest manual route with no console

Open `/Game/Maps/L_ProjectFogPropGameplayLab` in the prepared editor and click
Play. The editor is launched with `-PropLabMovingControls`, so no console command
is required.

1. The player begins facing **VISIBLE TRANSLATE**. Keep the cursor aimed at its
   green label, press F once, and watch the one-second hold plus four-second move.
   The HUD must reach `Scenario 1 / Motion RUNNING / position TRANSIT`, then
   `FINISHED / B`.
2. Walk to **RESET CURRENT EXPERIMENT**, press F, then use **VISIBLE ROTATE**.
   Confirm a continuous four-second rotation with multiple intermediate angles.
3. Reset, face the cabinet at A, press F on **OFFSCREEN A TO B**, walk behind the
   opaque divider, and step onto the orange pressure plate. Return to B first:
   B appears while A remains. Look at A only afterward to erase A.
4. Reset and repeat **A TO B TO C** with two pressure-plate visits. Observe C
   before revisiting A or B; A and B remain independent until each location is
   legally rechecked.
5. Reset and use **MULTI PROP**. Confirm two differently shaped objects move, the
   small box disappears, and the long table stays fixed.
6. Reset and use **COVERAGE EDGE**. Hold the view while the cabinet crosses the
   boundary for eight seconds and compare live, gray-memory, entry, and exit.

Legacy commands remain only for deterministic automation compatibility. They are
not part of user acceptance. There is no policy command. Mode 0/1 remain available
in the older Lab for regression only.

StableID and observation epochs are shown only as diagnostics. The HUD does not
tell the player that an item moved or connect A, B, and C as inferred knowledge.

## In-world control build and automation checkpoint

Runtime-room build after the final source change:

- `Saved/PropGameplayLab/MovingMulti/InWorldFinalBuild.log`
- `DarkwellEditor Win64 Development`: Success, 7/7 actions.
- Preserved warnings: Visual Studio 14.51 is newer than the preferred 14.50 and
  UE 5.8.2 engine headers emit existing deprecation warnings.

The focused in-world control test passed after the final source change:

- `Saved/AutomationReports/InWorldControls06`: 1/1 Success.
- It drives the seven mechanism actors without console commands and verifies at
  least 80 distinct translation samples, 80 distinct rotation samples, 160 edge
  samples, hidden A-to-B and A-to-B-to-C history, multi-prop isolation, focus
  stability, and current-zone-only reset.
- Rotation also asserts every changing spatial grid uses a matching presentation
  texture extent; this guards the D3D12 out-of-bounds failure found during GPU QA.

Final combined suite:

- Filter: frozen 16 tests + all moving rules + existing Lab runtime matrix + the
  new in-world control test.
- Report: `Saved/AutomationReports/InWorldControlsFinal24`.
- Log: `Saved/PropGameplayLab/MovingMulti/InWorldAutomationFinal24.log`.
- Result: 24/24 Success; 19 clean, 5 Success with warnings, 0 failed, 0 not run.
- Severe scan (`Fatal`, assertion, ensure, access violation, failed test): 0.
- The warnings are expected duplicate/capacity fail-closed diagnostics and the
  already preserved external/editor warnings in the frozen suite.

## D3D12/SM6 visual evidence

The final evidence run used D3D12, SM6, normal TSR, and 100% Screen Percentage.
The actual embedded PIE backbuffer was `1526x549`; this is recorded as the actual
available resolution and is not represented as strict 1080p or performance
evidence. The successful run produced 85 screenshot files:

- Log: `Saved/PropGameplayLab/MovingMulti/InWorldGPU02.log`.
- Data: `Saved/PropGameplayLab/MovingMulti/InWorldPIE_20260901_134507/checks.json`.
- Review sheets: the `review` child directory beside `checks.json`.
- 12+ distinct rendered translation positions over four seconds.
- 12+ distinct rendered rotation angles over four seconds.
- Continuous eight-second boundary crossing, hidden A-to-B results, and the
  four-item multi-prop state.
- Severe scan: 0 in the successful run.

The agent opened and inspected the translation, rotation, boundary, and
hidden/multi contact sheets plus individual full-size frames. Adjacent frames
showed continuous position and angle changes with no instant final-transform
jump, residual chain, duplicate prop, duplicate shadow, or D3D12 assertion.

One preserved failed attempt is material: `InWorldGPU01.log` reached rotation
and exposed an out-of-bounds texture upload when rotating bounds exceeded the
initial record texture width. The Lab now recreates that record's presentation
texture whenever its grid dimensions change; the subsequent focused automation,
Editor build, and complete D3D12 run passed. The older four resolution-sizing
failures remain preserved and are not recast as visual evidence.

The actual Standalone PIE window was also exercised with Windows input: after
mouse capture and aiming at the green mechanism, pressing F changed the HUD from
`Scenario 0 / Motion STOPPED` to `Scenario 1 / Phase 1 / Motion RUNNING /
position TRANSIT`. No console command was used.

## Checkpoint commits

- `2264130` — record UE 5.8.2 qualification.
- `9d79dc1` — add the multi-record spatial observation model and tests.
- `b1374da` — remove policy switching and make SpatialEvidenceOnly the only rule.
- `889618f` — add the isolated moving/multi-prop runtime room and automation.

The frozen tag still points to the documentation freeze commit and the accepted
runtime baseline remains `39908cc67eb91d72a7d5ac35fe813367b41a7919`.

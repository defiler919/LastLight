# DARKWELL Moving and Multi-Prop Rule Validation

## Status

`PARTIAL — SPATIAL_EVIDENCE_MOVING_PROP_GPU_EVIDENCE_PENDING`

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
the Lab world has the `MoveRules` URL option. It uses Engine basic geometry:

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

## Commands and shortest manual route

Start PIE in `/Game/Maps/L_ProjectFogPropGameplayLab`, then enter:

```text
Darkwell.PropLab moverules reset
Darkwell.PropLab moverules help
Darkwell.PropLab moverules scenario 1
Darkwell.PropLab moverules scenario 2
Darkwell.PropLab moverules scenario 3
Darkwell.PropLab moverules scenario 4
Darkwell.PropLab moverules scenario 5
Darkwell.PropLab moverules scenario 6
Darkwell.PropLab moverules scenario 7
Darkwell.PropLab moverules advance
Darkwell.PropLab moverules multi 2
Darkwell.PropLab moverules multi 8
Darkwell.PropLab moverules multi 32
Darkwell.PropLab moverules stop
```

`moverules reset` travels to the isolated room and forces Lab Mode 2. There is no
policy command. Mode 0/1 remain available in the older Lab for regression only.

The shortest SpatialEvidenceOnly check is:

1. Run `moverules reset`, then `moverules scenario 3`.
2. Run `moverules advance` once: movement happens while A and B are illegal.
   Confirm A remains a gray spatial record and B does not leak.
3. Run `moverules advance` again: B is viewed and appears through Mode 2 while A
   remains. The HUD record count becomes two.
4. Run `moverules advance` a third time: A is legally viewed and only A erases.
5. Run `moverules scenario 7`, then `advance` four times to observe A, B, and C.
   Confirm A and B remain while C is current. Two more `advance` commands verify
   and erase A and B independently.
6. Run `moverules multi 2`, `multi 8`, and `multi 32`; after each, run `advance`
   once to compare one moved, one absent, and unaffected neighbors.

StableID and observation epochs are shown only as diagnostics. The HUD does not
tell the player that an item moved or connect A, B, and C as inferred knowledge.

## Final build and automation checkpoint

Runtime-room build after the final source change:

- `Saved/PropGameplayLab/MovingMulti/MovingRoomBuild09.log`
- `DarkwellEditor Win64 Development`: Success, 7/7 actions.
- Preserved warnings: Visual Studio 14.51 is newer than the preferred 14.50 and
  UE 5.8.2 engine headers emit existing deprecation warnings.

The focused room test passed twice:

- `Saved/AutomationReports/MovingMultiRoomRuntime08`: 1/1 Success with the
  expected duplicate-StableID warning.
- The same test passed again inside the final combined suite.

Final combined suite:

- Filter: frozen 16 tests + all moving rules + existing Lab runtime matrix.
- Report: `Saved/AutomationReports/MovingMultiSpatialEvidenceFinal01`.
- Log: `Saved/PropGameplayLab/MovingMulti/AutomationSpatialEvidenceFinal01.log`.
- Result: 23/23 Success; 17 clean, 6 Success with warnings, 0 failed, 0 not run.
- Severe scan (`Fatal`, assertion, ensure, access violation, failed test): 0.
- The warnings are expected duplicate/capacity fail-closed diagnostics and the
  already preserved external/editor warnings in the frozen suite.

## GPU evidence limitation

No new moving-room GPU evidence is accepted in this checkpoint. Four bounded
1080p D3D12/SM6 attempts stopped before the first screenshot:

- `Saved/PropGameplayLab/MovingMulti/GPU1080_SpatialEvidence01.log`: UE 5.8.2
  floating PIE window had no `NetMode` title.
- `GPU1080_SpatialEvidence02.log`: `editor_request_begin_play()` used the embedded
  viewport, so there was no separate PIE top-level window.
- `GPU1080_SpatialEvidence03.log`: resizing a maximized editor window was ignored.
- `GPU1080_SpatialEvidence04.log`: the restored embedded viewport was clamped by
  the 1920x1032 Windows work area at 1914x1054, short of the required 1920x1080.

All four runs confirmed forced D3D12 and SM6, but none produced a valid visual
frame or performance claim. The 1440p run was intentionally not started after
the bounded 1080p failure. Previous accepted Mode 2 GPU evidence remains valid
for the frozen static-prop presentation, but it is not represented as moving-room
evidence. Manual PIE is available; formal dual-resolution moving-room visual
evidence remains pending.

## Checkpoint commits

- `2264130` — record UE 5.8.2 qualification.
- `9d79dc1` — add the multi-record spatial observation model and tests.
- `b1374da` — remove policy switching and make SpatialEvidenceOnly the only rule.
- `889618f` — add the isolated moving/multi-prop runtime room and automation.

The frozen tag still points to the documentation freeze commit and the accepted
runtime baseline remains `39908cc67eb91d72a7d5ac35fe813367b41a7919`.

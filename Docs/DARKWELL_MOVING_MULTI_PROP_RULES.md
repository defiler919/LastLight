# DARKWELL Moving and Multi-Prop Rule Validation

## Status

`PARTIAL — UE_5_8_2_QUALIFICATION_PASSED`

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

## Scope still pending after qualification

- Audit the current single-record snapshot path.
- Implement multiple world-space observation records for one StableID without
  duplicating actor identity.
- Add deterministic A to B and A to B to C moving scenarios.
- Add 2, 8, and 32-prop identity-isolation scenarios.
- Add Lab commands, automation, GPU evidence, and manual PIE instructions.


# DARKWELL Moving and Multi-Prop Rule Validation

## Status

`PARTIAL — READY_FOR_USER_ALL_IN_WORLD_CONTROLS_AND_FLICKER_RETEST`

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
   `FINISHED / B`. Its indicator changes to complete and the HUD names the next
   test.
2. Walk to **VISIBLE ROTATE** without resetting and press F. Confirm a continuous
   four-second rotation with multiple intermediate angles.
3. Face the cabinet at A, press F on **OFFSCREEN A TO B**, walk behind the
   opaque divider, and step onto the orange pressure plate. Return to B first:
   B appears while A remains. Look at A only afterward to erase A.
4. Use **COVERAGE EDGE** and hold the view while the cabinet crosses the legal
   coverage boundary for eight seconds.
5. Use **A TO B TO C** with two pressure-plate visits. Observe C
   before revisiting A or B; A and B remain independent until each location is
   legally rechecked.
6. Enter the lower room from its north side and use **MULTI PROP**. Confirm two
   differently shaped objects move, the
   small box disappears, and the long table stays fixed.
7. Use **RESET CURRENT EXPERIMENT** only when the active experiment should be
   cleared and recreated. Ordinary completion never resets other zones.

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

## 2026-09-01 user failure correction and control-lock fix

The previous claim that all seven mechanisms were ready was disproved by the
user's real Standalone PIE run. The user could operate only **VISIBLE
TRANSLATE**. The other six controls could not be used, and the old gray proxy at
the movement origin sometimes flickered. This result supersedes the earlier
agent-side conclusion.

All six inaccessible controls had the same runtime root cause. The room used
`bInWorldScenarioSelected` both as the identity of the currently selected zone
and as a permanent global interaction lock. It was set by the first mechanism
and remained set after that motion reached its final phase. Consequently the
other independent actors failed `CanActivateInWorldControl`; they could not
become the focused interaction target, show a usable F prompt, or dispatch their
scenario. The actors were spawned, had distinct interaction identities and
interfaces, and their labels matched their locations. Scenario state did not
consume them individually; the shared latch blocked all of them. A synthetic
test initially approached **MULTI PROP** and **RESET** from south of the room,
which is behind the exterior wall. Their intended reachable side is north; the
end-to-end test now uses that same interior approach.

The correction separates selection from activity. Each mechanism has an
independent Actor and completion state, while the room-wide busy state lasts
only while the current test is running or waiting for its required spatial
evidence. Completion releases the lock without clearing any D/V/R data,
Observation Epoch, or evidence in another zone. A completed control remains
focusable and reports its state instead of silently failing. The indicator and
prompt now distinguish ready, running, complete, busy, and reset states; the HUD
shows `Completed n/6` and `NEXT TEST`. Window focus changes do not reset or
consume a control.

The exact scenario/phase contracts are:

- **VISIBLE TRANSLATE**: Scenario 1, Phase 0 ready, Phase 1 moving, Phase 2
  finished at B.
- **VISIBLE ROTATE**: Scenario 2, Phase 0 ready, Phase 1 rotating, Phase 2
  finished at 180 degrees.
- **OFFSCREEN A TO B**: Scenario 3, Phase 0 armed, Phase 1 moving, Phase 2 at B
  waiting for legal B evidence, Phase 3 complete.
- **COVERAGE EDGE**: Scenario 6, Phase 0 ready, Phase 1 crossing, Phase 2
  finished.
- **A TO B TO C**: Scenario 7, Phase 0 ready, Phase 1 A-to-B, Phase 2 at B,
  Phase 3 B-to-C armed, Phase 4 B-to-C moving, Phase 5 at C waiting for legal C
  evidence, Phase 6 complete.
- **MULTI PROP**: Scenario 100, Phase 0 ready, Phase 1 moving/mutating, Phase 2
  finished.
- **RESET CURRENT EXPERIMENT**: Scenario 0 after rebuilding only the active zone;
  it is the seventh independent interaction and is never an implicit phase of
  another control.

## Stale-proxy flicker root cause and correction

The stale record was normally created once, but hidden motion performed its
first real-actor transform update before freezing the old epoch and creating the
proxy. This left a first-frame representation handoff in which live and stale
geometry could overlap at A or the stale representation could arrive one tick
late. In addition, an unchanged historical D/V/R presentation texture was
uploaded every frame. The record was not meant to toggle, but these two sources
of presentation churn made an occasional whole-proxy flicker possible.

Hidden motion now seals A atomically before the first transform interpolation:
the current visual state is finalized, source geometry is hidden for the
handoff, exactly one stale epoch and proxy are frozen at the fixed A transform,
its texture and cap are updated, and only then does the real actor begin moving.
The current live epoch alone follows the real actor. A historical record locks
its texture dimensions, does not upload an unchanged presentation buffer, and
is never rebuilt from rotating current bounds. The fix adds diagnostic counts
for freezes, proxy creations, visibility transitions, texture creation/uploads,
fixed dimensions, record ID, and D/V/R signature. It does not add delay or
change `.20/.18`, 4x4 AA, the dark-gray cap, or `SpatialEvidenceOnly`.

The focused flicker test repeats hidden A-to-B three times. Each cycle creates
one stale epoch and one proxy and produces no trail. The first cycle records 600
60-Hz-equivalent states over ten seconds with a fixed camera, followed by a slow
behind-wall sweep. Proxy visibility transitions remain zero; record ID, texture
dimensions, and D/V/R signature remain stable; unchanged frames do not cause
new texture uploads. The same test covers movement start overlap, departure,
transit, arrival, A/B repetition, and multi-prop isolation.

## Corrected validation checkpoint

The final source build is `DarkwellEditor Win64 Development` and succeeded: the
full source checkpoint completed 7/7 actions, and the final incremental build
after the evidence-hook return-value correction completed 4/4 actions. All
UBT/dotnet invocations were serial. A focused progression history is deliberately
preserved:

- `Saved/AutomationReports/MovingControlsFocused_20260901_142459`: 0/2. The
  synthetic route approached the lower controls through the south wall.
- `Saved/AutomationReports/MovingControlsFocused_20260901_142627`: 1/2. The
  flicker test passed; the remaining assertion incorrectly required an
  undiscovered low prop to own a record.
- `Saved/AutomationReports/MovingControlsFocused_20260901_142843`: 2/2 Success.
- `Saved/AutomationReports/MovingControlsFinal_20260901_142938`: 25/25 Success,
  19 clean and 6 with preserved warnings, with 0 failed/not-run tests and 0
  severe-log matches.
- `Saved/AutomationReports/MovingControlsClosure_20260901_144607`: the final
  post-build closure run is also 25/25 Success, 19 clean and 6 with preserved
  warnings, 0 failed/not-run/in-process, 80.69 seconds, and 0 severe-log
  matches. Its log is `Saved/Logs/MovingControlsClosure_20260901_144607.log`.

The native in-world automation follows the player interaction chain rather than
calling scenario functions: it moves the Pawn into range, performs the same
proximity query and visibility trace, verifies the F prompt, dispatches through
`UDarkwellInteractionComponent::TryInteract`, waits for continuous motion, and
continues to the next actor. It individually covers all seven interactions and
also runs them sequentially without a global reset. Intermediate samples prove
that translation and rotation are continuous.

The corrected D3D12/SM6 evidence run used normal TSR and 100% Screen Percentage.
Its actual embedded PIE backbuffer was `1526x549`; it is functional evidence,
not strict 1080p performance evidence. It wrote 261 screenshots under
`Saved/PropGameplayLab/MovingMulti/InWorldPIE_20260901_143312`, including 101
frames for the ten-second fixed stale-proxy observation. The agent opened the
all-motion, hidden-transit/fixed, ten adjacent fixed frames, slow sweep, and
A-to-B-to-C/multi contact sheets. They show continuous translation/rotation,
no whole-proxy disappearance, no path chain, no duplicate prop or shadow, and
no visible Z-fighting in the captured sequence.

The actual Windows Standalone window was used without the console for the first
control. A real F press produced `Scenario 1 / Phase 1 / Motion RUNNING /
TRANSIT`, then `Scenario 1 / Phase 2 / FINISHED / B`, and exposed the next-test
guidance. The available Windows input driver cannot hold a WASD axis across
simulation ticks, so it could not honestly walk the agent to the remaining six
controls. Those six are covered by the same range/trace/prompt/F-dispatch path in
native automation and by GPU runtime evidence, but the document does not claim
that the agent manually walked and pressed F on all seven. The user real PIE
retest remains the authority for final usability.

No production map, `/Game/Maps/L_Prototype`, frozen Mode 2 behavior, public
SightWeave contract, StableID contract, or `Darkwell.uproject` is part of this
correction.

## 2026-09-01 Scenario 2 rotation overlap correction

The user's real PIE result supersedes the earlier agent-side visual conclusion.
**VISIBLE TRANSLATE** remains user-passed. **VISIBLE ROTATE** started and moved
continuously, but finished with a gray proxy at the old orientation and the
current real cabinet at the new orientation sharing the same center. Their
intersecting and coplanar surfaces produced visible Z-fighting. The remaining
in-world mechanisms still require the user's manual pass after this correction.

The rotation control did not explicitly freeze the zero-degree pose. The shared
tracking path froze an epoch whenever the physical transform changed while no
cell had legal SightWeave coverage. A brief loss of legal coverage during an
otherwise visible rotation therefore sealed the last legally seen intermediate
pose. On reacquisition, the current pose received a new current epoch. The old
record could not be verified empty where the new, same-center cabinet occupied
the same samples, and the renderer had no per-sample ownership rule between the
current geometry and the stale proxy. Both representations could consequently
write the same pixels and depth. Automated occlusion telemetry sealed yaw
`36.87` degrees; the D3D12 route sealed an intermediate pose around `57.5`
degrees. This confirms a last-seen-pose record rather than a hard-coded
zero-degree duplicate.

The correction adds monotonic, legal-evidence-backed presentation ownership.
When newly discovered current geometry owns a world sample, historical
presentation at that sample contributes zero. The historical observation epoch
and its D/V/R authority remain intact; this rule does not mark the old space
empty, clear an entire StableID, hide the whole proxy, or use a depth offset.
When the real object later leaves and the old space is legally observed empty,
the existing `SpatialEvidenceOnly` erasure path remains responsible for
verification and record release. This keeps current and historical rendering
mutually exclusive at each sampled world position:

```text
CurrentLiveContribution + StaleProxyContribution <= 1
```

Scenario 2 diagnostics now expose `LIVE EPOCHS`, `STALE EPOCHS`, `VISIBLE
PROXIES`, `OVERLAP CONTRIBUTORS`, and legal coverage in the Lab HUD. Per-frame
telemetry records the live and stale epoch identities, actual and stale
transforms, real and proxy component visibility, D/V/R cell counts, legal
coverage ratio, and maximum contributor count.

The continuously visible `0 -> 180` rotation lasts four seconds and samples at
least 80 distinct intermediate angles. It remains `live=1`, `stale=0`,
`visible proxy=0`, and `overlap contributors=1` throughout. A fixed ten-second,
600-frame-equivalent observation after completion creates no proxy and no
visibility transition. The same automation also covers `180 -> 0` and
`0 -> 90 -> 180`, for three continuous visible rounds with one current epoch
and no stale epoch or path chain.

The visibility-loss test starts from legal view, loses coverage during rotation,
and freezes the actual last legally seen intermediate orientation. Returning to
view can retain one current and one stale epoch because StableID is not player
knowledge, but per-sample contributors never exceed one. The stale record is
resolved only by legal spatial evidence. Existing regressions continue to prove
visible translation has no trail, offscreen A-to-B retains A until A is checked,
A-to-B-to-C and multi-prop histories remain isolated, and Mode 0/1, `.20/.18`,
4x4 AA, the deep-gray cap, NeverRemember, and invalid-coverage-zero contracts
are unchanged.

### Corrected build and evidence

All UBT and dotnet operations were serial. The final source checkpoints built
successfully as `DarkwellEditor Win64 Development`: 7/7 actions before the
final diagnostic semantics correction and 5/5 actions afterward. Two build
failures are preserved: the first used an unavailable `TArray`
`CountByPredicate` helper in UE 5.8, and the second exposed an ambiguous integer
automation assertion. Both were compile-only issues and were corrected without
changing the authority thresholds.

Focused automation progression is retained rather than rewritten:

- `RotationOverlapFocused_20260901_154459`: 3/4 Success. The failed rotation
  segment tried to start on the exact floating-point completion frame.
- `RotationOverlapFocused_20260901_154657`: 4/4 Success.
- `RotationOverlapTelemetryFinal_20260901_155702`: 4/4 Success after the final
  contributor-count semantics correction. Its completed visible rotation logs
  `live_epoch=1 stale_epochs=0 actual_yaw=180 coverage=1 proxies=0
  overlap_contributors=1`.
- `RotationOverlapFullFinal_20260901_160355`: 27/27 Success, 18 clean and 9 with
  preserved warnings, 0 failed/not-run tests, 98.01 seconds. The startup
  `UnifiedErrorTest` deliberately emits `Condition failed` probes; the exported
  Darkwell report has no failed test or runtime fatal/assert/crash.

The final D3D12/SM6 run uses normal TSR and 100% Screen Percentage at the actual
embedded PIE backbuffer `1526x549`. It wrote 413 screenshots under
`Saved/PropGameplayLab/MovingMulti/InWorldPIE_20260901_155848`; this is functional
evidence, not strict 1080p performance evidence. The run covers the fully
visible rotation, deliberate mid-rotation view loss and reacquisition, 101
fixed adjacent samples over ten seconds, and the remaining in-world routes.
The agent opened the visible-rotation, loss/reacquisition, and ten-adjacent-frame
contact sheets. They show one continuously rotating asymmetric cabinet, no old
gray proxy during fully legal rotation, and no alternating whole-proxy frame,
coplanar flashing, duplicate cabinet, or path chain after reacquisition.

This checkpoint is ready for the user's Scenario 2 rotation contribution
exclusion retest. It does not claim that the user has accepted the correction,
and it does not revise the frozen Mode 2 baseline or production defaults.

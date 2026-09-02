# DARKWELL Moving and Multi-Prop Rule Validation

## Status

`PARTIAL — READY_FOR_USER_PARTIAL_ROTATION_STALE_CAP_RETEST`

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

## 2026-09-01 Scenario 2 observation-validity and cap-ownership closure

The user then reproduced a stricter failure on runtime checkpoint `5463ef6`:
Scenario 2 could leave approximately 0, 67, and 125 degree gray poses beside
the final 180 degree live cabinet. A dark-gray vertical cap could remain even
when the visible stale surface appeared suppressed, and some samples had two
render contributors. This real Standalone result supersedes the earlier visual
claim above. **VISIBLE TRANSLATE** remains user-passed; Scenario 2 and the five
remaining controls still require the user's new manual pass.

### Confirmed root cause

The repeated zeroes on the user's route were genuine legal-observation losses
(`OUTSIDE_LEGAL_SOURCE`, and in bounded cases occlusion), rather than rotation
grid allocation failures. The defect was that the shared moving-prop tracker
had no explicit armed/sealed observation lifecycle and did not carry coverage
validity plus authority/transform/grid revisions through the decision. Every
valid zero combined with another transform change could therefore seal another
intermediate pose. An invalid or not-ready sample was also indistinguishable
from a valid zero at the caller.

The second defect was independent. Historical presentation used current-evidence
suppression as part of its cut classification, so suppression could feed the cap
builder even though it was occupied evidence rather than an empty-space
verification. The old diagnostic counted surface projection contributors but
did not cover every cap primitive and could also conflate separate 3D samples
that happened to share XY. A resolved record could retain its proxy, cap,
texture, and MIDs, leaving resources capable of rendering again.

### Observation lifecycle and publication revisions

The project-side coverage query now returns the numeric coverage, validity,
authority revision, coverage revision, and an explicit zero reason. Moving Lab
snapshots additionally carry the transform and grid revisions used by that
coverage. The shared tracker uses `NeverObserved`, `ObservedArmed`, and
`UnobservedSealed` states:

- valid legal observation arms one current observation episode;
- a real, valid loss while the physical transform changes seals the last valid
  pose once and enters `UnobservedSealed`;
- repeated valid zeroes and later hidden transforms cannot create a pose chain;
- valid legal reacquisition starts the next episode and re-arms one future seal;
- invalid/not-ready coverage never seals, re-arms, advances the last valid pose,
  verifies empty space, or changes a historical record.

The one-frame invalid-coverage injection is test-only and exercises this shared
path; Scenario 2 contains no validity bypass, fixed-coverage override, delay,
or N-frame debounce.

### Spatial ownership and cap semantics

Historical presentation cells now distinguish unresolved memory, true
`VerifiedEmpty` evidence, and monotonic current-evidence supersession. A newer
legal current observation owns overlapping world samples, so older surfaces at
those samples contribute zero. This is presentation ownership only: it does not
clear a StableID, rewrite the historical transform, or claim that the player
observed empty space. Unresolved, non-overlapping history remains available
under `SpatialEvidenceOnly`.

The cap builder now accepts only a real `VerifiedEmpty` boundary. Current
evidence supersession is explicitly excluded and current-owned centers are
skipped. Therefore current suppression cannot manufacture a cap, while the
existing positive control still creates the accepted `#343A40` cap at a real
partial empty verification boundary. The 4x4 conservative presentation sample,
bilinear filtering, `.20/.18` timing, deep-gray material, Mode 0/1 behavior,
and D/V/R authority are unchanged.

Contributor diagnostics now report live/stale epochs, visible surface proxies,
visible cap primitives, surface/cap/total contributors across every epoch,
coverage validity and reason, all relevant revisions, observation episode/state,
seal count, and texture dimensions/generation. Separate 3D surface and vertical
cap loci are no longer added merely because their XY projections coincide. The
enforced bound is:

```text
CurrentSurface + all StaleSurfaces + all StaleCaps <= 1
```

When every renderable sample of an old epoch is either truly empty or
superseded, its proxy, cap, presentation texture, and material instances are
permanently retired. Later view loss cannot recreate them. Authority metadata
may remain under the existing history policy; retirement is not represented as
player empty-space knowledge.

### Checkpoints, build, and automation

The reliable runtime checkpoint is
`1e3fd7753e4a98df1a67e89cb73f66249aebc3ce` (`fix: gate rotated stale ownership
on valid observation`). It was pushed to
`origin/codex/darkwell-prop-memory-gameplay-lab`. It combines the bounded
diagnostic, observation-lifecycle, ownership/cap, resource-retirement, tests,
and evidence-driver changes; no intentionally failing intermediate commit was
published. The documentation closure is a separate subsequent checkpoint.

All UBT, dotnet, Editor, and automation processes ran serially. The final
`./Scripts/BuildEditor.ps1 -Configuration Development -EngineRoot 'D:\\UE_5.8'`
completed `DarkwellEditor Win64 Development` successfully (`5/5` actions after
the final source correction). Preserved compiler warnings are the installed
MSVC 14.51 toolchain being newer than UE's preferred 14.50 family and existing
UE deprecation warnings.

Final automation evidence is retained under ignored `Saved/AutomationReports`:

- `RotationCheckpointBC_20260901_180422`: 5/5 Success (2 clean, 3 with
  warnings, 0 failed/not-run), 92.6143 seconds. It covers invalid coverage,
  no-console continuous motion, true-loss last-seen pose, ten-second stale
  stability, and fully visible rotation exclusion.
- `RotationRelease_20260901_180746`: 1/1 clean Success, 31.9057 seconds. It
  logs proxy/cap/texture retirement and proves no later reappearance.
- `PropLabFinal_20260901_180933`: 31/31 Success (22 clean, 9 with warnings,
  0 failed/not-run), 172.7431 seconds.
- `FogVisualFinal_20260901_181255`: 8/8 clean Success, 0.1427 seconds.
- `M6P1Final_20260901_181324`: 4/4 Success (3 clean, 1 with warnings,
  0 failed/not-run), 0.1184 seconds.

The warning-class results retain engine startup/HTTP noise; none is a failed
Darkwell assertion. Severe scans found zero project `Assertion failed`, `Fatal
error`, `Unhandled Exception`, `Automation Test Failed`, GPU crash, or D3D12RHI
error. Earlier diagnostic failures remain preserved at
`RotationDiagA_20260901_174447`, `RotationLifecycle_20260901_175459`,
`RotationOwnership_20260901_175834`, and
`RotationOwnership2_20260901_180022`. They exposed respectively the original
duplicate contributors, over-eager static turn-away sealing, incomplete
ownership sampling, and the old XY-only contributor false positive; they are
not cited as passing evidence.

### D3D12/SM6 inspection

A real Windows Standalone run used the in-world prompt and an actual **F** press
to start **VISIBLE ROTATE**; no console command or direct Scenario function was
used. The requested window was 1600x900 (Windows capture 1602x932 including
frame). Its HUD showed `Scenario 2`, valid 100% coverage, `LIVE 1`, `STALE 0`,
`PROXIES 0`, `CAPS 0`, `SURFACE 1`, `CAP 0`, `TOTAL 1`, and `SEALS 0` at start
and completion. A further fixed ten-second observation showed no old pose,
vertical cap, double cabinet, or whole-object flash. This is functional manual
evidence, not user acceptance.

The repeatable D3D12/SM6 Editor PIE evidence used normal TSR and 100% Screen
Percentage at the actual `1526x549` backbuffer. It produced 420 telemetry rows
and 413 screenshots under
`Saved/PropGameplayLab/MovingMulti/InWorldPIE_20260901_183517`. The visible
rotation set has 22 rendered samples, 13 distinct sampled yaws, zero stale
epoch/cap/seal, and maximum total contributor 1; native automation supplies the
80-plus intermediate-angle state coverage. The deliberate loss/reacquisition
set has 152 samples, one seal, at most one stale epoch, and maximum total
contributor 1. The fixed post-reacquisition strip has 101 adjacent samples over
ten seconds and total contributor exactly 1 throughout.

The agent opened at original resolution the visible-rotation, loss/reacquisition,
and ten-frame fixed contact sheets plus adjacent full-size frames around the
seal and reacquisition. The current cabinet and historical presentation never
write the same sampled location. A cap resource can remain while genuinely
unverified old space remains, but current-evidence suppression contributes no
cap; the real-empty positive cap is independently retained by the 31/31 PropLab
regression. No Saved image, report, log, video, Binary, or Intermediate output
is tracked by Git.

### Required user retest

Final state is **PARTIAL — READY_FOR_USER_SCENARIO2_AND_REMAINING_IN_WORLD_CONTROLS_RETEST**.
On the home checkout, open `/Game/Maps/L_ProjectFogPropGameplayLab` with normal
D3D12/SM6, project TSR, and 100% Screen Percentage. Click Play and use only the
world controls and **F**:

1. run **VISIBLE TRANSLATE**, then **VISIBLE ROTATE**;
2. watch the full four-second 0-to-180 rotation and the completed pose for at
   least ten seconds, then move and turn slowly;
3. reject the build if any old gray angle, dark-gray vertical cap, duplicate
   cabinet, or Z-fighting returns during the continuously visible rotation;
4. continue **OFFSCREEN A TO B**, **COVERAGE EDGE**, **A TO B TO C**,
   **MULTI PROP**, and **RESET CURRENT EXPERIMENT**.

The user has not yet accepted Scenario 2 or the remaining controls. Nothing in
this correction changes the frozen Mode 2 tag, production maps, production
defaults, or the public SightWeave/StableID contracts.

## 2026-09-02 partial-rotation stale-cap 3D ownership closure

This follow-up started at
`9428ce929f3ff361ce70df5e3b63cd46fb2374d2`. The stricter user route was:

1. fully observe the asymmetric cabinet at 0 degrees;
2. start **VISIBLE ROTATE**, turn away during its one-second delay, briefly
   reacquire only an edge at an intermediate angle, then turn away again;
3. let the cabinet finish at 180 degrees while hidden;
4. slowly reacquire the final pose while unresolved stale epochs still exist.

Retaining stale epochs is correct `SpatialEvidenceOnly` behavior. The defect was
specifically a stale surface or cap occupying current-cabinet 3D space. The
reliable pre-fix report
`Saved/AutomationReports/PartialRotatedStaleCap_CheckpointA_02` kept stale
history and measured two stale cap segments inside current-owned 3D space, with
maximum 3D render ownership 2. Its focused log is
`Saved/PropGameplayLab/MovingMulti/PartialRotatedStaleCap/CheckpointA_Focused01.log`.
The old `TOTAL=max(surface,cap)` telemetry incorrectly remained 1 because those
were projected, class-local counts rather than one 3D ownership query.

### Root cause and geometry correction

Each observation record previously retained only world axis-aligned aggregate
bounds for ownership decisions. Surface suppression was an XY test, and the cap
builder emitted coarse horizontal quads accepted from only the cap center in
XY. A rotated stale OBB could therefore project a cap through the final current
cabinet without the old telemetry seeing two contributors.

Records now snapshot each primitive's local bounds plus world transform. A
vertical world ray is transformed into each primitive's local space to compute
the actual OBB Z interval at that XY sample. Current and newer legally observed
epochs publish their owned intervals; stale surfaces are suppressed only where
old and newer primitive intervals truly overlap in 3D. Each coarse cap quad is
split into four fine horizontal segments, restricted to the recorded primitive
OBB, then clipped by subtracting newer/current legally observed OBB Z intervals.
Only residual non-overlapping segments are emitted. Resource retirement uses
the same OBB model.

This is not a depth bias, offset, transparent material, whole-cap disable,
StableID clear, or history discard. Unresolved non-overlapping stale geometry
continues to render under `SpatialEvidenceOnly`; true `VerifiedEmpty` boundaries
still create the frozen opaque unlit deep-gray cap. Projected surface/cap
diagnostics remain as class-local observability, while `TOTAL`, compatibility
overlap, and the enforced ownership bound now use actual 3D render contributors.

New telemetry reports `CURRENT_3D_OVERLAP_STALE_SURFACE`,
`CURRENT_3D_OVERLAP_STALE_CAP`, `MAX_3D_RENDER_OWNERSHIP`, and the first
offending epoch, primitive, world point, and current transform. Current-owned
3D space must contain no stale surface or cap, and each actual world sample may
have at most one render owner.

### Checkpoints and deterministic regression

- `9fc96a6409da6be140e2d9bfdb3d59c06878d50e` (`test: reproduce partial
  rotated stale cap overlap`) adds the bounded diagnostics and deterministic
  failing reproduction. It was built and pushed before the fix.
- `54d7055143acb38987f0dd3b18a9733fde054d60` (`fix: clip stale cap against
  newer observed geometry`) adds transformed-OBB ownership, fine cap clipping,
  resource retirement correction, and multi-epoch regression coverage. It was
  built, tested, and pushed before this documentation closure.

`Darkwell.PropLab.MovingRules.InWorldControls.PartialRotatedStaleCapReproduction`
uses the real in-world control interaction and normal SightWeave/project-fog
coverage; it does not force a coverage ratio or write presentation cells.
`PartialCurrentMultiEpoch3DOwnership` creates two partial intermediate
observations and a partial final observation while three stale epochs remain.
It recorded first/second/final coverage ratios of 8.6821%, 5.2753%, and 41.6667%
respectively, with zero current/stale surface overlap, zero current/stale cap
overlap, and maximum 3D ownership 1.

The post-fix MovingRules report is
`Saved/AutomationReports/PartialRotatedStaleCap_CheckpointB_MovingRules06`:
14/14 Success (5 clean, 9 with preserved warnings), 0 failed/not-run, 381.887
seconds. It includes the rotation lifecycle, partial multi-epoch ownership,
resource retirement, and real `VerifiedEmpty` Mode 2 cap positive control.

### Final build, automation, and D3D12 evidence

The final serial build used
`Scripts/BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8`.
`DarkwellEditor Win64 Development` succeeded in 17.92 seconds with 4/4 actions.
Preserved warnings are MSVC 14.51 being newer than UE's preferred 14.50 and
existing UE header deprecations. Log:
`Saved/PropGameplayLab/MovingMulti/PartialRotatedStaleCap/CheckpointD_FinalBuild.log`.

The final NullRHI filter was
`Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1`. Report
`Saved/AutomationReports/PartialRotatedStaleCap_Final` contains 45/45 Success:
31 clean, 14 with preserved warnings, 0 failed/not-run, 443.2573 seconds. It
includes both new partial-rotation tests, all 14 MovingRules tests, the
`Mode2SymmetricDarkGrayCutCap` positive control, FogVisual, and M6P1. Severe
scans found no project fatal, assertion, unhandled exception, failed automation,
GPU crash, or D3D12 device error. Warning-class results retain expected
duplicate-fixture/StableID probes and engine HTTP/startup noise.

The repeatable GPU run used real Windows D3D12/SM6 Editor PIE with normal TSR
and 100% Screen Percentage. Its actual embedded PIE backbuffer was `2038x789`;
the screenshots are `2560x1440` engine captures, so this is functional evidence
rather than a strict requested-resolution performance claim. It produced 496
telemetry rows and 488 original screenshots under
`Saved/PropGameplayLab/MovingMulti/InWorldPIE_20260902_012149`; log:
`Saved/PropGameplayLab/MovingMulti/PartialRotatedStaleCap/CheckpointD_D3D12_02.log`.

The exact user route reached a 21.1738% intermediate edge observation and
finished with three stale epochs plus visible historical surface/cap evidence.
Across its 76 partial-rotation samples, maximum current-owned stale-surface
intrusion was 0, maximum current-owned stale-cap intrusion was 0, and maximum
3D render ownership was 1. The agent opened the original initial, intermediate
edge, resealed, hidden-final, and slow-reacquisition frames and found no stale
gray cap/surface crossing the current cabinet, double cabinet, or Z-fighting.
The first GPU attempt is intentionally retained as `CheckpointD_D3D12_01.log`;
it failed only because the Python reflection spelling of the digit/acronym
diagnostic getter differed from its C++ name, after native automation had
already validated the gameplay diagnostics.

### Required user retest

Final state is **PARTIAL — READY_FOR_USER_PARTIAL_ROTATION_STALE_CAP_RETEST**.
Open `/Game/Maps/L_ProjectFogPropGameplayLab` with normal D3D12/SM6, project TSR,
and 100% Screen Percentage, then use the world prompt and **F** only:

1. reset Scenario 2 and fully observe the cabinet at 0 degrees;
2. press **F** on **VISIBLE ROTATE**, immediately turn away, briefly look back
   at only one edge during rotation, then turn away again;
3. wait for hidden 180-degree completion and slowly look back across the final
   cabinet;
4. accept only if unresolved history may remain elsewhere but no stale gray
   surface or dark-gray cap intersects the current cabinet, flashes as a second
   cabinet, or Z-fights with it.

The user has not yet accepted this correction. It does not change the frozen
Mode 2 tag, production maps/defaults, or public SightWeave/StableID contracts.

## 2026-09-02 rotated edge-sliver render-ownership closure

This follow-up started from the company remote checkpoint
`8747acd1dbcee57d773a6171d02230cd1b8ed046`. The user's stricter partial-hidden
rotation retest confirmed that the earlier transformed-OBB correction removed
the large duplicate pose, but still failed on two stable fragments: a small
stale-looking piece above the current cabinet and a thin deep-gray cap strip on
its lower/side surface. Both persisted from four viewpoints, so the result was
treated as a real submitted fragment rather than TSR or screen-angle noise.

### Fragment classification and root causes

Per-fragment telemetry now records epoch, primitive, fragment type, texel or
world vertices, normal, material, ownership, clip result, overlap/contact
classification, nearest current primitive, and separation. The top fragment
was `STALE_SURFACE`, not current reveal geometry or a retired component. A raw
`SuppressedByCurrentEvidence` texel was zero, but its positive neighbor was
bilinearly filtered back into current-owned render space. Smooth historical
presentation and the hard ownership exclusion had been applied in the wrong
order for the final sample.

The lower strip was `STALE_CAP` using `M_ManualStaleCutCap`. The old cap
subdivision could retain a quad whose center was outside the current OBB while
an edge touched or crossed its closed projected boundary. Strict volume
overlap also did not express render-safe coplanar/touching ownership. The first
precision correction removed the large vertical residual but exposed a final
0.09 cm-wide horizontal strip: cap generation and its diagnostic both used the
same expanded projection boundary, so the exact split endpoint belonged to
both closed sets forever.

### Presentation-only correction

Historical fade and 4x4 sampling remain smooth. A conservative one-texel guard,
equal to the TF_Bilinear support, is derived from the raw hard ownership mask
and multiplied into the submitted stale presentation after filtering. This
prevents bilinear history from repopulating a location already owned by newer
legal geometry; it does not write D/V/R or `VerifiedEmpty`.

Cap boundaries are split at transformed primitive OBB projection entry/exit
and spatial-grid breakpoints. Each resulting fragment is restricted to the old
primitive, then newer/current owned vertical intervals are subtracted before
the quad is submitted. Cap cache signatures include newer-record suppression
and retirement state so the mesh cannot survive a relevant ownership change.

Render contact is a closed presentation set with a named `0.05 cm` tolerance.
Clipping uses `0.051 cm`: the additional `0.001 cm` (0.01 mm) is only a numeric
roundoff margin, and diagnostics continue to judge contact at `0.05 cm`.
Projection clipping therefore ends strictly outside the diagnostic closed set
instead of sharing the same endpoint. This is not depth bias, polygon offset,
geometry movement, whole-epoch hiding, or a larger knowledge/coverage epsilon.

`SpatialEvidenceOnly`, StableID separation, observation epochs, D/V/R,
`.20/.18`, 4x4 AA, the deep-gray `#343A40` cap, Mode 0/1, and public SightWeave
contracts are unchanged. Non-overlapping unresolved stale history remains.
`VerifiedEmptyCapPositiveControl` reaches real partial `VerifiedEmpty` evidence
and verifies that its legal non-overlapping cap is still visible.

### Published checkpoints

- `13961cfef3d8659535e6abbcc5364b2d42b58be3` — `test: reproduce residual
  rotated ownership sliver`; adds fragment attribution and the failing
  render-contact regression.
- `6d0f0f41922a4d6f265a7f6b63b3f30286828d3a` — `fix: close stale ownership
  boundary slivers`; adds the hard ownership guard and conservative
  surface/cap clipping.
- `8d3e6018579d710a71828c21a50395da0b45b7af` — `test: close rotated ownership
  edge regression`; adds the four named regressions, final closed-set endpoint
  ownership, D3D12 telemetry, and four-view capture timing.

### Build, automation, and GPU evidence

The serial source build completed 7/7 actions successfully in 20.31 seconds.
The final standard `DarkwellEditor Win64 Development` check was up to date and
succeeded in 6.95 seconds. Its ignored log is
`Saved/PropGameplayLab/MovingMulti/ResidualEdge/FinalBuild_20260902_100700.log`.
Preserved build warning: installed MSVC 14.51 is newer than UE's preferred
14.50 family; earlier compiled actions also retained existing UE header
deprecation warnings.

Focused reports retained under ignored `Saved/AutomationReports` include
`ResidualEdge_PrecisionFocused_20260902_095000` (6/6 Success) and
`ResidualEdge_EndpointOwnership_20260902_095200` (4/4 Success). The final filter
`Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1` is
`ResidualEdge_Final_20260902_100500`: **49/49 Success** (33 clean, 16 with
preserved warnings, 0 failed/not-run), 376.289 seconds. It includes
`EdgeContactSliver`, `FourViewStableResidual`, `HardOwnershipFiltering`,
`VerifiedEmptyCapPositiveControl`, both prior partial-rotation tests, every
MovingRules test, PropLab, FogVisual, and M6P1.

Three final complete D3D12/SM6 Editor PIE routes used project TSR and 100%
Screen Percentage. All passed with actual embedded viewport `1526x549`; engine
captures were `1920x1032`. Logs and screenshot roots:

- `Endpoint_D3D12_01.log`, `InWorldPIE_20260902_095415`: 493 screenshots;
- `Endpoint_D3D12_02.log`, `InWorldPIE_20260902_095731`: 493 screenshots;
- `Endpoint_D3D12_03_EvidenceFlush.log`, `InWorldPIE_20260902_100146`: 494
  screenshots, including four separately landed final viewpoints.

The third run's south/east/north/west original images are frames 0122-0125.
The agent opened all four originals plus the full-live frame and the start,
middle, and end of the ten-second adjacent-frame strip. South/east/west were
stable with no top stale piece, deep-gray current-surface sliver, duplicate
depth contributor, or Z-fighting. The north view was legally occluded by the
opaque wall and showed no leak. The ten-second sequence did not change its
submitted ownership. All four state rows recorded surface contact 0, cap
contact 0, hard-filter leak 0, and maximum ownership 1.

Earlier failures are deliberately retained. `CheckpointA_EdgeContact` exposed
318 stale-surface/filter-leak samples. `CheckpointC_D3D12_Final01.log` exposed
the initial cap contact at the 0.05 cm vertical boundary.
`Precision_D3D12_01.log` then exposed the final 0.09 cm split-endpoint strip;
this led to separating clipping clearance from diagnostic contact tolerance.
Severe scans over the final automation, all three passing GPU logs, and final
build found 0 assertion/fatal/unhandled/automation-failure/GPU-crash/D3D12
device-error entries.

### Required user retest

Final state is **PARTIAL — READY_FOR_USER_ROTATED_EDGE_SLIVER_RETEST**. In
`/Game/Maps/L_ProjectFogPropGameplayLab`, use normal D3D12/SM6, project TSR, and
100% Screen Percentage, then use the in-world **VISIBLE ROTATE** control and
**F** only:

1. fully observe the 0-degree cabinet;
2. start rotation, turn away, briefly expose only one mid-rotation corner or
   edge, and turn away again;
3. allow hidden completion to 180 degrees, then slowly reacquire the final
   cabinet;
4. inspect its top, bottom, cap endpoints, handle, and both sides from multiple
   viewpoints;
5. reject if a stale top fragment, deep-gray strip on the current surface,
   duplicate surface, or Z-fighting returns; legal non-overlapping gray history
   and its real `VerifiedEmpty` cap may remain.

The user has not accepted this edge correction yet. The DitherTemporalAA
checker pattern during partial masked reveal is explicitly outside this fix.

## 2026-09-02 cap-positive / current-residual follow-up (in progress)

The new user PIE result **FAILS** the `5038cf82a79515cd64387e9ac5976201b6aec28b`
checkpoint: a legitimate historical cut can be hollow, and a stable small
stale-looking fragment remains above the final current cabinet. The previous
49/49 automation and GPU claims do not supersede this user result. After
`git fetch origin`, the branch, local HEAD, upstream, and remote were confirmed
at that same SHA. No branch change or history rewrite was performed.

Source audit findings below are hypotheses/incomplete evidence, not a claim
that either user fragment has been conclusively attributed:

- `UpdateRecordCap` loses the partial-discovery cut classification when a
  current record is sealed: the absent branch requires `InitialRemembered > 0`
  and `VerifiedEmpty > 0` on the cut side. A never-discovered neighbor has
  neither. It also consults coarse ownership suppression before generating
  candidate caps, rather than only subtracting newer geometry afterward.
- Old primitive projection clipping checks the midpoint of a emitted span,
  rather than clipping both span endpoints to the old transformed primitive.
  A diagnostic now checks submitted cap vertices against their source bounds.
- Correction to the earlier "after filtering" description: the actual source
  upload zeros history RGB **before** the bilinear sampler. The material
  migration script wires proxy opacity to `saturate(State.b)` and contains no
  final hard-ownership multiplication. The one-texel support guard is a CPU
  prefilter guard, not a final shader gate. Actual material/GPU sampling still
  needs inspection before choosing a correction.
- Moving Lab history, proxies, cap building, and exclusion contain no CVar
  mode branch; reset selects mode 2 and spatial-history-managed furniture uses
  fixed reveal. A new same-route mode-selector comparison is prepared. This
  must not be confused with the ordinary/manual room's mode-1 implementation.
  Runtime classification of the bug is still pending.

Uncommitted diagnostic work is restricted to
`DarkwellMovingPropLabRoom.h/.cpp`, `DarkwellSightWeaveAdapterTests.cpp`, and
this document. It adds cap lifecycle counters, an original-geometry vertex
check, and three targeted tests. It does not change cap generation decisions,
ownership behavior, D/V/R, frozen AA/timing/material parameters, or maps.
The permitted local `Darkwell.uproject` difference remains untouched.

Execution is currently gated on closing the existing company Editor/Standalone
session (PID 24268). Escape, Alt-F4, and the window close button did not close
it through computer-use; querying its command line also returned no value.
Permission-level mismatch is possible but not established. The user was asked
to stop PIE and close that editor without saving maps or the project file.
No unrelated process was stopped. The original visible session was preserved
at `Saved/PropGameplayLab/MovingMulti/CapResidual/UserSessionBeforeEdits.png`.

**This follow-up has run zero builds, zero automation tests, and zero new GPU
routes. No new commit has been created or pushed.** Diagnostic changes are not
yet a reliable checkpoint. Do not report READY_FOR_USER_CAP_AND_RESIDUAL_RETEST.

Resume by confirming the retained diff and that the old editor is closed;
then run the standard serial Development build and the new diagnostic tests
before changing rendering behavior. Preserve failing reports for checkpoint A.
After attribution, separate genuine cut candidates from ownership subtraction,
validate the final material sample, and obtain both positive-cap and
negative-residual D3D12 evidence before the final regression/push.

### Checkpoint A: resumed after the user closed UE

The user closed the old editor and the retained diagnostic work built
successfully (7/7 actions, 42.09 seconds; subsequent targeted diagnostic builds
also succeeded). No frozen runtime behavior was changed by this checkpoint.
`CapResidual_CheckpointA` ran 3 tests in 104.697 seconds: 1 clean success,
1 success with warnings, 1 failure. The failure is intentional reproduction:
74 genuine partial-discovery history boundaries were expected after sealing
epoch 2, but zero candidates/caps were generated. Both selectors really set
their CVar values and reproduce the same missing-cut count.

The earlier route sealed at roughly 53 degrees and did not reproduce the top
fragment. The real GPU route sealed later; a separate late-observation variant
now preserves the early route and tests approximately 112 degrees as well.
Do not interpret the earlier `CurrentOwnedResidualZero` success as evidence
against the user's failure.

The D3D12 route `GPU_Mode2_20260902_141653` captured both positive empty-cut and
negative current scenes; original images were opened. Actual backbuffer is
1526x549, captures 1920x1032. The top sliver remains visible despite overlap
diagnostics reporting zero. The subsequent attribution route
`GPU_Mode2_20260902_142321` identifies it as `STALE_SURFACE`, epoch 2, primitive
0, `SpatialMemory_Lab.InWorld.Rotate.Cabinet_Epoch2_0`, at approximately
X=-366.87..-347.13, Y=692.41..694.88, Z=0..145. These old cells have legal
coverage 1, D=1, V=0, R=1 and opacity=1, but no actual primitive occupies them.
The broad aggregate actor bounds incorrectly block empty verification in the
space next to the door/handle. This is distinct from ownership intrusion:
the sliver lies outside actual newer geometry, so a current-overlap-only
diagnostic correctly reports zero while missing the wrong occupancy decision.
It is not demonstrated to be TSR noise, a current cap, or a retired component.

The two user failures therefore have different immediate causes: loss of the
sealed partial-discovery cut classification, and false-positive aggregate
occupancy preventing legal empty evidence from reaching old history. Increasing
render contact tolerance addresses neither. Actual material graph inspection
also confirms the earlier prefilter-only ownership ordering; it still needs a
real final shader gate and GPU readback verification.

Preserved attribution-run failure: UE Python does not expose
`GameplayStatics.begin_deferred_actor_spawn_from_class`. The forensic close-up
helper was changed to temporarily position the existing camera only; player
pose and SightWeave authority are untouched. This failure occurred after the
world-space residual was recorded, before close-up capture. No missing shot is
counted as evidence. Both failure logs and all original images remain in Saved.

The late native route now reproduces both failures: 10 legally observed empty
samples remain falsely occupied; cap vertices outside their own original
primitive also reach 2. The Mode1/Mode2 comparison passes because both actual
selectors reproduce the same incorrect result (71 missing cut candidates,
2 protruding vertices, one visible historical cap). Classification for this
moving Lab is **BUG_ALSO_REPRODUCED_MODE1**; its Mode 1 selector shares the
Mode 2 moving presentation path, not the ordinary/manual Mode 1 semantics.
This checkpoint intentionally preserves the failing behavior for reproduction.

### Checkpoint B: separate candidate cuts and final presentation exclusion

Checkpoint A is `80354e03a806a5a1cf5cc81432a9de532e75c5ae` (pushed).
The correction restores sealed partial-discovery boundaries without treating
ownership as an empty/cut signal. Candidate caps are then split at original
primitive, ownership-grid and newer-geometry boundaries. Interval subtraction
retains disconnected legal pieces. The existing 0.05/0.051/0.001 cm contact,
clipping and precision constants are unchanged; midpoint classification now
uses the same closed set as segment clipping. Candidate vertices are also
clipped against their own original transformed primitive.

The old-space occupancy broad phase now requires an actual transformed
primitive intersection. Legal empty coverage next to the door/handle reaches
the existing historical `Advance` path instead of being blocked by aggregate
bounds. This does not clear by identity or by observing the new location.

The actual moving proxy material is a project-side derivative,
`M_MovingAccumulatedMemory`. Frozen manual materials are untouched. The RGB
history signal remains the existing 4x4 conservative/bilinear result. Binary
ownership is stored separately in A and loaded with `Texture.Load` at the
**final opacity multiplication**, after smooth B sampling. The former CPU
one-texel RGB-zeroing guard is removed. No D/V/R cell is written by this gate.

Restoring positive candidates exposed a second cap-specific residue: exact
OBB subtraction left tiny strips above/below the newer door/handle while the
old outer surface was already excluded by its conservative fine texel. The
near-camera image remained unchanged when only stale outer surfaces were
temporarily hidden for attribution: these strips were `STALE_CAP`, not TSR.
Cap clipping now also applies the **same retained-side fine-texel ownership
domain as the outer surface**, after candidate generation. It neither turns
ownership into a cap nor hides an entire unresolved epoch. Retirement waits
for both residual surface contribution and clipped cap geometry to be empty.
Historical authority records are not identity-cleared.

Scope limitation: this room retains its existing conservative XY presentation
ownership grid and yaw-only basic geometry. Interval-unit tests exercise
partial vertical clipping, but are not evidence for arbitrary stacked meshes,
pitch/roll, or a full 3D volumetric knowledge system. The new cap support gate
matches the existing surface domain; it does not add such a system.

Serial standard Development build `CheckpointB_SupportGridBuild.log` succeeded
(4/4 actions, 14.62 seconds). Targeted report
`Saved/AutomationReports/CapResidual_CheckpointB_SurfaceDomain/index.json`:
6 tests, 2 clean + 4 success with warnings, 0 failed, 0 not-run, 135.330 seconds.
These include real early/late rotation routes, four-view residual zero,
positive historical/VerifiedEmpty caps, partial interval subtraction,
coplanar/endpoints and multi-epoch 3D ownership. Final rendered historical
proxy/cap counts are zero after the late route's full legal recheck.

Intermediate failures remain under `Saved/PropGameplayLab/MovingMulti/CapResidual`:
`CheckpointB` and `CheckpointB_ClosedSet` each failed the residual case;
the latter had 2 clean + 2 warnings + 1 failure in 129.297 seconds.
Do not count those as successful verification. The first shader-readback
fixture also failed compilation (a float3 output was incorrectly packed as
a scalar); its all-zero result was rejected, not counted as a pass.
Corrected real-texture shader readback checked 131,072 pixels across two
runtime textures: 3,339 blocked-positive-history samples and 66,934 allowed
positive samples, zero errors. Final route/GPU and full regression are pending
at this checkpoint; this is not user acceptance or final readiness.

### Checkpoint C evidence ledger (2026-09-02)

The pushed runtime correction is
`66ba4f70d7533c5439f0d5477a28f148ffb16ddc`. Subsequent source changes add
diagnostics only (up to 32 surviving surface samples per record, including
world span, D/V/R, legal coverage, smooth signal, hard gate and component).
Tests retain the early and late routes and add later observations near the
GPU pose; no old failure sample or original assertion was removed.

Mode classification remains **BUG_ALSO_REPRODUCED_MODE1**, specifically for
the moving Lab selector. `DarkwellPropGameplayLab.cpp` selects fixed reveal
for `bSpatialHistoryManaged`, and `BindSpatialState` sets
`FixedRevealEnabled=1`. `DarkwellMovingPropLabRoom.cpp` shares history,
proxy geometry, cap builder, suppression and material binding between mode
selectors; the mode CVar is used for reset selection/HUD, not an independent
moving rendering branch. Even selector 0 is not an independent moving-room
control path. In contrast, `DarkwellManualStaleRoom::UpdateStaleCap` explicitly
rejects `Mode != 2`, and its accumulated material binding is mode-2-only.
Do not generalize this reproduction to the ordinary/manual mode-1 renderer.
Those frozen manual paths and assets are unchanged.

Final completed GPU routes use D3D12 / SM6 on the company RTX 4060. Original
screenshots were opened, including near views and consecutive frames:

| Selector | Saved directory suffix | Actual PNG count | Backbuffer | PNG including Editor UI | Shader samples |
| --- | --- | ---: | --- | --- | ---: |
| 2 | `GPU_Mode2_20260902_150751` | 126 | 1526x549 | 1920x1032 | 131,072 |
| 1 | `GPU_Mode1_20260902_150951` | 126 | 1526x549 | 1920x1032 | 131,072 |

Both complete Scene A and Scene B. Scene A full recheck has proxies=0,
caps=0, surface contact=0, cap contact=0 and filtered ownership leak=0;
four near camera angles and four player positions are retained. Scene B's
real offscreen A-to-B movement followed by legal empty scanning produces a
solid dark-gray cap, especially visible in `0123_B_cap_camera_west00000.png`
and `0124_B_cap_camera_northwest00000.png`. The original southeast angle has
the unchanged shelf in front; it alone was not sufficient visual evidence.
Temporary stale-surface hiding is a separately named attribution frame and
is always restored; it is not used as passing visual evidence.

The GPU routes use the same scripted controls, player/camera positions and
scan steps. They are **not frame-locked deterministic replay**: the sealed
intermediate yaws were 127.28 degrees (mode 2) and 127.69 (mode 1).
The native mode comparison uses identical fixed-step timing. No 1080p/1440p
performance claim is made for these Editor captures; AA, TSR, dither,
ShadowReplace and screen-percentage settings were not modified by this work.

GPU final-gate readback uses actual proxy texture inputs and the loaded
material's final HLSL expression. Mode 2: 3,413 blocked samples with positive
smooth history and 67,069 allowed positive samples. Mode 1: 3,488 and 67,085.
Across 262,144 readback samples, blocked output is exactly zero and allowed
history is preserved within half-float readback tolerance; zero errors.
This supplements CPU masks rather than assuming those masks prove GPU output.

Preserved additional failure: `CheckpointC_GPU_Mode2.log` /
`GPU_Mode2_20260902_150505` stopped at a deliberately stronger assertion,
with one historical proxy still flagged visible after full current coverage
(sealed yaw 127.95 degrees), caps=0, owned contacts=0 and filter leaks=0.
The subsequent expanded diagnostic route did not reproduce that surviving
proxy. This earlier run is not counted as successful GPU evidence, nor is
its unexplained retained resource erased from the record. Native neighboring
pose checks are being run before deciding the final handoff status.

All evidence paths above are relative to
`Saved/PropGameplayLab/MovingMulti/CapResidual/`; original failures, generated
images and automation reports remain untracked. Material ownership is still
the existing conservative XY domain, not arbitrary volumetric geometry.

### Handoff result

**PARTIAL — READY_FOR_USER_CAP_AND_RESIDUAL_RETEST**

This means ready for a targeted user retest, not that every possible pose has
been proven clean. User acceptance is still pending. In particular the
127.95-degree preliminary GPU resource-count failure above remains a recorded
limitation: its exact surviving proxy was not attributed before that run
stopped. Do not describe it as a proven false positive or silently replace it
with the two successful GPU runs.

Final full regression: `CapResidual_CheckpointC_Full/index.json`,
54/54 success (33 clean, 21 with warnings), 0 failed, 0 not-run,
656.834 seconds. Command:
`Automation RunTests Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1`.
This includes all MovingRules, both mode selectors, positive cap controls,
fixed geometry, symmetric gray-cap material, hidden shadows, cycles, normal
Mode 0/1/manual behavior, FogVisual and M6P1.

The same residual test was then extended without dropping either original
pose and rerun separately in `CapResidual_CheckpointC_Boundary/index.json`:
1/1 clean success, 0 warnings/failed/not-run, 149.845 seconds. All four sealed
poses (112.6198, 122.5203, 126.8698, 129.4285 degrees) finish with proxies=0,
caps=0 and zero owned contact/filter leakage, including four-view checks.
These are deterministic neighboring-pose checks, not an assertion that the
earlier exact 127.95 GPU event was reproduced. No further timing retries or
tolerance changes were made after this bounded check.

Standard target build before the full suite: `CheckpointC_FinalBuild.log`,
5/5 actions, 22.63 seconds, succeeded. The last standard build after the test
extension: `CheckpointC_BoundaryTestsBuild.log`, 4/4 actions, 18.41 seconds,
succeeded. Both use `Scripts/BuildEditor.ps1 -Configuration Development
-EngineRoot D:\UE_5.8`; these are normal incremental full-target builds,
not Live Coding. The company engine identifies itself as
5.8.2-56702186 (despite the older 5.8.1 guidance); no engine was upgraded.

Retained warnings: MSVC 14.51 versus UE's preferred 14.50, existing UE header
deprecations, connectivity probe timeouts, expected duplicate-ID rejection
and capacity-limit test warnings. Fatal/assert/ensure/device-crash scan is
zero for the final full suite and both completed GPU logs. Each of those
logs also contains 13 `LogAutomationTest: Error: Condition failed` lines
during engine initialization, before the requested tests/PIE; they are
explicitly retained rather than reporting a literally error-free log.
Full-suite automation report failures are zero. Failed shader-probe and
earlier residual logs are separate and are not included in the passing count.

No source changes were made to D/V/R, SightWeave, normal/manual presentation,
production defaults, original geometry or frozen material assets. No maps
were saved. `Darkwell.uproject` was never staged or edited: its actual
pre-existing local diff is a missing final newline, not a changed GUID.
Screenshots/video/Saved/Binaries/Intermediate/DDC/AutomationReports stay out
of Git. The new moving-only material was created through Editor Python and
pushed via Git LFS in checkpoint B.

User retest (no console): open `L_ProjectFogPropGameplayLab`, click Play,
use the labeled `VISIBLE ROTATE` F mechanism. Fully observe the 0-degree
cabinet, start rotation and turn away during the one-second delay, briefly
observe an edge during the later part of rotation, turn away again, then
slowly reacquire the completed 180-degree cabinet. Inspect top, handle,
corners and both sides. Also stop with a genuinely partial historical cut:
the remaining gray body must have a dark-gray cap. For the independent empty
cut, use `OFFSCREEN A -> B`, walk behind the wall onto the orange pressure
plate, and return to scan old A. Use only the labeled reset mechanism between
fresh runs. Retest the later-intermediate-angle case especially; reject any
top fragment, hollow cut or duplicated surface. Existing masked/dither
checker visuals during partial live reveal remain outside this task.

Final validation/code checkpoint C:
`a1fd9349ebdf416eeedfb5dc6a2c082fc2cc8739` (pushed). A following documentation-only
closure records these verified Git and renderer details; it changes no runtime.
Local HEAD/upstream/origin matched at C, `git diff --check` passed,
`git lfs fsck` passed, and `git lfs push --dry-run origin HEAD` listed no upload.
`git fsck --no-reflogs` reported dangling objects (34 trees, 4 blobs, 1 commit),
not missing/corrupt objects; they were not deleted or garbage-collected.

Read-only renderer queries during handoff report `r.AntiAliasingMethod=4`
(TSR), `r.ScreenPercentage=0` and secondary percentage=0, all constructor
defaults. Thus this round preserves the existing automatic percentage policy;
it must not be described as forced-100%-screen-percentage evidence.
The Lab was reopened without a running route or PIE, for the user to click
Play. The small Python handoff probe was closed and replaced with a plain
Editor session, so no persistent evidence script remains in the user session.

## 2026-09-02: mixed-cell HistoryGridV2 work (supersedes prior retest readiness)

Starting branch: `codex/darkwell-prop-memory-gameplay-lab`.
Starting HEAD/upstream/origin after fetch:
`8073b8d1e09156f5968f5d5626ad9cf5c491a96e`.
User manual PIE FAILED again. Video `2026-09-02 15-44-20.mp4` begins with
an already-sealed partial gray pose and quickly reacquires the final cabinet.
Read-only extraction retained 44 native consecutive transition frames under
`Saved/PropGameplayLab/MovingMulti/CapResidual/UserFeedback_154420_Review`.
The surviving epoch was approximately 157.66 degrees; actual was 180 degrees.
Its existing proxy remained alive, rather than a new stale epoch being created.
Zero aggregate render-contact counters did not prove the absence of visible
residue. The older slow-scan evidence is retained, but is not acceptance of
this fast-reacquisition case.

Checkpoint A adds `FDarkwellHistoryGridV2` to each spatial observation record.
Its fixed dimensions are the old grid dimensions multiplied by four on each
axis, matching the existing presentation texels. Native evidence tags are
NeverObserved, Unresolved, VerifiedEmpty, SupersededByNewerEvidence.
Superseded is monotonic ownership, never a write to an empty-space fact.
The old per-record D/V/R is unchanged and still supplies ALL rendering at A.
V2 samples the same conservative SightWeave queries on the finer grid and
records independent legal-empty dwell/fade and newer observed ownership.
`GetFineHistoryTelemetry` exposes per-epoch counts, mixed original cells and
fine empty samples which the old cell still retains.

The frozen implementation has EnterSeconds=.20, ExitSeconds=.18,
EmptyConfirmationSeconds=.10 and EmptyFadeSeconds=.20. V2 reuses these
existing constants rather than reinterpreting .18 as an empty-fade duration.
Mode 1 and Mode 2 selectors in the MOVING room share this history model;
this does not change normal/manual-room Mode 0/1 contracts.
No source geometry, materials, assets, maps, SightWeave interfaces or temporal
constants are changed at A. The local uproject difference at task start is
a missing final newline (not an EngineAssociation change); preserve it exactly.

Evidence for this task is under `Saved/PropGameplayLab/MovingMulti/HistoryGridV2`;
none of it is committed. Stage results and final recovery instructions follow.

A validation: standard Editor Development final build succeeded (11 actions,
32.27 seconds). `HistoryGridV2_A_Final/index.json`: 2/2 Success, 0 warnings,
0 failures, 109.2771 seconds. The native route sealed approximately 151.22
degrees, not the user's exact 157.66-degree video pose. Its fast and slow
final V2 counts matched exactly: epoch 2 had never=6391, unresolved=0,
empty=30735, superseded=24890. This particular legacy route also retired;
it is NOT claimed to reproduce the user's persistent fragment. The pure
mixed-cell test independently proves all three evidence states in one cell.
An initial diagnostic-only run exposed premature diagnostic retirement;
the final run keeps V2 updating after legacy rendering retires and restricts
seeded knowledge to the original primitive footprint. Evidence is retained.
Current stage status: PARTIAL — MIXED_CELL_HISTORY_WORK_IN_PROGRESS.

A pushed: `70f4d8acde69c78d052416222f7500b3009f7f76`.
B switches historical surface texture RGB/ownership A to the fine record.
The frozen 4x4 envelope is retained at sealing; per-sample opacity now follows
its own evidence. The existing moving-only material still bilinearly samples
RGB and then multiplies by unfiltered A. Superseded and fully erased samples
have A=0; no post-gate smoothing is introduced. Coarse V cannot release a
fine record that still owns unresolved knowledge. Superseded is not relabeled
as verified empty to reclaim records. Cap generation remains coarse at B;
this is an intermediate buildable checkpoint, not the final GPU retest build.
B standard build: succeeded, 30.58 seconds. `HistoryGridV2_B/index.json`:
3/3 Success, 0 warnings/failures, 112.4748 seconds. The earlier route name/log
label `ParallelLateReacquire`/`PARALLEL_ONLY` is retained in this intermediate
test, but surface output at B is V2, not legacy. Fast/slow final counts agree.

B pushed: `3a57861ea2b464bca96bb7c69f01edffae44b192`.
C replaces historical cap candidates with fine-state boundaries. A retained
Unresolved sample can close against NeverObserved or VerifiedEmpty; it cannot
close against Superseded. Original transformed primitive clipping, newer
geometry subtraction and render-contact checks remain final safety guards.
Current/PRESENT cap generation remains on the frozen path. No epsilon,
geometry, shadow, material or .20/.18 constant changes are involved.
Fine unresolved samples and fading verified-empty samples prevent resource
retirement; only after both those and legal cap triangles are absent are the
proxy, cap, texture references and MIDs released. Authoritative records that
are only ownership-resolved remain distinguishable from verified-empty
records. The existing 64-record-per-identity capacity refuses new records
rather than pretending they were erased by the player. This is a Lab bound,
not a production memory optimization or a new global eviction policy.

C validation: standard Editor Development succeeded (10 actions, 22.40 sec).
`HistoryGridV2_C/index.json`: 6/6 Success (4 clean, 2 connectivity-probe
timeout warnings), 0 failed, 341.2012 sec. Actual late sealed poses were
120.752, 128.580, 144.648 and 156.716 degrees. All fast terminal proxy/cap
counts were zero. The 156.716-degree slow route matched fast terminal state
counts and also retired its proxy/cap. Positive fine cap and no-cap-for-
superseded tests passed. At those four poses, respectively 3/241/91/254 fine
empty samples were still inside coarse cells whose old V had not confirmed
empty. These are grid diagnostics, not screen-pixel counts. They establish
the coarse/fine evidence mismatch without changing thresholds or angles in
runtime logic. D3D12 and the full regression are still pending after C.

C pushed: `490a0a6524d75f0842e2dece492ca52644ed9298`.
D diagnostics add a deterministic ordered fine-state hash (state, empty fact,
initial discovery, opacity and frozen AA envelope), so equal state counts
alone cannot satisfy fast/slow comparison. Hash output uses quantized display
values and is diagnostic only. The HUD identifies HISTORY GRID V2.
D standard Editor Development build succeeded (10 actions, 20.62 sec).

Initial D3D12/SM6 Mode 2 run:
`HistoryGridV2/GPU_Mode2_20260902_170720` contains 94 actual Shot captures.
Its early `checks.json` directory count is 95 because one review contact
sheet was created during capture; 94 is the original-frame count. Later
driver counting explicitly matches the numbered capture filenames.
Fast/slow final hashes matched, both with zero historical proxies/caps.
The actual intermediate pose was about 154 degrees. Independent GPU shader
readback: 131072 samples, 9050 positive blocked controls, 59222 positive
allowed controls, 0 failures. Agent opened the 30 consecutive fast-entry
frames, final four-direction closeups and 12 slow stationary frames.
The earlier masked/dither appearance during entry is visible and retained;
this task does not claim to remove the frozen reveal transition.
Actual game backbuffer is 1526x549; Editor screenshots are 1920x1032.
Rendering uses D3D12, SM6, TSR=4, ScreenPercentage=100. Fixed simulation
step is 1/60 for matching routes; this is NOT measured 60fps performance.
Mode 1 and the extended independent empty-cap positive control follow.

Extended GPU runs `GPU_Mode1_20260902_171102` and
`GPU_Mode2_20260902_171514`: 106 original captures each, both completed
normally. Fast/slow terminal fine hashes match within each mode AND across
the two modes (epoch 1: 973158155534630339; epoch 2: 7835464520961056169).
Both modes use the same moving-room historical model. Each shader readback
again checks 131072 samples with 9050 positive blocked / 59222 positive
allowed controls and zero gate failures. Every recorded ownership/contact
counter is zero. The slow scan contains original coarse cells with all three
fine states simultaneously (for example yaw 147/141/138/135/132).
The agent opened both modes' consecutive target crops, final south/east/
north/west closeups and the independent offscreen A-to-B empty-cut views.
The final cabinet has no remaining old surface/cap; the separate legal
empty-cut control visibly retains a deep-gray cap, so this is not global
cap suppression. All original images remain under Saved and outside Git.

Capture limitation found by opening the images, despite passing counters:
Shot is deferred. The `full_final` image was contaminated by the immediately
following forensic camera move; the last stationary image was contaminated
by the following reset/teleport. These frames are retained and are NOT
claimed as stationary visual passes. Clear four-angle views and preceding
stationary frames support the terminal result. The new driver now inserts
capture-drain waits before these discontinuities and settles the restored
camera before a stationary burst. This changes only evidence scheduling,
not source code, temporal rules, sampling thresholds or the rotation path.
The corrected capture run and full regression results are recorded below.

Memory/coverage limits: a fine sample occupies 32 bytes in this Win64 build;
the two tested epochs use 1044480 and 1914880 bytes of fine state, excluding
textures/MIDs and the legacy diagnostic grid. Fine history is a world-XY
grid aligned to the existing 4x4 presentation footprint, with the existing
3D primitive/OBB guards retained; it is not a general stacked 3D voxel model.
No production performance or memory-budget acceptance is claimed.

Recovery / manual retest for this checkpoint (no console interaction):

```powershell
cd D:\UE_projects\LastLight
git fetch origin
git switch codex/darkwell-prop-memory-gameplay-lab
git pull --ff-only
git rev-parse HEAD
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'D:\UE_projects\LastLight\Darkwell.uproject' `
  '/Game/Maps/L_ProjectFogPropGameplayLab' -d3d12 -sm6 -PropLabMovingControls
```

1. Click Play; confirm HISTORY GRID V2 / MODE 2 / SpatialEvidenceOnly / ENEMY 0.
2. Use the labeled local RESET control only to start an independent trial.
   Walk to VISIBLE ROTATE and fully observe its original 0-degree cabinet.
3. Press F at the mechanism, then turn away during its one-second wait.
   During the middle/late rotation briefly observe only a corner/edge, then
   turn away again and allow the four-second rotation to finish hidden.
4. Start from the surviving gray fragment, as in the user's failed video.
   Quickly sweep the final cabinet into legal view. Check both the cabinet
   footprint and surrounding floor for old surface/cap fragments and overlap.
   Turn away and back; erased portions must not reappear.
5. Explicitly reset and repeat with a slow sweep. Also sample partial poses
   near 120/128/145/157 degrees. Fully unobserved old space must remain until
   legal evidence, while current-occupied old samples must not generate caps.
6. Check OFFSCREEN A TO B (find B first), A TO B TO C and MULTI PROP. Seeing
   a new location must not erase disjoint old locations through StableID.
   Use VISIBLE TRANSLATE as the previously user-passed regression control.

The GPU driver uses the existing evidence mechanism entry, not physical
keyboard F presses. Native automation covers Pawn focus/trace/TryInteract.
Neither is represented as new user manual acceptance. The next user action
after a READY handoff is manual PIE retest, not additional implementation.

D diagnostic checkpoint pushed:
`fa05cbc4d28fccdb9cd9ace93e6296edd50d0968`. This is the final runtime
implementation SHA unless a later entry explicitly supersedes it. The final
evidence/script closure commit follows it and does not change runtime files.
The old coarse history still advances for comparison/legacy diagnostics;
it no longer supplies moving historical surface or cap output. Current
live D/V/R and unrelated Lab/manual-room rendering remain on their prior
paths. StableID groups records but never erases a record by identity.

Source scope relative to 8073b8d: the new fine model and its native tests,
moving-room integration/diagnostics, spatial-record storage/lifecycle and
adapter route assertions. The new GPU driver and this document complete
the evidence scope. No Plugins, Config, maps, binary assets, materials or
frozen `DarkwellSpatialPropMemory` files differ from the starting commit.
The original uproject blob remains `e0e43e8ffd1a4b111a90feb1f85168b926363099`;
the only allowed local difference is its missing final newline.

Full D regression on the final runtime build:
`Saved/AutomationReports/HistoryGridV2_D_Full/index.json` reports 60/60
Success: 37 clean, 23 with warnings, 0 failed, 0 not run, 1379.841064 sec.
UnrealEditor-Cmd exited normally with code 0. This includes all six new V2
tests, Mode 1/2 shared-history comparison, visible translation/rotation,
last-seen pose, fixed stale-epoch stability, offscreen A/B/C spatial evidence,
multi-prop isolation, original manual-room cap/AA contracts and SightWeave
adapter authority/lifecycle regression. No test or assertion was removed.

```powershell
.\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot 'D:\UE_5.8'
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\UE_projects\LastLight\Darkwell.uproject' -unattended -nop4 -nosplash -NullRHI `
  '-ExecCmds=Automation RunTests Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportExportPath=D:\UE_projects\LastLight\Saved\AutomationReports\HistoryGridV2_D_Full'
```

Severe scan for the full suite: 0 fatal/assert/ensure/device-loss/unhandled
exception signatures. There are 13 existing engine-initialization
`LogAutomationTest: Error: Condition failed` lines before test execution;
these are retained and this is NOT a claim of an error-free log. There are
no other Error lines. The report contains 24 warning entries: 21 HTTP
connectivity-probe timeouts, and three intentional duplicate-ID/capacity
rejection warnings. Builds retain UE header API-deprecation C4996 warnings.
All A/B/C/D build/test logs and earlier GPU runs remain under the evidence
root. Git fsck also reports pre-existing dangling objects; they are retained,
not pruned. None of these are silently converted into user acceptance.

Final capture-drain verification:

| Run | Original PNGs | Fast/slow terminal history | Final proxy/cap | Positive empty cap |
| --- | ---: | --- | --- | --- |
| GPU_Mode1_20260902_174710 | 106 | identical | 0 / 0 | present |
| GPU_Mode2_20260902_175212 | 106 | identical | 0 / 0 | present |

Cross-mode ordered fine-state hashes also match. Each run's GPU material
probe checks 131072 samples, 9050 positive blocked and 59222 positive allowed
controls, with 0 failures. Every sampled contact/filter-leak counter is zero.
Both editor processes exited normally. Both final logs have 0 severe
signatures, the same 13 startup condition errors, and no other Error lines.
The agent opened all 30 consecutive fast-entry target crops and all 12+12
stationary target crops in EACH mode, including both last frames after the
capture-drain fix. No whole-object blink or persistent old fragment was seen
in these sampled sequences. The Mode 2 slow-sweep sheet (22 views) and final
positive deep-gray empty-cap image were also opened. Earlier four-direction
closeups remain available. Frozen reveal dithering is still visible during
entry; no claim of new AA behavior is made.

All five GPU runs retain 518 original screenshots total (94+106+106+106+106),
plus separate review sheets. Final comparison evidence is the last 106+106,
not the earlier contaminated terminal frames. Every run uses D3D12/SM6,
normal TSR, 100% screen percentage, actual 1526x549 game backbuffer inside
1920x1032 editor screenshots. Fixed 1/60 simulation ticks and short adjacent
bursts are functional evidence, not a measured 60fps/10-second GPU benchmark.
The unchanged native stale-stability test supplies the longer state checks.

Final implementation status:
`PARTIAL — READY_FOR_USER_MIXED_CELL_HISTORY_RETEST`.
HistoryGridV2 supplies moving historical surface and cap output; coarse
history remains diagnostic. All requested new fine-state regressions and
the original relevant suite pass. User manual PIE is still pending and has
priority over this report. No formal-map integration or new Mode decision
is included. Cost of fine queries, 64-record fail-closed capacity, world-XY
granularity and arbitrary real-world input paths remain limitations requiring
later explicit work/validation. Tomorrow: perform the manual route above
before any further implementation.

Unattended handoff follows the user's final shutdown instruction, superseding
older requests to leave Play ready. After the closing text/script commit is
pushed, recheck local/upstream/remote, LFS and the untouched uproject; only
then schedule the authorized 60-second shutdown. Do not shut down with
unsynced valuable work. This document and script are the only pending task
files after runtime checkpoint fa05cbc; generated evidence is never staged.

Final validated code + evidence-driver SHA (D):
`6990b6cfc840a0379d1ea956b2dc472f7a708cf8` — pushed and verified equal to
upstream and origin. All runtime files are identical to the built/tested
`fa05cbc4d28fccdb9cd9ace93e6296edd50d0968`; the D closure adds only the
validated Python driver and documentation. This final Git-status appendix
is a documentation-only descendant. Its exact delivery SHA is the branch
HEAD returned by the recovery command and is also recorded in the final
Codex handoff, rather than a self-referential commit hash inside its own tree.

Git/LFS closure at D: status contains only the preserved uproject newline
difference; no staged files, no pending push, LFS status has no upload objects,
LFS push dry-run is empty, `git lfs fsck` passes. `git diff --check` passes.
`git fsck --no-reflogs` exits 0 with the existing dangling objects retained
(`Saved/PropGameplayLab/MovingMulti/HistoryGridV2/Final_git_fsck.txt`). No
new binary assets, captures, videos or generated directories are committed.
No UnrealEditor, UnrealEditor-Cmd, UBT, dotnet, ShaderCompileWorker,
LiveCodingConsole or Python evidence process remains at the final check.
After this appendix is pushed, repeat the SHA/LFS/worktree checks immediately
before scheduling shutdown; any mismatch blocks shutdown.

## 2026-09-02 home HistoryGridV2 runtime-performance closure

This section supersedes the preceding mixed-cell handoff status, but not its
correctness rules. It uses evidence regenerated on the home machine under
`D:\UE_pro\Darkwell`; no company `Saved`, screenshot, report, binary or chat
state was treated as present. Older company evidence cited above remains
historical context only. The immutable correctness baseline is
`404a5820739638f1097eaae0aa7fba19733298c3`, and remote branch
`stable/moving-history-grid-v2-20260902` still points to that commit. All work
was made on `codex/darkwell-prop-memory-gameplay-lab`.

The home baseline editor was built successfully with:

```powershell
.\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot 'D:\UE_5.8'
```

### Checkpoint A: measurement before optimization

Checkpoint `9d75b70` (`test: diagnose history grid runtime cost and multi epoch
residue`) added counters and deterministic fixtures without changing the
HistoryGridV2 product rule. The same home executable path and fixture measured
0/1/2/4/8 sealed epochs over fixed windows. The baseline report is
`Saved/PropGameplayLab/MovingMulti/HistoryRuntime/CheckpointA_Percentiles_Report`.
Times below are the instrumented MovingPropLab game-thread update, not a claim
about rendered end-to-end frame rate.

| Epochs | Resident samples / bytes | Fine scans/frame | Coverage queries/frame | Occupancy tests/frame | GT avg / p95 / p99 | Texture uploads/s | Cap rebuilds/s |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 0 / 0 | 0 | 32,522 | 0 | 12.655 / 13.622 / 15.425 ms | 13.0 | 1.0 |
| 1 | 32,640 / 1,044,480 | 32,640 | 102,354 | 34,680 | 43.513 / 45.927 / 51.260 ms | 6.5 | 0.5 |
| 2 | 84,096 / 2,691,072 | 84,096 | 212,275 | 89,352 | 109.312 / 112.402 / 117.008 ms | 13.5 | 2.5 |
| 4 | 213,888 / 6,844,416 | 213,888 | 489,362 | 227,256 | 412.449 / 426.211 / 432.133 ms | 14.5 | 7.5 |
| 8 | 415,712 / 13,302,784 | 415,712 | 920,516 | 441,694 | 1,486.886 / 1,529.140 / 1,547.373 ms | 42.5 | 17.5 |

At eight epochs the old steady update additionally performed about 6.606
million primitive geometry intersection tests per frame. `AdvanceFineHistory`
alone averaged 273.131 ms and contribution refresh averaged 584.280 ms; the
whole tracked update averaged 1,486.846 ms. This proves the long-session stall
was principally an algorithmic `all epochs x all fine samples x every frame`
cost, amplified by grid-sample-to-all-geometry testing and full diagnostics.
Presentation calls also remained active while idle. Resident fine bytes grew
with intentionally resident evidence, but the lifecycle tests below did not
show an unbounded resource leak.

The deterministic composite route reported:

```text
MULTI_EPOCH_LIVE_DIAGNOSIS   A=0 B=0 C=0 D=0 OTHER=0 surviving_visible=0
MULTI_EPOCH_MEMORY_DIAGNOSIS A=0 B=0 C=0 D=0 OTHER=0 surviving_visible=0
```

Therefore the home fixture did **not reproduce** the user's compound cross/
wing-shaped survivor. Zero survivors is not proof that the reported manual
failure cannot occur, and it does not justify calling the symptom legal class
B. No multi-epoch knowledge or presentation policy was changed. In particular,
StableID still does not clear old epochs, legal Unresolved knowledge remains,
and the SpatialEvidenceOnly contract is unchanged. A/C/D remain implementation
bug categories if a later manual capture produces them; class B would require a
separate user presentation-policy decision.

### Checkpoint B/E: revision, event, and dirty-region update

Checkpoint `f126011` (`perf: make moving prop history revision driven`) replaced
the idle full traversal. Checkpoint `7f439f0` (`test: close history grid runtime
performance regression`) added the final percentile and lifecycle acceptance
coverage. The implementation keeps the 4x4 fine model and all four durable
states (`NeverObserved`, `Unresolved`, `VerifiedEmpty`, and
`SupersededByNewerEvidence`):

- Coverage caches are keyed by authority, draw, player-transform, and grid
  revisions. An unchanged coverage input does not query the whole grid again.
- Changed current/newer primitive bounds are rasterized into intersecting fine
  regions. The direction is geometry-to-grid, not every-grid-sample-to-all-
  geometry.
- Each record carries coverage, geometry/ownership, presentation, cap-topology,
  and active-fade state. `VerifiedEmpty` opacity advances through an active
  sample list; `SupersededByNewerEvidence` is a stable terminal presentation
  state.
- Texture work occurs only for presentation-dirty records and remains hash
  gated. Cap topology is rebuilt only when topology changes. Unchanged records
  produce no idle upload or DynamicMesh rebuild.
- Heavy contribution diagnostics reuse cached results and rotation logging is
  change-triggered/throttled. The diagnostic ability remains available.
- Retired visual records release proxy/cap/texture/MID presentation resources,
  while their evidence authority can still receive revisions. This preserves
  the baseline fast/slow terminal result instead of freezing a retired record.

No coarse fallback, precision reduction, periodic skip-frame rule, cap disable,
depth offset, TSR/material parameter change, StableID global clear, or automatic
Unresolved deletion was introduced. Texture dirty rectangles were not required
for this pass because the stronger idle gate already reduces unchanged uploads
to zero.

The final post-refactor steady report is
`Saved/PropGameplayLab/MovingMulti/HistoryRuntime/CheckpointB_Percentiles_Report`:

| Epochs | Resident samples / bytes | Fine scans/frame | Coverage / occupancy/frame | GT avg / p95 / p99 | Texture uploads/s | Cap rebuilds/s |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 0 / 0 | 0 | 0 / 0 | 0.082 / 0.109 / 0.131 ms | 0 | 0 |
| 1 | 32,640 / 1,044,480 | 0 | 0 / 0 | 0.078 / 0.088 / 0.095 ms | 0 | 0 |
| 2 | 84,096 / 2,691,072 | 0 | 0 / 0 | 0.080 / 0.100 / 0.124 ms | 0 | 0 |
| 4 | 213,888 / 6,844,416 | 0 | 0 / 0 | 0.084 / 0.107 / 0.141 ms | 0 | 0 |
| 8 | 415,712 / 13,302,784 | 0 | 0 / 0 | 0.086 / 0.092 / 0.139 ms | 0 | 0 |

Initialization and real revision events intentionally still perform bounded
work; these steady numbers apply only after the fixture is unchanged. At eight
epochs the resident fine-state allocation remains 13,302,784 bytes, while all
fine scans, coverage queries, occupancy tests, geometry tests, ownership tests,
texture/cap calls, uploads, rebuilds, refresh work and rotation-log work are
zero in the steady measurement window. Idle cost no longer scales by scanning
resident samples.

### Home lifecycle and correctness evidence

- `CheckpointB_DirtyRegionFix2_Report`: one moved primitive examined 11,009 of
  32,640 resident samples, with 13,049 occupancy and 11,150 geometry tests; a
  following unchanged update returned to zero heavy work.
- `CheckpointB_ResetLifetimeFix_Report`: 50 reset/rotate/reacquire/reset cycles
  passed. Periodic GC left UObject count 65,731 to 66,011 (+280, bounded by the
  fixture threshold), with stable resource ledger proxy 4 / cap 6 / texture 6 /
  MID 12 rather than cycle-over-cycle growth.
- `CheckpointB_RuntimeAcceptance_Report`: the accepted idle and soak entries
  passed (earlier dirty/reset entries in that combined exploratory report were
  superseded by the two final reports above). Eight epochs were stationary for
  600 frames with zero heavy counters and 84.422 microseconds/update. The
  18,000-frame, five-minute-equivalent soak averaged 82.321 microseconds/update;
  working set changed 3,073,409,024 to 3,073,802,240 bytes (+393,216 bytes, or
  0.375 MiB), UObject count stayed 68,049, and resources stayed proxy 8 / cap 10
  / texture 10 / MID 24.
- `CheckpointB_HistoryGridV2_Final_Report`: all four selected HistoryGridV2
  correctness groups passed, including 120/128/145/157 late poses, mixed cells,
  parallel fast/slow reacquire, positive historical cap/no superseded cap,
  ownership/fade, and exact fast/slow evidence equality.
- `Final_Full_NullRHI_Report`: the full filter
  `Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1` completed 66/66
  (39 clean plus 27 with warnings), 0 failed, 0 not run, in 859.948 seconds.
  The log has zero fatal/assert/ensure/device-loss/unhandled signatures. The 27
  warning-bearing tests contain network/HTTP/EOS shutdown warnings and existing
  intentional rejection diagnostics; they are not hidden as clean tests.

GPU memory was not added: there was no easy, reliable per-process counter in
the existing harness, and the task explicitly preferred skipping it over
spending time on a questionable measurement. Working set, UObject count, fine
bytes, spatial records, and proxy/cap/texture/MID ledgers remain available.

### Final home D3D12/SM6 evidence and exit correction

The accepted GPU run is:

```text
Saved/PropGameplayLab/MovingMulti/HistoryGridV2/GPU_Mode2_20260902_230736
Saved/PropGameplayLab/MovingMulti/HistoryRuntime/Final_D3D12_Mode2_CompleteLedger.log
```

It ran the in-world moving-control route with fixed 1/60 simulation ticks on
NVIDIA GeForce RTX 2070 SUPER, D3D12 and SM6.7. The evidence ledger contains
106 rows and 106 original PNGs. Fast and slow terminal states both have full
legal coverage, three epochs, zero proxy/cap, zero surface contact, zero cap
contact and zero ownership-filter leak. Their ordered fine histories are exact
matches (epoch-1 hash 973158155534630339; epoch-2 hash
7835464520961056169). The independent empty-cut positive control is present.
The material readback sampled 131,072 pixels: 9,050 positive blocked, 59,222
positive allowed, and 0 failures. Representative partial-cap, terminal,
stationary, four-angle and independent-cap PNGs were opened on the home machine;
no terminal stale surface/cap leakage was seen, while the legal positive cap
remained.

An earlier otherwise-passing GPU attempt exposed a separate evidence-driver
shutdown defect: after UE logged a complete exit, Windows showed the attached
`0xc0000005` application-error dialog and recorded the same `ntdll.dll` offset
plus FaultTolerantHeap. The driver had issued `QUIT_EDITOR` before unregistering
its Slate Python tick. It now drains deferred screenshots and PIE teardown,
unregisters the callback, and only then requests editor exit. Two subsequent
runs exited with process code 0, complete `LogExit: Exiting` / closed logs, and
no new Application Error or FaultTolerantHeap event in either run window. The
final run also asserts 106 ledger rows equal 106 landed PNGs. The crashing run
is retained as rejected diagnostic evidence and is not represented as passed.

### Required next user retest

```powershell
cd D:\UE_pro\Darkwell
git fetch origin
git switch codex/darkwell-prop-memory-gameplay-lab
git pull --ff-only
.\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot 'D:\UE_5.8'
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'D:\UE_pro\Darkwell\Darkwell.uproject' `
  '/Game/Maps/L_ProjectFogPropGameplayLab' -d3d12 -sm6 -PropLabMovingControls
```

Confirm the HUD says `HISTORY GRID V2`, `MODE 2`, `SpatialEvidenceOnly`, and
`ENEMY 0`. Run the exact manual compound route: fully remember 0 degrees,
start rotation, look away, briefly see only a middle-angle fragment, look away
through hidden completion, legally reacquire the final 180-degree cabinet,
look away until the final cabinet itself is gray memory, then reacquire again.
Check for a cross/wing survivor both outside and inside the final outline. Also
repeat fast and slow reacquire, sample 120/128/145/157 degrees, confirm the
independent positive cap, and spot-check OFFSCREEN A-to-B, A-to-B-to-C, VISIBLE
TRANSLATE and MULTI PROP. During a several-minute idle period, check that PIE
and the Windows desktop remain responsive. A new survivor needs a screenshot
and its nearby `MULTI_EPOCH_*_DIAGNOSIS` log so A/B/C/D can be decided from the
actual manual path.

No user acceptance has been inferred from automation. Final implementation
status for this checkpoint:

`PARTIAL — READY_FOR_USER_HISTORY_RUNTIME_RETEST`

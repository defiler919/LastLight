# DARKWELL furniture memory gameplay laboratory

Work in progress. Final readiness requires the dynamic matrix and PIE review below.
Baseline: `0d5e6cba7bb9ed4c2c605d7e9cf4281b38e902e1`, accepted gray/live presentation.
Branch: `codex/darkwell-prop-memory-gameplay-lab`.

Independent map: `/Game/Maps/L_ProjectFogPropGameplayLab`. Neither production map is edited.
The existing DARKWELL adapter and continuous world coverage are reused. No plugin changes,
screen composite, stencil, pixel clipping or black Unknown presentation are introduced.

The map contains 25 StableID furniture actors: 8+4 kitchen cabinets, two continuous counters,
a refrigerator, tall cabinet, island, open shelf, three box sizes, movable cabinet,
two identical-looking cabinets with different IDs, and a replacement subject. Native
structure provides a narrow doorway, corner, half-height wall and rear cabinet occlusion.
Navigation bounds cover the whole room. The laboratory uses a fixed oblique player camera.

Console commands (Development/Editor, exact laboratory map only):

```
Darkwell.PropLab help
Darkwell.PropLab reset
Darkwell.PropLab fridge
Darkwell.PropLab cabinet
Darkwell.PropLab destroy
Darkwell.PropLab replace
Darkwell.PropLab swap
Darkwell.PropLab torch
Darkwell.PropLab lantern
Darkwell.PropLab dark
Darkwell.PropLab mode 0
Darkwell.PropLab policy 0
Darkwell.PropLab route 0
```

Presentation CVar: `r.Darkwell.ProjectFogVisual.PropPresentationMode` (default 0).

- 0 AcceptedWholeObject: original CPU maximum sample enter/exit thresholds .50/.25;
  confirmed current source is fully lit. No change to this threshold behavior.
- 1 SurfaceSweepHard: unchanged known geometry remains fully present even below the
  object identity threshold; per-pixel world LiveCoverage chooses live material versus
  gray. This grants neither identity Live nor LiveOnly effects. New transforms or
  appearance revisions still require identity confirmation; unseen relocation displays
  only the old snapshot, never the hidden new transform.
- 2 SurfaceSweepSoft: same identity gate and raw coverage; a separate R16F field ramps
  current surface contribution over .20 seconds. Raw coverage caps every result and
  immediately clears lost coverage. No scene color, enemy, light, shadow or transform
  history is stored. Safety takes precedence over an afterglow fade outside Live.

Relocation CVar: `r.Darkwell.ProjectFogVisual.PropRelocationPolicy` (default 0).

- 0 VerifyOldLocation: retain old snapshots until observing that location empty,
  including when the current source has been found elsewhere.
- 1 RecognizedIdentityRelocation: observing B updates the same StableID immediately;
  no retired A proxy survives. Similar shape is never an identity match.

**Baseline discrepancy:** the actual accepted baseline already updates its sole snapshot
when the same source is seen at B. Therefore the lab implements the requested policy 0
as an explicit lab-only retention branch. Outside the lab the accepted source behavior
is unchanged. Presentation 0 and relocation policy 0 are independent controls; “accepted”
in the presentation name does not claim that the new retention branch was in the baseline.

Routes: 0 manual, 1 horizontal sweep, 2 oblique refrigerator sweep, 3 slow rotation,
4 movement parallel to cabinets, 5 relocation A-first, 6 relocation B-first,
7 Torch/Lantern/dark/Torch, 8 twins exchange, 9 destroyed box, 10 replacement.
Reset before each independent relocation experiment. Reset reloads the map and restores
both CVars to 0; leaving the map also restores 0. Route playback disables the Stalker
controller only while the deterministic route owns its motion; manual mode restores it.
Normal player movement, tools and threat HUD remain available.

Evidence and automation reports belong only under ignored `Saved/PropGameplayLab`.
`Scripts/RunPropGameplayLabEvidence.ps1` launches serial D3D12/SM6 runs, normal TSR,
with resolution and both controls explicit. It refuses existing evidence names.
Native capture records screenshots, subject/HUD authority and each dynamic furniture ID.

Initial validation: full editor builds passed after fixing TObjectPtr type deduction.
Initial FogVisual and new pure policy/scope automation passed. Asset audit confirmed 25
unique IDs, the 8+4 rows, and navigation bounds (1200,900,300) half extents.
Probe01 was stalled by the menu; Probe02 failed adapter activation because of the old
fixture declaration counts and missing Stalker ID. Both are retained as failed evidence.
Probe03 activated correctly but its texture made comparison hard; Probe04 uses plain
graybox material and was actually opened and inspected. Optional engine editor Python
toolsets emit startup errors under `-game`; these are not lab authority failures and
must remain listed in the final severe scan rather than being silently filtered away.

Capture checkpoint: native PNG compression now runs on background workers and waits for
all writes before exiting. `AsyncProbe01_1920_M2_P0_R2` produced all 100 requested 10 Hz
frames with no laboratory contract errors; its actual soft-mode image was inspected.
The earlier `Visual01` attempt is retained but rejected for short-transition evaluation:
synchronous PNG compression reduced its cadence to approximately four frames per second.
`Build09.log` records the full successful editor build for the asynchronous writer.

Lifecycle/navigation checkpoint: `Build11.log` succeeded; `Automation04` passed all 15
selected tests, including all six presentation/policy combinations, relocation, destruction,
NeverRemember, enemy/HUD revision agreement and laboratory exit reset. Automation03's
new exit assertion failed because its lightweight world skipped actor initialization;
the fixture test now completes initialization before BeginPlay/Destroy. The failed report
is preserved. Real-map `NavRelocationProbe01_1920_M1_P0_R6` confirms navigation projection
and policy-0 B-first retention followed by A verification. Reset uses console priority so
a prior console override cannot prevent return to zero. Saved Recast navigation is dynamic.

`Visual02` completed 24 D3D12/SM6/TSR runs (1080p/1440p, three modes, four movement routes),
2342 frames, with no laboratory contract/fatal/assert/ensure/material failures. All eight
cross-mode comparison contact sheets were opened and inspected. Capture cadence is about
0.10–0.11 seconds median, with a maximum 0.40-second startup gap; this is directional
evidence, not a performance certificate. These visuals precede the navigation/lifecycle
checkpoint; no furniture geometry or presentation material changed afterward.

Superseding correction: review found that mode 2 multiplied partial RawCoverage twice.
`MaterialGPU02.log` reproduces the error on the actual D3D12/SM6 shader (Raw=.1,
output=.009995). The lab-only surface now uses `min(Raw, Soft)` directly. In
`MaterialGPU03.log`, all 33 GPU readback checks pass: settled modes 1/2 agree at six
coverage values, opaque mode 0 remains whole-object, loss of Raw clears immediately,
and the 12-step .20-second rise stays bounded. `MaterialGPU01` was an initial Python
API-name harness error, retained separately. Visual02 mode-2 evidence is superseded;
mode-0/mode-1 graphics are unchanged. Corrected mode-2 captures are required before readiness.

`Build14.log` is the latest full successful editor target build; `Automation05` passes
all 15 selected tests after the added identity-isolation checks. The runtime test now
also checks that recognizing a similar-looking second StableID cannot clear the first
unseen identity. Event routes aim directly at their target; the tool route includes a
visible Stalker positive control, then returns it behind the cabinets. Capture checks
actual actor hidden state and absence of any enemy furniture-memory record.

Route timing correction: scripted movement/events now enter through the world's
PreActorTick delegate, before authority evaluation. Surface updates and capture remain
in PostUpdateWork. This prevents a scripted teleport after authority evaluation from
carrying a previous-frame visibility state into the image. The delegate is world-scoped
and removed at EndPlay. `Build15.log` and all 15 `Automation06` tests pass. Earlier
directional captures are historical; final visual/relocation/event runs are being refreshed
on this checkpoint, with no change to production maps or their default input/authority.

Final separation correction: known geometry selection no longer uses the whole-object
Live threshold in sweep modes. A matching valid snapshot transform and appearance allows
the complete source mesh to show its per-pixel surface; a separate geometry-only operation
does not change source Live state, subject state or LiveOnly effects. Mode 0 uses its
original source visibility. New transforms remain hidden. Initial lab materials are bound
before memory registration so the remembered appearance is the final bound appearance.
`Build16.log` succeeds and all 15 `Automation07` tests pass, including explicit checks for
known geometry without identity Live, hidden LiveOnly effects, exactly one full silhouette,
and all source primitives hidden after an unseen relocation. Final captures must use this
checkpoint. The earlier visual generations are retained as superseded development evidence.

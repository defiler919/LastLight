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
- 1 SurfaceSweepHard: identity confirmation still gates the source actor. Its entire
  opaque mesh remains present; world LiveCoverage chooses live material versus gray.
  Unseen relocation retains only the old snapshot, never the hidden new transform.
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

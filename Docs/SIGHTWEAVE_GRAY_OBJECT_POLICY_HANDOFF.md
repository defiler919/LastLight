# Gray object policy work and evidence

Status: **PARTIAL — implementation and validation in progress**. No user acceptance,
final gray stable branch, or black-layer work is authorized by this document.

## Starting state

Actual start, local/upstream/remote after fetch and fast-forward-only pull:
`9e57daf028ee652594041399573496a370b733a9` on
`codex/darkwell-prop-memory-gameplay-lab`.
Protected refs remain:
- `stable/sightweave-gray-core-20260903`: `7534163b9c5718700b610e7677f47fbaa79cf977`.
- `stable/moving-history-grid-v2-20260902`: `404a5820739638f1097eaae0aa7fba19733298c3`.

Initial status: only `Darkwell.uproject` modified (missing final newline).
Preserved without staging. Initial LFS status has no pending asset uploads.
All reports, logs, screenshots and packages are ignored under `Saved/GrayObjectPolicy`.

## Product contract for this task

Independent object fields: Reveal (`WholeObjectAfterSpan` / `SpatialPartial`),
world-space `MinimumObservedSpanCm`, and History (`Always` / `StationaryOnly` /
`Never`). Project settings supply defaults; per-field object overrides resolve
once at registration. Desired DARKWELL defaults: Whole / 100 cm / StationaryOnly.
Plugin no-config defaults must preserve SpatialPartial / Always compatibility.

Whole tentative observations union legal footprint samples only within one real
observation session. Valid view loss clears tentative progress; invalid authority
does not. Longest continuous local X row or Y column run determines span, excluding
holes, clamped to the object's maximum valid run. Zero requires first legal contact.
Confirmed persists through rigid motion and permits object-only full presentation,
without changing world coverage, occlusion or another object's state.
Unconfirmed Whole never captures history. Stationary Partial retains observed
local gray/caps. Moving StationaryOnly abandons current without new history, keeps
older spatial evidence, and requires fresh legal stationary observation after stop.

## Baseline evidence (checkpoint A)

New native diagnostic selector: `Darkwell.PropLab.GrayPolicyBaseline`.
It uses existing runtime Lab geometry, authority and motion APIs. The pre-feature
baseline cannot honestly be called WholeObject: that feature does not yet exist.
Eight/32 cases use existing stress fixtures; OneMoving and the soak have one moving
cabinet in the existing nine-object control room. These are reported separately
from an isolated one-object stress test.

BuildA exposed a pre-existing Unity translation-unit name collision: anonymous
`CellSize` in CurrentLiveGrid hid parameters in other included source files.
The constant was renamed, preserving value and all calculations. BuildA2 standard
DarkwellEditor Win64 Development succeeded, 16.02 s. Initial failure is retained.

A run is still executing at this checkpoint. Completed first case, 8 static objects
with rotating view / 600 updates: room GT mean 7.825 ms, p50 8.237, p95 9.629,
p99 11.141, peak 15.323; 34,910.395 high-level coverage queries per frame;
0 history scans, 953 texture submissions, 4 cap rebuilds, 20 retained textures,
65,789 UObjects, 2,608,222,208 bytes working set. This is NullRHI CPU evidence,
not GPU/whole-engine frame time or visual evidence. Final soak and matrix numbers
will be appended after process completion; no unfinished run is counted as passed.

Source audit: current lifecycle samples the world grid, CurrentLiveGrid samples
local primitive corners/centers, then each PartRaster repeats world corners/centers.
The point authority repeats source geometry and occlusion evaluation for these calls.
Static knowledge sets DiscoveredPresent immediately; appearance uses .20/.18.
Freeze currently derives its memory through the presentation-compatible world grid,
which must be audited independently from that immediate local knowledge.
COVERAGE EDGE under `-PropLabHistoryPolicies` is Never, an intentional negative
control. It is not proof that stationary history capture is broken.

## Reproduction

Build: `Scripts/BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8`.
Tests: `Scripts/RunGrayObjectPolicyTests.ps1 -RunName <unique> -Tests <selector>`.
The runner preserves exit code, report clean/warning/failure/not-run counts,
duration and severe text scan separately. Existing evidence is never overwritten.

## Checkpoints and open work

A is the containing baseline diagnostic commit; SHA recorded in a later checkpoint.
Plugin integration, full matrix, current sweep/raster optimization, final automation,
D3D12/SM6 original-image review, plugin packaging and three clean GPU shutdowns
remain open at this checkpoint. Prior 0xC0000005 teardown evidence is retained.
Final manual PIE is required. Only after actual user approval may
`stable/sightweave-gray-layer-policy-20260903` be created. Black-layer work follows
that acceptance; this task must not start it.

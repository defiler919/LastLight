# Gray object policy work and evidence

Home continuation and newly generated evidence are recorded in
[SIGHTWEAVE_GRAY_OBJECT_POLICY_HOME.md](SIGHTWEAVE_GRAY_OBJECT_POLICY_HOME.md).
The earlier company Saved paths below are historical references only.

The latest company continuation is documented in
[SIGHTWEAVE_GRAY_POLICY_LAB_V2_HANDOFF.md](SIGHTWEAVE_GRAY_POLICY_LAB_V2_HANDOFF.md).
It adds the independent Chinese-guided acceptance map and runtime profiling
architecture. Its accurate current status is
**PARTIAL — GRAY_POLICY_LAB_V2_READY_PERFORMANCE_BLOCKED**: the Lab and evidence
are ready, while the unchanged strict performance gate is not.

Status: **PARTIAL — GRAY_OBJECT_POLICY_PERFORMANCE_BLOCKED**. Automated build,
rendered D3D12/SM6, packaging, complete automation and normal teardown evidence
are closed. No user acceptance, final gray stable branch, or black-layer work is
authorized by this document.

## Home recovery entry, 2026-09-04

The historical company starting state below is not the recovery starting point.
This home work originally started at `ede6c69d85012b89710743232ce1a50bd91e8685`;
interruption recovery started at the verified remote checkpoint
`2a155c81e2a74e3c4373d4a2dab71c45cfab489c`. Company unpushed files were not recovered.
The home recovery audit found six unstaged text files, no unpushed commits, no
staged changes and no local `.uproject` delta. Salvage was audited, built, tested
and pushed before the subsequent implementation.

Recovery checkpoints, each immediately pushed and triple-verified:

- Salvage: `34f3bb9d1403cb2946c1cddc5b5d81d96e374b2b`.
- C1, independent current admission: `9ea27f0c9241a39a6865cae388dae53ef5496bb1`.
- C2, exact repeated-history geometry/ownership: `c0352bbd44ee2b0729244501e245f4e971113461`.
- C3, exact planar projection/coverage/terminal work: `7787cd45920636993fe0dfd3fb2d551ee7326d53`.
- C4, physical/ownership dirty-work split: `faa8d0122bea78381035ce0c79bfc71b859b9fd2`.
- C4 complete-soak documentation: `78f72f980d5162fe7eda3cb19f68f7b9e1464426`.
- Final GPU runner and room-scoped test bridge: `38f9721438e81bb35579dc9afcfcd7aa194be9b5`.

C4 completed 54,000 active + 600 idle steps with complete report/log, normal
Editor exit and zero lost-current checks. It removed all >100 ms steps in the
isolated run, but final-window p95 was 44.499200 ms and the longest consecutive
>33 ms run was 148. Detailed source provenance, measured costs, capacity/resource
limits and every earlier checkpoint SHA are in HOME.

Final-source `Home_Final_FullAutomation_20260904` ran all 188 selected tests:
152 clean, 35 warnings, 1 failed, 0 not-run; 5781.248047 s tests / 5811.064235 s
wall, Editor exit 0, severe 0. The sole failure is the unchanged strict performance
gate in `GrayHomeBaseline.FifteenMinuteInteractiveSoak`; it completed 54,000+600,
lost Current zero times, and ended with step p95 45.257099 ms / peak 86.132199 ms.
Peak resources were 187 records, 12 textures, 3 caps, 33 MIDs, 77,479 live
UObjects and 4,869,718,016 bytes working set. Idle p95 was .672203 ms with all
heavy-work counters zero. The legacy diagnostic also completed: p95 152.475300 ms,
peak 228.601200 ms. Performance is materially improved but still blocked.

Current contract: per-field Reveal / MinimumObservedSpanCm / History, resolved
once at registration; DARKWELL WholeObjectAfterSpan / 100 cm / StationaryOnly;
plugin no-config SpatialPartial / 100 cm / Always. Current uses one independent
unsealed reserve alongside at most 64 sealed histories. Capture refusal cannot
evict retained knowledge or consume the next legal current observation. Only
fully VerifiedEmpty permits release; Superseded ownership is not empty evidence.

Final rendered evidence has three consecutive normal exits on identical source:
Exit10 and Exit11 are 102-second short matrices; Exit12 is a 571.654162-second,
60-cycle long interaction. All have process exit 0, severe 0, D3D12/SM6 true,
GPU pass true, PIE stopped and callback unregistered. The long run wrote 79
original 2560x1440 PNGs; ownership stayed <=1 and hard leak stayed zero. Failed
Exit1-9 evidence is retained and is not reclassified. The final focused native
run passed 47/47. Final `BuildPlugin` succeeded for UnrealEditor Development,
UnrealGame Development and UnrealGame Shipping, UAT exit 0.

Next window must fetch and compare HEAD/upstream/remote, read HOME's final entry,
inspect worktree/LFS and preserve local `.uproject`/asset changes. The accurate
implementation entry is the remaining long-lived history representation/retention
decision; do not invent eviction. For manual acceptance, exercise Whole below and
above 100 cm, Partial cut/cap, all three History policies through motion/stop,
wall occlusion, fast/slow sweep, repeated rotation and capacity. Do not move either
protected ref or create the final gray Stable before explicit user acceptance.

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

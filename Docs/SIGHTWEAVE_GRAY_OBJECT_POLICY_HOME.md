# Gray object policy — home continuation

Status: **PARTIAL — GRAY_OBJECT_POLICY_PERFORMANCE_BLOCKED**.
User acceptance, a final gray stable branch and black-layer work remain pending.
The prior **TEARDOWN BLOCKER RETAINED** is not cleared by NullRHI tests.

## Source and environment

Home checkout `D:\UE_pro\Darkwell`, engine `D:\UE_5.8`, development branch
`codex/darkwell-prop-memory-gameplay-lab`. Starting HEAD, upstream and fetched
remote all equal `ede6c69d85012b89710743232ce1a50bd91e8685`.
Initial worktree and LFS status were clean; no `.uproject` delta existed here.
The installed Build.version reports UE 5.8.2, CL 56702186 (the repository
guidance says 5.8.1); the configured engine root is unchanged.
No company unpushed work was retrieved, inferred or reused.
Protected remote refs were verified, without changes:

- `stable/sightweave-gray-core-20260903`: `7534163b9c5718700b610e7677f47fbaa79cf977`.
- `stable/moving-history-grid-v2-20260902`: `404a5820739638f1097eaae0aa7fba19733298c3`.

## Checkpoint A — home diagnostics

The original ede6c69 executable was run before feature changes using
`Scripts/RunGrayObjectPolicyTests.ps1 -RunName Home_A_20260903_Original
-Tests Darkwell.PropLab.GrayPolicyBaseline`.
EightChangedView completed: 600 updates, GT mean 11.722781 ms, p50 12.495100,
p95 13.430300, p99 13.794700, peak 14.689400; 34,910.395 queries/frame.
The subsequent original soak was deliberately interrupted for cost after
243.584 wall seconds for the process. Exit -1, no final automation report.
The completed case and interruption JSON are retained under
`Saved/GrayObjectPolicy/Home_A_20260903_Original*`. This is not a soak pass.

The diagnostic-only supplement adds isolated count fixtures, compact independent
motion, synthetic history-count scaling, per-minute/partial-window log flushing,
current sample counts, texture submissions versus actual GPU uploads, creation
counts, source MIDs and claimed UObject slots. Exact duplicate-point hashing runs
in a separate frame, outside GT measurement windows. No coverage value, sampling
precision, presentation, capture or history rule was changed for these diagnostics.
The runner now reserves a unique RunName directory, records source provenance,
and treats missing/empty/failed/not-run reports as failure even with process exit 0.

Builds use `Scripts/BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8`.
Home_A_Build succeeded in 26.45 s; Home_A_Build2 also succeeded. Existing MSVC and
engine deprecation warnings are retained. No Live Coding evidence is used.

`Home_A_Matrix_20260903` uses `Darkwell.PropLab.GrayHomeBaseline` plus the original
static positive/negative case and `-GrayBaselineWallBudgetSeconds=180`.
The bounded soak deliberately reports failure, not a fifteen-minute success.
The first EightMoving fixture also failed: the single-motion helper correctly
rejects a second active group. Its timing is excluded from multi-moving evidence.
The corrected fixture explicitly updates each prop's motion state and pose and
checks that every target begins legally visible. Original failures are retained.

### Cost reproduced before feature implementation

All numbers below measure the MovingPropLab game-thread update in NullRHI,
not a rendered engine frame or GPU time. Synthetic history tests explicitly
seed 1/2/4/8 histories and then turn the view; this is separate from natural
interaction. Remaining histories can be legally resolved during each window.

| Case | Updates | Mean ms | p50 ms | p95 ms | p99 ms | Peak ms | Queries/update |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Eight static changed view, supplement | 600 | 12.222 | 12.820 | 14.508 | 15.157 | 16.821 | 34910.395 |
| Natural interaction, deliberately bounded | 1980 | 76.241 | 73.305 | 129.917 | 185.555 | 212.866 | 187925.066 |
| One history changed view | 120 | 40.078 | 39.651 | 52.995 | 54.496 | 54.648 | 125034.933 |
| Two histories changed view | 120 | 79.668 | 73.827 | 112.790 | 113.467 | 113.475 | 229362.883 |
| Four histories changed view | 120 | 256.816 | 241.555 | 403.369 | 434.374 | 435.783 | 499224.508 |
| Eight histories changed view | 120 | 849.276 | 806.804 | 1668.305 | 1736.695 | 1772.017 | 922654.958 |

The separate eight-static audit counted 36,393 point queries, 10,683 exact
duplicates in one revision/frame. Static settled idle averaged 0.025196 ms,
zero queries/scans/submissions/cap rebuilds. The bounded interaction's settled
600-update idle averaged 0.060517 ms with the same zero-work counters.
Synthetic 4/8-history idle windows retained a first-frame cost spike (115/582 ms);
these are preserved, not silently trimmed from the reported statistics.

The bounded interaction scanned 99,651,605 historical samples and touched
5,473.996 current samples/update; 3,888 texture submissions, zero real GPU
uploads, 36 texture creations, 24 room-owned MID creations and 423 cap rebuilds.
It ended with 3 historical records, 5 total records, 1 proxy, 3 caps, 9 textures,
3 historical MIDs, 67,796 UObject array slots and 2,745,823,232 bytes working set.
The first matrix's MID count excludes source MIDs; the corrected measurement
explicitly includes them. Claimed UObject slots are distinguished from the
array high-water size in the corrected telemetry.

These results establish a performance blocker, not a performance acceptance.
The full 54,000-update interactive soak, functional Reveal integration, full
regression, D3D12 originals, BuildPlugin and three normal GPU exits remain open.

### Corrected short matrix

Home_A_Build3: standard Editor Development build succeeded, 8 actions, 19.55 s.
The corrected run is `Saved/GrayObjectPolicy/Home_A_Corrected_20260903/`.
The one-object fixture is explicitly placed in legal view; the initial matrix's
one-object origin was outside coverage and its timings are excluded. Compact
eight-moving fixtures begin with all eight sources legally visible. Motion and
view rotation then proceed independently for 600 updates, followed by settled
idle. All before/after comparisons must use these corrected same trajectories.

| Case | Mean ms | p50 ms | p95 ms | p99 ms | Peak ms | Queries/update |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| One static changed view | 2.107 | 2.118 | 2.481 | 2.541 | 2.703 | 6372.000 |
| One moving plus changed view | 16.639 | 17.538 | 20.764 | 36.019 | 39.215 | 40534.640 |
| Eight moving plus changed view | 121.127 | 130.018 | 147.679 | 155.852 | 161.973 | 267049.487 |
| Thirty-two static changed view | 53.406 | 54.283 | 59.354 | 62.657 | 63.578 | 152061.435 |

Eight-moving ends with 12 histories, 5 proxies/caps/textures, 39 source+history
MIDs and 66,344 claimed UObject slots; 2,613,784,576 bytes working set. Across
600 updates it scanned 63,066,212 historical samples, touched 2,445.300 current
samples/update, submitted 3,570 textures (zero GPU uploads in NullRHI), created
43 textures and 45 MIDs, and rebuilt 446 caps. Settled 600-update idle averages
0.029399 ms and performs no queries/scans/uploads/cap rebuilds or creations.
The separately instrumented frame has 351,264 queries / 46,181 duplicates.
One-static audit: 6,372 / 2,001; one-moving audit: 56,420 / 6,580.

The original static control test observes 22.9412% legal coverage. StationaryOnly
seals one partial history with one visible cap; Never seals zero with zero caps
and historical resources. COVERAGE EDGE's Never assignment remains a negative
control; this run does not claim all stationary capture is broken.

Final A corrected short report: **5/5**, 2 clean + 3 successful with HTTP timeout
warnings, 0 failed/not-run, 154.315277 seconds, process exit 0, severe scan 0.
The earlier matrix is separately **8 total, 2 clean + 4 warnings, 2 failed**,
459.495239 seconds, process exit 0 but runner exit 1. The two failures are the
replaced multi-motion fixture and the explicitly budget-stopped soak.
This checkpoint records reliable short/synthetic baseline evidence and preserves
the permitted high-cost soak interruption; it does not claim a fifteen-minute pass.
Git diff was inspected; generated evidence remains ignored. The containing
`test: record home gray policy baseline and cost blockers` commit is pushed
before beginning plugin feature implementation.

## Checkpoint B — plugin policy contract

Home A was pushed as `d47fd0f74846ef89288fd9fc188ed1a6fc1b898d`; local/upstream/
remote equality and clean worktree/LFS status were verified before B source edits.

`ESightWeaveRevealMode` contains only WholeObjectAfterSpan and SpatialPartial.
`FResolvedSightWeaveObjectPolicy` carries RevealMode, MinimumObservedSpanCm and
HistoryMode. The new resolver consumes plain default/override data; the old
history-only overload remains. Components resolve all authoring/config fields
once in OnRegister. Runtime getters read the cached policy only.

Native plugin defaults remain SpatialPartial / 100 cm / Always. DARKWELL's
`Config/DefaultGame.ini` explicitly supplies WholeObjectAfterSpan / 100 cm /
StationaryOnly. All three fields permit independent component overrides.
Legacy PolicySource/HistoryMode retain names and enum values; legacy Override
affects History only and remains effective when new flags deserialize false.
Blueprint queries add GetResolvedRevealMode and GetResolvedMinimumObservedSpanCm.

This checkpoint supplies the policy contract; the Lab consumes Reveal in C.
It does not yet claim whole-object rendering or confirmation behavior. The
old plugin tests remain, with their pre-feature absence assertion updated to
require the newly implemented Reveal field. No old gameplay assertion was removed.

Home_B_Build: standard Editor Win64 Development succeeded, 25 actions, 28.93 s.
Native tests add all eight per-field override combinations across both Reveal
and all three History defaults, legacy property deserialization, cached
registration/isolation, real INI loading and Blueprint public-surface validation.
`Home_B_Plugin_20260903`: **10/10 clean**, 0 warning/failure/not-run, 0.080738 s,
process exit 0 and severe scan 0. Diff reviewed; only the named source, config,
tests and documentation files are staged in this checkpoint.

## Checkpoint C — continuous span confirmation

B was pushed as `2946912b51383212937ce7e6b64c76413fa99f4a`, with local/upstream/
remote equality verified. `FSightWeaveRevealObservation` now owns per-object
Unobserved/Tentative/Confirmed native Gameplay Tag state. It unions only legal
footprint bits, measures continuous local X/Y runs in world cm, clamps to the
largest footprint run, requires first legal contact for zero, preserves tentative
state on invalid revisions, clears on real contact loss and retains confirmation
through rigid motion. Confirmed releases tentative bits and stops span evaluation.

The Lab derives a fixed actor-local footprint from its registered primitives and
supplies current legal observations independently from alpha. The 100 cm fixture
at yaw 146 measures **79.059 cm**, remains tentative, and creates no history,
proxy or historical cap. Full legal observation confirms; subsequent partial
contact still presents the complete object after normal entry. Full presentation
uses a separate occlusion gate and never changes world Coverage or other objects.
Whole view loss freezes only a capture-eligible confirmed pose; otherwise the
unsealed current is abandoned. Previously sealed histories retain spatial rules.

An explicit Lab per-object reset API sets all three fields before registration.
No global reveal runtime switch exists. The old HistoryPolicy, MovingLiveContinuity
and adapter suites scope their original SpatialPartial/Always test configuration,
retaining all gameplay assertions despite the new project defaults.

Home_C_Build initially failed only because the new native test chained assignments
to Unreal bit references (whose bool assignment returns void). Separate assignments
correct the fixture. Home_C_Build2 succeeded, 9 actions, 13.12 s. Home_C_Focused:
48 total, 38 clean + 9 warnings + 1 failed, 150.758423 s, severe scan 0. The failure
was the new wall fixture's observer and prop being on the same side; its expected
zero coverage correctly failed with coverage 1. The corrected fixture uses the
existing divider at y=0 with the observer at (500,-600), without changing occlusion
rules. All old history and moving continuity assertions passed in that run.

The rate test supplies the same partial/full legal observations at 30/60/120/144
Hz and confirms at the same physical span, independent of entry alpha. This is
not yet evidence for a completely skipped intermediate positive observation.
Performance remains blocked until E; C deliberately makes no cheap-path claim.
Home_C_Build3 succeeded, 6.08 s. Corrected `Home_C_Corrected_20260903` passes
**23/23 clean**, 15.455410 s, process exit 0, severe scan 0. It includes a real
world query at a still-out-of-cone position while the confirmed object presents
fully, and the corrected opaque-divider rejection. Together with the preceding
focused run all 48 distinct tests have passing evidence; this is not labeled a
single 48/48 run. No GPU result is claimed at C.


## Home Checkpoint D — explicit partial capture and static controls

C was pushed as `b242b98d19d5295908800ee0bf848b1ad73f516b` and all three
local/upstream/remote refs matched. D names the primitive-local
`CurrentLegalObservationMask` and `LastLegalCaptureMask` separately from the
plugin's tentative confirmation mask. On sealing, the record captures binary
DiscoveredPresent knowledge at the existing fine resolution, restricts it to
recorded real geometry, and initializes FineHistory from that immutable mask.
AppearanceBlend, LiveBlend and FrozenAAEnvelope remain presentation fields.
No `.20` entry, dither result or last-frame alpha controls historical membership.
The previous code already acquired binary D on first legal contact; this change
makes that contract explicit and adds exact mask equality coverage.

Launch the same Lab with `-PropLabMovingControls -PropLabGrayObjectPolicies`
(or world URL `?InWorldControls?GrayObjectPolicies`) for six extra static controls,
IDs `Lab.Gray.Static.0` through `.5`. They are Whole/Always, Whole/StationaryOnly,
Whole/Never, Partial/Always, Partial/StationaryOnly, Partial/Never. Labels include
STATIC WHOLE MEMORY, STATIC PARTIAL MEMORY, and STATIC NEVER CONTROL. The original
nine objects remain; the main, rotate, edge, multi-high, multi-low and multi-box
fixtures also cover the six independent combinations. COVERAGE EDGE explicitly
uses Partial/Never and is a negative control. Optional legacy HistoryPolicies
now explicitly supplies Always for its original unoverridden objects, so changing
project fallback does not silently change that compatibility fixture.

New project cases assert partial static gray and legal caps, ignored span threshold,
no new StationaryOnly moving history, Never's zero historical resources, exact
frozen/capture mask equality, one-frame versus settled appearance equality, static
positive/negative controls, six simultaneous registrations, independent motion and
confirmation, pre-motion history retention, target-only reset and three destroyed
world/room/policy lifetimes with GC. Static test controls are moved into the known
contact fixture for reproducible angles; labels in the manual room stay at their
separate display positions.

Home_D_Build1 preserved two compile failures in new code (incorrect standalone
policy header name and unsupported TBitArray compound AND). Both are corrected
without gameplay changes. Home_D_Build2 succeeded. D evidence is under
`Saved/GrayObjectPolicy/Home_D_Focused_20260903`.

`Home_D_Focused_20260903`: **60/60**, 50 clean and 10 engine/network warnings,
173.323868 s test time, 193.540206 s wall, process exit 0, severe scan 0.
Build2: 12.77 s. Diff review also found the Gray Lab fixture could override an
explicit legacy History reset; its fallback now only applies to inherited policy.
An additional coexistence assertion exercises the explicit reset priority.

Home_D_Build3 succeeded (12.68 s). Home_D_ExplicitOverride_20260903 passes
1/1 clean, 0.060444 s, exit 0, severe scan 0. No GPU claim at D.


## Home E1 — canonical coverage (intermediate performance checkpoint)

D was pushed as `34fd2c72106ca641fb8a9e9b53e28701acec9c07` with matching refs.
The project adapter now shares exact point queries and conservative rasters within
one authority/draw revision. Cache keys retain exact world coordinates and raster
size; no quantization or frame skipping is used. Deactivation and occluder updates
invalidate cache state. Lifecycle and coarse/fine history request the same raster
API. Recursive tiles only reuse proven constant 0/1 values; ambiguous cells retain
the existing five samples and 2.5 cm / fine-grid resolution. An occlusion-free proof
checks the entire origin-to-rectangle ray fan, including short internal walls.

Home_E1_Build1 succeeded (21.81 s). Home_E1_Canonical_20260903: **36/36**,
35 clean + 1 warning, 38.073055 s, process exit 0, severe scan 0. The independent
oracle comparison exercised 513 positive, 3800 negative and 967 ambiguous tiles.
This includes all 26 D object-policy cases and the existing FogVisual suite.

Intermediate OneChangedView (project Whole/StationaryOnly, same home trajectory):
mean 4.710014 ms, p50 2.9928, p95 8.3207, p99 9.5066, peak 13.9848;
5736.317 high-level queries/frame, 702 current samples/frame, 10,553,197 history
scans, 162 submissions, 0 NullRHI GPU uploads, 6 texture / 6 MID creations,
20 cap rebuilds, two sealed records. Idle p95 .0817 ms, but current sample/query
telemetry exposed continued work after live loss; that is carried into E2.
This is not a performance pass or a like-for-like speedup over A: A used the old
SpatialPartial/Always defaults and had a different history lifecycle. Confirmed
Whole specialization and history/diagnostic costs remain open.

Home_E1_Build2 succeeded. Home_E1_RasterParity_20260903 passes 10/10 clean,
0.602728 s, exit 0, severe scan 0. It compares seven real world view revisions
and every raster cell against the original five independent point samples,
then asserts repeat raster requests perform zero new point computations.


## Home E2 — confirmed Whole presentation specialization

E1 was pushed as `5b713e32889b87996dc698c52f87963682351d98` with matching refs.
Confirmed objects use an existential exact-footprint contact query, release their
observation masks and no longer call span evaluation. When the complete object
ray fan is unoccluded, current RGB uses an exact constant in the **same full-size
atlas**. No raster/texture resolution is lowered. A rigid move updates bounds;
unchanged constant RGB performs no atlas rebuild/hash walk/upload. Canonical world
coverage remains separate, and a dense world capture snapshot remains available
for unchanged fine-history sealing. Wall-intersecting objects retain the precise
occlusion path. Partial grids also reuse proven uniform coverage where possible.
The absent-current transition timer now decays, restoring zero idle observation
work. Telemetry separates high-level requests from actual computations/cache hits.

Home_E2_Build1 failed only because the added telemetry accumulator accidentally
matched a line in the original baseline test; the extra references were removed.
Home_E2_Build2 succeeded. Home_E2_PolicyPerf_20260903 passes 29/29 behavioral/test
cases (28 clean, 1 warning), 147.634857 s, exit 0, severe scan 0. **This is not a
performance gate pass**; the benchmark cases report measurements without hiding
threshold violations.

| Home trajectory | mean / p50 / p95 / p99 / peak ms | requests / computations per frame | current samples/frame | history scans |
|---|---|---|---|---|
| 8 moving + changed view | 14.292 / 11.382 / 29.622 / 29.880 / 33.196 | 454.543 / 242.335 | 55.718 | 17,239,076 |
| 32 static + changed view | 169.686 / 169.976 / 272.497 / 276.891 / 283.098 | 11,344.465 / 5,373.880 | 1,171.032 | 126,067,028 |

The 8-object route includes its original stationary pause; five records are
legitimately sealed there, not while moving. End counts: 5 records, 5 proxies,
5 cap components, 5 textures, 39 total MIDs; idle p95 .0305 ms. The 32-object
route ends with 29 historical / 30 total records, 22 proxies, 23 cap components,
26 textures, 162 total MIDs; idle p95 .0915 ms. Both idle windows have zero
queries, computations, current samples, history scans, submissions or creations.
Whole captures more complete knowledge than A's old Partial/Always fixture, so
E2 exposes a severe history/diagnostic cost increase. E3 must resolve this;
these numbers are retained as failures, not summarized as a successful speedup.

Home_E2_Build3 succeeded (20.99 s). Home_E2_Regression_20260903 passes 22/22,
15 clean + 7 warnings, 103.038063 s, exit 0, severe scan 0. All old HistoryPolicy
and MovingLiveContinuity assertions remain intact. The new dedicated test checks
60 rigid poses: no confirmed span evaluation, no dense observation masks, a full
source atlas every frame, and zero current sample work after settled view loss.


## Home E3 — historical geometry and contributor diagnostics

E2 was pushed as `f5257a110f3e1cfb435a4a5998dc0d3d30a49b92`, with matching refs.
No thresholds or old assertions are relaxed. Contributor diagnostics now cache
against their rendered inputs and geometry instead of repeating on every authority
revision. Cap-point lookups preserve the original .25 cm predicate using adjacent
spatial buckets. A dedicated test compares cached results with forced diagnostic
recalculation through multiple view revisions and two historical records.

Physical occupancy snapshots are collected after all deterministic motion for the
frame. Every historical query reuses those exact primitive transforms and bounds;
region candidates reject only AABBs that cannot intersect that record. A separate
oracle test compares cached occupancy to direct live-geometry queries at six rotated
poses. Ownership revisions are per identity, since another object's reveal never
owns this identity's historical surface. Physical changes still invalidate global
occupancy. Dirty regions compare old/new primitive geometry, marking only changed
regions plus this identity's changed newer-knowledge footprint.

Incremental evidence (benchmark runners passing is not a performance-gate claim):

| E3 intermediate run | trajectory | mean / p95 / peak ms | tests / process |
|---|---|---|---|
| DiagnosticsPerf | EightMoving | 11.024 / 27.034 / 31.166 | 3/3; exit 0; severe 0 |
| DiagnosticsPerf | ThirtyTwoChangedView | 74.024 / 159.988 / 171.115 | 3/3; exit 0; severe 0 |
| PhysicalCachePerf | EightMoving | 4.635 / 10.948 / 13.162 | 3/3; exit 0; severe 0 |
| PhysicalCachePerf | ThirtyTwoChangedView | 16.382 / 31.872 / 60.564 | 3/3; exit 0; severe 0 |
| CandidatesPerf | EightMoving | 4.675 / 11.068 / 15.760 | 4/4; exit 0; severe 0 |
| CandidatesPerf | ThirtyTwoChangedView | 14.339 / 27.006 / 57.808 | 4/4; exit 0; severe 0 |

All evidence remains in its original unique Saved run. Build1 failed on `auto*`
deduction from a TObjectPtr array; using the declared component pointer type fixed
it. Build2/3/4 succeeded. Full behavioral regression and final dirty-region timing
are still pending at this point in the checkpoint.


Final E3 dirty-geometry run (`Home_E3_DirtyGeometryPerf_20260903`): 4/4,
3 clean + 1 warning, 24.471907 s, exit 0, severe scan 0. Build5 succeeded.
8 moving: mean 4.659392 ms, p50 .7588, p95 10.9781, p99 11.2027,
peak 12.8931. 32 static: mean 6.334754 ms, p50 5.1222, p95 11.8204,
p99 17.4771, peak 59.1153. Both p95 targets are met in this run; the isolated
59 ms peak is retained. Consecutive slow-frame counts, 15-minute interaction,
full automation, real GPU output and packaging remain required before handoff.
The unchanged request/computation counts demonstrate that these gains are from
physical geometry and diagnostic reuse, not omitted legal coverage samples.

The larger E3 regression is running as `Home_E3_Regression_20260903`; its selector
includes the complete new object-policy suite, old HistoryGridV2, FastSweep,
InWorldControls, dirty-region and idle-cost tests. Its final result is recorded
in the follow-up below when available.


E3 follow-up: `Home_E3_Regression_20260903` passes **65/65** (52 clean,
13 warnings), 513.549683 s tests / 533.032107 s wall, process exit 0, severe
scan 0. Old late-angle, fast sweep, positive cap, overlap, in-world control,
dirty-region and idle assertions are retained. E3 was pushed as
`471be18c4510713bf592ab2b0b0bcca4cc32b408`, with all three refs matching.

## Home E4 — observations between rendered frames

The original fixed-origin rotation proof now exposes an exact point-set entry,
while its original historical rectangle wrapper retains the same five points and
math. Whole observation can prove the original rotated primitive sample footprint
inside a supported interval. Tentative union occurs before genuine end-of-interval
loss clears an unconfirmed session. A confirmed stationary swept observation can
seal its pose directly, without ever displaying an illegal current endpoint.
Moving objects, invalid publications, changed origins and ambiguous >=170-degree
turns do not invent intermediate knowledge. A lower bound on each original cell's
legal angular interval avoids dense sweep work for normal small input changes.

`WholeObjectFastSlowConfirmationEquivalent` now also compares a 0-to-160-degree
one-frame turn with a 5-degree incremental route at 30/60/120/144 Hz. Both endpoints
of the fast turn miss the cabinet. Every route confirms, keeps the final current
source hidden and produces exactly the same binary frozen/capture mask. This
supersedes C's explicitly recorded limited endpoint-only evidence.

The original GrayPolicyBaseline suite is now explicitly scoped to its original
SpatialPartial/Always settings, just like the other pre-policy suites. Its static
positive and Never negative assertions are unchanged. GrayHomeBaseline continues
to use current project defaults. Object-occlusion queries have their own exact
revision cache, separate from world coverage; neither can read the other's values.

Home_E4_Build1 succeeded (21.10 s). Home_E4_PositiveSweep_20260903: **42/42 clean**,
116.341606 s, exit 0, severe scan 0. This includes the original FastSweep suite
and the expanded project policy suite. Additional short-interval proof and
confirmed partial-wall/cache checks are validated in the follow-up below.

Home_E4_Build2 succeeded. Home_E4_ProofGuards_20260903: **13/13 clean**,
4.906801 s tests / 25.174823 s wall, exit 0, severe scan 0. Confirmed Whole
retains the real partial-wall cut; object occlusion caches remain distinct from
world knowledge. The short-turn optimization is checked against the exact
continuous point-set proof over distances, bearings and turn magnitudes.
The GT metric above measures Room.UpdateRoom, not total editor/game frame time;
subsequent performance instrumentation must expose the enclosing fixture step.

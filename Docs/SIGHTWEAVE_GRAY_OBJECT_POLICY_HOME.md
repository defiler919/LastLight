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
Home hardware: AMD Ryzen 9 3900X (12 cores / 24 threads), 32 GB installed RAM,
NVIDIA GeForce RTX 2070 SUPER, Windows driver 32.0.16.1088.
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

## Home E5 — enclosing CPU step gates and world publication cost

E4 was pushed as `37296366b49598d8b4f517148216879d9d5ad3eb`; local,
upstream and remote matched. The original Room GT metric did not include adapter
publication. The new gate times Adapter.Tick + Room.UpdateRoom (including its
telemetry finalization) + Fixture.Tick. It reports both scopes, per-stage means,
p50/p95/p99/peak, >33/>100 counts and longest consecutive >33 run. This is a CPU
fixture step, not an assertion about the complete rendered editor frame.
The original baseline tests/trajectories remain; new explicit PerformanceGate
variants and FifteenMinuteInteractiveSoak enforce p95 <16.6 ms, idle p95 <1 ms,
no consecutive >33 ms and no repeated >100 ms. No samples are trimmed.

Home_E5_StepScope_20260903 failed both gates: enclosing p95 27.489/26.335 ms
(8 moving/32 static), adapter mean 12.990/13.023 ms. The process exited 0 but
the runner correctly returned 1. Opt-in `-GrayProfileAdapter` localized the cost
to dynamic authority, not fog material submission. Home_E5_AdapterProfile_20260903
passes its diagnostic case (1/1, 8.183379 s, exit 0); it is not a performance pass.

World exploration rasterization now skips polygon-disjoint tiles/rows with a
conservative margin; original scanline positions, crossings, inclusive boundary
bias, precision and packed bytes remain unchanged. The byte-for-byte reference
retains the old full-row loop. This first optimization alone still failed both
gates: enclosing p95 19.219/20.028 ms, adapter means 7.005/7.059 ms. Its parity
case passed. The attempted MemoryAuthority selector had no matching cases;
correct `SightWeave.M3P5.Memory.Authority` and Modifier selectors run below.

The adapter previously moved its body, cone and light through three separate
publications. Native `UpdateSourceGroupTransform` validates all group handles and
the transform before mutation, preserves source metadata and publishes their
coherent final pose once. Existing single-source APIs are unchanged. Tests check
one revision, immutable held snapshots, no-op updates, and rejection with no
partial mutation. Memory writing still uses the legal final world snapshot;
object reveal permission never enters it.

Home_E5_Build1 failed because a text edit also matched the legacy accumulator;
the edit was scoped correctly and Build2/3/4/5 succeeded (Build5 24.81 s).
Home_E5_AtomicPerf_20260903: **21/21** (19 clean, 2 warnings), 11.154674 s tests /
31.920989 s wall, exit 0, severe 0. Includes memory authority/modifier, M6P1,
object-policy, raster parity and atomic publication tests plus both gates.

| E5 final case | Room mean / p95 / peak ms | enclosing p50 / p95 / p99 / peak ms | >33 / >100 / longest >33 |
|---|---|---|---|
| EightMovingPerformanceGate | 4.630 / 11.004 / 12.813 | 3.817 / 13.795 / 14.094 / 16.229 | 0 / 0 / 0 |
| ThirtyTwoChangedViewPerformanceGate | 6.167 / 11.680 / 57.344 | 8.383 / 14.775 / 19.553 / 60.653 | 1 / 0 / 1 |

Enclosing idle p95 .164/.570 ms, with zero queries, scans, current samples,
uploads, texture/MID creations and cap rebuilds. Adapter mean 2.389/2.412 ms.
8 moving retains 5 records/proxies/caps/textures and 39 total MIDs; 32 static
retains 30 records, 22 proxies, 23 caps, 26 textures, 162 total MIDs. History scans,
legal coverage requests and current samples exactly match the E2/E3 trajectories.
This passes the short CPU gates, including the retained isolated 60.653 ms peak;
15-minute resource/cost stability, D3D12 and complete automation remain pending.

Home_E5_PolicyRegression_20260903: **52/52 clean**, 107.619957 s tests /
131.239407 s wall, exit 0, severe 0. Complete GrayObjectPolicy, FogVisual and
original FastSweep tests retain their assertions after atomic source publication.
The GPU Python driver and its unique-run PowerShell wrapper are included as
prepared validation tools; Python compile and PowerShell parse succeed, while
actual D3D12 execution/results are still pending and are recorded separately.

## Home E6 — long-run diagnostic failure and incremental history evidence

E5 was pushed as `4902a511b06f498af575c001281706b80bcb7e0c` with matching
local/upstream/remote refs and unchanged protected refs.
Home_E6_FifteenMinuteSoak_20260903 did **not** pass. The first 3,600-frame
window had enclosing p95 40.009 ms, 290 >33 ms steps, 4 >100 ms steps, a 131-step
slow run and 19 records. At frame 7,200 the most recent window reached p95
72.187 ms, peak 161.569 ms, 24 >100 ms steps, a 199-step slow run and 36 records.
Intermediate wall-minute rows are retained too. The process was stopped after
318.156 wall seconds after these conclusive failures; exit -1, no complete
report. interruption.json records the verified owned process and reason.
This is diagnostic failure, never fifteen-minute evidence.

The retained records accumulated repeated work. The user explicitly forbids
using automatic history clearing as a performance fix: all historical records,
frozen masks and existing verified-empty release rules are therefore unchanged.
Fine state updates now select actual legal-coverage threshold crossings, physical
or observation-ownership changes, and active fades. Cached diagnostic coverage is
still refreshed. A conservative angular bound avoids continuous interval proof
only when a sample cannot enter and leave legal coverage between both endpoints;
large turns retain the exact full proof. Coarse occupancy is cached by geometry
changes, retaining its original center query rather than approximating it from
fine cells. These are work-selection changes, not lower sampling precision.

An initial edit failed because Python's Windows default decoder treated UTF-8 as
GBK. Only an unused cache member had changed; the resulting header-only Build1
succeeded, but its redundant test run was stopped and explicitly excluded
(Home_E6_DirtyEvidence_20260903, exit -1). The runtime edit was reapplied using
explicit UTF-8; Build2 succeeded (16.52 s).
Home_E6_DirtyEvidence2_20260903: **51/51** (46 clean, 5 warnings), 190.636826 s tests /
214.005096 s wall, exit 0, severe 0. Includes all object policy, FastSweep,
HistoryGridV2 and both enclosing CPU gates, without relaxing old assertions.
8 moving enclosing p95 14.056 ms, peak 16.470, no >33 ms steps. 32 static enclosing
p95 13.136 ms, peak 61.790, one isolated >33 ms step and none >100 ms.
32-static history scans fall from 126,067,028 to 577,496, with identical coverage
requests, presentation submissions, captures and retained resource counts.
A separate 240-frame replay against the original full-update evidence path is
pending below; it hashes every retained sample's state, empty fact, opacity,
capture values, occupancy, ownership, submission and diagnostic coverage.

Home_E6_Build3 succeeded (25.12 s). Home_E6_FullUpdateParity_20260903:
**1/1 clean**, 5.412233 s, exit 0, severe 0. Every sample matches the full reference
at every one of 240 frames, including real rotation, partial observation, view
loss, reacquisition and a 160-degree one-frame sweep.
The new soak also checks visible confirmed sources throughout the trajectory and
owned resource bounds. It roots its test world and runs normal-cadence GC every
simulated minute because this synchronous fixture does not run the engine tick's
GC scheduler; each complete GC pause is included in enclosing-step statistics.
Original soak variants retain their original GC behavior. Peak resources and GC
cost are explicitly reported. Build4 succeeded (16.35 s).
Home_E6_ResourceGateCheck_20260903: **3/3** (2 clean, 1 warning), 10.443029 s tests /
29.951799 s wall, exit 0, severe 0 (both short gates and PlayStopResourceLifetime).

## Home E7 — ownership candidates during history growth

E6 was pushed as `6f7ec57fc5dc7f9f96ae4059970835584fae029d`. Push succeeded;
first ls-remote suffered a transport interruption, then retry verified all three
matching development refs and the two unchanged protected refs.
Home_E7_FifteenMinuteSoak_20260903 also failed and was stopped diagnostically
(exit -1, 260.764628 s wall; no complete report). At frame 3,600 its enclosing
p95 was 37.320 ms, 225 >33 ms steps, one >100 ms step and a 125-step slow run.
At frame 5,979 the next window had p95 59.563 ms and 37 >100 ms steps. Live UObject
slots fell from 66,199 to 63,100 after periodic GC, while retained evidence grew
from 19 to 30 records. GC does not remove retained historical evidence.

Remaining ownership queries repeatedly searched already-retired records and read
current primitive transforms. Each historical update now builds the exact set of
newer records that the original predicate permits (current, or non-retired
historical), and reuses this set inside fine footprint queries. Current geometry
comes from the existing same-update physical snapshot, with the old direct path
as fallback. Scope guards prevent these temporary views escaping the update.
No records, masks, or evidence facts are removed by this optimization.

Home_E7_Build1 succeeded (28.61 s). Its first candidate run passed the full-update
and diagnostic references but failed the 8-moving gate: p95 32.257 ms, four
consecutive >33 ms frames. Reordering old/new geometry checks had made empty
candidate cases more expensive. The original order was restored and explicit
empty-candidate returns added; no eligibility condition changed. A source-edit
substring check failed before writing, then the corrected edit and Build2
succeeded (18.37 s). Follow-up results are recorded below.

Home_E7_EmptyCandidates_20260903: **4/4** (3 clean, 1 warning), 14.801099 s tests /
59.483215 s wall, exit 0, severe 0. Full-update sample replay and forced diagnostic
recalculation both match. Enclosing 8-moving p95 10.632902 ms, peak 17.948400;
32-static p95 13.349101, peak 60.348600. The latter has one isolated >33 ms step;
no consecutive >33 or repeated >100. Idle p95 .162501/.571102 ms with no dense work.
The prepared GPU driver now resolves an absolute evidence path; its optional
LongInteraction run executes 60 rotation/view cycles (18,000 fixed steps, five
simulated minutes) after the complete visual matrix. Runtime GPU validation is
still pending; this is not yet visual or teardown evidence.


## Home E8 — complete-soak run and capacity failure

E7 was pushed as `2acd9a61d0dab43c38edf9043f2cc16fcc02714e` with matching
local/upstream/remote and unchanged protected refs.
`Home_E8_FifteenMinuteSoak_20260903` runs the complete 54,000-step
`Darkwell.PropLab.GrayHomeBaseline.FifteenMinuteInteractiveSoak` on that E7 binary,
without a diagnostic wall budget. At this documentation checkpoint it is still
running: **36,000/54,000 steps, not a completed fifteen-minute result**.
The source.json and source.patch captured at launch identify the tested source.
Later local query-cache edits have not been built and are not part of this run.
No build, GPU editor or packaging job is running alongside the soak.

The failure is established before completion. At 3,600 steps the enclosing p95
was 32.419 ms; at 21,600 it was 154.635 ms with 106 retained records. At 33,615,
p95 reached 936.809 ms and peak 1,064.242 ms. Window rows, including the shorter
wall-minute rows, remain untrimmed in the unique run log. These CPU measurements
include adapter, room, fixture and periodic GC; they do not represent a GPU frame.

At 13:37:48 UTC the existing per-object capacity guard first rejected a new
observation for Lab.Moving.Cabinet after 64 resident records. At step 36,000 there
were 166 total records and 2,585 capacity warnings. The existing current-source
path requires a current observation record, so this guard also threatens current
visibility; the soak's explicit lost-live check will provide the final count.
The existing CapacityFailsClosed test and 64-record limit are unchanged. No old
history has been automatically cleared to avoid this failure. Peak resource
counts, final visibility assertions, idle recovery and normal process exit are
still pending. This intermediate checkpoint is failure evidence, not a pass.

### E8 complete result (E7 executable)

The run finished all **54,000 active + 600 idle steps**. Automation **0/1**,
one failed test, no not-run tests, 5,914.513672 s test / 5,936.792222 s process
wall time. Editor process exit **0**, runner exit **1**, severe scan **0**.
This is a complete failing soak, not an interrupted diagnostic. There were
21,313 original capacity-warning events; AutomationController replay repeats
those messages in the log and must not be counted as additional events.

Enclosing CPU steps exceeded 100 ms **20,647** times, with **241** consecutive
steps above 33 ms. Worst reported-window p95 was **1,158.734698 ms**; overall
peak **1,228.885699 ms**. Confirmed sources with legal coverage > .5 failed the
visibility check **327** times during active interaction and **10** times in idle.
The retained 64-record guard and the current-source dependence on a current
record explain the capacity failure. No capacity change or automatic historical
clearing was introduced.

Per-minute room means below combine all complete subwindows with their actual
step counts. The p95 column is the worst reported-window enclosing p95 in that
minute, **not a pooled percentile**; all unrounded mean/p50/p95/p99/peak and work
counters remain in the run log.

| Simulated minute | Room mean ms | Worst window step p95 ms | Step peak ms | End records |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 8.677 | 32.419 | 92.627 | 19 |
| 2 | 21.300 | 57.475 | 141.205 | 36 |
| 3 | 36.597 | 108.965 | 210.807 | 54 |
| 4 | 50.666 | 144.747 | 259.214 | 72 |
| 5 | 64.624 | 157.000 | 356.328 | 89 |
| 6 | 74.920 | 257.393 | 409.202 | 106 |
| 7 | 94.335 | 362.864 | 494.841 | 124 |
| 8 | 105.652 | 401.588 | 573.358 | 142 |
| 9 | 119.263 | 440.323 | 648.690 | 159 |
| 10 | 137.362 | 936.809 | 1064.242 | 166 |
| 11 | 156.369 | 757.216 | 832.536 | 170 |
| 12 | 158.329 | 800.700 | 949.321 | 174 |
| 13 | 172.184 | 954.766 | 1058.945 | 178 |
| 14 | 180.364 | 1037.030 | 1147.914 | 181 |
| 15 | 216.994 | 1158.735 | 1228.886 | 185 |

Peaks: 185 records, 12 owned textures, 3 caps, 36 total MIDs, 66,199 live UObjects
(including the initial pre-GC population), 3,227,230,208-byte working set. Existing
record/resource-bound assertions passed. Fifteen periodic GC pauses totalled
519.1398 ms and are included in enclosing-step measurements. Final idle retained
185 records / 2 proxies / 3 caps / 6 textures / 33 MIDs, with 62,752 live UObjects
and 1,262,960,640-byte working set. Idle room mean/p95 .284197/.300300 ms;
enclosing p50/p95/p99/peak .424601/.455000/.477500/.524197 ms. Idle queries,
scans, submissions, texture/MID creations and cap rebuilds are all zero. Idle
performance recovered, but current visibility did not; this remains a blocker.

### E8 pending exact-query reuse and stronger teardown coverage

Physical occupancy now caches exact world points within a single fixed physical
snapshot, bounded to 131,072 cache entries and falling back to the unchanged
query when full. Newer active-record candidates remain scoped through historical
updates and diagnostics; fallback queries no longer allocate candidate arrays.
Diagnostic rasters with identical bounds and dimensions contribute the same
sample positions and are visited once. Distinct grids and all cap sample points
retain their original predicates. No stored record, mask or sample is removed.
Forced full diagnostics retain the original repeated-grid loop; an added real
repeated-pose test compares its results with the optimized path.

PlayStopResourceLifetime additionally captures weak references to actual sources,
policies, textures, MIDs, proxies and caps across Whole / Partial / Whole worlds,
including positive partial-cap allocation. All must be reclaimed after each
world teardown and GC. The GPU driver uses engine-verified digit-aware Python
names `get3d_ownership_telemetry_for_testing` and
`get_max3d_render_ownership_contributors_for_testing`. Build and validation of
these pending changes are recorded below once finished.

## Recovery salvage checkpoint — 2026-09-03

Recovery fetched origin and verified HEAD/upstream/remote all at
`2a155c81e2a74e3c4373d4a2dab71c45cfab489c`. There were no unpushed commits,
no staged changes, and exactly six modified text files: the GPU Python driver,
this document, two test sources, and MovingPropLabRoom.cpp/.h. No asset or
`.uproject` delta existed. Git diff/check/cached and LFS status were inspected.
No residual Editor, Editor-Cmd, ShaderCompileWorker or related test process was
running. The E8 report and log were complete; its failed result above is retained.

All six local files were audited. Candidate pointers are scoped after current
creation and before record erasure; every candidate still uses the original
eligibility predicate. Occupancy reuse uses exact points within the same fixed
physical snapshot. Diagnostic reuse removes only identical sample positions.
No historical record or evidence is deleted. The first E8 build failed on a
TObjectPtr range-loop deduction in the new resource test helper; the explicit
UStaticMeshComponent pointer fixed it. E8 Build2 succeeded in 14.11 s.
`Home_Salvage_Build_20260903` re-ran the standard Editor Development build:
Succeeded, target up to date, 1.52 s. GPU-driver Python syntax also passes.

`Home_Salvage_Targeted_20260903` passes **7/7** (5 clean, 2 with warnings),
55.686329 s test / 128.538703 s process wall; process/runner exit 0, severe 0.
Tests: IncrementalHistoryEvidenceMatchesFullUpdate,
CachedDiagnosticsMatchForcedDiagnostics, RepeatedPoseDiagnosticsMatchFullScan,
FramePhysicalCacheMatchesGeometryOracle, PlayStopResourceLifetime,
EightMovingPerformanceGate and ThirtyTwoChangedViewPerformanceGate.
The resource test reclaims concrete sources, policies, textures, MIDs, proxies
and caps across all three test worlds. CPU step p95 14.475700 / 13.287898 ms;
peaks 21.584701 / 69.931198 ms; the latter is one isolated >33 ms step.
No >100 ms steps; idle step p95 .159603 / .647198 ms. These are short CPU gates,
not long-growth, rendered-frame, GPU or three-normal-GPU-exit evidence.

This salvage saves the audited and minimally validated changes before the
next implementation. Historical capacity versus current observation remains
unfixed here. Final 54,000-step validation of subsequent fixes, full automation,
D3D12/SM6, BuildPlugin, three GPU exits and final handoff remain pending.
The status remains PARTIAL — GRAY_OBJECT_POLICY_PERFORMANCE_BLOCKED;
TEARDOWN BLOCKER RETAINED. No final gray Stable is created.

## Home recovery C1 — independent current admission

Salvage was pushed as `34f3bb9d1403cb2946c1cddc5b5d81d96e374b2b`; all three
refs matched, worktree/LFS were clean and protected refs unchanged.
`BeginCurrentObservation` now has one unsealed live reserve independent of the
64 sealed-history slots. The contiguous record view remains compatible; maximum
storage is 64 sealed records plus one current. Existing `BeginObservedLocation`
and its CapacityFailsClosed test keep the old strict 64-resident behavior.
Sealing cannot convert the reserve into a 65th history. On rejected capture the
host discards only the unsealed observation and its presentation resources.
The next real legal observation can immediately use the live reserve again.
Existing history is never evicted, overwritten or marked empty by capacity.
A diagnostic is emitted per refused capture rather than every live update.

Home_CurrentCapacity_Build1_20260903: standard Editor Development build succeeded,
11 actions, 39.74 s. Home_CurrentCapacity_Targeted_20260903: **9/9** (6 clean,
3 with expected capacity warnings), 8.375164 s tests / 33.826738 s wall, process
and runner exit 0, severe 0. Original spatial-history tests remain unchanged.
New model coverage fills all 64 histories, advances current, rejects capture,
repeats re-entry, preserves ordering/evidence, and resumes capture only after
legitimate verified-empty release. The host test uses synthetic small distant
records to isolate full storage and runs real Whole/Partial current coverage,
invalid-revision recovery, repeated view loss and moving observation. All stored
fine-evidence hashes stay unchanged. Existing no-moving-history cases and the
240-frame full-update parity replay pass. Long-run cost remains unvalidated by
this short recovery checkpoint; status stays performance-blocked.

## Home recovery C2 — exact repeated-history work reuse

C1 was pushed as `9ea27f0c9241a39a6865cae388dae53ef5496bb1`; local, upstream
and remote matched. No capacity eviction or historical-state rule was changed.
The user subsequently authorized unattended safe shutdown only after all valuable
work and handoff are committed/pushed, LFS integrity passes, all three refs match,
protected refs remain unchanged, and owned validation/writer processes have ended.
Use the requested non-forced 60-second shutdown and recheck during the countdown;
cancel immediately if synchronization or local-work safety cannot be confirmed.

`Home_Recovery_GrowthDiagnostic_20260903` tested C1 for the explicit 180-second
wall budget: 8,350 active steps, **failed**, editor exit 0 / runner exit 1,
180.506546 s test / 200.400311 s wall, severe 0. At 3,600 steps CPU p95 35.830 ms;
last window p95 88.557 ms, 42 records, 37 >100 ms steps, longest >33 run 193.
Lost-live checks were zero active and idle; this did not yet reach capacity.
The bounded run is not a full 54,000-step result.

Geometry dirty indices and exact physical occupancy can now be shared within a
fixed frame by records with identical raster, prior geometry/revisions and newer
geometry. Historical ownership checks share exact fine-cell results only when
old geometry and the complete eligible newer-epoch set match. Records are processed
in increasing epoch order: matched newer candidates have not yet been modified,
and current was processed before the historical phase. Diagnostics cannot read
this cache. Both caches are bounded to 64 scratch entries, falling back to original
queries when full; small scenes with no reusable history avoid their setup cost.
Full-reference tests disable these caches. Terminal Superseded samples avoid
redundant writes without changing any evidence or state transition. A fully dirty
fine raster marks its entire coarse raster directly using the identical mapping.

The first geometry-only build succeeded (23.44 s). Its 5 selected tests passed,
but a misspelled HistoryGridV2 selector matched nothing. The corrected suite in
`Home_HistoryGeometry_Growth_20260903` passes all six original HistoryGridV2 cases;
its separate 180-second growth case still fails. Combined 7 tests: 6 passed,
1 failed, 250.602524 s tests / 271.437530 s wall, editor exit 0 / runner exit 1,
severe 0. Growth reached 47 records, 4 >100 ms steps, longest >33 run 142;
lost-live checks remained zero. First-window p95 improved to 32.420 ms but
geometry-query totals were unchanged, identifying repeated ownership checks.
These failures are preserved before the additional ownership reuse.

Home_HistoryOwnership_Build1_20260904: standard Editor Development succeeded,
9 actions, 20.72 s. Home_HistoryOwnership_Targeted_20260904: **11/11** (5 clean,
6 with warnings), 89.481796 s tests / 109.182189 s wall, process/runner exit 0,
severe 0. Includes all six original HistoryGridV2 cases, capacity/current,
240-frame original full-update parity, repeated-history full-update parity and
both short performance gates. The repeated replay verifies every retained fine
sample at each of 150 frames and actually uses 57 geometry and 29,988 ownership
cache hits. 8-moving CPU p95 10.638300 ms / peak 15.814900; 32-static p95
12.453400 / peak 57.523500, one isolated >33 ms step, no >100 ms steps.
The complete post-recovery 54,000-step soak, full automation, actual GPU evidence,
BuildPlugin and normal GPU-exit proof remain pending. Performance-blocked status
is retained until long-growth evidence supports a change.


## Home recovery C2 complete soak and C3 exact projection/coverage reuse

C2 was pushed as `c0352bbd44ee2b0729244501e245f4e971113461`.
`Home_Recovery_CompleteSoak_20260904` ran that clean checkpoint through all
54,000 active plus 600 idle steps. Complete report/log/source and analysis.json
are retained under its Saved/GrayObjectPolicy directory. Result: **0/1, failed**,
Editor exit 0, runner exit 1, severe 0; 1744.407715 s test / 1767.038435 s wall.
All failures are performance gates. Current Live lost checks are **0 active and
0 idle**, despite 78 original capacity-capture refusals. Capacity no longer blocks
current admission. The 64 sealed-history cap still rejects further captures;
there is no automatic eviction, and only fully VerifiedEmpty history can release.

C2 has 84 steps >100 ms, longest consecutive >33 ms run 241, worst reported
window step p95 81.649400 ms and overall step peak 120.235000 ms. Weighted room
mean grows from 8.880445 ms in minute 1 to 43.130611 ms in minute 15; performance
remains blocked. Peak records 187, textures 12, caps 3, total MIDs 33, live UObjects
66,199, working set 3,290,890,240 bytes. Fifteen measured GC pauses total
486.882895 ms and are included in step timing. Final idle: 186 records / 184
historical, 0 proxies, 2 caps, 8 textures, 27 MIDs, 62,636 live UObjects, working
set 3,290,996,736 bytes. Allocator high-water memory does not return to the old
1.26 GB result. Idle room mean/p95 .220643/.227200 ms and step p50/p95/p99/peak
.356700/.365298/.378601/.415098 ms. Idle queries, scans, samples, submissions,
uploads, texture/MID creation, cap rebuilds and reuse counters are zero.
All timings are native CPU fixture/adapter/room/GC steps, not rendered GPU frames.

C3 caches the exact vertical slab for finite pure world-Z rotations; tilted or
singular transforms use the original slab algorithm. It preserves the original
inverse transform, inclusive tolerance and interval arithmetic. Frame-local
coverage reuse requires identical raster and previous authority/draw revisions;
it copies exact coverage values and threshold-crossing bits before geometry union.
A bounded 64-entry cache falls back to original sampling. A terminal-state bitset
omits already Superseded samples from future dirty work without deleting evidence,
changing opacity/facts, fabricating VerifiedEmpty, or releasing records/resources.
The full-reference replay disables all three optimizations.

`Home_ProjectionCoverage_Build1_20260904`: full Editor Development build succeeded,
12 actions, 20.83 s. `Home_ProjectionCoverage_Targeted_20260904`: **57/57** (51 clean,
6 with warnings), 220.572754 s tests / 239.833789 s wall; Editor/runner exit 0,
severe 0. Includes GrayObjectPolicy, all HistoryGridV2 and FastSweep cases plus
both short performance gates. The planar oracle checks 21,870 queries across
24 eligible and 66 fallback transforms using direct equality for returned intervals.
Repeated-history full-reference parity uses 57 geometry, 29,988 ownership and
35 coverage reuse hits; terminal-mask evidence/reinitialization tests pass.
8-moving step p95/peak 9.718198/15.199203 ms; 32-static 11.944599/53.659700 ms
(one isolated >33 ms step); no >100 ms steps. Idle step p95 .159699/.574902 ms.
C3 complete automation/long-growth, GPU, packaging and three GPU exits remain
pending at this checkpoint. PARTIAL — PERFORMANCE_BLOCKED; teardown blocker
retained. No final gray Stable or manual-acceptance claim.


## Home recovery C3 complete automation and C4 physical-work split

C3 checkpoint `7787cd45920636993fe0dfd3fb2d551ee7326d53` ran
`Home_Recovery_FullAutomation_20260904`: 188 tests, 153 clean, 33 warning,
2 failed, 0 not-run; 6044.690918 s tests / 6066.104711 s wall, Editor exit 0,
runner exit 1, severe 0. The strict FifteenMinuteInteractiveSoak completed all
54,000 active + 600 idle steps and failed only unchanged performance assertions:
3 steps >100 ms, longest >33 ms run 148. Current lost checks were 0; peak records
187, textures 12, caps 3, MIDs 33, live UObjects 77,471 and working set
4,771,012,608 bytes; measured periodic GC totaled 536.548093 ms. Final idle had
186 records, no proxies, 2 caps, 8 textures, 27 MIDs, 62,636 live UObjects and
4,771,024,896 bytes; step p95 .683900 ms. This full-suite process inherited
uncollected transient worlds from earlier tests, so its process high-water is not
an isolated-room steady-state memory measurement. The legacy GrayPolicyBaseline
54,000-step test completed its workload but was marked failed by an unrelated
Epic HTTP socket_send_failure logged during that test; its final window also
records the known old-policy cost (p95 150.332900 ms, peak 221.148600 ms).
The networking event is not reclassified as a pass; complete log/report remain.

C4 separates physical occupancy dirty regions from ownership/view dirty regions.
Ownership changes still advance every original evidence candidate, while unchanged
world geometry reuses its exact fine occupancy. Physical transforms are compared
exactly and update fine/coarse occupancy; same-frame fine results may be shared,
while every record still refreshes affected coarse centers. The terminal-evidence
bitset also limits sweep candidates; the force-full oracle continues the original
all-sample route. No evidence, state, opacity, release or history-capacity rule changes.
Public/runtime performance telemetry now exposes all reuse counters.

`Home_OwnershipPhysicalSplit_Build1_20260904`: standard Editor Development build
succeeded, 9 actions, 21.00 s. The first 57-test run exposed a new coarse-state
oracle mismatch at the real frame-120 rotation (56/57, Editor exit 0, severe 0);
that failed evidence is retained. The fix preserves coarse physical updates.
Rebuild succeeded, 9 actions, 20.06 s. The focused original/full parity test
passed, then `Home_OwnershipPhysicalSplit_Targeted2_20260904` passed **57/57**
(51 clean, 6 warnings), 213.661652 s tests / 232.762752 s wall, exits 0, severe 0.
The repeated replay uses 57 geometry, 29,988 ownership, 35 coverage and 1,345,809
fine occupancy reuse hits, while matching every fine/coarse evidence field each
frame. 8-moving step p95/peak 10.150500/15.359398 ms; 32-static
11.883002/53.659000 ms (one isolated >33 ms, none >100), with 76,314 occupancy
samples reused. Long-growth remains to be rerun on C4; status remains PARTIAL —
GRAY_OBJECT_POLICY_PERFORMANCE_BLOCKED and TEARDOWN BLOCKER RETAINED.


## Home recovery C4 complete 54,000-step evidence

C4 was pushed as `faa8d0122bea78381035ce0c79bfc71b859b9fd2`; local,
upstream and remote matched and protected refs were unchanged.
`Home_PhysicalSplit_CompleteSoak_20260904` completed 54,000 active plus 600 idle
steps on that clean checkpoint. Complete log/report/source remain in the unique
evidence directory. Result: 0/1, failed only unchanged performance assertions;
1113.551025 s test / 1132.575995 s wall, Editor exit 0, runner exit 1, severe 0.
Current lost checks were 0. Capacity/record peak remained 187, with 12 textures,
3 caps, 33 MIDs, 66,199 live UObjects and 3,278,741,504 bytes working set.
Measured periodic GC totaled 469.768699 ms. There were **0** steps >100 ms, but
the longest >33 ms run remained 148 and the final window step p95 was 44.499200 ms
(peak 86.895902 ms). Thus performance remains blocked. Final idle: 186 records /
184 historical, 0 proxies, 2 caps, 8 textures, 27 MIDs, 62,636 live UObjects,
3,279,233,024 bytes; room mean/p95 .527021/.549900 ms and step p50/p95/p99/peak
.656400/.693597/.833601/.871100 ms. Idle heavy-work and reuse counters were zero.
No record was evicted or cleared as a performance workaround. C4 eliminates
billions of repeated fine-occupancy evaluations (2,339,499,907 in the late
52,568-step window), but remaining evidence/ownership cost still grows with
retained history. An architectural/product decision on long-lived history
representation or retention is the accurate next performance entry; do not guess
an eviction policy. GPU, packaging and exit validation follow separately.


## Home recovery final GPU, package and automation closeout

The C4 soak documentation was pushed as
`78f72f980d5162fe7eda3cb19f68f7b9e1464426`. The final test-harness/code
checkpoint is `38f9721438e81bb35579dc9afcfcd7aa194be9b5`. It makes the rendered
runner wait for the actual GUI Editor process and preserve its exit code, drives
the selected room's real in-world F control, waits for motion-state transitions,
uses state-relative history assertions, and exposes only two Lab testing bridges:
room-scoped control activation and the existing native rotation helper. The
standard Editor Development build succeeded before this checkpoint (7 actions,
17.44 s after the final UFUNCTION change). The final focused native run
`Home_GPUBridge_Targeted_20260904` passed **47/47** (44 clean, 3 warnings),
81.964035 s tests / 101.612792 s wall, Editor/runner exit 0 and severe 0.

All failed rendered-driver attempts are retained. Exit1 had the original GUI
launcher race and then selected the wrong generic reset control. Exit2 fixed the
wait but retained that control error and returned 0xC0000005. Exit3/4 exposed
stale absolute motion/resource assertions. Exit5 sampled before motion began;
Exit6 reset while rotation was active; Exit7 reused the intentionally one-shot F
control and returned 0xC0000005; Exit8 exposed an invalid absolute slow-sweep
history count; Exit9 reused the one-shot control in the repeated-cycle phase and
returned 0xC0000005. Exit3-6 and Exit8 returned process 0 but contain the explicit
`GRAY_GPU_FAIL` severe marker, so none is a pass. These fixture failures were
corrected without changing gray-policy semantics, deleting history or weakening
ownership checks.

Three consecutive runs on the identical final driver and source then exited
normally. `Home_GPU_Exit10_20260904`: exit 0, 102.210776 s, severe 0;
`Home_GPU_Exit11_20260904`: exit 0, 102.391932 s, severe 0; and
`Home_GPU_Exit12_Long_20260904`: exit 0, 571.654162 s, severe 0. Every summary
has `gpu_pass`, `pie_stopped`, `callback_unregistered` and `d3d12_sm6` true.
Exit10 wrote 25 original 2560x1440 PNGs under `GPU_20260904_031722`; the long
run wrote 79 under `GPU_20260904_032343` and completed all 60 real rotation
cycles. Structured samples report `MAX_3D_RENDER_OWNERSHIP=1` and
`HARD_OWNERSHIP_FILTER_LEAK=0` at early, middle and final cycles. Original-image
review covered Whole below threshold/full/history, Partial current/history/cap,
Whole and Partial moving negatives, Never negatives, six-policy overview,
wall occlusion, fast/slow sweep and cycles 0/19/39/59. No visible duplicate layer,
ownership leak or occlusion break was found. This is automated rendered evidence,
not user acceptance.

`BuildPlugin` first hit a full D: drive twice. The first wrapper lost valid process
status while its redirected log filled and inherited a stale shell exit value;
the corrected wrapper recorded the second attempt as failure 902. Those failures
are environment evidence, not build passes. To recover space, only regenerable
non-asset package files and the root generated `Intermediate` directory were
removed. Four copied `.umap` files were
detected and deliberately retained; no `.uasset` or `.umap` was removed or
rewritten. The fresh `Home_BuildPlugin_20260904_034518` run succeeded, UAT exit 0,
184.402276 s. Its log contains UnrealEditor Win64 Development, UnrealGame Win64
Development and UnrealGame Win64 Shipping, ending `BUILD SUCCESSFUL`; package
`Saved/SW_Home_BP_20260904_034518` has 303 files and is about .62 GB.

`Home_Final_FullAutomation_20260904` is the final-source complete selector:
**188 total, 152 clean, 35 warnings, 1 failed, 0 not-run**; 5781.248047 s tests /
5811.064235 s wall, Editor exit 0, runner exit 1, severe 0. The sole failed test is
`Darkwell.PropLab.GrayHomeBaseline.FifteenMinuteInteractiveSoak`; every error is
one of its unchanged p95/continuous-33-ms performance assertions. It completed
all 54,000 active plus 600 idle steps. Final active-window step p50/p95/p99/peak
was 25.557201/45.257099/75.198699/86.132199 ms; one step exceeded 100 ms over
the complete run and the longest consecutive >33 ms run was 199. Current lost
checks were **0**. Peak resources were 187 records, 12 textures, 3 caps, 33 MIDs,
77,479 live UObjects and 4,869,718,016 bytes working set; measured periodic GC
was 527.097497 ms. Final idle retained 186 records / 184 histories, 0 proxies,
2 caps, 8 textures and 27 MIDs; live UObjects were 62,638 and working set
4,869,787,648 bytes. Idle step p50/p95/p99/peak was
.656899/.672203/.685301/.723798 ms, with zero history scans, queries, submissions,
uploads, creations, cap rebuilds or reuse work.

The non-gating 54,000-step companion completed in 1104.666138 s. The legacy
`GrayPolicyBaseline.FifteenMinuteInteraction` completed in 2877.958984 s and
passed its diagnostic contract; its final window p95 was 152.475300 ms and peak
228.601200 ms, with 341,867,102 history scans and about 4.80 GB working set.
The final suite therefore proves a substantial performance improvement and zero
capacity-related Current loss, but the strict 33 ms gate remains unsatisfied.

Final status is **PARTIAL — GRAY_OBJECT_POLICY_PERFORMANCE_BLOCKED**. Current
observation is independent from the 64-history store; no eviction, overwrite,
automatic clear or fabricated VerifiedEmpty state was added. The remaining work
requires a product/architecture decision about long-lived history representation
or retention. The next user session should first test the rendered Lab manually:
Whole below/above 100 cm, Partial cut/cap, Always/StationaryOnly/Never while
moving and stopped, wall occlusion, fast/slow rotation, and repeated rotations
after capacity. Only explicit user acceptance may create the final gray Stable.

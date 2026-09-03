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

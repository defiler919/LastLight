# SightWeave gray policy Lab V2 handoff

Status: **PARTIAL — GRAY_POLICY_LAB_V2_READY_PERFORMANCE_BLOCKED**.

The isolated acceptance Lab, Chinese guidance, deterministic hitch reproducer,
runtime telemetry, spatial sleeping, ownership work reduction, automation and
rendered verification are ready. The unchanged strict long-run performance gate
still fails, so this is neither user acceptance nor production readiness.

## Repository and machine provenance

- Repository: `D:\UE_projects\LastLight`.
- Branch: `codex/darkwell-prop-memory-gameplay-lab`.
- Actual task start: `f8d24b205b0728efe19ff1077b03cf5ee3d7455c`.
- Final implementation/source SHA: `4764e50d4bc3cb7fd2e608ae3b5e661f30f1f43e`.
- Documentation checkpoint: the commit containing this file; the handoff reply
  records the final repository SHA after push.
- Engine root: `D:\UE_5.8`. Repository guidance names UE 5.8.1; the installed
  `Build.version` is 5.8.2, CL 56702186, compatible CL 55116800.
- Company machine: Intel Core i5-13500, 6 cores / 12 logical processors, 32 GB
  RAM, NVIDIA GeForce RTX 4060 (7956 MB), driver 610.88 / 32.0.16.1088.
- Protected refs remained unchanged:
  `stable/sightweave-gray-core-20260903` =
  `7534163b9c5718700b610e7677f47fbaa79cf977` and
  `stable/moving-history-grid-v2-20260902` =
  `404a5820739638f1097eaae0aa7fba19733298c3`.
- `stash@{0}: abandoned-company-work-before-f8d24b` was not applied, popped,
  copied from or dropped.

## Checkpoints

| Checkpoint | SHA | Result |
| --- | --- | --- |
| A — isolated map | `e61d69f374084848574072fc1c223f293e47aeed` | New map, director, lobby, six rooms, local reset/return and dormant stress room |
| B — Chinese experience | `992876edc95cfac49a558c318c808f7695143320` | Chinese screen/world guidance, CJK font, launcher and Lab automation |
| C — hitch reproduction | `da39959787d4c72ad2c8370e4bb24516e04e4dd4` | Deterministic turn matrix, ring buffer, per-stage telemetry and trace |
| D — candidates/sleeping | `f92a5c224147ca8513d9cdb7bd23983f78fe7764` | Unresolved-tile spatial index, dirty-region candidates and cold-history sleeping |
| E — ownership batching | `c2659c7945661b84e4a2e8e737054ca5cc39adfd` | Current ownership dirty runs, exact single-current path and monotonic newer-epoch frontier |
| F — regression/visual drivers | `7214891bde511b7edd25aaff5f2477365bdaee6f` | Full regression, 27-shot D3D12 tour and ten-minute soak drivers |
| F follow-up — wall clock | `4764e50d4bc3cb7fd2e608ae3b5e661f30f1f43e` | Real `perf_counter` frame deltas plus game-delta and active/candidate/sleeping percentiles |

Each checkpoint was pushed to the same development branch. No new development or
stable branch was created.

## Map and user experience

The new map is `/Game/Maps/L_SightWeaveGrayPolicyLab`, stored as
`Content/Maps/L_SightWeaveGrayPolicyLab.umap`. The old
`/Game/Maps/L_ProjectFogPropGameplayLab` remains in place for historical
automation and regression; its asset was not moved or rewritten.

The `.umap` is deliberately small. A native
`ADarkwellSightWeaveGrayPolicyLabDirector` builds the lobby, isolated room
geometry, controls, runtime subjects and Slate status panel. Core policy rules
remain in C++. The idempotent Editor script
`Content/Python/build_sightweave_gray_policy_lab.py` creates/updates only this
map, saves it and does not change the default game map.

The lobby has six Chinese F-interaction entrances:

1. `01 整体显示` — one WholeObjectAfterSpan subject, short/complete observation
   paths and a partial blocker.
2. `02 局部切块` — one long SpatialPartial subject for left/middle/right reveal,
   retained fragment and dark cap checks.
3. `03 移动物体` — separate Whole and Partial StationaryOnly subjects; switch,
   translate and rotate controls.
4. `04 永不记忆` — one Never negative control.
5. `05 遮挡与快速扫视` — a wall, observation gap, Whole and Partial subjects,
   and 90-degree, 160-degree and repeating sweep controls.
6. `06 性能压力测试` — off, 1, 8, 32, overlap-64, same-StableID-64,
   distributed-184 and mixed-policy modes, plus repeated turning and full reset.

Every room has its own `返回大厅` and `重置当前房间` controls. Reset removes only
that room's evidence; tests cover cross-room isolation. Stress defaults to mode 0,
with no active stress object/history work, no long test and no log flood during
ordinary functional checks.

The screen-space Slate panel uses a semi-opaque dark background and `FText` /
`LOCTEXT`. It always shows the map, checkout SHA, current room, goal, steps,
correct result, abnormal result and live status. World labels identify entrances,
objects and controls without relying on colour alone. Both use Unreal's bundled
`Engine/Content/Slate/Fonts/DroidSansFallback.ttf`; no downloaded font is stored.
All 27 final original screenshots were opened at original resolution and reviewed:
Chinese glyphs were readable, with no boxes/question marks, and the legacy cyan
technical overlay is suppressed only on the V2 map.

## Launch and manual acceptance

Run `Scripts/LaunchGrayPolicyLab.ps1`; it opens the new map with D3D12 and SM6.
Press Play, use WASD and the mouse, face a round control console and press `F`.

- Room 01: perform the short scan, turn away and confirm there is no gray memory;
  reset, sweep the complete marked path, turn away and confirm the whole gray
  object. The wall must not grant confirmation.
- Room 02: reveal left, middle and right separately, then turn away. Only legally
  observed pieces may remain, and exposed cuts must have dark-gray caps.
- Room 03: select Whole or Partial, establish old stationary evidence, start move
  or rotation and turn away. There must be no moving trail/new stale/proxy/cap.
  After the subject stops offscreen, the destination must remain unknown until a
  fresh legal observation; old evidence must remain intact.
- Room 04: observe the Never subject and turn away. Disappearance with zero
  history/proxy/cap is the expected negative result.
- Room 05: test through the gap, behind the solid wall, by manual snap turn, then
  the 90-degree, 160-degree and repeat consoles. No wall-through confirmation or
  sweep-path memory is legal.
- Room 06: begin with the empty baseline, cycle through 1/8/32/overlap-64/
  same-StableID-64/distributed-184/mixed, use manual or continuous rapid turns,
  and watch record/candidate/active/sleeping plus stage counters. Use `完全重置性能房`
  before returning to functional rooms.

## Hitch reproduction and diagnosis

`Scripts/RunGrayPolicyLabPerformance.ps1` runs the deterministic short D3D12
matrix. `Scripts/RunGrayPolicyLabTenMinute.ps1` alternates five real two-minute
phases: overlap-64 A, distributed-184 A, overlap-64 B, distributed-184 B and
overlap-64 C. The latter records wall-clock and game-clock frame intervals,
p50/p95/p99/peak, hitch counts, active/candidate/sleeping histories, work and
resource counts, stage timing and an Insights trace.

The root cause is no longer a guess. Long-lived histories were all admitted to
every turn update before D. A view/geometry ownership revision could then revisit
fine evidence for every retained record, and overlapping/current ownership work
could repeatedly test the same current geometry. Distributed cold histories were
therefore unnecessarily active; overlapping histories still magnified coverage
and ownership work. Mode changes also publish many records at once, so the first
render/update frame pays large historical ownership, presentation and cap costs.
There was no evidence that GC, an unbounded texture/MID leak, or Current-history
capacity coupling caused the turn hitch.

The final wall-clock trace makes the remaining worst path explicit. Each
distributed-184 transition produced an 11.55–11.65 second wall frame; room update
peaked at 11.49–11.58 seconds, `historical_us` at 11.04–11.12 seconds and
`ownership_us` at 10.55–10.62 seconds. Cap and refresh peaks were respectively
345.3 and 433.8 ms. `gc` telemetry was not asserted on those hitch samples.
The synchronous mode-selection calls themselves were 810–818 ms; the much larger
cost landed on the following room/render update, which is included in phase data.

## Implemented performance architecture and parity

- A persistent 2D index maps each record's still-unresolved world tiles, not its
  original complete bounds. Membership is rebuilt/updated with record/evidence
  changes and queried by view, sweep and geometry dirty regions.
- Records outside the candidate/transition/fade/cap/diagnostic regions remain
  authoritative but sleeping and perform zero per-frame evidence work.
- Existing frame-local coverage and physical occupancy snapshots remain the
  canonical exact evidence for their revisions; the change does not invent a
  weaker plugin/public cache.
- Current contribution ownership now emits only newly-owned row runs. The common
  one-current-contributor route tests that exact geometry without temporary
  interval arrays. A per-visual monotonic maximum epoch prevents old unchanged
  contributors being reconsidered while leaving the current growing mask eligible.
- Persistent candidate/index storage and inline scratch arrays avoid recurring
  hot-path allocation. No history was evicted, cleared, merged by StableID or
  fabricated as VerifiedEmpty. Screen percentage, TSR, 4x4 precision, `.20/.18`,
  cap rendering and Current capacity independence are unchanged.

Optimized-versus-force-full tests compare authoritative fine/coarse evidence,
state, coverage, occupancy, ownership, proxy/cap/texture presentation and visual
signature across movement, rotation, sweeps, overlap and distributed histories.
The new D spatial-index oracle and E ownership-frontier oracle pass. Existing
MovingLiveContinuity parity remains passing. StableID is used only to share exact
work, never as proof that an old pose is empty.

## Performance evidence

The pre-task strict baseline at `f8d24b` completed 54,000 active + 600 idle with
active p50/p95/p99/peak 25.56/45.26/75.20/86.13 ms, longest >33 ms run 199,
Current lost 0 and idle p95 0.67 ms.

The short C baseline, before spatial sleeping, admitted every distributed record:

| D3D12 short case | p50 / p95 / p99 / peak ms | candidates / active / sleeping |
| --- | --- | --- |
| overlap-64 | 17.00 / 24.61 / 35.62 / 36.57 | 56 / 56 / 0 |
| same-StableID-64 | 18.01 / 24.36 / 29.85 / 53.64 | 63 / 63 / 0 |
| distributed-184 | 24.47 / 29.61 / 31.95 / 32.26 | 183 / 183 / 0 |
| distributed-184 repeated 1000 turns | 24.56 / 29.91 / 32.19 / 56.06 | 183 / 183 / 0 |

The best same-machine D run after the spatial index shows the intended broad-phase
gain: distributed candidates/active fell from 183 to 63 and 120 records slept.

| D3D12 short case | p50 / p95 / p99 / peak ms | >33 / >100 | candidates / active / sleeping |
| --- | --- | --- | --- |
| overlap-64 | 17.22 / 23.23 / 32.95 / 35.35 | 2 / 0 | 56 / 56 / 0 |
| same-StableID-64 | 18.41 / 24.54 / 31.04 / 48.09 | 2 / 0 | 63 / 63 / 0 |
| distributed-184 | 16.83 / 21.88 / 23.31 / 24.36 | 0 / 0 | 63 / 63 / 120 |
| distributed-184 repeated 1000 turns | 17.26 / 21.94 / 24.64 / 44.82 | 1 / 0 | 63 / 63 / 120 |

E reduced the OneSpatial ownership mean/peak from 4.838/55.426 ms immediately
before dirty runs to 1.332/11.026 ms, and its historical peak from 78.308 to
36.982 ms. Its whole-frame run was made on a slower host interval (empty p95
20.31 ms) and is not presented as a frame-rate win: overlap p95 was 34.02 ms,
same-StableID p95 28.73 ms, distributed p95 33.89 ms and repeated distributed
p95 29.56 ms. The strict requirement is still failed.

Current-runtime full regression evidence, recorded immediately before the
V2-map-only legacy-overlay suppression, is
`Saved/GrayObjectPolicy/Company_20260904_CheckpointF_FullRegression`. It ran 174
tests: 144 clean, 28 with warnings, 2 failed, 0 not-run; Editor exit 0 and severe
scan 0. Only `GrayHomeBaseline.FifteenMinuteInteractiveSoak` and
`ThirtyTwoChangedViewPerformanceGate` failed. The 54,000+600 soak completed with
Current lost checks 0. Its final active window was p50/p95/p99/peak
28.943/84.099/98.143/196.324 ms; the complete run had 273 >100 ms steps and a
longest >33 ms run of 246. Idle p50/p95/p99/peak was
1.208/3.648/5.814/6.386 ms with zero scans, coverage queries, uploads or cap
rebuilds. Peaks were 187 records, 12 textures, 3 caps, 33 MIDs, 77,352 UObject
slots, 4,699,090,944 bytes working set and 1,127.323 ms measured periodic GC.
Resources stayed bounded, but active and idle budgets both failed.

The final ten-minute wall-clock evidence is
`Saved/GrayPolicyLabV2/Company_20260904/Company_20260904_CheckpointF_TenMinute_D3D12_2_WallFrame`.
It measured 600.047 seconds (628.385 seconds process wall), exited 0, had zero
severe lines, stopped PIE, unregistered the callback and wrote a 1,070,133,129-byte
trace. Overall wall-frame p50/p95/p99/peak was
21.766/27.060/36.479/11653.167 ms, with 381 frames >33 ms, 7 >100 ms and a
longest >33 ms run of 8. The two distributed phases held candidates and active at
p95 64 while sleeping p95 was 120; transitional maxima include all 184.

| Phase | wall p50 / p95 / p99 / peak ms | >33 / >100 | histories: max / candidate p95 / active p95 / sleeping p95 |
| --- | --- | --- | --- |
| overlap-64 A | 22.02 / 31.61 / 43.36 / 717.85 | 229 / 1 | 56 / 56 / 56 / 0 |
| distributed-184 A | 22.86 / 27.72 / 35.27 / 11653.17 | 59 / 2 | 184 / 64 / 64 / 120 |
| overlap-64 B | 21.32 / 25.46 / 29.06 / 741.23 | 20 / 1 | 64 / 64 / 64 / 0 |
| distributed-184 B | 21.41 / 25.99 / 29.40 / 11554.71 | 25 / 2 | 184 / 64 / 64 / 120 |
| overlap-64 C | 21.49 / 26.74 / 32.45 / 736.85 | 48 / 1 | 64 / 64 / 64 / 0 |

The JSON field `passed=true` and runner `perf_pass=true` mean only that the driver
ran its full 600-second protocol. They do **not** override the product thresholds.
Against p95 <16.6 ms, p99 <33 ms, no >100 ms and no consecutive >33 ms frames,
this run fails.

## Automation, rendered verification and exit

- Standard `DarkwellEditor Win64 Development` build succeeded after the final
  C++ UI change. Only existing engine/compiler deprecation warnings remained.
- Final focused selector
  `Darkwell.GrayPolicyLabV2+Darkwell.PropLab.MovingLiveContinuity` passed 28/28
  clean with no warnings, failures, not-run tests or severe lines.
- The focused set includes map load, MapCheck, PlayerStart, Chinese guidance,
  widget/font construction, room isolation, local reset/return, all six room
  contracts, stress modes/default dormancy, cross-room evidence and legacy-map
  loading.
- The full selector covered the requested policy, grid, sweep, rate, cap,
  capacity, multi-prop, visual, M6P1, serialization and lifetime suites. Its two
  failures are the retained performance gates described above; 172 other tests
  did not fail.
- Final rendered tours 4 and 5 are consecutive successes on the same source:
  GPU pass, D3D12/SM6, PIE stopped, callback unregistered, process exit 0 and
  severe 0. Earlier tour 2's transient missing-player failure is retained.
- Tour 5 wrote and reviewed 27 original 1920x1032 editor screenshots. The game
  viewport reported 1526x549, Screen Percentage 100 and AA method 4 (TSR).
  Review covered lobby/instructions, all functional states, 1/8/32, overlap-64,
  same-ID-64, distributed-184, mixed/reset and residual/Z-fighting negatives.
- After all automated runs there were no UnrealEditor-Cmd, UBT, UAT, Python,
  ShaderCompileWorker or trace-recording processes left.

SightWeave plugin source/public API was not modified in this task; the index,
Lab and ownership changes are DARKWELL adapter/project work. Therefore a new
BuildPlugin package was not applicable. The previously documented plugin package
proof remains historical evidence, not a claim for this task.

## Git and LFS closure

`Content/Maps/L_SightWeaveGrayPolicyLab.umap` is tracked with the `lfs` filter
(object SHA-256
`9514b291bed6f924d1a3e1f607afd050b585e03027f86db81c041d0faffa670b`).
The old and new maps are both tracked. Saved evidence, screenshots, logs, traces,
Binaries, Intermediate and DDC remain ignored and are not staged. The project
file has no local diff. Final `git lfs status`, `git lfs fsck`, worktree and
local/upstream/remote equality checks are performed after the documentation
checkpoint and reported in the final handoff; dangling objects are not cleaned.

## Remaining blocker and scope boundary

The spatial broad phase is effective for distant histories, and the ownership
frontier removes much repeated same-current work. It does not make 64 overlapping
active histories cheap enough, and mass history publication/transition still has
an unacceptable synchronous ownership/presentation spike. A follow-up should
profile and redesign that visible-priority transition path—most likely finer
canonical world-evidence batching and bounded offscreen publication—while retaining
the force-full oracle and every policy rule. It must not delete legal history or
delay visible corrections.

Black-layer work was not started because this task explicitly scoped it out and
the gray runtime performance gate remains open. No final gray stable branch was
created because automated evidence is not user acceptance; only the user's real
manual pass authorizes that stable point.

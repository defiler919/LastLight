# SightWeave M2P.3 Tail Latency Attribution Handoff

## Status

`IN_PROGRESS`

SightWeave M2P.3 is limited to tail-latency attribution and CPU Authority
finalization. Do not begin M3, GPU mask, post-processing, memory textures, the
DARKWELL gameplay integration, or `/Game/Maps/L_Prototype` work from this
branch.

## Git state

- Baseline branch: `codex/m2p2-sightweave-motion-event-index`
- Verified baseline SHA: `c1907352441a6053de02726e375d1bf94a903d01`
- Working branch: `codex/m2p3-sightweave-tail-latency-finalization`
- Latest safe commit: `59a3d93f1d37b7d0c75c6e0ab4f4f800661b2b23`
- The company workstation keeps a local-only `Darkwell.uproject`
  `EngineAssociation` GUID. Preserve it and never stage or commit it.
- The remote M2P.3 branch exists and matched the latest safe commit before the
  current performance checkpoint began.

## Open tail-latency questions

1. Batch512 remains zero-growth/zero-allocation with ordinary medians near
   90--132 us, but later ordinary processes observed p99 near 228--243 us after
   the dedicated 30/30 distributions had reported about 173--186 us.
2. Dedicated dynamic-door-only and door-plus-motion tests reported about 51 us
   and 119 us p99 respectively, while the broad four-vision/two-illumination
   RuntimePipeline observed p99 values near 261/401/244 us. The same slow
   process also raised a normally 30--50 us source transform to about 220 us.

Both tails now have complete ordinary-process raw evidence. Some overruns have
specific plugin, scheduler, or migration/frequency support, but others remain
`Unknown`; those rows cannot be relabeled as host noise and currently prevent a
`COMPLETED` result.

## Planned timing and scheduling evidence

Every measured target and adjacent fixed-work control will retain raw evidence
for:

- QPC-backed UE wall time;
- Windows current-thread cycle delta (`QueryThreadCycleTime`);
- current-thread kernel/user execution time (`GetThreadTimes`) when its measured
  resolution is usable;
- start/end thread ID and processor number, including migration detection;
- process page-fault deltas;
- available processor-frequency/performance-state evidence sampled outside the
  target operation;
- explicit ETW ContextSwitch availability/permission status and trace artifact
  location when a trace is requested.

Two fixed, preallocated, compiler-observable controls will run on the same
thread adjacent to each target sample: a compute workload targeting about
100 us and a memory workload targeting about 100--200 us. Controls are for
attribution only and never correct, subtract from, or replace SightWeave
measurements. Windows-only APIs will remain in `SightWeaveTests` or scripts;
Shipping Runtime must not acquire a Windows diagnostic dependency.

## Attribution rules under investigation

Each wall overrun will be assigned exactly one of:

- `Plugin CPU overrun` when current-thread execution evidence and an internal
  stage identify plugin work above budget;
- `Scheduler/preemption` when wall time grows without corresponding thread
  execution and context-switch/deschedule or synchronized adjacent-control
  evidence supports the gap;
- `Core migration/frequency` when migration or performance-state/cycle-rate
  evidence is the specific supported cause;
- `Measurement/instrumentation anomaly` when clocks, thread identity, counters,
  or trace integrity fail validation;
- `Unknown` when the evidence is insufficient.

`Unknown` is not host-noise evidence and prevents final `COMPLETED` status.

## Dual-clock calibration checkpoint

The test-only probe is implemented in
`Plugins/SightWeave/Source/SightWeaveTests/Private/SightWeaveM2P3Timing.*`.
Windows system libraries are linked only by the Editor-only `SightWeaveTests`
module. The probe places processor/frequency/page-fault calls outside the QPC
wall interval and brackets the target with `QueryThreadCycleTime` immediately
outside the QPC markers. Its raw cycle delta is conservative by the two QPC
markers and the closing cycle query; no measured overhead is subtracted.

The preserved first calibration report failed as intended at
`Saved/SightWeaveM2P3/TimingCalibration/Report/index.json`: it proved that this
Windows 11/i5-13500 host advances `GetThreadTimes` in 15.625 ms quanta, so its
nominal 100 ns FILETIME unit is not usable as a 100--250 us per-sample clock.
It also showed the initial memory control was about 1.01 ms. The control was
then shortened without dynamic allocation.

The latest preserved report passed 1/1 at
`Saved/SightWeaveM2P3/TimingCalibration3/Report/index.json` after making the
compute input volatile-seeded so unity compilation cannot fold its fixed loop:

- empty probe wall median/p99: 0.000/0.100 us;
- fixed compute wall median/p99: 120.400/125.200 us;
- fixed memory wall median/p99: 175.600/209.100 us;
- compute median current-thread cycles: 301,267;
- per-sample non-zero `GetThreadTimes`: compute 0/101, memory 0/101;
- aggregate 23.825 ms workload: 15.625 ms FILETIME CPU and 58,757,089
  current-thread cycles;
- 19.612 ms sleep: 0 FILETIME CPU, only 35,692 thread cycles, and an observed
  processor migration from 0 to 2.

Therefore raw current-thread cycles are the high-resolution per-sample
execution evidence, while kernel/user FILETIME is retained only as coarse
aggregate evidence. Microsoft explicitly documents that `QueryThreadCycleTime`
includes user and kernel execution cycles but must not be converted to elapsed
time because counter behavior can depend on CPU implementation and frequency:
https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime.
No cycles-to-microseconds conversion is allowed in the implementation. Exact
running microseconds require ContextSwitch ETW reconstruction; UE 5.8's local
Windows platform implementation requires administrator rights for that stream.
This ordinary process was not elevated, and the test reports that limitation
explicitly instead of claiming per-sample FILETIME authority.

## Attribution implementation checkpoint

`SightWeaveM2P3AttributionTests.cpp` now records every raw Batch and door sample
with total wall/cycles/coarse kernel-user time, thread/core endpoints, migration,
processor-frequency evidence, process page-fault delta, adjacent compute and
memory controls, capacity/prepared-index/revision evidence, and classification.
Batch stage probes separate uniform classification/prefilter work from all-field
result materialization. Dynamic probes cover normalization, spatial patch,
prepared-input invalidation, affected-source discovery, vision and illumination
solve, compatible-geometry reuse, snapshot materialization, compatibility, and
the immutable pointer publication. Internal stage probes omit frequency and
page-fault calls so expensive auxiliary queries do not contaminate nested timed
regions.

The attribution-only Runtime changes are callback sites and plain diagnostics under
`WITH_DEV_AUTOMATION_TESTS`; the callback implementation and every Windows API
remain in the Editor-only Tests module. Shipping has no probe fields, calls, or
Windows test-library dependency.

`Scripts/RunSightWeaveM2P3Attribution.ps1` launches a fixed count of independent
ordinary NullRHI Editor processes without priority or affinity changes, refuses
to overwrite prior runs, requires exactly two clean tests per process, verifies
all row/distribution counts, merges every CSV, and emits `summary.json`.

Two preserved one-process runner validations succeeded:

- `Saved/SightWeaveM2P3/Attribution/manual-script-01`;
- `Saved/SightWeaveM2P3/Attribution/manual-script-02` (final 80-column schema).

The final-schema validation contains exactly 10 Batch distributions/1,010 raw
Batch samples and 314 door/control samples. Its adjacent control medians remain
about 115/168 us. Batch wall p50/p95/p99/max was
95.9/142.6/190.2/515.9 us. Broad 4v2l door wall was
167.2/309.5/390.4/390.5 us. Current evidence does not justify a host-noise
conclusion: several broad-door overruns also show materially higher
current-thread cycles concentrated in the vision-solve stage while adjacent
controls remain stable.

The first fixed ordinary run is preserved at
`Saved/SightWeaveM2P3/Attribution/ordinary-10p-final`: 10/10 reports, 20/20
tests, 10,100 Batch rows/100 independent distributions, and 3,140 door rows.
Batch wall p50/p95/p99/max was 92.4/142.3/180.6/378.2 us. Broad-door wall was
160.0/282.0/327.6/490.5 us; vision solve was the dominant stage at
107.3/188.4/210.5/385.6 us. The exact repeated door work was constant (260
candidate segments, 2,082 rays, and about 1,466 tested segment/ray pairs), so
the high-cycle tail was not caused by sample selection or changing workload.

## Confirmed hotspot and retained optimization

Development-only detailed probes split vision rebuild into input preparation,
prepared-index acquisition/commit, geometry solve, and result materialization.
`deep-vision-02` passed 1/1 with 314/314 rows and showed broad-door geometry as
the hotspot: geometry total wall p50/p95/p99/max was
94.1/168.0/176.4/181.0 us. Its candidate-event, event-sort, acceleration,
ray-cast, and topology medians were 16.9/24.7/7.6/35.2/8.3 us respectively.
Input preparation, index work, and result materialization were materially
smaller.

A full-circle cone event-merge prototype passed differential parity 3/3 but was
rejected and removed: `deep-vision-03-cone-merge` changed geometry median from
94.1 to 95.5 us because rebuilding the dynamically invalidated absolute event
cache offset its sort/raycast savings.

The retained production change preserves an exclusive source binding's exact
prepared-slot keys across an index miss, uses the non-validated cached solver on
that miss, and recomputes only changed segment slots. It still invalidates and
rebuilds every derived endpoint/event structure affected by the changed
segment; no old result, reduced event set, async window, or stale snapshot is
used. `partial-prepared-differential-01` passed 3/3 and
`partial-prepared-index-01` passed 7/7. `deep-vision-04-partial-prepared` passed
1/1 and reduced broad candidate-event p50 16.9 to 12.3 us and geometry
p50/p95/p99 from 94.1/168.0/176.4 to 91.6/149.8/162.2 us. Dedicated-door
geometry median fell from 21.7 to 20.3 us.

The post-change ordinary run is preserved at
`Saved/SightWeaveM2P3/Attribution/ordinary-10p-partial-prepared-02`. It passed
10/10 reports and 20/20 tests with exact row/distribution counts. No priority or
affinity was changed. Batch wall p50/p95/p99/max was
92.6/141.5/177.8/1468.7 us; 62 samples exceeded 200 us: 9 plugin CPU, 4
scheduler/preemption, 29 core migration/frequency, and 20 Unknown. Broad door
was 150.0/273.6/384.1/980.7 us; 78 samples exceeded 250 us: 53 plugin CPU, 4
scheduler/preemption, 12 core migration/frequency, and 9 Unknown. Eight broad
samples exceeded 400 us (4 plugin CPU and 4 migration/frequency). The maximum
980.7 us row migrated processors and accumulated only 575,829 total cycles;
its 894.6 us internal QPC stage value therefore remains wall/preemption
telemetry, not a claim of equivalent running CPU time. Prepared deltas remained
exactly 2 hits/4 misses/4 rebuilds/0 evictions for all 1,010 broad samples.

## Checkpoints

- [x] `docs: start SightWeave tail latency attribution` --
  `056098289299c02cdd3daebfe7a668f69ad77c6d`, pushed to origin
- [x] `test: add SightWeave dual-clock timing evidence` (source and calibration
  complete) -- `e040486592e2c87b58d17773b4a166e090822bd1`, pushed to origin
- [x] `test: attribute SightWeave batch and door tails` (instrumentation and
  runner complete) -- `59a3d93f1d37b7d0c75c6e0ab4f4f800661b2b23`, pushed to origin
- [x] `perf: eliminate confirmed SightWeave tail costs` (retained exact partial
  prepared-segment rebuild; current checkpoint pending commit/push)
- [ ] `test: add SightWeave frame-level soak coverage`
- [ ] `test: stabilize SightWeave performance contracts`
- [ ] `docs: record SightWeave CPU authority final validation`

After each checkpoint: inspect the diff, stage exact paths only, exclude
`Darkwell.uproject`, run `git diff --cached --check`, commit, push immediately,
update this handoff, and confirm the remote SHA.

## Commands executed at task start

```powershell
cd D:\UE_projects\LastLight
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight fetch origin --prune
git -c safe.directory=D:/UE_projects/LastLight switch codex/m2p2-sightweave-motion-event-index
git -c safe.directory=D:/UE_projects/LastLight pull --ff-only origin codex/m2p2-sightweave-motion-event-index
git -c safe.directory=D:/UE_projects/LastLight rev-parse HEAD
git lfs pull
git lfs status
git -c safe.directory=D:/UE_projects/LastLight switch -c codex/m2p3-sightweave-tail-latency-finalization
.\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8
D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound "-ExecCmds=Automation RunTests SightWeave.M2P3.Timing.DualClockCalibration" "-TestExit=Automation Test Queue Empty"
.\Scripts\RunSightWeaveM2P3Attribution.ps1 -RunCount 1 -Label manual-script-02 -EngineRoot D:\UE_5.8
```

The baseline resolved exactly to
`c1907352441a6053de02726e375d1bf94a903d01`; LFS reported no pending objects;
the only pre-existing worktree change was `Darkwell.uproject`.

## Unverified work

- ContextSwitch ETW permission and trace completeness.
- Trace marker/reconstruction analysis that supplies exact running intervals
  when elevated ContextSwitch evidence is available.
- NullRHI 10-minute/equivalent-large-update soak and a separate rendered
  Editor/PIE soak.
- Intrinsic CPU, wall telemetry, and frame-soak performance contracts.
- Full M1/M2/M2P/M2P1/M2P2/M2P3 correctness, performance, allocation,
  packaging, clean-host, Shipping, dependency, Git, and LFS validation matrix.

## Exact recovery command

```powershell
cd D:\UE_projects\LastLight
git -c safe.directory=D:/UE_projects/LastLight switch codex/m2p3-sightweave-tail-latency-finalization
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight rev-parse HEAD
```

Then read this file, confirm the local-only `Darkwell.uproject` difference is
still excluded, and continue with the frame-level soak checkpoint. The latest
ordinary attribution command was:

```powershell
pwsh -NoProfile -File .\Scripts\RunSightWeaveM2P3Attribution.ps1 -RunCount 10 -Label ordinary-10p-partial-prepared-02 -EngineRoot D:\UE_5.8
```

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
- Latest safe commit: `056098289299c02cdd3daebfe7a668f69ad77c6d`
- The company workstation keeps a local-only `Darkwell.uproject`
  `EngineAssociation` GUID. Preserve it and never stage or commit it.
- Remote M2P.3 branch did not exist when work started.

## Open tail-latency questions

1. Batch512 remains zero-growth/zero-allocation with ordinary medians near
   90--132 us, but later ordinary processes observed p99 near 228--243 us after
   the dedicated 30/30 distributions had reported about 173--186 us.
2. Dedicated dynamic-door-only and door-plus-motion tests reported about 51 us
   and 119 us p99 respectively, while the broad four-vision/two-illumination
   RuntimePipeline observed p99 values near 261/401/244 us. The same slow
   process also raised a normally 30--50 us source transform to about 220 us.

Neither tail is classified yet. An observed wall overrun remains `Unknown`
until per-sample thread execution evidence and an independent adjacent control
support a more specific classification.

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

The second preserved report passed 1/1 at
`Saved/SightWeaveM2P3/TimingCalibration2/Report/index.json`:

- empty probe wall median/p99: 0.000/0.100 us;
- fixed compute wall median/p99: 115.900/189.900 us;
- fixed memory wall median/p99: 170.200/316.200 us;
- compute median current-thread cycles: 289,786;
- per-sample non-zero `GetThreadTimes`: compute 0/101, memory 1/101;
- aggregate 22.801 ms workload: 15.625 ms FILETIME CPU and 56,193,194
  current-thread cycles;
- 19.210 ms sleep: 0 FILETIME CPU, only 101,089 thread cycles, and an observed
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

## Checkpoints

- [x] `docs: start SightWeave tail latency attribution` --
  `056098289299c02cdd3daebfe7a668f69ad77c6d`, pushed to origin
- [x] `test: add SightWeave dual-clock timing evidence` (source and calibration
  complete; pending checkpoint commit/push)
- [ ] `test: attribute SightWeave batch and door tails`
- [ ] `perf: eliminate confirmed SightWeave tail costs` (only if evidence proves
  a production hotspot)
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
```

The baseline resolved exactly to
`c1907352441a6053de02726e375d1bf94a903d01`; LFS reported no pending objects;
the only pre-existing worktree change was `Darkwell.uproject`.

## Unverified work

- ContextSwitch ETW permission and trace completeness.
- Trace marker/reconstruction analysis that supplies exact running intervals
  when elevated ContextSwitch evidence is available.
- Per-sample Batch512 evidence from 10 ordinary Editor processes, 10
  distributions per process, and 101 samples per distribution.
- Per-stage broad dynamic-door evidence from 10 ordinary processes plus
  dedicated door and door-plus-motion controls.
- NullRHI 10-minute/equivalent-large-update soak and a separate rendered
  Editor/PIE soak.
- Any production optimization; none is authorized without stage-level plugin
  CPU evidence.
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
still excluded, and continue with the Batch512/dynamic-door attribution and
ContextSwitch trace-analysis checkpoint.

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
- Latest safe commit: `c1907352441a6053de02726e375d1bf94a903d01`
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

## Checkpoints

- [x] `docs: start SightWeave tail latency attribution` (this handoff; pending
  initial commit/push)
- [ ] `test: add SightWeave dual-clock timing evidence`
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
```

The baseline resolved exactly to
`c1907352441a6053de02726e375d1bf94a903d01`; LFS reported no pending objects;
the only pre-existing worktree change was `Darkwell.uproject`.

## Unverified work

- Dual-clock resolution, overhead, counter semantics, and self-tests on this
  workstation.
- ContextSwitch ETW permission and trace completeness.
- Fixed compute/memory control calibration without timed-region allocation.
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
still excluded, and continue with the dual-clock calibration checkpoint.

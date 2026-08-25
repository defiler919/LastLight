# SightWeave M2P.3 CPU Authority Final Validation

## Final state

`PARTIAL`

M2P.3 implemented calibrated dual-clock diagnostics, raw attribution captures,
one evidence-backed exact optimization, two 36,000-frame soaks, and a
fail-closed performance-contract audit. Correctness, capacity, allocation,
packaging, Shipping isolation, and frame-soak gates pass. The requested final
CPU Authority contract is not complete for three independent reasons:

1. this ordinary Windows process cannot obtain authoritative per-sample
   running microseconds: `GetThreadTimes` advances in 15.625 ms quanta, raw
   thread cycles cannot legally be converted to time, and ContextSwitch ETW
   needs elevation on this UE/Windows host;
2. preserved captures contain supported `Plugin CPU overrun` and unsupported
   `Unknown` tail rows, neither of which may be relabeled as host noise;
3. the unchanged Batch512 and broad dynamic-door wall-clock assertions do not
   pass across every required independent process.

No M3, GPU mask, post-processing, memory-texture, DARKWELL gameplay-integration,
or `/Game/Maps/L_Prototype` work was started.

## Repository identity and checkpoints

- Repository: `D:\UE_projects\LastLight`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Branch: `codex/m2p3-sightweave-tail-latency-finalization`
- Exact baseline: `c1907352441a6053de02726e375d1bf94a903d01`
- Last validated code/test checkpoint before this report:
  `92d6f1fabdd5f366c1a594cadce2a5187623d35c`
- Checkpoints pushed before final documentation:
  - `056098289299c02cdd3daebfe7a668f69ad77c6d` -- start attribution;
  - `e040486592e2c87b58d17773b4a166e090822bd1` -- dual-clock evidence;
  - `59a3d93f1d37b7d0c75c6e0ab4f4f800661b2b23` -- tail attribution;
  - `ed755a276cc8e02e57a65d1efe3e8ba38762c558` -- retained optimization;
  - `50cc0bb5d8b198b80ce4ecb0aa84a16f9428cea1` -- frame soak;
  - `92d6f1fabdd5f366c1a594cadce2a5187623d35c` -- fail-closed contracts.
- The final documentation checkpoint is the commit containing this report;
  use `git rev-parse HEAD` to resolve it after checkout.

The workstation's pre-existing `Darkwell.uproject` `EngineAssociation` change
remained local-only and was never staged.

## CPU timing authority

The test-only probe records QPC wall time, raw `QueryThreadCycleTime` deltas,
coarse kernel/user `GetThreadTimes`, thread/core endpoints, migration,
page-fault deltas, and processor-frequency evidence. Frequency and fault
queries are outside target intervals. Adjacent preallocated fixed compute and
memory controls are attribution evidence only; nothing is subtracted from a
SightWeave measurement. Windows diagnostics remain in the Editor-only
`SightWeaveTests` module and scripts.

Final calibration evidence is
`Saved/SightWeaveM2P3/TimingCalibration3/Report/index.json`:

| Probe | Wall median / p99 | Other evidence |
|---|---:|---|
| Empty | 0.000 / 0.100 us | measurement overhead floor |
| Fixed compute | 120.400 / 125.200 us | median 301,267 raw cycles |
| Fixed memory | 175.600 / 209.100 us | preallocated fixed work |
| Aggregate compute | 23.825 ms | 15.625 ms FILETIME; 58,757,089 cycles |
| Sleep | 19.612 ms | 0 FILETIME; 35,692 cycles; core 0 to 2 |

Per-sample non-zero `GetThreadTimes` counts were 0/101 for both controls. In
the final attribution, 60/10,100 Batch and 14/1,010 broad-door sub-millisecond
samples nevertheless crossed one 15.625 ms FILETIME quantum. This proves that
FILETIME is not a 100--250 us sample clock. Microsoft documents raw thread
cycles as implementation/frequency dependent, so the implementation does not
convert cycles to microseconds. Exact running intervals require ContextSwitch
ETW; UE 5.8's local Windows implementation requires administrator rights and
the ordinary captures were not elevated. The absence of ETW authority is
reported, not inferred away.

## Ordinary-process tail attribution

The final fixed capture is
`Saved/SightWeaveM2P3/Attribution/ordinary-10p-partial-prepared-02`.
`RunSightWeaveM2P3Attribution.ps1` launched 10 independent ordinary NullRHI
Editor processes with no priority or affinity changes. It produced 10 clean
reports, 20/20 tests, 10,100 Batch rows in 100 distributions, and 3,140 door
rows. Every raw row is retained.

### Batch512

Aggregate wall p50/p95/p99/max was
92.6/141.5/177.8/1468.7 us. Aggregate raw-cycle p50/p95/p99/max was
230,632/347,942/392,870/3,654,893. Of 62 rows above 200 us, classification was:

| Classification | Rows |
|---|---:|
| Plugin CPU overrun | 9 |
| Scheduler/preemption | 4 |
| Core migration/frequency | 29 |
| Measurement/instrumentation anomaly | 0 |
| Unknown | 20 |

All 100 distributions passed the 150 us median limit, 99 passed the 180 us p95
limit, and 86 passed the 200 us p99 limit; 86 passed all three. The worst
per-distribution p99 was 332.7 us. Uniform classification/prefilter wall
p50/p95/p99/max was 2.7/5.4/8.5/100.5 us, while all-field result
materialization was 87.3/131.6/165.4/1452.6 us. Capacity remained stable, so
the tail is not explained by result-array growth.

### Dynamic door and motion

Broad four-vision/two-illumination door wall p50/p95/p99/max was
150.0/273.6/384.1/980.7 us; raw-cycle p50/p95/p99/max was
370,156/646,462/813,193/1,325,130. Its ten process p99 values were
327.9, 388.7, 316.8, 465.4, 335.7, 316.5, 305.5, 276.0, 277.1, and 513.4 us:
0/10 met 250 us. Dedicated-door wall p50/p95/p99/max was
33.5/62.4/83.4/200.7 us and met 250 us in 10/10 processes. Held-door p99 was
287.0 us; door-plus-motion p99 was 447.2 us.

Of 78 broad rows above 250 us, 53 were plugin CPU, 4 scheduler/preemption, 12
migration/frequency, and 9 Unknown. Eight exceeded 400 us: four plugin CPU and
four migration/frequency. The 980.7 us maximum migrated from processor 5 to 2
and accumulated 575,829 raw cycles, so its long QPC interval is not claimed as
equivalent running time. A separate non-migrating 534.2 us row accumulated
1,325,130 cycles, including 492,461 in vision and 622,752 in snapshot work,
and is classified plugin CPU.

Broad dynamic-stage wall and raw-cycle p50/p95/p99/max was:

| Stage | Wall us | Raw cycles |
|---|---:|---:|
| normalization | 0.1/1.5/2.6/28.6 | 749/4,243/7,239/17,721 |
| spatial patch | 0.6/2.0/2.6/4.5 | 1,997/5,492/7,239/11,731 |
| affected-source discovery | 0.3/0.9/1.2/3.8 | 1,248/2,746/3,744/9,984 |
| prepared invalidation | 0.5/2.1/3.2/4.4 | 1,997/5,990/8,736/11,482 |
| vision solve | 99.1/177.2/261.0/911.5 | 249,850/430,810/516,921/834,414 |
| illumination solve | 28.4/50.9/64.1/122.3 | 71,636/127,546/153,754/202,422 |
| compatible reuse | 0.6/1.6/3.1/28.9 | 3,244/6,240/9,485/28,454 |
| snapshot materialization | 5.2/10.4/18.4/249.1 | 13,728/26,957/45,427/622,752 |
| compatibility | 0.1/0.3/0.4/0.9 | 749/1,248/1,747/3,494 |
| immutable publication | 0.0/0.1/0.1/0.2 | 499/749/998/4,744 |

Exact prepared-index deltas were 2 hits, 4 misses, 4 rebuilds, and 0 evictions
for every one of the 1,010 broad samples.

## Root cause and retained optimization

`deep-vision-02` held the broad workload constant at 260 candidate segments,
2,082 rays, and about 1,466 tested segment/ray pairs. Geometry wall
p50/p95/p99/max was 94.1/168.0/176.4/181.0 us. Candidate construction, event
sort, acceleration, ray-cast, and topology medians were
16.9/24.7/7.6/35.2/8.3 us.

A full-circle cone event-merge prototype passed 3/3 differential tests but was
rejected and fully reverted because geometry median regressed from 94.1 to
95.5 us. The retained production change preserves an exclusive source
binding's exact prepared segment slots on an index miss and recomputes only
changed segment slots, while invalidating every affected derived endpoint and
event structure. It does not reuse old results, reduce inputs, add async
latency, or publish a stale snapshot.

`partial-prepared-differential-01` passed 3/3 and
`partial-prepared-index-01` passed 7/7. `deep-vision-04-partial-prepared`
reduced candidate-event median 16.9 to 12.3 us and geometry
p50/p95/p99 to 91.6/149.8/162.2 us; dedicated geometry median fell 21.7 to
20.3 us. This improvement is retained, but it does not erase the residual
plugin and Unknown tails.

## Frame-level soak evidence

The explicit 60 Hz transient-world soak ran 36,000 measured frames (600
simulated seconds) after a 2,400-frame warmup. It continuously moved/rotated a
camera cone, exercised close radial and intermittent remote queries, changed a
door every 30 frames, moved a torch, retained another light and guard, and ran
Batch512 against 25 rooms and 100 segments. The first preserved NullRHI attempt
(`nullrhi-36000-final01`) correctly failed because a 600-frame warmup allowed
256 bytes of later capacity growth; the assertion was not relaxed.

| Capture | Renderer | Wall p50/p95/p99/p99.9/max us | >1 ms | Longest tail streak | Correctness / growth |
|---|---|---:|---:|---:|---|
| `nullrhi-36000-final02` | NullRHI | 100.2/239.6/353.7/532.3/882.8 | 0 | p99: 4 | 0 / 0 |
| `rendered-36000-final01` | D3D12 SM6, RTX 4060 | 159.2/256.8/408.0/511.9/2124.8 | 2 | >1 ms: 1 | 0 / 0 |

NullRHI raw-cycle p50/p95/p99/p99.9/max was
248,104/567,091/803,212/1,234,273/1,889,722. Rendered was
395,368/602,534/988,165/1,173,619/5,296,013. The two rendered >1 ms rows remain
Unknown. Both final runs recorded exactly 60 prepared hits, 51,600 misses,
51,600 rebuilds, zero evictions, and revisions 3,096 through 49,416. Neither
run changed process priority or affinity. The fixed-rate benchmark completed
in 15.409 seconds under NullRHI and 17.041 seconds with D3D12; those host
durations are reported only as whole-process context, not as simulated-time or
intrinsic-CPU evidence.

## Fail-closed contract result

`Scripts/TestSightWeaveM2P3PerformanceContracts.ps1` consumes the exact final
attribution and both final soaks, verifies counts and D3D12 identity, refuses
to overwrite evidence, writes its result, and exits non-zero unless every
layer is complete. The latest audit is
`Saved/SightWeaveM2P3/Contracts/performance-contract-final02.json`. Its
expected result is `PARTIAL`/exit 1:

| Contract component | Result |
|---|---|
| Exact evidence counts | PASS |
| Frame-soak hard gates | PASS |
| Authoritative intrinsic CPU microseconds | FAIL |
| No plugin/Unknown overruns | FAIL |
| Existing wall-clock limits unchanged and all passing | FAIL |

No legacy timing assertion was removed, loosened, renamed, or split to obtain a
pass. The suite remains intact because host-independent intrinsic timing has
not yet been demonstrated. Recovery requirements are documented in
`Docs/SIGHTWEAVE_M2P3_PERFORMANCE_CONTRACTS.md`.

## Regression, performance, and allocation matrix

All counts below are from fresh final runs at code checkpoint
`92d6f1fabdd5f366c1a594cadce2a5187623d35c`. Exported automation reports had
zero report warnings, NotRun tests, or InProcess tests.

| Invocation | Passed | Failed | Result |
|---|---:|---:|---|
| `SightWeave.M1` | 21 | 0 | PASS |
| `SightWeave.M2` | 83 | 1 | FAIL: Batch512 distribution 5 p99 319.101 us |
| `SightWeave.M2P` | 28 | 0 | PASS |
| `SightWeave.M2P1` | 7 | 0 | PASS |
| `SightWeave.M2P2` | 11 | 0 | PASS |
| `SightWeave.M2P3` | 4 | 0 | PASS |
| full `SightWeave` | 105 | 0 | PASS for that independent run |
| `SightWeave.Performance.M2P` | 3 | 0 | PASS |
| `SightWeave.Performance.M2P2` | 2 | 1 | FAIL: Batch512 distribution 9 p99 226.900 us |
| `Darkwell` | 24 | 0 | PASS |
| optimized extended benchmark | 1 | 0 | PASS |

The independent full-suite pass does not supersede the two preserved timing
failures. The M2 failure's worst distribution had median/p95/p99/max
93.501/167.198/319.101/537.697 us. The M2P2 performance failure's named
distribution had p99/max 226.900/333.402 us; another distribution reached an
absolute 697.501 us maximum.

Focused parity/index coverage also passed: M2 Query 14/14, M2P Differential
3/3, M2P2 PreparedEventIndex 7/7, and M2P2 Performance 3/3 inside the full
suite. M2P regular solver typical 8x64 single-solve p99 was 29.799 us and total
p50/p95/p99 was 201.698/210.300/213.299 us. Final optimized extended results:

| Workload | Total p50 / p95 / p99 / max us |
|---|---:|
| typical 8x64 radial | 203.297/232.700/414.800/414.800 |
| dense 8x512, 4,096 total | 1080.800/1265.403/1434.594/1434.594 |
| dense 8x4096 each | 9682.000/11036.597/11036.597/11036.597 |

Pure Source Transform p99 remained below its 100 us contract: radial 19.200,
cone 12.301, camera 27.101, 1 cm translation 26.099, 5 cm 16.101, 20 cm
16.499, room transition 58.200, and teleport 7.503 us. Dedicated door was
45.598 us; combined door-plus-motion was 159.696 us and shared-source motion
was 61.400 us, reported separately from the pure-transform gate. Prepared
4096/source wall p50/p95/p99/max was
936.303/1477.700/1570.500/2079.900 us, so the contractual median below 1 ms
and p99 below 2 ms both passed. The regular RuntimePipeline process reported
Batch p50/p95/p99/max 89.101/90.800/104.699/130.702 us, broad dynamic door
130.799/138.599/153.702/167.601 us, and source transform
29.799/30.402/32.898/34.001 us; these favorable runs do not replace the fixed
multi-process attribution results.

The final allocation proof label was `M2P3Final_20260825`. Unreal Insights
produced a 433,473,708-byte trace; capture and analysis each passed 1/1. The
strict set covered 17 workloads and 51 measured rows with exactly zero
allocations, reallocations, bytes, or violations. Explicitly permitted range,
held-door, and door-plus-motion setup rows were reported separately and were
not smuggled into the strict result.

## Builds, package isolation, and lab map

- Final `DarkwellEditor Win64 Development` build: PASS, four actions, 8.88 s.
- `BuildPlugin` output:
  `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM2P3Final-20260825-1315`.
- Clean-host plugin `UnrealEditor Win64 Development`: PASS.
- Clean-host plugin `UnrealGame Win64 Development`: PASS.
- Clean-host plugin `UnrealGame Win64 Shipping`: PASS.
- Runtime forbidden-reference scan: no `Darkwell`, `UnrealEd`,
  `SightWeaveEditor`, `Tests`, `AutomationTest`, or direct Windows diagnostic
  reference in Runtime source/package outputs.
- Runtime module dependencies remain only `Core`, `CoreUObject`, `Engine`, and
  `DeveloperSettings`; the plugin descriptors keep distinct Runtime, Editor,
  and Editor-only test modules.
- `dumpbin` imports for packaged Runtime binaries were limited to the expected
  UE runtime modules, `KERNEL32`, and Visual C++ runtime libraries.
- Both game package configurations contained Runtime objects/precompiled data
  and no Tests path.

`/SightWeave/Maps/L_SightWeave_Lab` was opened twice. Both runs exited 0 and
reported query status 0 with authority/live/vision/close-bypass all active and
snapshot revision 58. The first run exposed 40 unrelated Experimental Toolsets
Python errors for missing `AgentSkill`, `ToolsetDefinition`, and
`PythonTestRunner`. A second run used the engine-supported `-DisablePython`,
logged that Python was disabled, reproduced the same lab query state, and had
zero Python errors. No `.uasset` or `.umap` was moved, rewritten, or
manipulated by ordinary filesystem commands. The map's exact LFS oid is
`sha256:5132f55808ffa48c0bf3136a696e1e61ce782aaa74ba2d71658854da08849172`
and its pointer size is 235,928 bytes.

## Warning and error audit

- Final builds emitted the installed MSVC 14.51 versus preferred 14.50 warning.
- Plugin packaging emitted UE-header C4996 deprecation warnings, not
  SightWeave-source warnings.
- Every Unreal automation log included 13 engine pre-discovery
  `AutomationTest` self-diagnostic `Condition failed` lines; they occur before
  the selected tests and are not SightWeave assertions.
- Controller errors in selected test execution were limited to the two
  preserved hard timing failures above.
- The first lab run had the 40 Experimental Toolsets Python errors; the
  `-DisablePython` rerun had none.
- Other non-fatal host/engine warnings included scalability, navigation,
  Editor Data Storage UI, one Zen lock warning, and a D3D TSR driver warning.
- A cross-log severe scan found no ensure failure, assertion failure, fatal or
  critical error, or unhandled exception.

## Git and LFS audit

Before this documentation commit, `git diff --check` and cached diff checks
passed; no generated `Binaries`, `DerivedDataCache`, `Intermediate`, or `Saved`
path was tracked. `git lfs fsck` passed, `git lfs status` reported no pending
LFS commit/push object, and `.gitattributes` covered Unreal assets and source
media. The generic `Plugins/SightWeave/Config/FilterPlugin.ini` emitted by
`BuildPlugin` was a regenerable untracked packaging artifact and was removed.
The only remaining worktree difference was the preserved local
`Darkwell.uproject` association.

## Remaining risk and exact recovery boundary

Completion requires a permitted ContextSwitch ETW capture (or another
authoritative host-independent running-time source), valid trace-marker
reconstruction, zero unresolved plugin/Unknown overruns at the applicable
limits, and all unchanged wall gates passing across the specified independent
processes. Until all four are demonstrated, the contract must remain
`PARTIAL`. Do not proceed to M3 on the basis of the frame-soak or full-suite
passes alone.

```powershell
cd D:\UE_projects\LastLight
git -c safe.directory=D:/UE_projects/LastLight switch codex/m2p3-sightweave-tail-latency-finalization
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight rev-parse HEAD
```

Then read this report and `Docs/SIGHTWEAVE_M2P3_PERFORMANCE_CONTRACTS.md`,
confirm the local-only `Darkwell.uproject` difference is still excluded, and
resume only the unresolved CPU Authority work above.

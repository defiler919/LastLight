# SightWeave M2P.4 Elevated ETW and Dynamic Sector Handoff

## Status

`IN_PROGRESS` (elevated ETW authority complete; Batch gate passes and the
Broad Dynamic Door gate authorizes an exact incremental angular-sector design.)

M2P.4 is limited to elevated ContextSwitch attribution, evidence-driven CPU
tail closure, and an exact incremental dynamic-occluder angular-sector update
only if authoritative on-CPU evidence proves it is required. Do not start M3,
GPU masks, post processing, memory textures, DARKWELL gameplay integration,
`/Game/Maps/L_Prototype`, a `main` merge, or unrelated work.

## Git state

- Baseline branch: `codex/m2p3-sightweave-tail-latency-finalization`
- Verified baseline SHA: `e3e5a833e58ce4571653fcef0f7b44698ae80dae`
- Working branch: `codex/m2p4-sightweave-etw-dynamic-sector`
- Latest safe SHA: `b8f19ac15e3b23ae660c29a3739a054b0fa75637`
- The company workstation's local-only `Darkwell.uproject`
  `EngineAssociation` difference is preserved and must never be staged.

## Administrator and ETW capability checkpoint

The default command host measured on 2026-08-25 is not elevated:

- `whoami`: `游戏策划1\defiler919`;
- integrity: `Mandatory Label\Medium Mandatory Level` (`S-1-16-8192`);
- `BUILTIN\Administrators` and local-administrator membership are `deny only`;
- `WindowsPrincipal.IsInRole(Administrator)`: false;
- `whoami /priv` does not contain `SeSystemProfilePrivilege`;
- `fltmc`: access denied, `0x80070005`;
- `wpr.exe`: installed at `C:\Windows\System32\wpr.exe`, version
  10.0.26100.8972;
- `wpaexporter.exe`: installed under Windows Performance Toolkit, version
  11.7.395.0;
- `xperf.exe`: installed under Windows Performance Toolkit, version
  10.0.26100.9169;
- `wpr -status`: no active recording;
- `wpr -start GeneralProfile -filemode`: failed before capture with
  `0xc5585011`, "Failed to enable the policy to profile system performance";
- no ETL was produced and the final WPR status remained stopped.

The user then approved the secure-desktop UAC prompt raised by the bounded
M2P.4 scripts. The child token is authoritative high-integrity evidence:

- `WindowsPrincipal.IsInRole(Administrator)`: true;
- integrity: `Mandatory Label\High Mandatory Level` (`S-1-16-12288`);
- `fltmc`: exit 0 with the filter list;
- capability evidence:
  `Saved/SightWeaveM2P4/EtwCalibration/admin-uac-final02/capability.json`.

The account is a member of `Performance Log Users`, but local UE 5.8 source in
`EventTracingForWindows.cpp` explicitly leaves that membership check as a
TODO. `FPlatformEvents::CanEnable` accepts only an elevated token, the trace
channel says Windows game/editor runtime should run as administrator, and
`StartETW` maps `ERROR_ACCESS_DENIED` to "Administrator rights required for
ETW". UE emits QPC-based `(StartTime, EndTime, ThreadId, CoreNumber)` intervals
through `PlatformEvent.ContextSwitch`; TraceServices can enumerate them per
thread, but only after the kernel session succeeds.

M2P.4 now uses WPR `GeneralProfile.Verbose.File` plus the Windows kernel Thread
provider (`3d6fa8d1-fe05-11d0-9dda-00c04fd7ba7c`), CSwitch opcode 36, and
ReadyThread opcode 50. Test-only markers carry PID, TID, sample ID, QPC
begin/end/frequency, exact stage invocation, wall, cycles, and core endpoints.
The offline consumer validates Thread lifecycle ownership and reconstructs
running, ready, blocked, preemption, migration, and per-core residency. Trace
loss, clock mismatch, ownership conflict, malformed events, or an unclosed
timeline are hard Unknown failures. Raw cycles remain auxiliary evidence and
are never converted using an average frequency.

Final elevated calibration `admin-uac-final02` passed:

- 188/188 marker timelines closed; 0 events lost, 0 buffers lost, 0 Unknown;
- empty probe on-CPU p50/p99 0.0/3.0 us;
- fixed compute on-CPU p50/p99 116.3/127.4 us;
- fixed memory on-CPU p50/p99 168.5/187.3 us;
- sleep wall/on-CPU/blocked p50 9800.2/39.8/9649.6 us;
- loaded yield wall/on-CPU/ready p50 81301.7/2133.1/79159.1 us;
- loaded yield: 2,840 context switches/preemptions and 1,184 migrations;
- test-only stage-marker probe on-CPU p50/p95/p99 2.5/4.1/6.6 us;
- synthetic tests prove event-loss fail-closed and cross-PID/TID rejection;
- summary:
  `Saved/SightWeaveM2P4/EtwCalibration/admin-uac-final02/summary.json`.

The one-process formal smoke `admin-uac-smoke01` also passed all capture and
analysis gates: 10 Batch distributions/1,010 totals, 314 Door totals, 10,512
total/control/stage markers, 0 loss, 0 Unknown. It is not the formal decision
matrix. Its preliminary Batch on-CPU p50/p95/p99/max was
107.2/166.1/232.5/784.4 us; broad-door on-CPU was
251.8/297.2/308.9/320.1 us. This indicates both production decision branches
may open, but no Runtime source may change until all ten independent processes
complete. Smoke summary:
`Saved/SightWeaveM2P4/EtwAttribution/admin-uac-smoke01/summary.json`.

The fixed ten-process matrix `admin-uac-formal01` is complete:

- Batch: 100 distributions/10,100 totals; authoritative on-CPU
  p50/p95/p99/max 93.7/148.4/191.7/787.5 us; intrinsic gate passes;
- Broad Door: 10 processes/1,010 totals; on-CPU p50/p95/p99/max
  160.2/277.4/351.1/522.6 us; intrinsic p99 gate fails;
- all Door workloads: 3,140 totals;
- total/control/stage markers: 105,120;
- loss/buffer loss/unclosed/Unknown: 0/0/0/0;
- Batch final classification: 9,947 within, 81 Plugin CPU, 72 Scheduler;
- Broad Door: 925 within, 79 Plugin CPU, 6 Scheduler;
- 69/79 Broad Door Plugin CPU tails have vision solve as the largest
  stage-growth contributor;
- formal classification:
  `Docs/SIGHTWEAVE_M2P4_ETW_CLASSIFICATION.md` and
  `Saved/SightWeaveM2P4/EtwAttribution/admin-uac-formal01/authority-classification.json`.

The production decision is exact: do not rewrite Batch because its aggregate
intrinsic p99 is <=200 us; design and implement Dynamic angular-sector update
because Broad Door intrinsic p99 is >=250 us. Production source is still
unchanged at this handoff point.

## Current tail boundary inherited from M2P.3

- Batch512: 100 ordinary distributions/10,100 samples; aggregate wall
  p50/p95/p99/max 92.6/141.5/177.8/1468.7 us. Of 62 samples above 200 us, the
  provisional non-ETW labels were 9 plugin, 4 scheduler, 29 migration/frequency,
  and 20 Unknown. Only 86/100 distributions met every wall limit.
- Broad four-vision/two-illumination dynamic door: ten process p99 values
  327.9/388.7/316.8/465.4/335.7/316.5/305.5/276.0/277.1/513.4 us; 0/10 met
  250 us. Of 78 samples above 250 us, provisional labels were 53 plugin, 4
  scheduler, 12 migration/frequency, and 9 Unknown.
- Vision solve was the dominant dynamic stage; however the M2P.4 production
  decision gate requires authoritative on-CPU evidence, not raw-cycle
  heuristics.

## Planned evidence and decision gate

1. [done] Add test-only sample markers containing unique ID, PID/TID, QPC begin/end,
   operation, run/distribution/sample, stage, wall/cycles, and core endpoints.
2. [done] Add a fail-closed offline analyzer keyed by PID + TID + QPC interval + ID.
3. [done] Calibrate empty/compute/memory/sleep/yield/migration/preemption cases and
   prove loss/cross-process isolation before formal attribution.
4. [done] Capture and classify the fixed Batch100/Dynamic10 matrix without
   affinity or priority changes.
5. Implement an exact dynamic angular-sector architecture and production path
   only if Dynamic on-CPU p99 remains at or above 250 us. Likewise modify Batch
   only if authoritative stage on-CPU evidence proves a >200 us plugin tail.
6. Re-run both 36,000-frame soaks and the complete validation matrix before
   finalization.

## Checkpoints

- [x] `docs: start SightWeave elevated tail analysis` (`0fcff72`, pushed)
- [x] `test: add SightWeave elevated context-switch attribution` (`b8f19ac`, pushed)
- [ ] `docs: record SightWeave authoritative tail classification` (document,
  interval timelines, and reusable classifier complete; commit/push next)
- [ ] `feat: add exact dynamic occluder sector updates` (only if authorized by
  ETW evidence)
- [ ] `perf: close confirmed SightWeave intrinsic tails` (only if supported)
- [ ] `test: expand SightWeave incremental authority coverage`
- [ ] `test: finalize SightWeave CPU performance contracts`
- [ ] `docs: record SightWeave CPU authority finalization`

After every checkpoint, inspect the diff, stage exact paths only, exclude
`Darkwell.uproject`, run `git diff --cached --check`, commit, push immediately,
and verify the remote SHA. Preserve failed captures and reports.

## Commands executed

```powershell
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight fetch origin --prune
git -c safe.directory=D:/UE_projects/LastLight switch codex/m2p3-sightweave-tail-latency-finalization
git -c safe.directory=D:/UE_projects/LastLight pull --ff-only origin codex/m2p3-sightweave-tail-latency-finalization
git -c safe.directory=D:/UE_projects/LastLight rev-parse HEAD
git -c safe.directory=D:/UE_projects/LastLight lfs pull
git -c safe.directory=D:/UE_projects/LastLight lfs status
git -c safe.directory=D:/UE_projects/LastLight switch -c codex/m2p4-sightweave-etw-dynamic-sector
whoami
whoami /groups
whoami /priv
fltmc
wpr -status
wpr -profiles
wpr -start GeneralProfile -filemode
pwsh -NoProfile -ExecutionPolicy Bypass -File .\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8
# UAC-approved child:
pwsh -NoProfile -ExecutionPolicy Bypass -File .\Scripts\RunSightWeaveM2P4EtwCalibration.ps1 -Label admin-uac-final02 -EngineRoot D:\UE_5.8
pwsh -NoProfile -ExecutionPolicy Bypass -File .\Scripts\RunSightWeaveM2P4EtwAttribution.ps1 -RunCount 1 -Label admin-uac-smoke01 -EngineRoot D:\UE_5.8
# UAC-approved child:
pwsh -NoProfile -ExecutionPolicy Bypass -File .\Scripts\RunSightWeaveM2P4EtwAttribution.ps1 -RunCount 10 -Label admin-uac-formal01 -EngineRoot D:\UE_5.8
pwsh -NoProfile -ExecutionPolicy Bypass -File .\Scripts\SummarizeSightWeaveM2P4EtwClassification.ps1 -RunRoot .\Saved\SightWeaveM2P4\EtwAttribution\admin-uac-formal01 -AttributionPath .\Saved\SightWeaveM2P4\EtwAttribution\admin-uac-formal01\attribution-timeline-all.csv
```

## Exact recovery command

```powershell
cd D:\UE_projects\LastLight
git -c safe.directory=D:/UE_projects/LastLight switch codex/m2p4-sightweave-etw-dynamic-sector
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight rev-parse HEAD
pwsh -NoProfile -ExecutionPolicy Bypass -File .\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8
```

Then read this document, `Docs/SIGHTWEAVE_M2P4_ETW_CLASSIFICATION.md`, and
`Docs/SIGHTWEAVE_M2P3_FINAL_VALIDATION.md`. Commit/push the classification
checkpoint if still unstaged. Next write
`Docs/SIGHTWEAVE_M2P4_DYNAMIC_SECTOR_ARCHITECTURE.md` before changing Runtime
source. Batch optimization remains forbidden; only the evidence-backed Dynamic
vision sector path is authorized.

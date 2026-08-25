# SightWeave M2P.4 Elevated ETW and Dynamic Sector Handoff

## Status

`IN_PROGRESS` (ETW authority gate currently blocked; production algorithm
changes are forbidden until the gate is satisfied.)

M2P.4 is limited to elevated ContextSwitch attribution, evidence-driven CPU
tail closure, and an exact incremental dynamic-occluder angular-sector update
only if authoritative on-CPU evidence proves it is required. Do not start M3,
GPU masks, post processing, memory textures, DARKWELL gameplay integration,
`/Game/Maps/L_Prototype`, a `main` merge, or unrelated work.

## Git state

- Baseline branch: `codex/m2p3-sightweave-tail-latency-finalization`
- Verified baseline SHA: `e3e5a833e58ce4571653fcef0f7b44698ae80dae`
- Working branch: `codex/m2p4-sightweave-etw-dynamic-sector`
- Latest safe SHA: `e3e5a833e58ce4571653fcef0f7b44698ae80dae`
- The company workstation's local-only `Darkwell.uproject`
  `EngineAssociation` difference is preserved and must never be staged.

## Administrator and ETW capability checkpoint

The task stated that Codex App had been launched as administrator, but the
actual command-host token measured on 2026-08-25 does not match that premise:

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

A bounded `Start-Process -Verb RunAs` helper was attempted. It would have run
only `whoami`/`fltmc`, a minimal WPR GeneralProfile capture, and immediate stop
under `Saved/SightWeaveM2P4/Capability`. The helper never reached its script
entry: no transcript or ETL was created, and no WPR session remained. The
automation host therefore cannot confirm or interact with the secure-desktop
UAC elevation path.

The account is a member of `Performance Log Users`, but local UE 5.8 source in
`EventTracingForWindows.cpp` explicitly leaves that membership check as a
TODO. `FPlatformEvents::CanEnable` accepts only an elevated token, the trace
channel says Windows game/editor runtime should run as administrator, and
`StartETW` maps `ERROR_ACCESS_DENIED` to "Administrator rights required for
ETW". UE emits QPC-based `(StartTime, EndTime, ThreadId, CoreNumber)` intervals
through `PlatformEvent.ContextSwitch`; TraceServices can enumerate them per
thread, but only after the kernel session succeeds.

This is fail-closed evidence. No ContextSwitch trace, authoritative on-CPU
microseconds, off-CPU intervals, ReadyThread state, migration residency, loss
accounting, or sample timeline exists yet. Raw cycles must not be converted to
time and the M2P.3 classifications must not be upgraded on this basis.

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

1. Add test-only sample markers containing unique ID, PID/TID, QPC begin/end,
   operation, run/distribution/sample, stage, wall/cycles, and core endpoints.
2. Add a fail-closed offline analyzer keyed by PID + TID + QPC interval + ID.
3. Calibrate empty/compute/memory/sleep/yield/migration/preemption cases and
   prove loss/cross-process isolation before formal attribution.
4. Capture the fixed Batch100/Dynamic10 matrix without affinity or priority
   changes once an actually elevated kernel ETW session is available.
5. Implement an exact dynamic angular-sector architecture and production path
   only if Dynamic on-CPU p99 remains at or above 250 us. Likewise modify Batch
   only if authoritative stage on-CPU evidence proves a >200 us plugin tail.
6. Re-run both 36,000-frame soaks and the complete validation matrix before
   finalization.

## Checkpoints

- [x] `docs: start SightWeave elevated tail analysis` (this document;
  commit/push pending)
- [ ] `test: add SightWeave elevated context-switch attribution`
- [ ] `docs: record SightWeave authoritative tail classification`
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
```

## Exact recovery command

```powershell
cd D:\UE_projects\LastLight
git -c safe.directory=D:/UE_projects/LastLight switch codex/m2p4-sightweave-etw-dynamic-sector
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight rev-parse HEAD
whoami /groups
fltmc
wpr -status
```

Then read this document and
`Docs/SIGHTWEAVE_M2P3_FINAL_VALIDATION.md`. Confirm the token is high-integrity,
`fltmc` succeeds, and an elevated ContextSwitch calibration capture can be
started before authorizing any production algorithm change.

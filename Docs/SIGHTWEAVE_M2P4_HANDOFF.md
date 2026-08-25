# SightWeave M2P.4 Elevated ETW and Dynamic Sector Handoff

## Status

`IN_PROGRESS — PRODUCTION EDITS PAUSED` (elevated ETW authority complete;
Batch gate passes and the Broad Dynamic Door gate authorizes an exact
incremental angular-sector design. Repeated UE-bundled `dotnet.exe`
`0xe0434352` failures must be resolved before another build or production edit.)

M2P.4 is limited to elevated ContextSwitch attribution, evidence-driven CPU
tail closure, and an exact incremental dynamic-occluder angular-sector update
only if authoritative on-CPU evidence proves it is required. Do not start M3,
GPU masks, post processing, memory textures, DARKWELL gameplay integration,
`/Game/Maps/L_Prototype`, a `main` merge, or unrelated work.

## Git state

- Baseline branch: `codex/m2p3-sightweave-tail-latency-finalization`
- Verified baseline SHA: `e3e5a833e58ce4571653fcef0f7b44698ae80dae`
- Working branch: `codex/m2p4-sightweave-etw-dynamic-sector`
- Latest pushed diagnostic SHA before the post-Verify experiment: `bf82be522f7528ccc6646966d52e5427243c0182`
- The company workstation's local-only `Darkwell.uproject`
  `EngineAssociation` difference is preserved and must never be staged.

The architecture checkpoint is pushed. Four uncommitted Runtime files contain
an initial, unverified sector implementation and must not be staged or extended
while the build-host crash pause is active:

- `Plugins/SightWeave/Source/SightWeaveRuntime/Private/SightWeaveGeometry.cpp`;
- `Plugins/SightWeave/Source/SightWeaveRuntime/Private/SightWeaveOptimizedSolveCache.h`;
- `Plugins/SightWeave/Source/SightWeaveRuntime/Private/SightWeaveWorldSubsystem.cpp`;
- `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveWorldSubsystem.h`.

## UE bundled dotnet crash diagnostic (2026-08-25)

Production work was frozen immediately after the user reported repeated native
Windows error dialogs. No crashing command was relaunched, and no dialog was
hidden, dismissed, or automated during this diagnostic.

### Fault identity

The crashing executable is the Unreal Engine 5.8 bundled .NET host, not system
.NET, Codex, or the M2P.4 ETW analyzer:

- exact executable:
  `D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe`;
- file version: `10.0.726.21808`; product version: `.NET 10.0.7`;
- failing signed exit code: `-532462766`, exactly `0xE0434352`;
- system x64/x86 hosts are both version `9.0.525.21509`, so they do not match;
- the installed Codex package
  `C:\Program Files\WindowsApps\OpenAI.Codex_26.818.8289.0_x64__2p2nqsd0c76g0`
  contains zero `dotnet.exe` files; its live command host is native
  `codex.exe` plus bundled `pwsh.exe`;
- the ETW capture scripts invoke `pwsh.exe`, `wpr.exe`, and
  `UnrealEditor-Cmd.exe`; the analyzer is C++ inside
  `UnrealEditor-SightWeaveTests.dll`. No M2P.4 script or analyzer invokes or
  generates a dotnet host.

The direct version match is corroborated by Application/WER record 49201 at
12:32:42: `RADAR_PRE_LEAK_64`, `P1=dotnet.exe`,
`P2=10.0.726.21808`, report ID
`477143e6-19e3-46d5-ad6c-9356623a7f28`. That record is a resource-leak
diagnostic, not a crash report, but its exact host version matches only UE's
bundled dotnet on this machine.

### Command line and parent chain

The latest failed wrapper invocation was:

```text
pwsh.exe -NoProfile -ExecutionPolicy Bypass -File D:\UE_projects\LastLight\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8
```

`BuildEditor.ps1` invoked:

```text
D:\UE_5.8\Engine\Build\BatchFiles\Build.bat DarkwellEditor Win64 Development D:\UE_projects\LastLight\Darkwell.uproject -WaitMutex -FromMsBuild
```

`GetDotnetPath.bat` sets `PATH` and `DOTNET_ROOT` to the UE bundled 10.0
directory and disables multilevel lookup. `Build.bat` lines 75-80 then execute
the effective faulting command:

```text
D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe D:\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll DarkwellEditor Win64 Development D:\UE_projects\LastLight\Darkwell.uproject -WaitMutex -FromMsBuild
```

The reconstructed parent chain is:

```text
C:\Program Files\WindowsApps\OpenAI.Codex_26.818.8289.0_x64__2p2nqsd0c76g0\app\resources\codex.exe
  -> C:\Users\defiler919\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe
  -> C:\Windows\System32\cmd.exe (Build.bat)
  -> D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe (UnrealBuildTool.dll)
```

Historical Security 4688 process creation auditing contained no matching
records, and no Kernel-Process/Sysmon operational log was enabled, so the
transient PID/parent PID cannot be recovered after exit. The paths, arguments,
and chain above are reconstructed from the exact Codex invocation, the
repository wrapper, UE batch files, and UBT's own command log rather than a
4688 record.

### Windows event evidence

System `Application Popup` event 26 records the repeated failures with the same
exception address `0x00007FF9C3D2187A`:

| Local time | System record ID |
|---|---:|
| 12:23:04 | 3324282 |
| 12:54:15 | 3324663 |
| 12:54:56 | 3324672 |
| 12:56:53 | 3324697 |
| 13:50:16 | 3325348 |
| 13:52:30 | 3325377 |
| 14:48:37 | 3326055 |

Each message is `dotnet.exe - Application Error` / unknown software exception
`0xe0434352`. The last UBT log started at 14:27:57 with the exact DarkwellEditor
command above and internally recorded `Result: Succeeded` after 10.23 seconds,
but the wrapper later returned `-532462766`; therefore the Editor build is not
accepted as successful evidence. The current Runtime edits were not compiled
into `UnrealEditor-SightWeaveRuntime.dll` (its timestamp remained 12:40:11).

Application log search from 12:00 through 15:00 produced:

- `.NET Runtime` 1026 matching dotnet/UBT: **0**;
- `Application Error` 1000 matching dotnet/UBT: **0**;
- WER 1001 crash matching dotnet/UBT: **0**;
- WER 1001 `RADAR_PRE_LEAK_64` matching dotnet: **1** (record 49201 above).

No dotnet/UBT report directory exists in WER ReportArchive, ReportQueue, or the
user WER directory. Thus the requested 1026/1000/1001 crash triplet was not
emitted; the authoritative crash evidence currently available is System event
26 plus the exact native exit code. This absence is recorded explicitly rather
than treating the RADAR report as a crash record.

### Epic Verify and restart recovery baseline (2026-08-25)

Epic Games Launcher Verify completed and Windows was restarted before this
recovery experiment. No Unreal Editor, Visual Studio compiler, Live Coding,
UBT, bundled/system dotnet, or UBA process was present at the preflight. Git
remained on `codex/m2p4-sightweave-etw-dynamic-sector` at
`bf82be522f7528ccc6646966d52e5427243c0182`, tracking the matching origin
branch. LFS had no pending object, the five local modifications were unchanged,
and `Darkwell.uproject` remains excluded from staging.

The preserved Runtime files were readable, contained no conflict markers, and
had these post-restart SHA-256 values:

| File | Bytes | SHA-256 |
|---|---:|---|
| `SightWeaveGeometry.cpp` | 90,118 | `29C7D2E260F6DDF05D02AE0FD2FBD7E92B9D928BE00552142FFAE5AC03430944` |
| `SightWeaveOptimizedSolveCache.h` | 3,805 | `04E562F2A524A92E69EF775580A5C0785ADDA3CBD56D7EDC62EBEF27F42908DC` |
| `SightWeaveWorldSubsystem.cpp` | 105,121 | `15FE63959F5B1B7F0CF92E8DC1DE6636499E2654EB2FC439FCC4C1B9A00EA0A9` |
| `SightWeaveWorldSubsystem.h` | 20,509 | `AE8DB63496B2DDE618616E3CE01C9C84C615BAF8EAFAE9FDBA3D7AB62D04E6F5` |

Post-Verify bundled file fingerprints are saved at
`Saved/SightWeaveM2P4/Toolchain/epic-verify-20260825/baseline/bundled-file-fingerprints.json`:

| File | Version | Last write | SHA-256 |
|---|---|---|---|
| `D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe` | `10.0.726.21808` / `10.0.7` | `2026-08-24 10:18:00` | `A8D3105441B568CFD44AC5EAB8C0FC190CDEFB0047E3E84E49CBCE819A197A7A` |
| `...\host\fxr\10.0.7\hostfxr.dll` | `10.0.726.21808` / `10.0.7` | `2026-08-24 10:18:00` | `D6CB726D200D3468360C85454728884186DEA19BDBD971C9B8BC1E6EB5897749` |
| `...\shared\Microsoft.NETCore.App\10.0.7\coreclr.dll` | `10.0.726.21808` / `10.0.7` | `2026-08-24 10:18:16` | `8B57354E0D877B210A34384A5A97E1F6BDCEC43E32173173D54A3A9F4262C8BD` |
| `D:\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll` | `5.8.0.0` | `2026-08-24 10:17:52` | `B0931427529B907EEA171F1913ED8A50C5753A3CAE733AC2773BE537F633D1A8` |

Every timestamp predates Verify/restart, so Verify produced no observable
replacement. The pre-Verify diagnostic did not capture SHA-256 for all four
files; therefore only the timestamps/versions can be compared for all four,
and a cryptographic before/after equality claim is deliberately not made.
No engine file was replaced and system dotnet/workload restore were not used.

The bundled smoke command
`D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe --info`
ran from `2026-08-25T16:01:25.5376737+08:00` through
`2026-08-25T16:01:28.7597232+08:00`, exited `0`, and produced zero System 26
and zero Application 1026/1000/1001 events. It reported SDK `10.0.203`, host
and runtimes `10.0.7`, RID `win-x64`, no workloads, and bundled-only base/runtime
paths. Full output and metadata are in the adjacent `dotnet-info.txt` and
`dotnet-info-summary.json` files under the baseline directory.

UE 5.8 source confirms the correct second experiment switch. In
`Engine/Source/Programs/UnrealBuildTool/Configuration/BuildConfiguration.cs`,
`bAllowUBAExecutor` is mapped to `-UBA`/`-NoUBA`; the older
`bAllowUBALocalExecutor` field is deprecated. In `ExecutorFactory.cs`, UE 5.8
states that it still uses the UBA executor but disables detouring to mirror
legacy behavior when this setting is false. Consequently group B uses the
official `-NoUBA` switch and is described precisely as the no-detour/legacy
path, not as proof that no UBA process exists.

The existing local-only
`Saved/UnrealBuildTool/BuildConfiguration.xml` was preserved byte-for-byte
(length 132, SHA-256
`1E7CD8512DEDF37D7B103C5592F5600433E7FE97DA23434E69990F8A3370508A`); it
contains an empty `Configuration` element and is not needed for the command-line
experiment. The serial experiment harness is
`Scripts/RunSightWeaveM2P4ToolchainExperiment.ps1`. Builds use an isolated
detached worktree at the safe diagnostic commit so the four unverified Runtime
edits and local `Darkwell.uproject` association cannot affect toolchain
classification. Raw build output, event windows, WER/dump inventories, and UBT
artifacts are retained under
`Saved/SightWeaveM2P4/Toolchain/epic-verify-20260825/`.

### Pause boundary

The user authorized only the bounded post-Verify bundled-dotnet/UBT recovery
experiment described above. Production edits, ETW capture/analyze, automation
tests, packaging, and the full M2P.4 verification matrix remain paused. Preserve
the four uncommitted Runtime files and the local-only `Darkwell.uproject`
association. Resume production validation only after the strict toolchain gate
(`Result: Succeeded`, outer exit `0`, and no exception popup/event) closes.

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
because Broad Door intrinsic p99 is >=250 us. The required architecture now
exists at `Docs/SIGHTWEAVE_M2P4_DYNAMIC_SECTOR_ARCHITECTURE.md`. Production
source was unchanged at the classification/architecture checkpoints; the later
unverified edits are preserved under the dotnet crash pause documented above.

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
- [x] `docs: record SightWeave authoritative tail classification` (`88937aa`,
  pushed)
- [x] dynamic-sector architecture contract (`666c6a9`, pushed before Runtime
  changes)
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
```

Then read this document, `Docs/SIGHTWEAVE_M2P4_ETW_CLASSIFICATION.md`,
`Docs/SIGHTWEAVE_M2P4_DYNAMIC_SECTOR_ARCHITECTURE.md`, and
`Docs/SIGHTWEAVE_M2P3_FINAL_VALIDATION.md`. Do not build, test, or modify
production source until the UE bundled dotnet crash pause above is explicitly
cleared. Batch optimization remains forbidden.

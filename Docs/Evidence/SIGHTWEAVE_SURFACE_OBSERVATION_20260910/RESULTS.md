# Surface Observation contract verification — 2026-09-10

Source parent: `3447970d799a09080e5bdb91255ed07b4a881bbd`; tested working changes are the two SurfaceObservation test files committed alongside this report. Runtime/Memory/P4 and assets were not modified. The previous audit is included for provenance.

- Full target build: `Scripts/BuildEditor.ps1`, DarkwellEditor Win64 Development, **Succeeded**, not Live Coding. See `build.log`.
- NullRHI automation: **22 succeeded, 0 succeededWithWarnings, 0 failed, 0 notRun**. See `final-report.json`: 4 new contract/characterization tests, 3 StaticKnowledge and 15 M2.Query tests.
- No D3D12/pixel, interactive gameplay, performance, persistence migration or new production receiver acceptance is claimed. Spec is an independent analytic reference. KnownGaps passes by reproducing the current limitation.
- Environment actually reported UE **5.8.2-56702186**, engine root `D:\UE_5.8`, versus AGENTS.md's 5.8.1 reference. Build reports non-preferred MSVC 14.51.36256. No engine/config change made.

Reproduction from repository root in PowerShell:

```powershell
./Scripts/BuildEditor.ps1
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE_projects\LastLight\Darkwell.uproject' -unattended -nop4 -nosound -nullrhi '-ExecCmds=Automation RunTests Darkwell.SightWeave.SurfaceObservation+Darkwell.SightWeave.StaticKnowledge+SightWeave.M2.Query' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\SurfaceObservationFinalReport' '-abslog=D:\UE_projects\LastLight\Saved\SurfaceObservationFinal.log'
```

Read the exported JSON counts, not just process exit status: the editor returned zero even on the initial failed fixture run.

Failures retained rather than hidden:

1. Initial test compilation used ambiguous braced arguments to templated FBox constructors; corrected to explicit FVector endpoints. First broad build also emitted pre-existing include-order/deprecation/uninitialized-variable diagnostics in unrelated source. Those sources were not edited. Final full-target invocation succeeded; it is not a clean-from-scratch warning-free rebuild.
2. First automation run: 3/4 passed; PolicyAndMemory failed two real-store write assertions because fixture snapshot BodyRadius=0 violated existing snapshot validity. Corrected fixture metadata to positive radius and added explicit IsValid/IsReady assertion. Runtime has no body source until the later dedicated bypass test, and HardAuthority remains the coverage source; no production behavior or thresholds were changed. Original `initial-fixture-failure.json` is preserved.
3. Startup log contains engine UnifiedErrorTest/Condition-failed diagnostics before the selected test session and an existing scalability priority warning. Final selected tests have no warning/error events; this report does not describe the entire engine startup log as clean.

Final source review: changes limited to test-only helper/test source, contracts/audit and these evidence files. `git diff --check` passed. No `.uasset/.umap`, gameplay implementation, build configuration or stable tag changes.

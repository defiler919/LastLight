# SightWeave M1 handoff

## Status

- State: **IN PROGRESS**
- Baseline branch: `design/independent-vision-plugin`
- Baseline SHA: `d3060604db18f406f8eecef6fff7d3602c82ee85`
- Working branch: `codex/m1-sightweave-skeleton-lab`
- Current phase: Checkpoint 2 — lab map and complete M1 test suite passed; commit and push pending
- Last completed checkpoint: Checkpoint 1 committed and pushed
- Next command: `git -c safe.directory=D:/UE_projects/LastLight add Docs/SIGHTWEAVE_M1_HANDOFF.md Plugins/SightWeave/Content/Python/create_sightweave_lab.py Plugins/SightWeave/Content/Maps/L_SightWeave_Lab.umap Plugins/SightWeave/Source/SightWeaveTests/Private/SightWeaveM1Tests.cpp`

## Approved identity

- Plugin: `SightWeave`
- Runtime module: `SightWeaveRuntime`
- Editor module: `SightWeaveEditor`
- Tests module: `SightWeaveTests`
- Public/internal C++ prefix: `SightWeave`
- Plugin content root: `/SightWeave/`
- Lab map: `/SightWeave/Maps/L_SightWeave_Lab`

## M1 minimum public API inventory

- Strong handles: `FSightWeaveVisionSourceHandle`, `FSightWeaveIlluminationSourceHandle`, and `FSightWeaveSubjectRevealHandle`; each has an invalid default and generation-safe identity.
- Revision: `FSightWeaveRevision`, monotonically changed by authoritative registration, update, and removal.
- Floor/height: `FSightWeaveFloorId` and `FSightWeaveHeightRange` remain independent of source descriptions.
- Vision source: `FSightWeaveVisionSourceDescription` with shape, active state, floor/height, illumination policy, and full compatibility profile.
- Legal illumination source: `FSightWeaveIlluminationSourceDescription` with shape, active state, floor/height, and emitted compatibility identifiers.
- Compatibility: `FSightWeaveIlluminationCompatibilityProfile` retains a normalized complete accepted set; it is not a global boolean or prematurely fixed GPU bit layout.
- Subject reveal: `FSightWeaveSubjectRevealSpecification` and a distinct reveal handle, kept outside the normal vision registry.
- Queries: `FSightWeaveVisibilityQueryResult` with explicit authoritative, not-ready/unsupported, invalid-handle, and invalid-input/floor status; M1 returns not-ready rather than visible.
- Settings: `USightWeaveSettings` containing only neutral M1-safe defaults.
- World lifecycle: `USightWeaveWorldSubsystem` for register/update/unregister, validation, ownership, revision, cleanup, and explicit placeholder queries without per-frame ticking.

## Completed checks

- Read repository guidance, all five vision design/baseline/audit documents, decisions, project descriptor, and build script.
- Confirmed `HEAD` and `origin/design/independent-vision-plugin` both equal the baseline SHA.
- Confirmed the only pre-existing worktree change is the unstaged `Darkwell.uproject` EngineAssociation GUID.
- Confirmed the index is empty and Git LFS has no pending object.
- Fetched `origin` successfully.
- Created and pushed `codex/m1-sightweave-skeleton-lab`; local HEAD, upstream, and remote branch initially matched the baseline SHA.
- Approved the formal plugin/module/API identity as SightWeave.
- Created the content-capable `SightWeave.uplugin` descriptor with Runtime, Editor, and editor-only Tests modules using UE 5.8-supported module types.
- Implemented neutral strong handles, floor/height/source/compatibility/reveal/query types, settings, copied per-world registries, owner cleanup, revisions, and explicit not-ready queries.
- Added 21 `SightWeave.M1.*` automation definitions covering the M1 foundation, lab loading, and dependency isolation.
- Created `/SightWeave/Maps/L_SightWeave_Lab` through Unreal Editor APIs and verified it loads as a `UWorld`.
- Added labeled fixtures for straight/diagonal walls, a right-angle corner, closed room, doorway, rotating-door placeholder, height bands, overlapping floor IDs, vision/legal-illumination overlap, visible/infrared isolation, body-circle bypass, and two remote cones.
- Rebuilt the editor after the sole test-code fix and passed all 21 M1 automation tests with zero report warnings/errors.

## Modified files

- `Docs/DECISIONS.md`
- `Docs/VISION_SYSTEM_REQUIREMENTS.md`
- `Docs/VISION_SYSTEM_ARCHITECTURE.md`
- `Docs/VISION_SYSTEM_MIGRATION_PLAN.md`
- `Docs/SIGHTWEAVE_M1_HANDOFF.md`
- `Plugins/SightWeave/SightWeave.uplugin`
- `Plugins/SightWeave/README.md`
- `Plugins/SightWeave/Source/SightWeaveRuntime/`
- `Plugins/SightWeave/Source/SightWeaveEditor/`
- `Plugins/SightWeave/Source/SightWeaveTests/`
- `Plugins/SightWeave/Content/Python/create_sightweave_lab.py`
- `Plugins/SightWeave/Content/Maps/L_SightWeave_Lab.umap`

## Commands executed

```powershell
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight log -5 --oneline
git -c safe.directory=D:/UE_projects/LastLight diff -- Darkwell.uproject
git -c safe.directory=D:/UE_projects/LastLight diff --cached
git -c safe.directory=D:/UE_projects/LastLight lfs status
git -c safe.directory=D:/UE_projects/LastLight fetch origin
git -c safe.directory=D:/UE_projects/LastLight switch -c codex/m1-sightweave-skeleton-lab
git -c safe.directory=D:/UE_projects/LastLight push -u origin codex/m1-sightweave-skeleton-lab
rg -n -U '"Name"\\s*:\\s*"[^"]*(Tests|Test)[^"]*"[\\s\\S]{0,240}?"Type"\\s*:\\s*"[^"]+"' D:\UE_5.8\Engine\Plugins -g '*.uplugin'
Scripts/BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecutePythonScript=D:\UE_projects\LastLight\Plugins\SightWeave\Content\Python\create_sightweave_lab.py' '-AbsLog=D:\UE_projects\LastLight\Saved\Logs\SIGHTWEAVE_M1_LAB_FINAL_CREATE.log'
& D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UE_projects\LastLight\Darkwell.uproject -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave.M1' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM1_20260824_Rerun' '-AbsLog=D:\UE_projects\LastLight\Saved\Logs\SIGHTWEAVE_M1_AUTOMATION_RERUN.log'
```

## Build and test results

- Editor build: **passed** on 2026-08-24 from `17:17:00.5630187+08:00` to `17:17:23.9905069+08:00` (23.43 seconds), process exit code 0, UBT `Result: Succeeded`.
- Build warnings: 1 UBT warning — MSVC `14.51.36256` is newer than the UE preferred `14.50.35717`; no compile/link warning was emitted from SightWeave source.
- Build errors: 0.
- Generated module binaries verified locally: `UnrealEditor-SightWeaveRuntime.dll`, `UnrealEditor-SightWeaveEditor.dll`, and `UnrealEditor-SightWeaveTests.dll` under ignored plugin `Binaries/Win64`.
- Editor rebuild after the test-only fix: **passed** from `17:22:39.4699221+08:00` to `17:22:44.6069202+08:00` (5.14 seconds), exit code 0, UBT `Result: Succeeded`, 1 compiler-version warning, 0 errors.
- SightWeave M1 first run: discovered/ran 21; passed 20, failed 1 (`SightWeave.M1.OwnershipCleanup`), warnings 2, errors 26 in that failed report entry, skipped/not-run 0. Root cause was the test directly constructing UE 5.8's abstract base `UObject`; it produced a handled ensure before the owner-cleanup assertions. Fixed by using a concrete `USceneComponent` owner.
- SightWeave M1 final rerun: **21 discovered, 21 run, 21 passed, 0 failed, 0 succeeded-with-warnings, 0 skipped/not-run, 0 in-process**; process exit code 0; report duration 0.1733 seconds. JSON: `Saved/AutomationReports/SightWeaveM1_20260824_Rerun/index.json`.
- Lab map generation: final idempotent Editor API run exited 0 and logged `SightWeave created and saved /SightWeave/Maps/L_SightWeave_Lab`; no post-startup Python/asset error. Map size after save: 120,714 bytes.
- Lab map load/dependency validation: passed inside `SightWeave.M1.LabMapLoads` and `SightWeave.M1.DependencyIsolation`; no `/Game/` or `/Script/Darkwell` asset dependency was reported.
- DARKWELL regression: not yet run for this branch.
- Lab map PIE: not yet run; headless load is covered by automation, and a separate smoke remains pending.
- Independent BuildPlugin: not yet run.

## Known warnings and blockers

- Git requires the command-local `-c safe.directory=D:/UE_projects/LastLight` option because repository ownership differs from the process user. No global Git configuration was changed.
- `Darkwell.uproject` has the expected machine-local EngineAssociation GUID change. It must remain unstaged and uncommitted.
- No current blocker.

## Commits

- `9b5ecbce40a0356c4c0e91c6c2f7293c912f8d16` — `docs: approve SightWeave plugin identity`
- `4aad75fcac89e56239d284499a1912306ae826e5` — `feat: add SightWeave plugin foundation`
- Pending Checkpoint 2: `test: add SightWeave M1 lab and lifecycle coverage`

## Remote synchronization

- Remote: `origin`
- Remote branch: `origin/codex/m1-sightweave-skeleton-lab`
- State: Checkpoints 0 and 1 are pushed; Checkpoint 2 is validated locally and pending commit/push.

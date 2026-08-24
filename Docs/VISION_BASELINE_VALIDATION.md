# Vision system baseline validation

Validation date: 2026-08-24 (Asia/Shanghai)

## Scope and evidence policy

This document records the repository and machine state observed before designing an independent vision plugin. Repository source at commit `46d9f9d093e3ceaf8afab71ff29367ab9b1ec2c1` is treated as authoritative. Historical build, automation, PIE, and performance claims are not repeated as current results unless they were reproduced during this validation.

No gameplay code, build configuration, material, map, or Unreal asset was changed. Generated files under `Saved` and `Intermediate` were produced by the attempted build/test commands and remain ignored by Git.

## Git baseline

| Check | Observed result |
| --- | --- |
| Initial branch | `main` |
| Initial HEAD | `46d9f9d (HEAD -> main, origin/main, origin/HEAD) Implement fog, aiming, movement, and facing interaction` |
| Initial worktree | Clean |
| Git LFS | No objects to push, commit, or stage |
| Design branch | Created locally as `design/independent-vision-plugin` from `46d9f9d` |
| Remote operations | None; no fetch, merge, or push was performed |

The first Git invocation was rejected by Git's dubious-ownership protection because the directory owner is `BUILTIN/Administrators` and the process user is `defiler919`. All subsequent Git commands used the command-local option `-c safe.directory=D:/UE_projects/LastLight`; global Git configuration was not changed.

## Repository guidance reviewed

The repository contains `AGENTS.md`. The following project guidance and planning/handoff documents were read before documentation changes:

- `AGENTS.md`
- `README.md`
- `Docs/DESIGN_PILLARS.md`
- `Docs/PROGRESS.md`
- `Docs/DECISIONS.md`
- `Docs/VISIBILITY.md`
- `Scripts/BuildEditor.ps1`

`Scripts/BuildEditor.ps1` accepts `-Configuration` (`Debug`, `DebugGame`, `Development`, `Shipping`, or `Test`) and optional `-EngineRoot`. It resolves the engine from `DARKWELL_UE_ROOT` or defaults to `D:\UE_5.8`, then invokes:

```text
Build.bat DarkwellEditor Win64 Development <project> -WaitMutex -FromMsBuild
```

## Editor build

Result: **failed before source compilation; Editor target not validated**.

Two attempts were made:

1. The sandboxed attempt could not create `Saved/UnrealBuildTool` or write `C:\Users\defiler919\AppData\Local\UnrealBuildTool\Trace.uba`. The script returned UnrealBuildTool exit code 6. This was a permissions failure, not a compiler result.
2. The same script was rerun with permission to write Unreal build state. UnrealBuildTool initialized successfully, but rejected Win64 because no valid Windows SDK was discoverable: minimum required `10.0.19041.0`, expected AutoSDK `10.0.22621.0`. It returned exit code 6 before compiling DARKWELL.

Additional machine evidence:

- Unreal Engine reports `5.8.1-56057345`.
- Visual Studio Community 2022 `17.14.36202.13` is registered at `D:\Program Files\Microsoft Visual Studio\2022\Community`.
- MSVC tool directories `14.38.33130` and `14.44.35207` exist.
- `HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots` has no `KitsRoot10` value, and no usable Windows SDK `bin`/`Lib` version directory was found.
- UnrealEditor also reported VC++ Redistributable `14.44.35211.0` as older than its required `14.50.35719.0`.
- `Binaries/Win64/UnrealEditor-Darkwell.dll` and a project target receipt were absent.

Required environment repair before a genuine full build: install a UE 5.8.1-compatible Windows 10/11 SDK (at least `10.0.19041.0`; the engine's AutoSDK metadata asks for `10.0.22621.0`), update the x64 VC++ Redistributable using the engine-provided installer, and rerun `Scripts/BuildEditor.ps1`. This validation did not install or alter machine dependencies.

## Unreal automation tests

### Source definitions

Exactly **24** Unreal automation tests are defined in source, all with `IMPLEMENT_SIMPLE_AUTOMATION_TEST` under `WITH_DEV_AUTOMATION_TESTS`:

| # | Test path |
| ---: | --- |
| 1 | `Darkwell.Player.Input.WeaponWheels` |
| 2 | `Darkwell.Player.Aim.PlanarDirection` |
| 3 | `Darkwell.Player.Aim.LimitedTurn` |
| 4 | `Darkwell.Player.Aim.CursorPlaneIntersection` |
| 5 | `Darkwell.Player.Movement.DirectionalSpeed` |
| 6 | `Darkwell.Player.Input.PrimaryFireGesture` |
| 7 | `Darkwell.Player.Interaction.FacingSelection` |
| 8 | `Darkwell.Gameplay.Save.SerializationRoundTrip` |
| 9 | `Darkwell.Gameplay.Visibility.FogKnowledge` |
| 10 | `Darkwell.Gameplay.Save.VersionCompatibility` |
| 11 | `Darkwell.Gameplay.Inventory.ItemDefinitions` |
| 12 | `Darkwell.Gameplay.Inventory.StackingAndSplitting` |
| 13 | `Darkwell.Gameplay.Inventory.ContainerTransfer` |
| 14 | `Darkwell.Gameplay.Inventory.CraftingTransaction` |
| 15 | `Darkwell.Gameplay.Resources.ShotgunReload` |
| 16 | `Darkwell.Gameplay.Resources.PickupCapacity` |
| 17 | `Darkwell.Gameplay.Resources.TorchDrain` |
| 18 | `Darkwell.Gameplay.Survival.PlayerDamage` |
| 19 | `Darkwell.Gameplay.Mission.Progression` |
| 20 | `Darkwell.Gameplay.Tags.NativeStates` |
| 21 | `Darkwell.Gameplay.Loadout.ReloadTorch` |
| 22 | `Darkwell.Gameplay.Loadout.RightToolGestures` |
| 23 | `Darkwell.Gameplay.Enemy.Intent` |
| 24 | `Darkwell.Gameplay.Enemy.Archetypes` |

Sources: `Source/Darkwell/Private/Tests/DarkwellPlayerMathTests.cpp` (7) and `Source/Darkwell/Private/Tests/DarkwellGameplayRuleTests.cpp` (17).

### Actual run result on this machine

The full `Darkwell` filter was launched through `UnrealEditor-Cmd.exe` with `-unattended`, `-NullRHI`, and `-TestExit`. UnrealEditor exited before loading the game module because `UnrealEditor-Darkwell.dll` was missing and the SDK failure prevented rebuilding it.

| Metric | Actual result |
| --- | ---: |
| Source definitions | 24 |
| Tests discovered by a loaded DARKWELL module | 0 |
| Tests run | 0 |
| Passed | 0 |
| Failed test cases | 0 |
| Skipped | 0 |
| Process result | Exit code 1 before test discovery |

There are no failing test names because no test case started. The process-level error was: game module `Darkwell` could not be found; confirm that it exists and has been compiled. Therefore **24/24 was not established**.

The full log is the generated, ignored file `Saved/Logs/VISION_BASELINE_AUTOMATION.log`.

### 23/23 handoff discrepancy

`Docs/PROGRESS.md` records a historical handoff result of 23/23. Current source has 24 definitions. The commit diff from the previous revision shows that the repository moved from 21 definitions to 24 by adding three player tests:

- `Darkwell.Player.Movement.DirectionalSpeed`
- `Darkwell.Player.Input.PrimaryFireGesture`
- `Darkwell.Player.Interaction.FacingSelection`

The handoff count changed from 21 to 23 in the same commit, so its denominator did not include one of those three new definitions. No durable automation report in the repository proves which 23 were discovered or run at handoff. The discrepancy is a documentation/counting drift; a successful rebuilt Editor run is still required to establish the executable result.

## Save version and documentation drift

The source-of-truth save contract is:

- `UDarkwellSaveGame::MinimumSupportedVersion = 1`
- `UDarkwellSaveGame::CurrentVersion = 6`
- versions 1 through 6 are accepted; versions below 1 or above 6 are rejected.

`UDarkwellSaveSubsystem::ApplyPendingLoad` gates restoration as follows:

- v3+: torch heat and lantern fuel;
- v4+: authoritative `ExploredFogCells`;
- v5+: fine `ExploredFogPresentationCells`;
- v6+: `ExploredFogPresentationCellSize`; older fine-memory saves are interpreted at 25 cm and migrated to the current 10 cm presentation cells.

Document comparison:

| Document | Description | Status against source |
| --- | --- | --- |
| `README.md` | Current continuation save v6 | Current |
| `Docs/PROGRESS.md` | Current save v6; v1-v5 supported | Current |
| `Docs/VISIBILITY.md` | v6 persists 100 cm and 10 cm fields; v5/v4 migration | Current |
| `Docs/DECISIONS.md` | Current choice says v4 and v1-v4 | Stale |

No file existed under `Saved/SaveGames` during validation, so an on-disk continuation payload could not be decoded. “Actual save version” here means the version emitted and accepted by current source, not a claim about a missing local save file.

## PIE and gameplay regression

PIE result: **not executed / not verified**.

Reason: the DARKWELL Editor module is absent, cannot be rebuilt without the Windows SDK, and UnrealEditor exits when asked to load the project. No editor-control path can start PIE until the module loads. Command-line engine startup is not a substitute for a gameplay PIE result.

Required manual regression after toolchain repair:

- player movement, directional speed, sprint-facing, and mouse aim;
- quick/held shotgun fire, two shots, dangerous reload, and ammunition state;
- torch passive light, swing, held deterrence, heat, and reload-lowered state;
- lantern passive light, focus buildup/full stun, flash, fuel, and cooldown;
- enemy world visibility and threat HUD visibility only in legal current sight;
- black unknown, live vision, exploration write, and gray memory after leaving sight;
- save, exit/reconstruct, load, transform/resources/mission/object restoration, and fog-memory restoration;
- fuse collection, powered exit, and escape;
- damage, death, restart, successful escape, and replay.

## Performance sample

No current-machine fog performance sample was collected. The project could not enter PIE, and static code inspection is not a runtime performance measurement. Historical statements in `Docs/PROGRESS.md` are not promoted to current evidence. Any later sample must record CPU/GPU, resolution, RHI, scene, observer/occluder counts, capture duration, and whether it is Editor or packaged Development; it must remain labeled as a sample from that machine, not a minimum-spec conclusion.

## Baseline disposition

- Repository/source audit may proceed.
- Architecture and migration documentation may proceed.
- Build, automation, PIE, and runtime performance acceptance remain blocked on local toolchain repair.
- No formal plugin implementation should begin until the architecture questions in `VISION_SYSTEM_ARCHITECTURE.md` receive human confirmation.

## Commands executed

The following shell commands were actually executed during validation and audit. Commands that only read source are included so the investigation is reproducible.

```powershell
# Initial checks (Git rejected these four operations because safe.directory was not yet command-local)
git status --short --branch
git branch --show-current
git log -1 --oneline --decorate
git lfs status
rg --files -g 'AGENTS.md' -g '!Binaries/**' -g '!DerivedDataCache/**' -g '!Intermediate/**' -g '!Saved/**'
rg --files Docs -g '*.md'

# Required Git baseline, rerun without altering global config
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight branch --show-current
git -c safe.directory=D:/UE_projects/LastLight log -1 --oneline --decorate
git -c safe.directory=D:/UE_projects/LastLight lfs status
rg --files -g '*.md' -g '!Binaries/**' -g '!DerivedDataCache/**' -g '!Intermediate/**' -g '!Saved/**'

# Branch creation (first sandboxed attempt failed to write .git; approved rerun succeeded)
git -c safe.directory=D:/UE_projects/LastLight switch -c design/independent-vision-plugin

# Guidance, handoff, design, and build-script reads
Get-Content -Raw -LiteralPath AGENTS.md
Get-Content -Raw -LiteralPath README.md
Get-Content -Raw -LiteralPath Docs/DESIGN_PILLARS.md
Get-Content -Raw -LiteralPath Docs/PROGRESS.md
Get-Content -Raw -LiteralPath Docs/DECISIONS.md
Get-Content -Raw -LiteralPath Docs/VISIBILITY.md
Get-Content -Raw -LiteralPath Scripts/BuildEditor.ps1

# Full Editor build (sandboxed attempt, then approved rerun)
& '.\Scripts\BuildEditor.ps1'
& '.\Scripts\BuildEditor.ps1'

# Source/test/toolchain discovery
rg -n "IMPLEMENT_SIMPLE_AUTOMATION_TEST|IMPLEMENT_COMPLEX_AUTOMATION_TEST|BEGIN_DEFINE_SPEC|DEFINE_SPEC" Source/Darkwell/Private/Tests
rg -n -A 2 "IMPLEMENT_SIMPLE_AUTOMATION_TEST" Source/Darkwell/Private/Tests
rg --files Source Config Scripts Content
rg -n "Fog|Visibility|Memory|Occlusion|SceneCapture|RenderTarget|PostProcess|LineTrace" Source Config Content/Python
rg -n "CurrentVersion|MinimumSupportedVersion|SaveVersion" Source/Darkwell/Public/Save Source/Darkwell/Private/Save Docs
Get-Content -Raw -LiteralPath Source/Darkwell/Private/Tests/DarkwellGameplayRuleTests.cpp
Get-Content -Raw -LiteralPath Source/Darkwell/Private/Tests/DarkwellPlayerMathTests.cpp
Get-Item D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe,D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe,Binaries/Win64/UnrealEditor-Darkwell.dll
Get-ChildItem 'C:/Program Files (x86)/Windows Kits/10/bin','C:/Program Files (x86)/Windows Kits/10/Lib'
Get-Command cl.exe,msbuild.exe
& 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe' -all -format json -utf8
& 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe' -products '*' -requires Microsoft.VisualStudio.Component.Windows10SDK.19041 -format json -utf8
& 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe' -products '*' -requires Microsoft.VisualStudio.Component.Windows11SDK.26100 -format json -utf8
Get-ItemProperty 'Registry::HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Windows Kits/Installed Roots'
Get-ChildItem 'D:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC'
Get-ChildItem Binaries/Win64,Saved/Logs,Saved/SaveGames
Get-Content -Raw -LiteralPath 'C:/Users/defiler919/AppData/Local/UnrealBuildTool/Log.txt'

# Full Darkwell automation attempt
& 'D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UE_projects/LastLight/Darkwell.uproject' -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests Darkwell; Quit' '-TestExit=Automation Test Queue Empty' '-AbsLog=D:/UE_projects/LastLight/Saved/Logs/VISION_BASELINE_AUTOMATION.log'
Get-Content -Raw -LiteralPath Saved/Logs/VISION_BASELINE_AUTOMATION.log
Get-Content -Raw -LiteralPath Saved/Logs/AutoSDKInfo.txt

# Historical count and final baseline state
git -c safe.directory=D:/UE_projects/LastLight show --stat --oneline HEAD
git -c safe.directory=D:/UE_projects/LastLight diff HEAD^ HEAD -- Source/Darkwell/Private/Tests Docs/PROGRESS.md Docs/DECISIONS.md Docs/VISIBILITY.md
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
```

```powershell
# Detailed implementation audit reads/searches
Get-Content Source/Darkwell/Public/Gameplay/DarkwellVisibilityComponent.h
Get-Content Source/Darkwell/Private/Gameplay/DarkwellVisibilityComponent.cpp
Get-Content Source/Darkwell/Public/Gameplay/DarkwellVisibilityMath.h
Get-Content Source/Darkwell/Private/Gameplay/DarkwellVisibilityMath.cpp
Get-Content Source/Darkwell/Public/Gameplay/DarkwellFogSubject.h
rg --files Content
rg -n "Fog|fog|Visibility|PostProcess|RenderTarget|SceneCapture|Blendable|Texture2D|GBuffer|SceneDepth|WorldNormal|BaseColor|Memory" Source/Darkwell/Private/Game Source/Darkwell/Private/UI Source/Darkwell/Public/UI Content/Python Config
Get-Content Source/Darkwell/Private/UI/DarkwellHUD.cpp
Get-Content Source/Darkwell/Public/UI/DarkwellHUD.h
Get-Content Content/Python/create_fog_memory_material.py
Get-Content Source/Darkwell/Public/Save/DarkwellSaveGame.h
Get-Content Source/Darkwell/Private/Save/DarkwellSaveSubsystem.cpp
rg -n -C 5 "VisibilityComponent|CaptureExplored|RestoreExplored|ExploredFog|Fog" Source/Darkwell/Private/Save/DarkwellSaveSubsystem.cpp Source/Darkwell/Private/Player/DarkwellCharacter.cpp Source/Darkwell/Private/Game/DarkwellGameMode.cpp Source/Darkwell/Public/Player/DarkwellCharacter.h
rg -l "SetPlayerFogState" Source/Darkwell
rg -n -C 28 "SetPlayerFogState" Source/Darkwell/Private/AI/DarkwellStalkerCharacter.cpp Source/Darkwell/Private/World/DarkwellAmmoPickup.cpp Source/Darkwell/Private/World/DarkwellScrapPickup.cpp Source/Darkwell/Private/World/DarkwellFusePickup.cpp Source/Darkwell/Private/World/DarkwellExitGate.cpp Source/Darkwell/Private/World/DarkwellStorageContainer.cpp
rg -n -C 10 "IsWorldLocationCurrentlyVisible|IsPickupPresentationVisible|bFogPresentation" Source/Darkwell/Private/Interaction Source/Darkwell/Private/Player Source/Darkwell/Private/UI Source/Darkwell/Private/World Source/Darkwell/Private/AI
Get-Content Source/Darkwell/Private/World/DarkwellExitGate.cpp
Get-Content Source/Darkwell/Private/World/DarkwellStorageContainer.cpp
rg -n "ADarkwellGameMode::|SpawnActor|PrototypeRoom|Door|ExitGate|Storage|Pickup|Stalker|Warden|PostProcess|Visibility" Source/Darkwell/Private/Game/DarkwellGameMode.cpp Source/Darkwell/Public/Game/DarkwellGameMode.h
rg -n -C 8 "ECC_Visibility|SetCollisionResponseToChannel|SetCollisionProfileName|Wall|Door" Source/Darkwell/Private/World/DarkwellPrototypeRoom.cpp Source/Darkwell/Private/World/DarkwellDoor.cpp Source/Darkwell/Public/World/DarkwellPrototypeRoom.h Source/Darkwell/Public/World/DarkwellDoor.h
Get-Content Config/DefaultEngine.ini
Get-Content Darkwell.uproject
Get-Content Source/Darkwell/Darkwell.Build.cs
Get-Content Source/Darkwell/Private/Game/DarkwellGameMode.cpp
Get-Content Source/Darkwell/Private/Player/DarkwellCharacter.cpp
rg -n "SetVisibility\(|SetAttenuationRadius|SetOuterConeAngle|SetIntensity|TorchLight|LanternBaseLight|LanternFocusLight" Source/Darkwell/Private/Player/DarkwellCharacter.cpp Source/Darkwell/Private/Combat/DarkwellLoadoutComponent.cpp

# Documentation verification before commit
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight diff --stat
git -c safe.directory=D:/UE_projects/LastLight diff --name-only
git -c safe.directory=D:/UE_projects/LastLight diff --check
Get-Item Docs/VISION_BASELINE_VALIDATION.md,Docs/VISION_EXISTING_SYSTEM_AUDIT.md,Docs/VISION_SYSTEM_REQUIREMENTS.md,Docs/VISION_SYSTEM_ARCHITECTURE.md,Docs/VISION_SYSTEM_MIGRATION_PLAN.md
rg -n "ClearMemory|BlockMemoryWrites|SuppressMemoryPresentation|SuppressLiveVision|FloorId|ZMin|ZMax|visibility polygon|Scene Capture|GPU raymarch|24/24|23/23|CurrentVersion = 6|separate.*commit|single-authority|single authority" Docs
```

Some `Get-Content` reads used `Select-Object -First/-Skip` only to prevent tool-output truncation; the complete referenced files were covered across adjacent ranges. Findings are catalogued with file/symbol references in `VISION_EXISTING_SYSTEM_AUDIT.md`.

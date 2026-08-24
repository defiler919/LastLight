# SightWeave

SightWeave is a reusable Unreal Engine plugin intended to provide authoritative 2.5D vision, explicit legal illumination, floor-aware knowledge, and a presentation-independent query API.

## M1 capabilities

- Three isolated modules: `SightWeaveRuntime`, `SightWeaveEditor`, and `SightWeaveTests`.
- Neutral project settings and public, serializable source descriptions.
- Distinct generation-safe handles for vision sources, legal illumination sources, and Subject Reveal Overrides.
- A world subsystem that owns copied registration data, validates handles, updates revisions, removes owner registrations, and clears state during world teardown.
- Explicit placeholder visibility queries that return `NotReady` rather than inventing visibility.
- An isolated lab map at `/SightWeave/Maps/L_SightWeave_Lab`.
- Automation tests under `SightWeave.M1`.

## Not implemented in M1

M1 does not implement visibility polygons, occlusion solving, legal-illumination intersections, GPU masks, fog composition, exploration memory, last-seen proxies, final subject-reveal rendering, or host-game integration. The plugin is in development and must not yet be treated as a complete vision system.

## Module responsibilities

- `SightWeaveRuntime`: neutral public types, settings, per-world registration lifecycle, revisions, and explicit not-ready queries. It has no host-game dependency and no editor dependency.
- `SightWeaveEditor`: editor-only extension point for later authoring and validation tools; M1 contains only the module boundary.
- `SightWeaveTests`: editor-only M1 automation and lab/dependency validation.

## Build and test

From the host project root:

```powershell
Scripts/BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8
& 'D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UE_projects/LastLight/Darkwell.uproject' -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave.M1' '-TestExit=Automation Test Queue Empty'
```

For standalone validation, use Unreal Automation Tool's `BuildPlugin` command against `SightWeave.uplugin` and a temporary package directory outside the repository.

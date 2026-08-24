# SightWeave

SightWeave is a reusable Unreal Engine plugin intended to provide authoritative 2.5D vision, explicit legal illumination, floor-aware knowledge, and a presentation-independent query API.

## M2 capabilities

- Three isolated modules: `SightWeaveRuntime`, `SightWeaveEditor`, and `SightWeaveTests`.
- Neutral project settings and public, serializable floor, source, occluder, suppression, snapshot, and query data.
- Distinct strong handles for vision, legal illumination, occluders, M2 hard-live suppression, and Subject Reveal Overrides.
- A deterministic endpoint-ray reference solver shared by distinct vision and illumination polygon types.
- Blueprint-spawnable floor, vision, legal-illumination, occluder, and hard-suppression components with safe lifecycle registration.
- A deterministic floor-local uniform-grid segment index with local dynamic updates and stable candidate ordering.
- Synchronously published immutable CPU snapshots. Only dirty source polygons are rebuilt; snapshots contain ordinary data and stable handles, not UObject pointers.
- Source-specific compatibility resolution, illumination bypass, and post-union hard-live suppression.
- Authoritative point, source-specific, bounds, anchor/any/all/required-count sample, and batch queries with attribution, rejection flags, and memory-write eligibility.
- An isolated lab map at `/SightWeave/Maps/L_SightWeave_Lab`.
- Automation tests under `SightWeave.M1` and `SightWeave.M2`.

The legacy M1 registration APIs remain available. A query against a world with no registered floor still returns `NotReady`; registered active floors use M2 authority.

## Deliberately outside M2

M2 does not implement GPU masks, fog composition, exploration-memory tiles, last-seen proxies, final Subject Reveal presentation, the full modifier system, persistence, or host-game integration. `FSightWeaveHardSuppressionDescription` is intentionally only the minimal hard `SuppressLiveVision` circle required to verify authority ordering.

## Module responsibilities

- `SightWeaveRuntime`: neutral public types, components, geometry, spatial index, immutable snapshots, and authoritative queries. It has no host-game dependency and no editor dependency.
- `SightWeaveEditor`: editor-only extension point for authoring and validation tools.
- `SightWeaveTests`: editor-only M1/M2 automation and lab/dependency validation.

## Build and test

From the host project root:

```powershell
Scripts/BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8
& 'D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UE_projects/LastLight/Darkwell.uproject' -unattended -nop4 -nosplash -NullRHI -NoSound '-ExecCmds=Automation RunTests SightWeave.M1' '-TestExit=Automation Test Queue Empty'
```

For standalone validation, use Unreal Automation Tool's `BuildPlugin` command against `SightWeave.uplugin` and a temporary package directory outside the repository.

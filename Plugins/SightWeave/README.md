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
- Ordinary-data debug snapshots plus explicit non-Shipping `DrawDebug` for floors, cells, height bands, endpoint events/rays, distinct polygons, compatibility, suppression, and attributed query markers.
- Thin generic authoring actors that host one SightWeave component each; authority remains in the components/subsystem and no actor ticks are required.
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

The idempotent `Content/Python/create_sightweave_lab.py` script rebuilds 20 M2 fixture zones using normal Unreal asset APIs. The map contains real floor, vision, legal-illumination, occluder, hard-suppression, dynamic-door, and no-tick debug-query components; it has no `/Game/` or `/Script/Darkwell` dependency.

For standalone validation, use Unreal Automation Tool's `BuildPlugin` command against `SightWeave.uplugin` and a temporary package directory outside the repository.

## Object history capture policy

`SightWeaveObjectPolicy.h` adds a host-neutral capture contract. This controls
whether a **new Live observation may become gray history**; it does not decide
legal visibility, empty-space knowledge, unknown/black presentation, or identity.
Hosts consume it at their observation capture/lifecycle boundary. The plugin
does not own the DARKWELL HistoryGridV2 renderer.

Project Settings > SightWeave > Object History provides **Default History Mode**,
initially **Always**. Add the optional `USightWeaveObjectPolicyComponent` to an
object to choose **Use Project Default** or **Override** and a History Mode:

- **Always**: allow stationary and observed moving last-seen capture.
- **StationaryOnly**: no new moving history; abandon an unsealed moving
  observation on coverage loss. Motion end requires fresh legal stationary
  observation before capture can rearm. Previously sealed history is unaffected.
- **Never**: Live-only; no historical epochs/grids/proxies/caps/textures/MIDs.
  Transient state needed to show current Live remains permitted.

Configure before registration. `OnRegister` resolves once into
`FResolvedSightWeaveObjectPolicy`; runtime reads do not query config/reflection.
This component has no Tick and does not register a second subject identity.
Authoring changes require an explicit host reset plus re-registration; version
one does not migrate history during an arbitrary runtime policy change.

```cpp
#include "SightWeaveObjectPolicy.h"

auto* Policy = NewObject<USightWeaveObjectPolicyComponent>(Actor);
Policy->PolicySource = ESightWeaveObjectPolicySource::Override;
Policy->HistoryMode = ESightWeaveHistoryMode::StationaryOnly;
Actor->AddInstanceComponent(Policy);
Policy->RegisterComponent();
Policy->SetSightWeaveMoving(true);  // before motion starts
// ... game-owned movement; no automatic transform detection ...
Policy->SetSightWeaveMoving(false);
```

Blueprint/C++: `SetSightWeaveMoving`, `IsSightWeaveMoving`,
`GetResolvedHistoryMode`, `GetMovingRevision`, `IsHistoryEligible`,
`RequiresFreshStationaryObservation`. There is one idempotent motion Boolean,
no Begin/End stack or negative depth. Only actual changes increment revision.
The C++ adapter calls `NotifyLegalObservation` after valid spatial evidence.
The plain `FSightWeaveObjectHistoryCapture` and resolver can also be consumed by
non-component hosts.

The adapter must abandon ineligible **current** observations without sealing,
retain qualified older histories, and avoid identity-based invalidation.
StationaryOnly/Never are not permission to erase all records for a StableID.
No WholeObject, confirmation threshold, profile, automatic motion detector or
world/region/black options are exposed by this API.

Generic tests: `SightWeave.ObjectPolicy`. Host integration and evidence:
`Docs/SIGHTWEAVE_HISTORY_POLICY_HANDOFF.md` in DARKWELL (documentation only;
the plugin and its tests have no dependency on that host module).

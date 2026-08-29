# SightWeave M6P1 DARKWELL adapter integration contract

Status: **FROZEN FOR M6P1 IMPLEMENTATION**

Audit baseline: `25eb813a87cacd7dadb34f1b234c582df1467964`

Frozen SightWeave M4 baseline: `94be5835212a7f10491cd359676fcb5ee06dc08b`

Implementation branch: `codex/m6p1-sightweave-darkwell-adapter-vertical-slice`

## 1. Objective and authority

M6P1 adds the first product-owned SightWeave integration without extending the SightWeave plugin. In the dedicated `/Game/Maps/L_VisionIntegration` world, a DARKWELL World Subsystem owns one SightWeave scope, translates existing player/loadout/enemy state into existing public Runtime APIs, selects the existing Render presentation scope, and publishes one DARKWELL-neutral visibility snapshot consumed by both Stalker presentation and the threat HUD.

The product authority modes are:

- `Legacy`: the default for every world. The existing `UDarkwellVisibilityComponent`, legacy fog callbacks, threat filtering, and legacy fog material remain authoritative.
- `SightWeave`: selected only by the native integration GameMode in `L_VisionIntegration`. Legacy visibility ticking/writes/callbacks and the legacy fog blendable are disabled before SightWeave becomes the published authority.

Runtime hot switching is not supported in M6P1. A mode is selected once for a world session. `L_Prototype` remains Legacy and is not modified.

## 2. Transactional activation

SightWeave activation is one game-thread transaction:

1. acquire the existing Runtime and, on render-capable non-server builds, Render World Subsystems;
2. validate one DARKWELL player, one integration fixture, one real base Stalker, one active floor, and one world identity;
3. register the floor, explicit fixture occluder, body-circle source, directional source, semantic torch illumination source, static-environment eligibility, and `NeverRemember` Stalker subject;
4. publish the Runtime snapshot, configure Coarse exploration memory, acquire the exact memory scope, and validate every handle/count;
5. prepare the existing SightWeave presentation scope; NullRHI retains CPU authority and reports presentation unavailable rather than inventing visual evidence;
6. disable the player's legacy visibility component and force the legacy HUD blendable to zero;
7. select the SightWeave presentation scope and publish `SightWeave` as the only product authority.

Any failure before publication unregisters the partial registrations, disables exploration memory, clears presentation selection, restores Legacy ticking/presentation where applicable, and emits an error containing the World and requested authority mode. The integration map may remain fail-closed; other maps remain Legacy. A dual-authority invariant is an automation failure, never an implicit preference.

## 3. Adapter ownership and lifecycle

The project-owned `UDarkwellSightWeaveWorldSubsystem` (or an equivalently focused final symbol) is the only DARKWELL-to-SightWeave translation seam. It owns:

- world-start authority selection and read-only diagnostics;
- stable Knowledge Owner and Floor IDs;
- all floor/source/light/occluder/static-environment handles;
- the plain `FSightWeaveSubjectMemoryAuthority` and stable Stalker subject registrations;
- source transform/state updates and immutable Runtime publication;
- one per-frame/revision DARKWELL subject visibility snapshot;
- approved Stalker render visibility and the matching HUD eligibility;
- deterministic unregister/reset during actor destruction, map change, PIE restart, and world teardown.

Character, Loadout, Stalker, HUD, AI, and GameMode may call only DARKWELL-facing methods/types. They do not include SightWeave Render internals, reproduce SightWeave geometry, inspect GPU pixels, or use presentation as gameplay authority. Runtime identities use stable named IDs; Actor addresses are never durable identity.

## 4. Frozen source and light semantics

### Body circle

- radial `BypassLegalIllumination` vision source;
- player-centered and permanently registered while the player is valid;
- radius `120 cm`, taken from the existing legacy awareness contract;
- obeys explicit occlusion, floor/height, and hard suppression;
- remains live when the torch is unavailable;
- also supplies the minimum legal close-range observation path without making the outer cone live.

### Player cone

- directional source centered on the player and following the player's actual actor facing, which is driven by current aim/movement rules;
- range `2200 cm`;
- half-angle interpolates from `52 degrees` to `35 degrees` using the existing shotgun aim progress;
- `RequiresLegalIllumination` with canonical capability `Darkwell.Visible.Torch`;
- never derives direction from camera jitter and never uses SceneCapture.

### Torch legal illumination

- one explicit radial legal-illumination source with capability `Darkwell.Visible.Torch`;
- range `1250 cm`, matching the existing normal raised-torch design range but stored once in the Adapter contract rather than sampled from `UPointLightComponent`;
- active only when the existing Loadout semantically reports the torch equipped/on with positive charge and an active player owner;
- updates immediately on equip, depletion, restore, death, and world teardown;
- obeys the same explicit fixture occluder geometry as vision;
- rendered intensity, attenuation pixels, shadows, Scene Color, and light-component discovery have no CPU authority.

The current product control cycles right-hand items with `E`. M6P1 does not add a second torch inventory, durability system, or lantern authority. Cycling away from the torch deactivates only the M6P1 torch legal-light source; the existing lantern remains outside the integration contract.

## 5. Floor, occlusion, and static memory

M6P1 uses one floor named `Darkwell.Integration.Ground`, a stable floor-local origin, a height range containing the player/fixture/Stalker, and Coarse HardMemory (`25 cm/texel`). M3.4 Width=50 inward-only feather remains unchanged.

The native integration fixture declares:

- one static ground plane;
- two straight wall runs with a deliberate doorway gap;
- one explicit immutable landmark footprint with a distinct neutral intensity;
- explicit occluder segments matching the wall runs;
- no dynamic door, multiple floor, general collision import, or production-world conversion.

Only CPU EffectiveLive writes HardMemory. Unknown stays black, legal live shows current Scene Color, and an eligible explored static footprint becomes neutral gray after live coverage leaves. A wall-hidden region never becomes remembered merely from presentation feather or rendered light.

## 6. Stalker and HUD consistency

M6P1 reuses `ADarkwellStalkerCharacter` and its existing AI/controller, collision, combat, damage, navigation, and behavior state. The Adapter registers the base Stalker under its configured persistent ID with subject policy `NeverRemember` and a positive per-world instance generation.

For every Adapter update:

1. source/light transforms and active state are updated;
2. one immutable SightWeave snapshot is published;
3. the Stalker is queried once using its approved bounds/samples;
4. the Adapter records one neutral result containing stable subject ID, hard-live boolean, authority mode, Runtime snapshot revision, and DARKWELL publication revision;
5. that exact stored result controls the Stalker presentation and is returned unchanged to the HUD.

The HUD never re-queries SightWeave or legacy visibility for a SightWeave-owned Stalker. Enemy presentation and threat eligibility therefore cannot use different frames/revisions. `NeverRemember` never creates a Last-Seen descriptor/proxy or gray enemy silhouette. Player attacks do not reveal the Stalker; damage-source reveal is outside M6P1.

## 7. Module and packaging boundary

- `Darkwell` privately depends on `SightWeaveRuntime` for all targets.
- `Darkwell` privately depends on `SightWeaveRender` only when the target is not `Server`.
- presentation code is compiled out under `UE_SERVER` and fails closed when rendering is unavailable;
- Runtime code does not depend on `SightWeaveEditor`, `SightWeaveTests`, `UnrealEd`, `AutomationTest`, or test/readback APIs in Shipping;
- no existing DARKWELL public gameplay/HUD interface exposes a SightWeave Render type;
- no plugin source, shader, Build.cs, descriptor, Lab, or binary plugin asset changes are permitted.

The repository has no Server target, so M6P1 proves server-safe compile guards, NullRHI CPU behavior, Game Development, and Game Shipping. It does not claim a dedicated-server process/package result.

## 8. Integration map contract

The only authorized new binary asset is:

`/Game/Maps/L_VisionIntegration.umap`

It is created and saved through Unreal Editor Python/API, not by filesystem or binary manipulation. It uses the native integration GameMode and native fixture, contains a PlayerStart and a real base Stalker, and requires no Blueprint, Material, Texture, Data Asset, Niagara, or second map. It must load after save/reopen, contain no missing class or external-actor leakage, and cook independently.

`/Game/Maps/L_Prototype`, `Darkwell.uproject`, every other binary asset, and the SightWeave plugin remain byte-for-byte unchanged from the audit baseline.

## 9. Read-only diagnostics and invariants

Diagnostics expose at least:

- requested and active authority mode;
- activation state/failure reason;
- world name and generation;
- Runtime snapshot and DARKWELL publication revisions;
- floor/source/light/occluder/static/subject counts;
- legacy tick/presentation state;
- presentation availability and selected scope;
- last Stalker/HUD stable ID, hard-live state, and revision.

Automation treats any of these as failure: two authorities active, more than one final fog composite, Legacy memory writes in SightWeave mode, SightWeave subject/HUD effects in Legacy mode, duplicate registrations after PIE restart, stale handles after teardown, differing Stalker/HUD revision, non-authoritative visibility accepted as live, or rendered-light values affecting CPU results.

## 10. Explicit non-goals and rollback

M6P1 does not implement SaveGame/M4P3 blob binding, save-version changes, lantern, Warden, other enemies, damage-source reveal, player-attack reveal, camera, radar, remote sources/lights, blackout/region production wiring, gray-to-black production behavior, multiple floors, dynamic doors, `L_Prototype` adoption, legacy deletion, shader/plugin API changes, SceneCapture authority, or gameplay/AI/resource tuning.

Rollback is loading `L_Prototype`, whose native default GameMode never requests SightWeave and therefore remains Legacy. No M6P1 data is persisted.

# Existing DARKWELL vision and fog audit

Audit baseline: commit `46d9f9d093e3ceaf8afab71ff29367ab9b1ec2c1`, Unreal Engine 5.8.1.

## Executive finding

The current system is a game-specific, player-owned CPU visibility implementation with two different spatial representations:

- a 100 cm sparse XY grid refreshed at 10 Hz for gameplay visibility and durable exploration;
- a 10 cm sparse XY grid plus a viewport-sized transient texture refreshed at 30 Hz for presentation memory and current-screen compositing.

It does **not** use a Scene Capture or a world-space Render Target. The HUD builds a half-resolution `UTexture2D` on the CPU, uploads it every update, and supplies its red/green channels to a camera post-process material. Occlusion is a radial field of line-trace hit distances, not an explicit wall-edge visibility polygon. This explains both the moving/wavy straight-wall boundary and the limited gray-memory fidelity.

The implementation is valuable as a gameplay prototype, but its player ownership, screen dependence, global actor/light scans, single XY plane, and actor-origin/coarse-cell visibility make it unsuitable as the foundation of a general Fab plugin.

## Implementation inventory

### Core knowledge and occlusion

| File | Type/symbol | Responsibility |
| --- | --- | --- |
| `Source/Darkwell/Public/Gameplay/DarkwellVisibilityMath.h` | `EDarkwellFogCellState` | `Unexplored`, `Explored`, `Visible` state vocabulary |
| `Source/Darkwell/Private/Gameplay/DarkwellVisibilityMath.cpp` | `WorldToCell`, `CellToWorldCenter`, `IsInsideVisionCone`, `ResolveFogCellState` | XY-grid mapping and elementary cone/state rules |
| `Source/Darkwell/Public/Gameplay/DarkwellVisibilityComponent.h` | `FDarkwellLightPresentationSource` | Point/circular or spot/conical light influence in XY |
| same | `FDarkwellVisionPresentationState` | Snapshot of one player sight cone, awareness disk, and at most 32 discovered local lights |
| same | `UDarkwellVisibilityComponent` | Owns authoritative visible/explored cells and fine presentation-memory cells |
| `Source/Darkwell/Private/Gameplay/DarkwellVisibilityComponent.cpp` | `RefreshVisibility` | 10 Hz 100 cm-cell discovery using player-to-cell and light-to-cell `ECC_Visibility` traces |
| same | `BuildVisualOcclusionRanges` | 1024-angle radial wall-distance field; full-range traces in the cone, 128 short samples outside it, interpolation, then circular median-of-three |
| same | `AddActiveLocalLights` | Global actor scan for enabled `ULocalLightComponent`; pawns other than the player are excluded |

### Screen mask and composition

| File/asset | Symbol | Responsibility |
| --- | --- | --- |
| `Source/Darkwell/Private/UI/DarkwellHUD.cpp` | `ADarkwellHUD::Tick`, `UpdateFogOfWar` | Deprojects the viewport to the player's Z plane, computes the live mask, records presentation memory, filters memory, and uploads a transient texture |
| same | `EnsureFogComposite`, `SetFogCompositeWeight` | Creates the material instance and adds it to the top-down camera as a blendable |
| `Source/Darkwell/Public/UI/DarkwellHUD.h` | fog texture/CPU arrays | Owns `FogTexturePixels`, remembered coverage/scratch arrays, occlusion ranges, projection cache, and memory revision |
| `Content/UI/Fog/M_FogMemoryComposite.uasset` | post-process material | Composites live scene, gray memory, and black unknown using the mask texture |
| `Content/Python/create_fog_memory_material.py` | material generator | Documents and creates the material graph from `PostProcessInput0`, GBuffer base color/world normal, and scene depth |

No `USceneCaptureComponent*`, `ASceneCapture*`, `UTextureRenderTarget*`, `CanvasRenderTarget`, render-graph pass, or project-owned shader was found. `FogTexture` is a transient `UTexture2D`, not a Render Target.

### Observer/light integration and world authoring

| File | Symbol | Responsibility |
| --- | --- | --- |
| `Source/Darkwell/Private/Player/DarkwellCharacter.cpp` | constructor | Attaches `UDarkwellVisibilityComponent` directly to the player; creates the top-down camera, torch point light, lantern point light, and lantern spot light |
| `Source/Darkwell/Private/Combat/DarkwellLoadoutComponent.cpp` | presentation refresh near lines 678-738 | Changes light visibility, radius, intensity, and cone while equipping, reloading, focusing, or flashing |
| `Source/Darkwell/Private/Game/DarkwellGameMode.cpp` | constructor, `StartPlay` | Selects `ADarkwellCharacter` and `ADarkwellHUD`; hard-spawns the prototype room, dynamic door, lights/pickups/facilities, two enemies, and other gameplay actors when absent |
| `Source/Darkwell/Private/World/DarkwellPrototypeRoom.cpp` | `CreateBlock` | Creates floor/walls/cover with `BlockAll`, which includes `ECC_Visibility` and therefore becomes fog occlusion geometry implicitly |
| `Source/Darkwell/Private/World/DarkwellDoor.cpp` | `DoorPanel`, `InteractionHitProxy` | Dynamic door and proxy block `ECC_Visibility`; the door rotates over time but has no fog-subject/last-seen policy |

GameMode does not explicitly initialize fog. Fog is created indirectly because its selected pawn class owns the component and its selected HUD class owns mask presentation. GameMode's hard-coded world actors matter because every local light is automatically treated as illumination and every `ECC_Visibility` blocker becomes an occluder.

### Object visibility and last-seen presentation

| File | Actor/policy | Actual behavior |
| --- | --- | --- |
| `Source/Darkwell/Public/Gameplay/DarkwellFogSubject.h` | `IDarkwellFogSubject` | Game-specific C++ interface with one `SetPlayerFogState` callback; no Blueprint API or policy metadata |
| `Source/Darkwell/Private/AI/DarkwellStalkerCharacter.cpp` | Stalker; inherited by Warden | `SetActorHiddenInGame(true)` unless the actor-origin cell is `Visible`; no enemy memory |
| `Source/Darkwell/Private/World/DarkwellAmmoPickup.cpp` | ammunition | Hides only the mesh and disables its `ECC_Visibility` response outside current sight; its point light intentionally remains discoverable |
| `Source/Darkwell/Private/World/DarkwellScrapPickup.cpp` | scrap | Same current-sight-only mesh policy; point light remains active |
| `Source/Darkwell/Private/World/DarkwellFusePickup.cpp` | fuse | Same current-sight-only mesh policy; point light remains active |
| `Source/Darkwell/Private/World/DarkwellExitGate.cpp` | fixed stateful facility | Stops updating its status-light presentation while not visible and refreshes when visible again |
| `Source/Darkwell/Private/World/DarkwellStorageContainer.cpp` | fixed stateful facility | Stops presentation refresh and panel animation while not visible; reacquisition snaps/applies the current target state |
| `Source/Darkwell/Private/World/DarkwellDoor.cpp` | dynamic door | Does not implement `IDarkwellFogSubject`; geometry and passage-light state can change in gray memory |
| `Source/Darkwell/Private/UI/DarkwellHUD.cpp` | threat rows | Independently checks `IsWorldLocationCurrentlyVisible(enemy origin)` so HUD does not list an unseen enemy |

`UDarkwellVisibilityComponent::UpdateFogSubjects` performs a world-wide `TActorIterator<AActor>` scan after each 10 Hz refresh and evaluates only `Actor->GetActorLocation()`. Bounds, multiple sample points, floor, height, and partial visibility are not represented.

### Persistence and tests

| File | Symbol | Responsibility |
| --- | --- | --- |
| `Source/Darkwell/Public/Save/DarkwellSaveGame.h` | `FDarkwellPlayerSaveData` | Saves authoritative cells, presentation cells, and presentation cell size |
| same | `UDarkwellSaveGame` | Current version 6, minimum version 1 |
| `Source/Darkwell/Private/Save/DarkwellSaveSubsystem.cpp` | `CaptureCurrentGame` | Serializes sorted exploration arrays and 10 cm cell size |
| same | `ApplyPendingLoad` | Restores v4 knowledge, v5 fine memory, and v6 scale; older fine memory defaults to 25 cm |
| `Source/Darkwell/Private/Tests/DarkwellGameplayRuleTests.cpp` | `Darkwell.Gameplay.Visibility.FogKnowledge` | Tests cell mapping/state, source margins, sight/light separation, 25-to-10 cm migration, and subject-interface presence |
| same | save tests | Tests v6 memory serialization and v1-v6 compatibility boundaries |

There is no test for straight-wall temporal stability, door occlusion during motion, screen-mask/authoritative-query agreement, multi-observer union, multiple floors, modifier regions, memory-detail fidelity, or CPU/GPU budgets.

## Current parameters

### Authoritative gameplay knowledge

| Parameter | Value | Location |
| --- | ---: | --- |
| Gameplay cell size | 100 cm | `UDarkwellVisibilityComponent::CellSize` |
| Refresh interval | 0.10 s (10 Hz) | `RefreshIntervalSeconds` |
| Sight range | 2200 cm | `SightRange` |
| Normal sight half-angle | 52 degrees | `SightHalfAngleDegrees` |
| Fully aimed half-angle | 35 degrees | `AimedSightHalfAngleDegrees` |
| Awareness radius | 120 cm | `AwarenessRadius` |
| Trace height above owner/cell | 52 cm | `VisibilityTraceHeight` |
| Maximum authoritative remembered cells | 262,144 | `MaximumRememberedCells` |
| Source-light limit | 32 | `FDarkwellVisionPresentationState::MaximumLightCount` |

For each 10 Hz update, `RefreshVisibility` scans a square covering the 2200 cm range at 100 cm resolution. A candidate in the cone must be within at least one local light shape, have a light-to-cell trace, and have a player-to-cell trace. Pawns are ignored by occlusion traces. A trace hit within `CellSize * 0.55` (55 cm) of the candidate is accepted as reaching the cell.

### Presentation

| Parameter | Value | Location |
| --- | ---: | --- |
| Fine memory cell size | 10 cm | `PresentationCellSize` |
| Maximum fine remembered cells | 1,048,576 | `MaximumRememberedPresentationCells` |
| Mask cadence | 30 Hz | `ADarkwellHUD::UpdateFogOfWar` |
| Texture scale | half viewport | same |
| Texture limits | 320x180 minimum; 1024x576 maximum | same |
| Occlusion angular samples | 1024 | same |
| Outside-cone awareness samples | approximately 128 | `BuildVisualOcclusionRanges` |
| Live edge feather | max(18 cm, two projected mask pixels) | `VisionEdgeFeatherWorldUnits` |
| Per-pixel angular footprint samples | 3 | center and +/- projected pixel radius |
| Memory contour filter | 3 horizontal+vertical box passes, radius 3 | `MemoryContourPassCount`, `MemoryContourRadius` |
| Memory hardening thresholds | smoothstep 0.54 to 0.60 | remembered green channel |

The texture's R channel is current reveal; G is remembered coverage; B is zero; A is 255. The texture is bilinear-filtered and uploaded with `UTexture2D::UpdateTextureRegions` after CPU work.

## Actual data flow

```text
ADarkwellCharacter facing/aim + every eligible ULocalLightComponent
    -> FDarkwellVisionPresentationState (one player, <=32 lights)
    -> two independent occlusion paths
       A. 10 Hz, 100 cm candidate cells + player/light ECC_Visibility traces
       B. 30 Hz, 1024-angle radial hit-distance field + screen-pixel sampling
    -> A: VisibleCells + permanent ExploredCells (gameplay/object authority)
    -> B: transient red current-mask channel
    -> B current silhouette: permanent 10 cm ExploredPresentationCells
    -> bilinear reconstruction + 3x box contour filter: green memory-mask channel
    -> transient half-resolution UTexture2D upload
    -> M_FogMemoryComposite camera post process
       live PostProcessInput0 * current R
       + current-frame GBuffer-derived gray geometry * memory G * (1-current R)
       + black elsewhere
    -> IDarkwellFogSubject callback and HUD queries from A's actor-origin cell
    -> save v6: A's explored cells + B's presentation cells + presentation scale
```

The gameplay and screen paths share source shapes but not their final visibility representation. The post-process feather never grants gameplay visibility, which is conceptually correct. However, the coarse 10 Hz grid and the continuous 30 Hz radial field can disagree visibly at boundaries.

## Why the black boundary waves and jitters

### Architectural causes

1. **A straight wall is stored as angular distances from a moving point.** `BuildVisualOcclusionRanges` does not retain the wall segment or its line equation. It casts discrete rays and stores hit distance per angle. As the player moves, every ray/wall intersection and tangent sample moves. Reconnecting those distances by angular interpolation approximates the straight edge with a changing polar curve.
2. **Median filtering moves the silhouette.** The circular median-of-three removes isolated spikes, but near a tangent or thin object it selects one neighboring hit distance. Which neighbor wins changes as a wall edge crosses ray bins, so the boundary can pop by one or more angular samples.
3. **Screen reconstruction is view dependent.** The system deprojects the current viewport to the player's Z plane, samples three angular offsets based on projected pixel footprint, evaluates at half resolution, bilinear-filters the texture, and uploads at 30 Hz. Resolution, camera projection, player motion, and update phase therefore affect the displayed edge.
4. **There is no temporal identity for an occluder edge.** Each update starts from fresh physics traces. Collision triangles, door rotation, contact tolerances, or adjacent components can change the nearest hit; no stable segment ID or analytic edge is carried across frames.
5. **Two visibility authorities disagree at the seam.** Actors/HUD use 100 cm cells at 10 Hz, while the black boundary uses the softened radial field at 30 Hz. An enemy can hide/show at a cell transition that does not coincide with the visual edge.

### Parameter-sensitive contributors

- 1024 angular samples set the maximum angular precision. More samples reduce the angular chord error but do not turn moving radial reconstruction into stable wall geometry.
- the 30 Hz cadence creates visible stepping at sufficiently fast movement/turning;
- the 1024x576 cap and half-resolution scale make edge shape depend on output resolution;
- the `max(18 cm, two pixels)` feather changes world-space softness with projection;
- median-of-three can suppress real narrow openings as well as noise;
- `ECC_Visibility` collision quality and the fixed 52 cm trace height affect which surface is hit.

Increasing resolution/rate/ray count can reduce symptoms at CPU/upload cost, but cannot remove the underlying polar-resampling instability. Stable straight walls require stable explicit occluder edges (or an equivalently stable world-space depth representation), not only more samples.

## Why gray memory lacks convincing scene detail

### What the material really remembers

Only coverage is stored. The fine grid says “this XY area was seen”; it does not store color, material, mesh, lighting, or object state. In an explored-not-visible screen pixel, `M_FogMemoryComposite` samples the **current frame's** GBuffer:

- Base Color is collapsed to luminance and multiplied by `0.10`;
- a fixed factor uses `0.65 + abs(WorldNormal.Z) * 0.35`;
- `ddx/ddy(SceneDepth)` produces a narrow dark edge (`0.02` scale, `0.45` strength);
- a dark constant base `(0.018, 0.022, 0.026)` dominates the low-contrast result;
- live illumination is intentionally excluded.

This preserves some base-color texture luminance, surface orientation, and silhouettes, but not a trustworthy last-seen image. Very dark scaling compresses material contrast; similarly colored floor/wall/decor surfaces collapse together; vertical surfaces receive less fixed-normal factor than horizontal surfaces; depth gradients emphasize outlines. The result naturally reads as contour art rather than a detailed gray scene.

### Architectural consequences

- A static environment can be shown from current Base Color because it is assumed not to change, but no historical surface snapshot exists.
- A door or other mesh that moves off-screen changes the current GBuffer and therefore can leak its new geometry into memory unless a subject-specific proxy freezes it. `ADarkwellDoor` currently has no such policy.
- Enemy hiding works because the actor is removed from rendering outside current sight, not because memory rejects the enemy at composition time.
- Exit/container code freezes selected presentation properties, but this is per-class manual logic rather than a general object snapshot.
- translucent, unlit, masked, Niagara, decal, or custom-rendering details are not guaranteed to contribute useful Base Color/normal/depth data.

Raising the luminance multiplier, changing the normal factor, or reducing depth-edge strength can improve readability. Those are tuning changes. They cannot create last-seen material/object state because that data is never captured.

## Parameter issues versus principle/architecture issues

| Symptom | Parameter/tuning portion | Principle/architecture portion |
| --- | --- | --- |
| Wall edge too soft | feather width, mask resolution, filtering | screen-space resampling from radial distances |
| Wall edge waves during motion | ray count, 30 Hz cadence, collision precision | no explicit stable wall segment/polygon; no temporal edge identity |
| Thin doorway disappears | median filter, trace tolerance | radial samples cannot robustly preserve topology at all distances |
| Actor pops before/after visual edge | 100 cm cell size, 10 Hz rate | separate coarse gameplay grid and visual mask without shared exact polygon query |
| Memory looks like outlines | luminance/normal/depth constants | only coverage is persisted; current GBuffer is reconstructed instead of a last-seen presentation |
| Off-screen dynamic door leaks state | none sufficient | door has no snapshot policy and the material samples current geometry |
| Large actor visibility feels wrong | cell size | only actor origin is queried; no bounds/sample policy |
| Far/remote observer cannot write memory | none | fine memory recording is restricted to the player's current viewport and the component models only one sight origin |
| Multi-floor leakage | fixed trace height can be tuned | grids have no `FloorId`, `ZMin`, or `ZMax` |
| Light unexpectedly grants reveal | light radius/intensity/visibility | all eligible UE local lights are automatically treated as knowledge illumination; no explicit legal-source registration |
| Memory eventually stops growing | maximum-cell caps | no tile streaming/eviction/partition strategy; writes silently fail at capacity |

## Capability gaps against the requested independent system

- one player-owned observer only; no registered camera/remote-item/special-source API;
- UE local light discovery is implicit, global, capped at 32, and dependent on actor iteration order;
- no explicit occluder component, segment spatial index, floor/height metadata, or authoring validation;
- no `ClearMemory`, `BlockMemoryWrites`, `SuppressMemoryPresentation`, or `SuppressLiveVision` region model;
- no circle/box/room/polygon modifier shapes;
- no multi-floor or 2.5D height band;
- no generic subject policies (`NeverRemember`, immutable, automatic last-seen, adapter-provided snapshot);
- no Blueprint public API for the fog-subject contract or queries;
- no world subsystem, per-world source handles, or ownership independent of the player/HUD;
- no deterministic exact visibility result for bounds/points derived from the same polygons that feed rendering;
- no world-space render mask or resolution-independent stable UV mapping;
- no debug view for sources, occluder segments, polygons, modifiers, floors, subjects, or query reasons;
- no performance counters/budgets and no coverage for geometry stability or presentation fidelity.

## What should be retained

Retain these concepts and game-facing behaviors during migration:

- the three knowledge states and the rule that current vision is authoritative while memory is stale;
- strict separation between gameplay visibility and visual edge softness;
- enemies, moving threats, removable pickups, prompts, and threat HUD being current-sight only;
- fixed/stateful facilities exposing only their last-seen presentation;
- versioned save migration and stable persistent object IDs;
- separation between sight and illumination as a DARKWELL adapter rule;
- `ECC_Visibility` authoring lessons as migration input, although the plugin should prefer explicit occluders;
- existing math/tests as behavioral references, not as plugin-namespaced code copied unchanged;
- old system operation until the new prototype passes its independent-map acceptance gates.

The game-specific `IDarkwellFogSubject` implementations are useful adapter prototypes. They must not be moved into the generic plugin under DARKWELL names.

## What should eventually be retired

After new-system acceptance, retire in a dedicated deletion milestone/commit:

- HUD ownership of fog simulation and per-frame CPU texture generation;
- the 1024 radial ray-distance screen-mask algorithm and `UpdateTextureRegions` upload path;
- the 100 cm cell as the only actor-visibility authority;
- fine-memory writes limited to currently deprojected screen bounds;
- global scans of every actor/light/subject on repeated updates;
- implicit conversion of arbitrary UE local lights into knowledge sources;
- fixed single-plane XY memory without floors/heights;
- manual one-method subject interface as the entire policy/snapshot system;
- the current `M_FogMemoryComposite` as production memory architecture (it may remain a visual reference during prototyping);
- save fields tied directly to the old grids once a separately versioned plugin snapshot has shipped and migrated.

No part should be deleted in the current design phase.

## Coexistence conflicts and controls

| Conflict | Failure mode | Required control |
| --- | --- | --- |
| Two camera blendables | doubled darkness, incorrect mask ordering, different unknown regions | only one system may own final fog composition in an integration build |
| Two actor-visibility authorities | old system hides an actor the new system exposes, or vice versa; hidden state can remain latched | one feature flag/adapter selects the sole subject authority; explicitly restore render state when switching |
| Two exploration writers | divergent memories and save payloads | independent test map first; during integration select one writer and treat the other as read-only/disabled |
| Old 10 Hz grid vs new exact query | HUD/interactions/enemies disagree | route all visibility consumers through one adapter API per run |
| `ECC_Visibility` vs explicit occluders | decorative/collision proxies become unintended blockers; door proxy can double-block | new plugin uses registered occluder geometry and an explicit import/bake tool; do not silently scan collision at runtime |
| Automatic UE lights vs legal sources | ambient facility/pickup light changes knowledge unexpectedly | DARKWELL adapter explicitly registers legal sources/illumination fields |
| Old v6 grid save vs plugin snapshot | load restores one memory but not the other | plugin owns an independently versioned snapshot embedded by the DARKWELL save adapter; migration is one-way only after acceptance |
| Stateful memory proxies vs live actors | duplicate meshes, collision, audio, VFX, or shadows | memory proxy is render-only and policy-controlled; gameplay/collision remains on the authoritative actor |

The safest coexistence model is not two active fog systems in the same gameplay session. Keep the old system fully active on the production prototype, run the new plugin on a separate lab map, then use an explicit runtime authority switch for integration/A-B testing. Enemy hiding must never be controlled by both systems in one run.

## Audit conclusion

The current prototype demonstrates the desired knowledge rules but not the reusable architecture. The highest-leverage change is to make explicit 2.5D occluder geometry and per-source visibility polygons the shared stable authority, rasterize those polygons into world-space live/memory masks for presentation, and query the polygons directly for gameplay subjects. The gray layer must use immutable environment detail plus explicit last-seen subject proxies rather than treating current GBuffer geometry as historical truth.

The proposed replacement is specified in `VISION_SYSTEM_REQUIREMENTS.md` and `VISION_SYSTEM_ARCHITECTURE.md`; the non-destructive handoff path is in `VISION_SYSTEM_MIGRATION_PLAN.md`.

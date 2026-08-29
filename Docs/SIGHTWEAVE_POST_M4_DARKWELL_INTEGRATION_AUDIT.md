# Post-M4 SightWeave -> DARKWELL production integration audit

This document is a repository audit and implementation recommendation. It does not authorize or record a gameplay implementation.

Evidence labels used throughout:

- **[Document requirement]** means a frozen requirement, architecture, migration, or completed-milestone document.
- **[Current implementation fact]** means the checked-in source/config/asset inventory at the frozen baseline.
- **[Recommendation]** means the conclusion of this audit; it is not an already implemented feature or a historical roadmap fact.

## 1. Audit status and date

**COMPLETED — documentation-only audit, 2026-08-29 (Asia/Shanghai).**

The repository was inspected without starting Unreal Editor, running UBT/UAT, building, cooking, packaging, or running automation. No C++, header, shader, Build.cs, config, descriptor, Blueprint, `.uasset`, or `.umap` was changed.

## 2. Audit branch

`codex/post-m4-sightweave-darkwell-integration-audit`

The branch was created from the exact frozen M4P3 revision after local/upstream/remote equality and a clean Git/LFS state were confirmed.

## 3. Frozen M4 baseline

- Frozen implementation baseline: `94be5835212a7f10491cd359676fcb5ee06dc08b`.
- Completed source branch: `codex/m4p3-sightweave-persistence-restore-closure`.
- M4P3 is **COMPLETED** with no open correctness gate (`Docs/SIGHTWEAVE_M4P3_FINAL_VALIDATION.md:5`).
- The host owns save slots and I/O; world integration is through `USightWeaveWorldSubsystem`, and M4P3 deliberately does not bind DARKWELL SaveGame slots (`Docs/SIGHTWEAVE_M4P3_HANDOFF.md:13`).

This audit freezes, and does not reopen, M1-M4.

## 4. Frozen M1-M4 capability ledger

| Capability | Frozen result and evidence | Integration consequence |
| --- | --- | --- |
| M1 plugin/Lab | **[Document requirement/completed fact]** Independent plugin, generic modules, isolated `/SightWeave/Maps/L_SightWeave_Lab`; the current plugin inventory still contains that map (`Plugins/SightWeave/README.md:5-20`). | Do not create another vision core or put DARKWELL logic in the plugin. |
| M2 CPU authority | **[Current implementation fact]** `USightWeaveWorldSubsystem` owns floor/source/illumination/occluder registration and exact queries (`Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveWorldSubsystem.h:127-301`). | The Adapter maps product state into existing handles and consumes existing query results. |
| M3 presentation/memory | **[Document requirement/completed fact]** CPU HardMemory is authority, the GPU is derived, selected memory precision is Coarse 25 cm, and Width=50 is the frozen inward-only presentation feather. M3.5 final validation records the completed CPU/GPU split and black/gray/live order (`Docs/SIGHTWEAVE_M3P5_FINAL_VALIDATION.md:82-86`, `:112-119`). | Do not reconstruct a DARKWELL mask, read GPU pixels for gameplay, or re-tune the plugin contract in the integration milestone. |
| M4P1 subjects | **[Current implementation fact]** The five policies and game-thread `FSightWeaveSubjectMemoryAuthority` exist (`Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveSubjectMemory.h:20`, `:304-333`). M4P1 changed no product asset or DARKWELL source (`Docs/SIGHTWEAVE_M4P1_FINAL_VALIDATION.md:43`). | Map Stalker to `NeverRemember`; do not add a second Last-Seen state machine. |
| M4P2 delivery | **[Document requirement/completed fact]** BuildPlugin, clean host, Shipping isolation, staged D3D12/SM6, full regression, lifecycle, and frozen performance gates are complete (`Docs/SIGHTWEAVE_M4P2_FINAL_VALIDATION.md:5-16`, `:29-62`). | Project integration must preserve module isolation; it need not repeat plugin packaging work unless plugin source changes. |
| M4P3 persistence | **[Current implementation fact]** `CapturePersistenceSnapshot`/`RestorePersistenceSnapshot` already bridge the world subsystem to deterministic V1 capture/restore (`Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveWorldSubsystem.h:343-353`). | DARKWELL eventually embeds the opaque blob; it must not invent a second serializer or move slots into the plugin. |
| Full regression | **[Document requirement/completed fact]** M4P3 reports M4P3 15/15 + 16/16, M3P5 16/16 + 26/26, M4P1 9/9 + 12/12, full SightWeave 191/191 + 283/283, and DARKWELL 24/24 (`Docs/SIGHTWEAVE_M4P3_HANDOFF.md:29`). | These are frozen evidence. This audit does not rerun them. |

The current `Plugins/SightWeave/README.md` still describes an M2-era capability subset (`:5-25`) and is not a reason to reimplement M3/M4. Final milestone reports and current public headers are the stronger evidence.

### Frozen DARKWELL product contract

M6P1 intentionally implements only a vertical subset, but the Adapter boundary must preserve the complete product direction:

1. **[Document requirement]** The player always has a small attached circular source that bypasses legal illumination but still obeys occlusion, floor/height, and live suppression (`Docs/VISION_SYSTEM_REQUIREMENTS.md:61`).
2. **[Document requirement]** The primary player view is an occluded directional cone; generic V1 source shapes include directional and remote sources (`Docs/VISION_SYSTEM_REQUIREMENTS.md:50-59`).
3. **[Document requirement]** Enemies display only under effective live coverage, which for the main cone requires compatible explicit legal illumination (`Docs/VISION_SYSTEM_REQUIREMENTS.md:71-79`, `:159`).
4. **[Document requirement]** Torch and lantern are product-owned legal lights with resource/durability semantics; the Adapter registers their legal profiles and never lets rendered brightness become authority (`Docs/VISION_SYSTEM_REQUIREMENTS.md:71-79`; existing resources at `Source/Darkwell/Public/Combat/DarkwellLoadoutComponent.h:58-68`).
5. **[Document requirement]** Complete darkness is valid; when legal light is unavailable, only the body-circle path may remain (`Docs/VISION_SYSTEM_REQUIREMENTS.md:61`, `:71-79`).
6. **[Document requirement]** Camera, radar, and remote observation/lighting extend the same Knowledge Owner only through explicitly Adapter-activated sources (`Docs/VISION_SYSTEM_REQUIREMENTS.md:50-59`).
7. **[Document requirement]** A qualifying hit on the player may reveal only the resolved attacking Subject for a bounded time (`Docs/VISION_SYSTEM_REQUIREMENTS.md:169-171`).
8. **[Document requirement]** The player attacking an enemy does not automatically trigger that reveal (`Docs/VISION_SYSTEM_REQUIREMENTS.md:169`).
9. **[Document requirement]** Static floor, walls, fixed decoration, and approved fixed uncollected items may contribute readable neutral-gray memory (`Docs/VISION_SYSTEM_REQUIREMENTS.md:115-117`, `:161`).
10. **[Document requirement]** Enemies and current dynamic state never remain as gray remembered silhouettes (`Docs/VISION_SYSTEM_REQUIREMENTS.md:117`, `:159`).
11. **[Document requirement]** Every ordinary query remains exactly `Unknown`/black, `Remembered`/gray, or `Visible`/live (`Docs/VISION_SYSTEM_REQUIREMENTS.md:30-38`).
12. **[Document requirement]** Black becomes gray only after legal hard live coverage writes memory; camera/GPU/material/light pixels cannot write it (`Docs/VISION_SYSTEM_REQUIREMENTS.md:40`, `:79`).
13. **[Document requirement]** Regional rules retain distinct clear, block-write, suppress-memory-presentation, suppress-live, and composed permanent-blackout semantics (`Docs/VISION_SYSTEM_REQUIREMENTS.md:133-147`).
14. **[Document requirement]** Floors, height bands, rooms/regions, sources, occluders, subjects, memory, and modifiers remain explicitly scoped (`Docs/VISION_SYSTEM_REQUIREMENTS.md:177-179`).
15. **[Document requirement]** SightWeave remains generic; DARKWELL-specific source activation, combat qualification, HUD, audio/VFX, mission, and save policy remain in the project Adapter (`Docs/VISION_SYSTEM_REQUIREMENTS.md:11`).
16. **[Document requirement]** CPU hard queries/HardMemory remain gameplay authority; Material, GPU output, final composition, and memory proxies never become authority (`Docs/VISION_SYSTEM_ARCHITECTURE.md:354-376`).
17. **[Document requirement]** SceneCapture is not the formal View or a replacement authority; it is only an optional, disabled-by-default presentation fallback, while completed real-view evidence uses the GameViewport/camera (`Docs/VISION_SYSTEM_ARCHITECTURE.md:414`; `Docs/SIGHTWEAVE_M4P2_EXECUTION_REPORT.md:94`, `:148`).
18. **[Document requirement]** DARKWELL SaveGame/slot eventually embeds the M4P3 blob, while slots, I/O, level loading, and autosave policy remain host-owned (`Docs/SIGHTWEAVE_M4P3_PERSISTENCE_RESTORE_CONTRACT.md:9-15`; `Docs/VISION_SYSTEM_ARCHITECTURE.md:574`).

## 5. Current DARKWELL gameplay and system boundaries

### Runtime ownership

| Concern | Current owner | Evidence |
| --- | --- | --- |
| Pawn, movement, aim, camera | **[Current implementation fact]** `ADarkwellCharacter` owns movement/aim input, top-down spring arm/camera, loadout, interaction, inventory, and legacy visibility. | `Source/Darkwell/Public/Player/DarkwellCharacter.h:26-50`, `:104-142`; construction at `Source/Darkwell/Private/Player/DarkwellCharacter.cpp:39-132`. |
| Controller and product flow | **[Current implementation fact]** `ADarkwellPlayerController` owns menu/input/product-flow calls into the save subsystem. | Save calls at `Source/Darkwell/Private/Player/DarkwellPlayerController.cpp:181-323`. |
| World bootstrap | **[Current implementation fact]** `ADarkwellGameMode` selects native Pawn/Controller/GameState/HUD and spawns the greybox world and roster when missing. | `Source/Darkwell/Private/Game/DarkwellGameMode.cpp:52-57`, `:74-200`. |
| World/save lifecycle | **[Current implementation fact]** GameMode calls `BeginWorldSession`, then after actor creation calls `ApplyPendingLoad` and `CompleteWorldStart`; `UDarkwellSaveSubsystem` is the only project `UGameInstanceSubsystem`. | `Source/Darkwell/Private/Game/DarkwellGameMode.cpp:60-69`, `:197-200`; `Source/Darkwell/Public/Save/DarkwellSaveSubsystem.h:14`. |
| Torch/lantern gameplay | **[Current implementation fact]** `UDarkwellLoadoutComponent` owns charge, heat, fuel, equipped state, tool actions, and semantic getters such as `IsTorchOn`/`IsLanternOn`. | `Source/Darkwell/Public/Combat/DarkwellLoadoutComponent.h:33-72`, `:112-223`; state queries at `Source/Darkwell/Private/Combat/DarkwellLoadoutComponent.cpp:337-373`. |
| Rendered player lights | **[Current implementation fact]** Character creates `TorchLight`, `LanternBaseLight`, and `LanternFocusLight`; loadout mutates their visibility/intensity/range/cone for presentation. | `Source/Darkwell/Private/Player/DarkwellCharacter.cpp:84-126`; `Source/Darkwell/Private/Combat/DarkwellLoadoutComponent.cpp:624-738`. |
| Enemies and AI | **[Current implementation fact]** `ADarkwellStalkerCharacter` owns enemy durable/presentation state; `ADarkwellWardenCharacter` derives the second archetype; `ADarkwellStalkerController` owns AI Perception sight/hearing and navigation behavior. | `Source/Darkwell/Public/AI/DarkwellStalkerCharacter.h:16-68`; `Source/Darkwell/Public/AI/DarkwellStalkerController.h:43-49`; `Source/Darkwell/Private/AI/DarkwellStalkerController.cpp:18-44`, `:196-308`. |
| Enemy fog display | **[Current implementation fact]** Stalker implements legacy `IDarkwellFogSubject` and calls `SetActorHiddenInGame` unless the legacy cell is `Visible`. | `Source/Darkwell/Public/AI/DarkwellStalkerCharacter.h:16-29`; `Source/Darkwell/Private/AI/DarkwellStalkerCharacter.cpp:76-79`. |
| HUD | **[Current implementation fact]** `ADarkwellHUD` generates legacy fog presentation and independently suppresses threat rows using the legacy point query. | `Source/Darkwell/Private/UI/DarkwellHUD.cpp:108-138`, `:226-275`, `:539-553`, `:987-1027`. |
| Interaction | **[Current implementation fact]** `UDarkwellInteractionComponent` owns facing/proximity selection and obstruction traces; pickup classes currently make themselves unavailable by changing mesh/collision in legacy fog callbacks. | `Source/Darkwell/Private/Interaction/DarkwellInteractionComponent.cpp:82-184`; example pickup callback at `Source/Darkwell/Private/World/DarkwellAmmoPickup.cpp:53-60`. |

### `/Game/Maps/L_Prototype` runtime composition

- **[Current implementation fact]** Both game and editor default maps are `/Game/Maps/L_Prototype`, and the global GameMode is `/Script/Darkwell.DarkwellGameMode` (`Config/DefaultEngine.ini:4-6`).
- **[Current implementation fact]** That GameMode selects `ADarkwellCharacter`, `ADarkwellPlayerController`, `ADarkwellGameState`, and `ADarkwellHUD` (`Source/Darkwell/Private/Game/DarkwellGameMode.cpp:52-57`).
- **[Current implementation fact]** It spawns the prototype room, door, facilities, pickups, workbench, Stalker, and Warden when absent (`Source/Darkwell/Private/Game/DarkwellGameMode.cpp:74-194`).
- **[Current implementation fact]** The binary map itself is `Content/Maps/L_Prototype.umap`, is tracked through Git LFS, and the README records that it contains navigation support while the native GameMode supplies the greybox actors (`README.md:40`). This documentation-only audit does not inspect or alter internal Unreal asset references.

## 6. Current SightWeave integration degree

**Result: zero product integration, with plugin discovery/loading availability only.**

1. **[Current implementation fact]** `Plugins/SightWeave/SightWeave.uplugin` has `EnabledByDefault=true` and Runtime modules `SightWeaveRender` and `SightWeaveRuntime` (`:8-21`). The project can therefore discover/load the plugin modules.
2. **[Current implementation fact]** `Source/Darkwell/Darkwell.Build.cs:11-23` contains no `SightWeaveRuntime` or `SightWeaveRender` dependency.
3. **[Current implementation fact]** A repository search finds no `SightWeave` token anywhere under `Source/Darkwell`, `Config`, or `Darkwell.uproject`.
4. **[Current implementation fact]** There is no DARKWELL Adapter, bridge component, bridge subsystem, temporary source, subject registration, presentation-scope selection, or snapshot binding.
5. **[Current implementation fact]** `Darkwell.uproject:13-90` does not explicitly list SightWeave, though enabled-by-default project-plugin discovery makes explicit listing unnecessary for availability.

Therefore “loaded as an enabled project plugin” must not be confused with “linked and used by DARKWELL gameplay.”

## 7. Legacy fog status and coexistence

The legacy system remains the entire product authority:

- **[Current implementation fact]** `UDarkwellVisibilityComponent` owns `VisibleCells`, `ExploredCells`, and fine presentation memory and ticks a 10 Hz refresh (`Source/Darkwell/Public/Gameplay/DarkwellVisibilityComponent.h:69-120`; `Source/Darkwell/Private/Gameplay/DarkwellVisibilityComponent.cpp:138-164`, `:522-573`).
- **[Current implementation fact]** It globally discovers ordinary local light components (`Source/Darkwell/Private/Gameplay/DarkwellVisibilityComponent.cpp:201-283`), which is incompatible with the product requirement for explicit legal illumination.
- **[Current implementation fact]** It scans all actors implementing `IDarkwellFogSubject` and supplies an actor-origin legacy cell state (`Source/Darkwell/Private/Gameplay/DarkwellVisibilityComponent.cpp:640-652`).
- **[Current implementation fact]** HUD creates the legacy camera blendable from `/Game/UI/Fog/M_FogMemoryComposite` and drives its transient CPU texture (`Source/Darkwell/Private/UI/DarkwellHUD.cpp:108-112`, `:539-987`).
- **[Document requirement]** The migration contract requires an explicit single-authority switch and forbids both systems controlling hiding, HUD, interactions, memory, save, or final composition in one run (`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:7-12`).

**[Recommendation]** Preserve three world-start modes named by the existing M6 roadmap: `Legacy`, `SightWeaveObserveOnly`, and `SightWeave` (`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:241-251`). Observe-only may compare CPU results and diagnostics but must never write memory or alter rendering/subjects/HUD. `SightWeave` must disable legacy component ticking/writes/callbacks and force the legacy blendable weight to zero before enabling SightWeave presentation. The first slice should select the mode at world start only; runtime hot switching is deferred until a later M6 slice.

`L_Prototype` remains `Legacy`; the new integration map uses `SightWeave`. That is rollback by map/mode, not dual authority.

## 8. DARKWELL SaveGame status

- **[Current implementation fact]** `UDarkwellSaveGame::CurrentVersion` is 6 and supports versions 1-6 (`Source/Darkwell/Public/Save/DarkwellSaveGame.h:129-161`).
- **[Current implementation fact]** v6 `FDarkwellPlayerSaveData` stores `ExploredFogCells`, `ExploredFogPresentationCells`, and presentation cell size (`Source/Darkwell/Public/Save/DarkwellSaveGame.h:66-74`).
- **[Current implementation fact]** `UDarkwellSaveSubsystem::CaptureCurrentGame` reads both legacy grids from the Character visibility component (`Source/Darkwell/Private/Save/DarkwellSaveSubsystem.cpp:352-390`); `ApplyPendingLoad` restores them (`:172-230`).
- **[Document requirement]** The host must embed the opaque SightWeave value; SightWeave never owns `SaveGameToSlot`, level loading, or autosave timing (`Docs/VISION_SYSTEM_ARCHITECTURE.md:574-599`; `Docs/SIGHTWEAVE_M4P3_PERSISTENCE_RESTORE_CONTRACT.md:9-16`).
- **[Document requirement]** Legacy v6 fog cells are not converted into SightWeave memory; without a valid SightWeave blob, SightWeave begins empty (`Docs/VISION_SYSTEM_ARCHITECTURE.md:599`).

**[Recommendation]** Do not change save version or embed the V1 blob in the first visible integration slice. Runtime authority, lifecycle, and presentation must be stable first, following the existing M6 integration order, where save embedding is step 6 (`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:272-280`). Save integration is a later bounded M6 milestone.

## 9. Player, light, enemy, HUD, and map integration points

| Product seam | First-slice mapping | Ownership rule |
| --- | --- | --- |
| Player body circle | Register one player-attached radial source with `BypassLegalIllumination`, range 120 cm, current floor/scope. Requirement: `Docs/VISION_SYSTEM_REQUIREMENTS.md:61`. Existing source type/API: `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveTypes.h:230-299`, `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveWorldSubsystem.h:157-169`. | Adapter owns registration; Character only supplies transform/floor context. |
| Player main cone | Register one directional source requiring legal illumination, using Character facing, 2200 cm range, and current 52-to-35 degree aim profile. Legacy values: `Source/Darkwell/Public/Gameplay/DarkwellVisibilityComponent.h:132-142`; Character exposes aim state at `Source/Darkwell/Public/Player/DarkwellCharacter.h:57-59`. | Player gameplay owns facing/aim; Adapter translates it. |
| First legal light | Map the **torch** semantic `IsTorchOn()` and durability to one explicit radial legal-illumination source. Existing semantic getter: `Source/Darkwell/Public/Combat/DarkwellLoadoutComponent.h:58-65`; plugin API: `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveWorldSubsystem.h:173-184`. | Loadout owns charge/heat/equipment; Adapter owns legal-light activation/profile. It must not infer authority from rendered intensity, Scene Color, or `ULightComponent` discovery. |
| First enemy | Map the base **Stalker** to `NeverRemember`, query bounds/samples through SightWeave, hide/show its presentation, and use the same hard-live result for its HUD row. Existing enemy legacy seam: `Source/Darkwell/Private/AI/DarkwellStalkerCharacter.cpp:76-79`; policy API: `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveSubjectMemory.h:20-27`. | Enemy AI/combat/collision remain gameplay-owned; Adapter controls only approved presentation and visibility-derived HUD eligibility. |
| Minimal static memory | Register only the integration fixture's explicitly immutable floor/wall description and configure Coarse HardMemory; do not implement a production-wide material/provider importer. Existing components: `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveComponents.h:201-251`; memory setup: `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveWorldSubsystem.h:314-331`. | Fixture/Adapter declares immutable eligibility. GPU remains derived. |
| HUD | Make legacy fog texture generation conditional on selected authority and route the one integrated Stalker threat row through a DARKWELL-neutral visibility facade. Existing direct dependency: `Source/Darkwell/Private/UI/DarkwellHUD.cpp:226-275`, `:539-553`. | HUD depends on DARKWELL facade/result, not plugin render internals. |
| Final View | In SightWeave mode, set one presentation scope through `USightWeaveRenderWorldSubsystem::SetPresentationScope` and ensure the legacy blendable is zero. APIs: `Plugins/SightWeave/Source/SightWeaveRender/Public/SightWeaveRenderWorldSubsystem.h:49-68`; legacy control: `Source/Darkwell/Private/UI/DarkwellHUD.cpp:1019-1027`. | World Adapter selects scope; Render module presents only. |
| Dedicated map | Create `/Game/Maps/L_VisionIntegration` through Unreal Editor APIs, with a C++ integration GameMode/fixture. Existing route requires this separation (`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:235-251`). | Map/World Settings are asset wiring; all authority rules remain C++. |

The Stalker is selected before Warden because it is the base enemy implementation, already owns the legacy visibility seam, and has fewer archetype-specific presentation variables (`Source/Darkwell/Public/AI/DarkwellStalkerCharacter.h:16-68`). The torch is selected before lantern because it is the default right-hand item and maps to one radial legal-light profile; lantern would introduce base plus focus/flash profiles and more state transitions (`Source/Darkwell/Private/Player/DarkwellCharacter.cpp:103-126`, `Source/Darkwell/Private/Combat/DarkwellLoadoutComponent.cpp:724-738`).

## 10. Plugin/Product Adapter responsibility boundary

```text
DARKWELL gameplay state
  Character facing/aim + Loadout semantic state + Enemy gameplay state
                             |
                             v
  UDarkwellSightWeaveWorldSubsystem / DARKWELL adapter components
  - authority mode and world/scope lifetime
  - project IDs/profiles and stable registrations
  - query facade, subject presentation, HUD eligibility
  - later: SaveGame blob binding, damage reveal, regions
              | Runtime public API         | Render public API
              v                            v
  SightWeaveRuntime CPU authority      SightWeaveRender derived View
  - sources/lights/occluders           - one selected presentation scope
  - hard queries and HardMemory        - Width=50 black/gray/live composite
  - subject policy/persistence         - never gameplay authority
```

### Recommended module and lifecycle design

1. **[Recommendation] Adapter location.** Keep it in the existing `Darkwell` Runtime module under a focused `Visibility` or `Integration/SightWeave` area. Do not create a game-specific plugin and do not change SightWeave public contracts for project conveniences. `AGENTS.md` already places runtime gameplay under `Source/Darkwell`.
2. **[Recommendation] Module dependencies.** Add `SightWeaveRuntime` as an unconditional **private** dependency of `Darkwell`. Add `SightWeaveRender` privately only for non-Server targets and compile the small presentation-scope seam out under `UE_SERVER`; do not expose plugin types in existing DARKWELL public gameplay/HUD interfaces. Runtime public API is required for CPU registration/query; Render public API is required only for explicit presentation-scope enable/clear. Current absence is visible at `Source/Darkwell/Darkwell.Build.cs:11-23`.
3. **[Recommendation] World owner.** A project-owned `UDarkwellSightWeaveWorldSubsystem : UWorldSubsystem` owns the authority mode, stable owner/floor IDs, `FSightWeaveSubjectMemoryAuthority`, adapter registrations, teardown, and calls into the two existing SightWeave world subsystems. It does not replace them or own GPU resources.
4. **[Recommendation] Actor calls.** Character/loadout/enemy/HUD use a DARKWELL facade or small DARKWELL adapter component. They must not include SightWeave Render internals, allocate proxies, or duplicate query math. The Adapter is the only product-to-plugin translation seam.
5. **[Recommendation] Gameplay split.** Movement, aim, input, durability, heat/fuel, AI Perception, navigation, combat, stun, collision, and persistence identities remain in current gameplay classes. SightWeave decides knowledge/visibility; the Adapter applies only approved presentation and HUD eligibility.
6. **[Recommendation] Static memory.** The first slice registers explicit integration-fixture eligibility. General world conversion, material support, rooms/floors, and production providers remain later M6 work.
7. **[Recommendation] Save.** `UDarkwellSaveSubsystem` remains slot/level-load owner and later asks the world Adapter to capture/restore the existing M4P3 blob. No SaveGame code belongs in `SightWeaveRuntime`.
8. **[Recommendation] Dedicated Server/NullRHI/Shipping.** CPU registration/query code must remain available without a rendered RHI. The Adapter must guard presentation-scope calls with `UE_SERVER`, `FApp::CanEverRender()`, and world/net-mode policy and never depend on Editor/Tests. Existing render-state paths explicitly classify `GUsingNullRHI || !FApp::CanEverRender()` as no-GPU/fail-closed (`Plugins/SightWeave/Source/SightWeaveRender/Private/SightWeaveSingleTileRenderState.cpp:222-224`; `Plugins/SightWeave/Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.cpp:2099-2101`). Shipping must compile only the existing Runtime/Render modules; Editor/Test modules remain descriptor-only metadata as already proven by M4P2/M4P3. **[Current implementation fact]** This repository has Game and Editor targets, not a Server target (`Source/Darkwell.Target.cs:6-10`; `Source/DarkwellEditor.Target.cs:6-10`), and `SightWeaveRender` is presently an enabled Runtime module without a descriptor-level Server exclusion (`Plugins/SightWeave/SightWeave.uplugin:8-21`). Therefore M6P1 can make the DARKWELL Adapter server-safe and prove NullRHI/Shipping, but it must not claim process-level dedicated-server module exclusion without a separately added Server target and packaging proof. If that becomes a current product requirement, stop and scope it separately rather than editing the completed plugin silently.

## 11. Binary asset and map impact

- **[Current implementation fact]** Product maps contain only `Content/Maps/L_Prototype.umap`; the only SightWeave map is the generic `Plugins/SightWeave/Content/Maps/L_SightWeave_Lab.umap`.
- **[Document requirement]** The generic Lab contains no `/Game/` or `/Script/Darkwell` dependency (`Plugins/SightWeave/README.md:18-20`, `:37-39`), so it cannot be the product integration map.
- **[Recommendation]** The selected milestone requires exactly one new binary map, `/Game/Maps/L_VisionIntegration`, created/saved through Unreal Editor or official Editor APIs. The map owns only World Settings/navigation/layout wiring; a native C++ integration GameMode/fixture owns repeatable actors and rules.
- **[Recommendation]** No Blueprint asset is required for the first slice. Blueprint may later bind art/tuning, never core authority.
- **[Recommendation]** Do not edit `Content/Maps/L_Prototype.umap`, `Content/UI/Fog/M_FogMemoryComposite.uasset`, or `Darkwell.uproject`.

If new binary-asset creation is not authorized, Candidate B cannot achieve its required formal product View without violating the M6 map boundary. In that case the correct action is to stop and ask, not silently fall back to `L_Prototype` or the plugin Lab.

## 12. Candidate milestone comparison

### Candidate A — M6 Adapter and world-lifecycle foundation (observe-only)

- **Repository evidence:** M6 begins with observe-only source/light/occluder/scope mapping (`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:241-278`); currently no DARKWELL dependency or bridge exists (`Source/Darkwell/Darkwell.Build.cs:11-23`).
- **Player value:** diagnostic/risk-reduction only; no visible player-facing improvement.
- **Prerequisites:** module dependency, world owner, stable floor/owner/profile IDs, lifecycle tests.
- **SightWeave contract change:** none.
- **DARKWELL modules:** `Darkwell` only.
- **Assets/project descriptor:** no asset or `Darkwell.uproject` change required; no integration map strictly required for CPU-only tests.
- **`L_Prototype`:** unchanged.
- **Automation:** lifecycle, registration, CPU result comparison, no-authority side effects, NullRHI teardown.
- **Agent View inspection:** none meaningful; debug-only evidence is not a formal product View.
- **User PIE:** none.
- **Risk/rollback:** lowest risk; remove private dependencies/Adapter. Main risk is establishing an abstraction that has never controlled a real consumer.
- **Estimated size:** small-to-medium.
- **Disposition:** **defer as a standalone milestone.** It is a useful first checkpoint inside Candidate B, but fails the audit requirement for clear player/product value and cannot prove final composition/subject authority.

### Candidate B — M6 minimal formal visibility vertical slice

- **Repository evidence:** M6 is the named next product-integration stage and requires a dedicated integration map plus mutually exclusive modes (`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:235-288`). M4P3 removed the prior persistence prerequisite (`Docs/SIGHTWEAVE_M4P3_FINAL_VALIDATION.md:5`).
- **Player value:** a real DARKWELL player can navigate black/gray/live space, use the body circle and torch-gated cone, and see one Stalker plus its threat row only under legal current vision.
- **Prerequisites:** Candidate A's foundation, but only as phase 1 of the same end-to-end milestone.
- **SightWeave contract change:** none; use existing public Runtime/Render APIs.
- **DARKWELL modules:** `Darkwell` Runtime, existing Game target, existing HUD/Character/Loadout/Stalker seams.
- **Assets/project descriptor:** one new `.umap`; no Blueprint or `Darkwell.uproject` change.
- **Integration map:** required, `/Game/Maps/L_VisionIntegration`.
- **`L_Prototype`:** unchanged and kept on Legacy authority.
- **Automation:** authority exclusivity, world lifecycle, source/light transform/state, static wall, memory, Stalker `NeverRemember`, threat-HUD eligibility, teardown, NullRHI, D3D12 real View, build/Shipping boundaries.
- **Agent View inspection:** required D3D12/SM6 screenshots for body circle, torch cone/light gating, wall occlusion, black-to-gray memory, Stalker hidden/live/hidden with no gray silhouette, and no double composite.
- **User PIE:** required short route validating movement/aim/tool/visibility feel and Stalker/HUD transitions.
- **Risk/rollback:** medium and bounded. Roll back by selecting `Legacy`/loading `L_Prototype`; no existing product asset or save schema is touched.
- **Estimated size:** medium; one focused branch, one map, one light, one enemy.
- **Disposition:** **RECOMMENDED.** It is the smallest slice that proves the architectural seam and delivers observable product value.

### Candidate C — M6 DARKWELL SaveGame binding for the M4P3 blob

- **Repository evidence:** current save v6 stores only legacy grids (`Source/Darkwell/Public/Save/DarkwellSaveGame.h:66-74`, `Source/Darkwell/Private/Save/DarkwellSaveSubsystem.cpp:385-390`); M4P3 deliberately leaves slots to the host (`Docs/SIGHTWEAVE_M4P3_HANDOFF.md:13`).
- **Player value:** eventual continuation of SightWeave knowledge, but no visible gameplay change until runtime integration exists.
- **Prerequisites:** stable DARKWELL world/scope IDs, subject/provider registry, authority selection, world-start restore ordering, and a functioning SightWeave product world.
- **SightWeave contract change:** none.
- **DARKWELL modules:** SaveGame/SaveSubsystem plus the future world Adapter.
- **Assets/project descriptor:** no binary asset required by serialization itself; no `Darkwell.uproject` change.
- **Integration map:** required for meaningful end-to-end restore validation, though not for byte-level tests.
- **`L_Prototype`:** must remain unchanged at this stage.
- **Automation:** slot version/corruption/failure, opaque blob round trip, atomic restore after level reconstruction, no-v6-fog conversion.
- **Agent View inspection:** later D3D12 before/after load evidence.
- **User PIE:** later save/exit/load route.
- **Risk/rollback:** medium-high before runtime scope is stable; save schema becomes durable and ordering defects can affect continuation files. Rollback requires ignoring the new optional field while preserving v1-v6 support.
- **Estimated size:** medium, but coupled to an unproven product Adapter if done now.
- **Disposition:** **defer.** It is explicitly sixth in M6's integration order (`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:272-280`) and must not be used to avoid the first visible integration proof.

## 13. Unique recommended next milestone

Choose **Candidate B only**. Candidate A is incorporated as its first internal checkpoint; Candidate C remains a later independent M6 slice.

This is post-M4 DARKWELL production integration, not M4P4 and not new plugin work.

## 14. Accurate milestone name

The repository authority already names the stage **`M6 — DARKWELL adapter integration without authority overlap`** (`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:235`). Preserve that name and number.

For branch/checkpoint clarity, this audit recommends the bounded subdivision:

**`M6P1 — DARKWELL Adapter Minimal Visibility Vertical Slice`**

`M6P1` is an audit-recommended subdivision, not a previously completed or historically named roadmap milestone.

## 15. One-sentence objective

On a dedicated DARKWELL integration map, use one project-owned Adapter and a single SightWeave authority to deliver the player body circle, illumination-gated facing cone, semantic torch legal light, one `NeverRemember` Stalker, matching threat-HUD eligibility, and the formal D3D12 black/gray/live View while keeping `L_Prototype`, SaveGame, and legacy rollback intact.

## 16. Explicit scope

1. Add private `SightWeaveRuntime`/`SightWeaveRender` dependencies to the `Darkwell` module.
2. Add the three M6 authority modes, selected once at world start.
3. Add `UDarkwellSightWeaveWorldSubsystem` as project lifecycle/facade owner.
4. Create one stable Knowledge Owner, one active floor, Coarse memory, and one presentation scope.
5. Register/update one 120 cm illumination-bypass body circle.
6. Register/update one 2200 cm directional player cone requiring the torch capability; preserve 52-to-35 degree aim narrowing.
7. Register one explicit torch legal-illumination source from `UDarkwellLoadoutComponent::IsTorchOn()` and resource state, not rendered light sampling.
8. Register one static wall/doorway occluder and one explicitly immutable fixture surface for gray-memory proof.
9. Route one base Stalker through `NeverRemember`; use one shared hard-live result for world presentation and its threat HUD row.
10. Disable legacy ticking/writes/callbacks and legacy blendable in SightWeave mode; keep them intact in Legacy mode.
11. Create `/Game/Maps/L_VisionIntegration` with a native integration GameMode/fixture.
12. Add focused C++ automation, NullRHI lifecycle/authority coverage, and D3D12/SM6 real-view capture.
13. Complete Agent screenshot inspection and a short user-operated PIE gate.

## 17. Explicit non-goals

- No SightWeave Runtime/Render public-contract or shader change unless a genuine blocking defect is separately reported and approved.
- No M4P4, plugin authoring expansion, plugin Lab expansion, or BuildPlugin re-closure.
- No `L_Prototype` edit or production default-map switch.
- No SaveGame version change, M4P3 blob embedding, slot I/O, or legacy-v6 fog conversion.
- No damage-source reveal.
- No lantern, camera, radar, remote source, Warden, pickups, doors/containers/machines, Last-Seen provider, or all-enemy conversion.
- No general static-world conversion pipeline, material matrix, multiple floors, room/region authoring, blackout/lock-gray rules, or dynamic-door integration.
- No legacy code/material/asset deletion.
- No runtime authority hot switching; world-start selection only.
- No SceneCapture, screen mask, GPU query, Material authority, or proxy authority.
- No gameplay AI, combat, durability, resource-balance, movement, or encounter tuning.

## 18. M1-M4 contracts that must remain frozen

1. CPU hard queries and CPU HardMemory are authority; GPU and final View are derived.
2. Effective live is per-source compatible vision/illumination union plus explicit bypass, then hard suppression.
3. Body circle is illumination-bypass but still occluded/floor-scoped/suppressible.
4. Legal illumination is explicitly registered and never inferred from rendered brightness, Scene Color, or ordinary light discovery.
5. Black/gray/live ordering and legal black-to-gray write rules remain exact.
6. Coarse 25 cm memory and Width=50 inward-only feather remain unchanged.
7. Stalker uses `NeverRemember`; no enemy Last-Seen proxy or gray silhouette.
8. Subject/world/HUD decisions consume the same authoritative revision/result.
9. SceneCapture is not formal View or authority.
10. M4P3 V1 blob format/provider/atomic restore remains unchanged and unused by M6P1.
11. Plugin remains DARKWELL-neutral; project rules remain in `Source/Darkwell`.
12. M4P2 Shipping isolation remains: no Editor/Tests dependency in game runtime.

## 19. Expected modules and files

The exact filenames are implementation recommendations; equivalent focused names are acceptable if the ownership stays unchanged.

| Area | Expected change |
| --- | --- |
| Module | `Source/Darkwell/Darkwell.Build.cs`: unconditional private `SightWeaveRuntime`; non-Server private `SightWeaveRender`; no public or Editor/Test dependency. |
| Authority/facade | New `Source/Darkwell/Private/Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h/.cpp` plus a small neutral DARKWELL result/mode header where needed. |
| Player bridge | New small DARKWELL player Adapter component or equivalent subsystem binding; minimal changes to `DarkwellCharacter`/`DarkwellLoadoutComponent` only to expose semantic state/events/approved transforms. |
| Enemy bridge | New DARKWELL subject Adapter and minimal Stalker presentation hook; AI/controller behavior unchanged. |
| HUD | `DarkwellHUD` mode-aware legacy composite and threat eligibility through the facade; other HUD content unchanged. |
| Integration fixture | New native integration GameMode/fixture under `Source/Darkwell`; do not branch production behavior on a string comparison to `L_Prototype`. |
| Tests | Focused additions under `Source/Darkwell/Private/Tests`, with real-view automation only where a rendered RHI is required. |
| Asset | New `Content/Maps/L_VisionIntegration.umap` only, created through Unreal Editor APIs. |
| Docs | M6P1 contract/final-validation/handoff documents after implementation. |

No proposed DARKWELL public interface should expose SightWeave Render types. If a UCLASS header requires plugin value types, keep it private to the module or hide them behind a private implementation so `SightWeaveRuntime`/`SightWeaveRender` remain private dependencies.

## 20. Dedicated Integration Map decision

**Yes, required.** Use `/Game/Maps/L_VisionIntegration`, as already prescribed by M6 (`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:237-251`). It is the smallest safe place to prove a real DARKWELL pawn, camera, torch, Stalker, HUD, wall, memory, and final View without placing algorithm/integration churn in production content.

## 21. `L_Prototype` decision

**Do not modify or enable SightWeave in `L_Prototype` during M6P1.** It remains the Legacy rollback/baseline. Production-content acceptance is M7 and is explicitly after M6 approval (`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:290-328`).

## 22. SaveGame decision

**Do not integrate SaveGame in M6P1.** Do not change save v6, do not add a blob field, and do not import legacy fog. The later save milestone will embed the already completed M4P3 V1 blob after the world/scope/subject lifecycle is stable.

## 23. Phased implementation plan

### Phase 0 — contract and baseline

- Freeze this audit SHA, confirm the chosen map-asset authorization, create the implementation branch from the audit tip, and add an M6P1 contract.
- Reconfirm clean worktree and no `Darkwell.uproject`/`L_Prototype` difference before any work.

### Phase 1 — Adapter foundation, no presentation

- Add private module dependencies, authority enum, world subsystem/facade, stable IDs, one floor, and deterministic teardown.
- Implement `Legacy` and `SightWeaveObserveOnly` behavior and automated proof that observe-only has no side effects.
- Register body/cone/torch/occluder in a transient test world and compare exact CPU results.

### Phase 2 — single View authority

- Add `SightWeave` world-start mode.
- Disable legacy visibility tick/write/subject callbacks and set the legacy HUD blendable to zero before selecting SightWeave presentation.
- Configure Coarse memory/static fixture and one Render presentation scope.
- Verify NullRHI retains CPU authority while GPU presentation is explicitly unavailable/fail-closed.

### Phase 3 — one product subject and HUD consumer

- Register Stalker `NeverRemember` with stable identity and bounded samples.
- Apply one authoritative result to approved enemy presentation and the matching threat-HUD row.
- Ensure collision, AI Perception, navigation, attacks, damage, and light reactions remain gameplay-owned and unchanged.

### Phase 4 — dedicated map asset

- Through Unreal Editor/official APIs, create `/Game/Maps/L_VisionIntegration` with C++ integration GameMode/fixture, navigation support, one floor, one straight wall/doorway, one player start, and one Stalker route.
- Save only the new map; inspect asset diffs and prove `L_Prototype` unchanged.

### Phase 5 — closure

- Build after every relevant C++ checkpoint, run the focused/full matrices below, capture formal D3D12 views, inspect them, then ask the user to run the bounded PIE route.
- Record failures honestly; push only reliable non-empty checkpoints.

## 24. Automation matrix

| Layer | Required cases |
| --- | --- |
| Pure rules | Mode exclusivity; stable product ID/profile mapping; body bypass versus cone-gated source; torch semantic activation/deactivation; no ULight intensity authority. |
| World lifecycle | World initialize/deinitialize/restart; exact one floor/scope; register/update/unregister handles; actor destroy during update; no stale handles/callbacks; no monotonic UObject/subject/proxy growth. |
| Legacy isolation | `Legacy` has zero Adapter effect; observe-only has zero presentation/subject/HUD/memory write effect; `SightWeave` disables legacy tick/write/callback/blendable; exactly one final composite and subject authority. |
| CPU authority | Body circle works without illumination; cone fails without torch; cone succeeds inside compatible torch polygon; wall occludes; outside cone/light fails; same revision reaches query consumer. |
| Memory/static | Only legal hard live writes gray memory; feather does not; wall/unknown remains black; fixture surface becomes gray; re-exploration works. |
| Subject/HUD | Stalker visible + HUD row inside legal current vision; both hidden outside; no Last-Seen snapshot/gray silhouette; AI/collision state unchanged. |
| NullRHI | Runtime authority and all nonvisual tests pass; no GPU allocation/readback; presentation is explicit fail-closed/unavailable; teardown clean. |
| D3D12 real View | Real GameViewport/player camera, not SceneCapture; black/gray/live pixels and wall boundary; Width=50; body circle; torch gate; Stalker/HUD transition; no double composition. |
| Regression | Full DARKWELL filter; focused SightWeave Runtime/M3.4/M3.5/M4P1/M4P3 prefixes affected by project integration; full SightWeave final if shared configuration or plugin source changes. |
| Build/package | Required `DarkwellEditor Win64 Development`; Game Development and Game Shipping compile to prove Runtime/Render-only dependency. Static/guard tests prove the Adapter has no Server-side Render calls, but no dedicated-server runtime claim is allowed because no Server target exists. BuildPlugin only if plugin source/config/descriptor changes, which M6P1 should avoid. |

Every run must report exact discovered/performed/pass/warning/fail counts and retain failed evidence. No historical threshold may be weakened.

## 25. NullRHI and D3D12 validation boundary

### NullRHI

NullRHI proves CPU authority, Adapter lifecycle, mode exclusivity, source/light/occluder/query behavior, memory writes, Stalker/HUD decision data, restore-free teardown, and no render allocation. It cannot prove the final View and must not classify “no pixels” as a visual pass.

### D3D12/SM6

D3D12 proves the real GameViewport/camera presentation, Runtime-to-Render publication, Width=50 composition, static gray memory, strict black, live scene, subject hiding, and absence of duplicate legacy/SightWeave blendables. The test must use the formal player View; SceneCapture is forbidden as a substitute.

## 26. Agent screenshot inspection

The Agent must open and inspect, not merely generate, at least these stable D3D12/SM6 captures:

1. body-circle-only region behind/outside the facing cone, wall clipped;
2. facing cone with active torch legal illumination;
3. same cone region without legal torch contribution, strict black except body circle;
4. legally viewed static fixture, then turned-away neutral-gray memory with current light excluded;
5. Stalker live with its threat row;
6. Stalker outside legal live vision: actor and threat row absent, no gray silhouette;
7. wall/doorway edge during a deterministic movement/turn sequence, with no legacy double edge or duplicate composite.

Record resolution, RHI/SM, camera, state/revision IDs, expected ROIs, black/nonblack/nonfinite counts, and direct visual observations. Automated pixels and Agent visual inspection are separate evidence.

## 27. User-operated PIE acceptance

The minimum user gate is a short run only in `/Game/Maps/L_VisionIntegration`:

1. move/turn/aim and confirm the body circle remains available in darkness and behind the facing direction while obeying walls;
2. use the default torch and confirm only the intersection of facing cone, torch legal illumination, and occlusion becomes live;
3. turn away and confirm legally seen static structure becomes stable gray while unknown remains black and current torch lighting does not remain in memory;
4. approach/turn through the doorway so the Stalker and matching threat row appear together, then leave legal vision so both disappear immediately with no remembered enemy outline;
5. confirm no doubled/wavy fog edge, white flash, stale proxy, interaction leak, or obvious input/gameplay regression;
6. stop PIE cleanly and report any warning/error or perceptual defect.

Without this user pass, the milestone may be **PARTIAL** after automated/Agent gates but not **COMPLETED**.

## 28. Suggested implementation branch

`codex/m6p1-sightweave-darkwell-adapter-vertical-slice`

Create it only in the separately authorized implementation task, from the final pushed audit SHA—not from an older M4 branch and not during this audit.

## 29. Reliable Git checkpoints

Suggested reviewable sequence:

1. `docs: define SightWeave M6P1 integration contract`
2. `build: add Darkwell SightWeave adapter boundary`
3. `feat: bridge Darkwell player vision and torch light`
4. `feat: route Stalker visibility through SightWeave`
5. `feat: add Darkwell SightWeave integration map`
6. `test: validate SightWeave Darkwell vertical slice`
7. `docs: record SightWeave M6P1 validation`

Each source checkpoint must pass the repository Editor build before commit; each asset checkpoint must be created/saved through Unreal APIs and checked for unintended `.uasset`/`.umap` changes. Push reliable checkpoints normally. No force push, rebase, reset, clean, or generated-output commit.

## 30. COMPLETED / PARTIAL / BLOCKED classification

### COMPLETED

All scoped code/asset work exists; Editor/Game Development/Game Shipping builds pass; focused and required full automation passes with exact counts; NullRHI and D3D12 scopes pass; formal View captures pass pixel checks and Agent inspection; user PIE passes; local/upstream/remote and Git/LFS/object integrity close; only the new integration map is the intended binary asset change.

### PARTIAL

Implementation is reliable and pushed but one required non-equivalent gate remains, most commonly user PIE, a formal D3D12 View, Shipping compile, or a declared regression. Do not relabel automated screenshots as user acceptance.

### BLOCKED

Stop without destructive recovery for an unknown dirty tree, incompatible branch history, missing authorization to create the required map asset, reproducible engine/toolchain failure, plugin public-contract defect requiring out-of-scope changes, or inability to maintain single authority without changing forbidden production content.

## 31. Risks and rollback boundary

| Risk | Required control | Rollback |
| --- | --- | --- |
| Dual final composition | Assert legacy blendable weight zero before SightWeave scope is enabled; count active presentation authorities. | Load `L_Prototype` in `Legacy`; clear SightWeave presentation scope. |
| Dual subject/HUD authority | One DARKWELL facade result drives both Stalker presentation and row; legacy callbacks disabled in SightWeave mode. | Re-enable legacy component only in a new Legacy world session. |
| Rendered-light coupling | Use loadout semantic state and Adapter profiles, not light-component discovery/intensity. | Remove torch registration; no gameplay/resource data changes. |
| Lifecycle/stale handles | WorldSubsystem owns handles, owner unregistration, subject authority, and teardown order. | Destroy integration world/map; no persisted blob exists. |
| NullRHI/server render dependency | Guard presentation and keep CPU-only tests; no Editor/Tests dependencies. | Disable presentation while retaining Runtime authority. |
| Asset contamination | Create one new map via Unreal APIs and inspect all asset diffs; never save `L_Prototype`. | Delete the new map only through Unreal asset APIs in a separately authorized rollback. |
| Save compatibility | Keep save v6 untouched and perform no blob restore. | No save rollback is needed. |
| Scope creep | One floor, torch, Stalker, wall/doorway, HUD row; explicit non-goals enforced. | Revert only the M6P1 branch before production enablement; legacy branch/map remains intact. |

## 32. User decisions that genuinely change scope

Only one decision remains blocking for the recommended next task:

### Authorize one new binary Integration Map?

- **Recommended:** Yes—authorize creation of `/Game/Maps/L_VisionIntegration.umap` through Unreal Editor/official APIs, with no other new Blueprint asset and no `L_Prototype` edit.
- **Reason:** M6 explicitly requires a dedicated map; the formal D3D12 product View, player route, wall, Stalker, HUD, and navigation cannot be closed honestly in a CPU-only transient test or the DARKWELL-free plugin Lab.
- **If no:** Candidate B is blocked. Candidate A could be performed as a separate observe-only code milestone, but it would not meet this audit's player-value/formal-View criterion.
- **If broader asset work is authorized:** Do not consume that authorization in M6P1; additional Blueprint/art/production-map changes remain non-goals unless the user explicitly expands the milestone.

The audit resolves the other proposed choices from repository evidence: use the dedicated map rather than `L_Prototype`; use world-start authority switching rather than parallel control or direct replacement; use torch rather than lantern; use base Stalker rather than Warden; exclude SaveGame; require the six-step user PIE gate above. The user only needs to override these if product intent has changed.

## 33. Complete next-implementation prompt skeleton

Replace `<AUDIT_FINAL_SHA>` with the pushed branch-tip SHA reported by this audit's final handoff.

```text
Continue DARKWELL / SightWeave from the completed documentation audit.

Implement only:

M6 — DARKWELL adapter integration without authority overlap
M6P1 (audit-recommended subdivision) — DARKWELL Adapter Minimal Visibility Vertical Slice

Repository: D:\UE_pro\Darkwell
Engine: D:\UE_5.8
Audit branch: codex/post-m4-sightweave-darkwell-integration-audit
Frozen audit baseline: <AUDIT_FINAL_SHA>
Implementation branch: codex/m6p1-sightweave-darkwell-adapter-vertical-slice
Frozen M4 implementation baseline: 94be5835212a7f10491cd359676fcb5ee06dc08b

User authorization for this task:
- Create exactly one new binary map through Unreal Editor/official asset APIs:
  /Game/Maps/L_VisionIntegration
- Do not modify /Game/Maps/L_Prototype.
- Do not add Blueprint assets unless a concrete blocker is reported and separately approved.

Start by fully reading AGENTS.md and
Docs/SIGHTWEAVE_POST_M4_DARKWELL_INTEGRATION_AUDIT.md.
Read the frozen M4P3 handoff/contract and the M6 section of
Docs/VISION_SYSTEM_MIGRATION_PLAN.md. Do not reopen M1-M4.

Baseline procedure:
1. git fetch origin
2. switch to codex/post-m4-sightweave-darkwell-integration-audit
3. git pull --ff-only
4. confirm local/upstream/remote are exactly <AUDIT_FINAL_SHA>
5. confirm a clean worktree and clean git lfs status
6. preserve any allowed Darkwell.uproject EngineAssociation-only difference without
   staging, restoring, formatting, or overwriting it
7. inspect whether the implementation branch exists; create it from the audit SHA only
   if it does not; never force-push, rebase, reset, clean, or stash unknown work

One-sentence goal:
On /Game/Maps/L_VisionIntegration, use one project-owned Adapter and a single
SightWeave authority to deliver the body circle, illumination-gated player cone,
semantic torch legal light, one NeverRemember Stalker, matching threat HUD, and
formal D3D12 black/gray/live View while keeping legacy rollback intact.

Required scope:
- add SightWeaveRuntime as an unconditional private Darkwell dependency and
  SightWeaveRender as a non-Server private dependency; guard presentation with UE_SERVER;
- add Legacy, SightWeaveObserveOnly, and SightWeave world-start modes;
- add a UDarkwellSightWeaveWorldSubsystem or equivalently small project-owned world
  lifecycle/facade with stable owner/floor IDs and subject authority;
- register one 120 cm bypass body-circle source;
- register one 2200 cm directional source requiring legal illumination and preserving
  the existing 52-to-35 degree aim narrowing;
- register one explicit torch radial legal-light source from loadout semantic state and
  durability, never by sampling rendered light intensity/Scene Color;
- register one explicit static wall/doorway occluder and one immutable fixture surface;
- configure frozen Coarse 25 cm HardMemory and Width=50 presentation;
- route one base Stalker through NeverRemember and use the same hard-live result for its
  approved world presentation and threat HUD row;
- in SightWeave mode, disable legacy visibility tick/write/subject callbacks and legacy
  fog blendable before selecting the SightWeave presentation scope;
- keep Legacy unchanged in L_Prototype and make observe-only side-effect free;
- create the integration map through Unreal asset APIs with a native C++ GameMode/fixture;
- add focused automation, formal D3D12/SM6 capture, Agent image inspection, and request
  the exact bounded user PIE gate from the audit.

Responsibility boundary:
- Character owns input, movement, facing, aim, and camera.
- Loadout owns torch charge/heat/equipment/gameplay state.
- Enemy/controller own AI Perception, navigation, combat, stun, collision, and damage.
- DARKWELL Adapter owns translation, handles, query facade, subject presentation, and
  visibility-derived HUD eligibility.
- SightWeaveRuntime owns CPU authority/memory/subject policy.
- SightWeaveRender owns derived formal View only.
- HUD and gameplay actors must not depend on plugin render internals.

Frozen non-goals:
- no SightWeave public API, shader, Lab, or plugin feature expansion;
- no M4P4;
- no L_Prototype or default-map change;
- no SaveGame version/blob/slot work and no v6 fog conversion;
- no damage-source reveal;
- no lantern, Warden, camera/radar/remote source, pickups, facilities, general regions,
  multiple floors, general static conversion, or dynamic-door production integration;
- no legacy deletion or runtime hot switching;
- no SceneCapture, GPU/Material/proxy gameplay authority;
- no gameplay/resource/AI tuning;
- no BuildPlugin unless plugin source/config/descriptor actually changes after explicit
  scope approval.

Implementation checkpoints:
1. freeze M6P1 contract;
2. adapter/module/lifecycle foundation and observe-only tests;
3. body/cone/torch/occluder CPU authority;
4. single View authority and legacy isolation;
5. Stalker plus threat-HUD consumer;
6. new integration map through Unreal APIs;
7. focused/full validation and documentation closure.

Verification:
- run Scripts/BuildEditor.ps1 after C++ or Build.cs changes;
- compile Game Development and Game Shipping for Runtime/Render isolation; do not claim a
  dedicated-server runtime result unless a separately authorized Server target exists;
- run exact focused mode/lifecycle/source/light/memory/subject/HUD tests under NullRHI;
- run required affected SightWeave and full DARKWELL regressions with exact counts;
- run a real GameViewport/player-camera D3D12/SM6 test on L_VisionIntegration;
- never use SceneCapture as evidence;
- capture and open the audit-required seven visual states, record pixel/ROI/nonfinite and
  direct Agent observations;
- scan severe logs and retain failed attempts;
- inspect Git diff and prove only the approved new map is a binary asset change;
- do not claim COMPLETED until the user passes the six-step manual PIE gate.

Git discipline:
- use apply_patch for source/document edits;
- use Unreal Editor/official APIs for .uasset/.umap operations;
- never modify, stage, restore, or format Darkwell.uproject;
- never modify L_Prototype;
- commit/push only reliable non-empty checkpoints normally;
- no merge, rebase, force-push, reset, clean, stash, or generated-output commit;
- finish with status, diff-check, local/upstream/remote equality, LFS status/fsck, and
  git fsck --no-reflogs.

If the required new map is not authorized, an unknown worktree change appears, single
authority cannot be established without forbidden production changes, or a plugin public
contract defect is discovered, stop and report BLOCKED. Do not expand the milestone.

After automated and Agent gates, ask the user to perform only the exact PIE route in the
audit. Final status is COMPLETED only after that pass; otherwise report PARTIAL with the
remaining gate. Stop after M6P1 and do not begin SaveGame, damage reveal, wider enemies,
regions, L_Prototype acceptance, or legacy deletion.
```

---

The recommended delivery sequence after this bounded slice remains: later M6 consumers and providers -> later M6 M4P3 blob embedding -> M7 production-content acceptance -> separately approved M8 legacy deletion. This audit authorizes none of those later steps.

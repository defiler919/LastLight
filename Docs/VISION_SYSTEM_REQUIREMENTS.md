# Independent vision system requirements

Status: design proposal; implementation is not authorized until human architecture approval.

Working product name in this document: **WorldVision**. This is a neutral placeholder, not an approved Fab listing name or C++ namespace commitment.

## Product intent

WorldVision is a reusable Unreal Engine plugin for authoritative 2.5D field-of-view, fog-of-war memory, object visibility policy, regional memory control, persistence, debugging, and editor authoring. Its generic modules must not reference DARKWELL classes, gameplay tags, mission rules, enemies, items, Niagara systems, or audio.

DARKWELL integrates through project-owned adapters for the player, security cameras, remote observation/lighting items, monster black fog, doors, containers, machines, pickups, HUD, Niagara, and audio.

## Terminology

| Term | Meaning |
| --- | --- |
| Vision source / observer | Explicitly registered component allowed to create current vision and, unless blocked, memory |
| Occluder | Explicit 2.5D line/polygon geometry with floor and height metadata that blocks a source |
| Live vision | Stable, authoritative area currently visible to at least one active legal source |
| Memory | Persistent player knowledge produced only by prior legal live vision |
| Subject | Actor/component whose live and remembered presentation is governed by policy |
| Modifier | Region that clears, blocks writing, suppresses memory presentation, or optionally suppresses live vision |
| Floor | A 2.5D spatial layer identified by a stable ID plus `ZMin`/`ZMax` |
| Presentation mask | World-space raster used only to render live/memory/unknown states; it is not gameplay authority |

## Required knowledge states

`REQ-STATE-001` Every queryable point/subject for one knowledge owner has exactly one effective state:

1. **Visible**: normal current presentation and current subject state are allowed.
2. **Remembered**: a gray, non-authoritative last-seen presentation is allowed.
3. **Unknown**: completely black; no geometry, subject, prompt, or threat information is presented.

`REQ-STATE-002` Live vision wins over ordinary remembered/unknown state unless an active `SuppressLiveVision` modifier applies.

`REQ-STATE-003` Memory may be created only by a legal source. Screen color, UE light contribution, camera frustum presence, proximity, sound, AI perception, or a softened post-process edge must never write memory by themselves.

`REQ-STATE-004` Gameplay visibility, memory-write eligibility, and presentation softness must be independently inspectable results.

## Observer requirements

`REQ-SOURCE-001` Sources must be explicit registered components with stable runtime handles. The plugin must not treat every UE light or pawn as a source.

`REQ-SOURCE-002` Version 1 must support:

- a directional cone with range and configurable near awareness region;
- a camera-style directional cone;
- a point/radial source where the game explicitly requests it;
- remote observation/illumination sources located away from the player;
- multiple simultaneous sources whose valid live areas are unioned for one knowledge owner;
- runtime activation, deactivation, transform/profile change, floor change, and owner reassignment.

`REQ-SOURCE-003` A remote illumination/observation item is a real source. If its exact visibility query succeeds, it may show enemies and write memory. It is not inferred from an ordinary `ULightComponent`.

`REQ-SOURCE-004` The core plugin must support an optional source-filter callback or channel/tag profile so a game adapter can model rules such as “directional sight intersected with legal illumination” without hard-coding DARKWELL light behavior.

`REQ-SOURCE-005` Source computation is world-space and independent of whether its area is inside the player's current screen.

`REQ-SOURCE-006` Source edge softness is presentation-only. A query point is inside or outside the authoritative polygon using stable geometric rules and documented boundary epsilon.

## Occlusion requirements

`REQ-OCC-001` The primary v1 occlusion representation is explicit 2.5D geometry: line segments/polygon edges, floor ID, `ZMin`, `ZMax`, enabled state, and static/dynamic classification.

`REQ-OCC-002` Static geometry must be held in a spatial index and reused until authoring or streaming changes it.

`REQ-OCC-003` Doors and other supported dynamic occluders must update their registered segments without rescanning the entire world. A door opening must alter vision deterministically and be visible in debug tools.

`REQ-OCC-004` Pawns and ordinary moving subjects do not occlude by default. A game may opt an actor into dynamic occlusion explicitly.

`REQ-OCC-005` The solution must preserve straight-wall boundaries in world space. Movement, turning, viewport resolution, and post-process interpolation must not create obvious moving waves along an unchanged straight wall.

`REQ-OCC-006` Doorways, corners, thin walls, adjacent collinear segments, source-on-boundary cases, and near-parallel rays require deterministic tolerances and automation coverage.

`REQ-OCC-007` UE `Visibility` collision may be imported or validated by editor tools, but it must not silently become the runtime source of truth for the general plugin.

## Live-vision and gameplay-query requirements

`REQ-LIVE-001` Authoritative live areas are computed from source shape, explicit occluders, source filters, floor/height rules, and live-suppression modifiers.

`REQ-LIVE-002` Subject/HUD/gameplay visibility must query authoritative polygons or an equivalent exact geometric result. It must never read post-process color, filtered Render Targets, antialiased pixels, SceneDepth outlines, or memory masks.

`REQ-LIVE-003` Point, sphere/bounds, and multi-sample subject queries are required. Subject policy chooses “anchor visible”, “any sample visible”, “all required samples visible”, or adapter-provided samples.

`REQ-LIVE-004` Query output must include at least: effective state, source handle(s) that contributed, floor, whether occluded, whether blocked by a modifier, and a frame/revision identifier for debugging.

`REQ-LIVE-005` Queries and render-mask generation consume the same immutable visibility snapshot for a frame/update. Presentation may lag by a bounded configured amount but may not feed back into authority.

## Memory requirements

`REQ-MEM-001` Live vision writes permanent memory only where memory writes are allowed.

`REQ-MEM-002` Fixed environment—including floor, walls, and explicitly fixed decoration—must preserve readable surface/material and spatial detail in gray memory. A SceneDepth/WorldNormal outline alone is insufficient.

`REQ-MEM-003` Memory presentation must not reveal current moving enemies, current dynamic-object state, current lights/shadows, hidden VFX, or other live changes that were not legally reacquired.

`REQ-MEM-004` The memory store is world-space, floor-aware, sparse/tiled or otherwise bounded, deterministic to serialize, and independent of current viewport resolution.

`REQ-MEM-005` Memory writes use the hard authoritative area. Visual feather/blur cannot expand memory, bridge an occluder, or make a subject remembered.

`REQ-MEM-006` Re-exploration after memory clearing must work with no special reset.

`REQ-MEM-007` Capacity exhaustion may not silently stop writes. The plugin must expose budgets/counters and a defined partition, eviction, or hard-error policy.

## Regional modifier requirements

Every modifier supports circle, oriented box, authored room volume, and 2D polygon footprints plus floor/`ZMin`/`ZMax`. Modifiers may be placed in the editor or created/updated through C++ and Blueprint.

`REQ-REGION-001` `ClearMemory(region, duration)` permanently removes gray memory in the target region, removes/invalidates remembered subject snapshots in the region, and permits later legal re-exploration. `duration == 0` is immediate; positive duration may animate presentation, but cleared knowledge must not remain queryable as remembered.

`REQ-REGION-002` `BlockMemoryWrites` prevents new memory writes while active. Live vision remains normal; when live vision leaves, the effective presentation is immediately black. Pre-existing underlying memory is preserved by default rather than cleared, so removing the blocker may restore that older memory.

`REQ-REGION-003` `BlockMemoryWrites` therefore combines a write gate with memory-presentation suppression while active. It does not imply `SuppressLiveVision` and it does not mutate/clear the underlying memory. An option may explicitly discard prior memory by composing a separate `ClearMemory` call.

`REQ-REGION-004` `SuppressMemoryPresentation` makes an area appear black while preserving underlying memory and subject snapshots. Removing the modifier restores them.

`REQ-REGION-005` `SuppressLiveVision` is separately configurable. Ordinary memory-write blockers do not suppress current vision. Special black fog/energy fields may suppress both live presentation and authoritative subject visibility.

`REQ-REGION-006` Overlap resolution is deterministic: clear is a mutation; write blocking is the union of active blockers; effective memory-presentation suppression is the union of active blockers and active presentation suppressors; live suppression is the union of active live suppressors. No modifier softness changes gameplay boundaries.

`REQ-REGION-007` Modifier state changes increment revisions and invalidate only affected spatial tiles/sources/subjects where practical.

## Subject and last-seen requirements

`REQ-SUBJECT-001` The generic subject component supports at least these policies:

- `NeverRemember`: enemies, active monsters, and other moving threats; visible only in legal current vision;
- `StaticEnvironment`: immutable geometry/details may be shown wherever remembered;
- `LastSeenSnapshot`: current render state is shown while visible; a render-only cached presentation is shown in memory;
- `VisibleOnly`: pickup or transient object shown only while currently visible, with no remembered proxy;
- `Custom`: game adapter supplies samples, snapshot data, and presentation callbacks.

`REQ-SUBJECT-002` Enemies and active monsters must never remain in gray memory. The same current-vision query gates world rendering, interaction prompts, target UI, and threat HUD.

`REQ-SUBJECT-003` Doors, containers, machines, and uncollected items may use last-seen state. The plugin provides generic snapshot identity/version/timestamp and render-proxy lifecycle; game-specific semantic state remains in the adapter.

`REQ-SUBJECT-004` Memory proxies are render-only. They do not own collision, AI, interaction, audio, Niagara simulation, gameplay state, replication, or save-slot policy.

`REQ-SUBJECT-005` Reacquisition hides/removes the memory proxy and presents the current authoritative subject immediately. Losing vision captures at most one stable last-seen update per visibility transition unless the adapter requests otherwise.

`REQ-SUBJECT-006` Clearing memory or unloading a floor removes affected remembered proxies. Suppression may hide proxies without deleting their snapshots.

## Multi-floor and height requirements

`REQ-FLOOR-001` Version 1 is 2.5D, not a 3D voxel system.

`REQ-FLOOR-002` Every source, occluder, subject, memory tile, and modifier resolves to a stable `FloorId` (or equivalent handle) with `ZMin`/`ZMax`.

`REQ-FLOOR-003` A source computes on its active floor and against occluders whose height range intersects the configured observer/target visibility band.

`REQ-FLOOR-004` Floor changes, streaming, stairs, elevators, and explicitly authored inter-floor portals must invalidate/reassign data predictably. V1 may restrict normal presentation to one active floor per local view, but must not merge all floors into one XY memory plane.

`REQ-FLOOR-005` Simultaneously visible stacked floors/atria are an explicit advanced case and must be either supported by authored portals/layers or rejected by validation; silent leakage is unacceptable.

## Rendering requirements

`REQ-RENDER-001` Hard live and memory masks are rasterized in stable world-space coordinates, preferably into floor-aware tiled R8 textures/Render Targets. Camera-relative half-resolution texture generation is not the world authority.

`REQ-RENDER-002` Live and memory masks are separate resources/channels. Modifier masks are separate or independently addressable.

`REQ-RENDER-003` Limited edge antialiasing/feather is allowed after hard rasterization. It may darken/soften presentation but cannot change hard queries or memory writes.

`REQ-RENDER-004` Gray environment presentation retains base material texture/detail and stable spatial cues while excluding live illumination changes. The first implementation must document which material domains/features are supported and provide a fallback for unsupported translucent/unlit/VFX surfaces.

`REQ-RENDER-005` Unknown and effectively suppressed areas output pure black before UI. UI itself remains legible and must not reveal hidden subject data.

`REQ-RENDER-006` Rendering supports DX12/SM6 as DARKWELL's primary configuration and must document any fallback behavior. Hardware ray tracing is not required.

## Save/persistence requirements

`REQ-SAVE-001` The plugin exposes a plain versioned snapshot structure and capture/restore API. It does not own DARKWELL's `USaveGame`, slot names, autosave timing, missions, or level-loading flow.

`REQ-SAVE-002` Snapshot data includes plugin schema version, floor identity/bounds metadata, compressed memory tiles, modifier mutations that are defined as persistent, and last-seen subject records keyed by adapter-provided persistent IDs.

`REQ-SAVE-003` Live vision, active source polygons, transient fades, GPU resources, and derived caches are recomputed after load and are not serialized as authority.

`REQ-SAVE-004` Snapshot ordering and serialization are deterministic. Corrupt, incompatible, oversized, duplicate-ID, missing-floor, and future-version data fail with explicit diagnostics and a documented fallback.

`REQ-SAVE-005` Plugin schema migrations are independent of the host game's save version. The host adapter decides how a plugin snapshot is embedded and when old DARKWELL v6 grid data is migrated.

## Public API requirements

`REQ-API-001` Source, occluder, subject, floor, and modifier lifecycle operations are available in C++ and Blueprint where runtime authoring is safe.

`REQ-API-002` Query APIs are callable without accessing renderer internals and support one-shot and batched forms.

`REQ-API-003` Memory clear/block/suppress and live-suppress operations return handles/results and explicit errors; no string-based actor lookup is required.

`REQ-API-004` Events include source registration/state change, subject effective-state transition, memory changed/cleared, modifier changed, floor loaded/unloaded, and snapshot restored. Events are revisioned to avoid recursive duplicate work.

`REQ-API-005` APIs use generic plugin types and neutral names. Host-specific tags/classes are carried as opaque IDs, user data, or adapter callbacks.

## Editor and debugging requirements

`REQ-EDITOR-001` An Editor module provides occluder/polygon authoring, floor assignment, modifier visualization, validation, and optional conversion/bake assistance from selected meshes/collision.

`REQ-EDITOR-002` Editor tools never modify source assets silently. Generated/baked data has explicit ownership, rebuild commands, undo support, and stale-data diagnostics.

`REQ-DEBUG-001` Runtime debug views display source shapes, per-source polygons, union live area, occluder segments/endpoints/heights, floor IDs, hard vs feathered masks, memory tiles, modifier effects, subject samples/policy/result, and contributing source/rejection reason.

`REQ-DEBUG-002` Runtime stats expose source/segment/polygon/subject/tile counts, solve time, query time, raster/composite GPU time, dirty tiles, upload/readback bytes, memory allocation, and save size.

## Automation and acceptance requirements

`REQ-TEST-001` Pure C++ geometry tests cover cone clipping, visibility polygons, segment endpoints, collinear/duplicate edges, tangents, door openings, height bands, floor isolation, and deterministic epsilon behavior.

`REQ-TEST-002` Runtime tests cover registration, multiple-source union, subject policies, modifier overlaps, clear/re-explore, block-write behavior, suppression restore, save round-trip/migration, streaming/floor lifecycle, and authority switching.

`REQ-TEST-003` Rendering tests compare hard masks and selected visual reference images for straight walls, diagonal walls, corners, doorway motion, resolution changes, and memory detail. Image differences are presentation tests only and never define gameplay truth.

`REQ-TEST-004` Performance tests use declared source/occluder/subject/floor workloads and record hardware, build type, RHI, resolution, and frame statistics.

`REQ-ACCEPT-001` Before DARKWELL integration, a standalone plugin test map must pass straight wall, wall corner, room, doorway, dynamic door, different heights, multiple observers, all modifier types, subject-memory policies, save/restore, and debug inspection.

`REQ-ACCEPT-002` A static straight wall must remain visually stable during a repeatable lateral-motion/rotation camera path at supported resolutions. Acceptance thresholds are set after a capture harness exists; subjective “looks better” is insufficient.

`REQ-ACCEPT-003` No enemy can be visible or listed by HUD outside hard current vision in automated/runtime checks, regardless of edge feather.

## Provisional performance budgets

These are design targets to validate on an agreed minimum-spec Windows machine, not claims about the current workstation:

- no steady-state per-frame heap allocation in the visibility solve/query hot paths;
- game-thread registration/dispatch below 0.25 ms at the reference workload;
- worker visibility solves below 1.0 ms median and 2.0 ms 99th percentile for up to 8 active sources and 4,096 relevant segments total;
- batched visibility updates for 512 subjects below 0.25 ms median;
- mask rasterization plus final composite below 1.0 ms GPU at 1920x1080 and below 1.5 ms at 2560x1440;
- dirty memory-mask update below 0.25 ms amortized CPU and below 0.25 ms GPU at the reference workload;
- runtime plugin memory below 64 MiB for the reference map/floor set, excluding host subject assets;
- compressed plugin snapshot below 8 MiB for the reference campaign partition.

All counts and thresholds are provisional until the user supplies minimum hardware, map extents, expected simultaneous sources, floor count, and save-size target.

## Explicit non-goals for version 1

- full 3D voxel visibility or arbitrary volumetric fog simulation;
- using hardware ray tracing as gameplay truth;
- multiplayer/replication infrastructure;
- AI perception, stealth/noise rules, lighting renderer replacement, Niagara, audio, quest, inventory, or mission logic;
- automatic semantic snapshots of arbitrary game actors with no adapter contract;
- copying DARKWELL source names or game-specific behavior into plugin core;
- third-party runtime libraries or external fog-of-war plugins;
- deleting or modifying the existing DARKWELL fog system during the design/prototype phase.

## Requirement decisions still requiring approval

The unresolved product/architecture choices are listed with recommendations in `VISION_SYSTEM_ARCHITECTURE.md`. They include product name, minimum hardware/workload, illumination semantics, first-floor/atrium scope, remembered material treatment, automatic versus adapter-provided object snapshots, modifier persistence, and the initial supported material domains.

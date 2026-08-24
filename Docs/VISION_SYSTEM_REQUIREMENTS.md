# Independent vision system requirements

Status: design requirements revised with the latest human product decisions; this documentation-only revision does not authorize implementation.

Temporary internal code name in this document: **WorldVision**. It is not an approved Fab listing name, plugin directory, module prefix, or public C++ namespace commitment.

## Product intent

WorldVision is a reusable Unreal Engine system for authoritative 2.5D field-of-view, legal illumination, fog-of-war memory, object visibility policy, regional memory control, persistence, debugging, and editor authoring. Its generic modules must not reference DARKWELL classes, gameplay tags, mission rules, enemies, items, Niagara systems, or audio.

DARKWELL integrates through project-owned adapters for the player, security cameras, remote observation/lighting items, monster black fog, doors, containers, machines, pickups, HUD, Niagara, and audio.

## Terminology

| Term | Meaning |
| --- | --- |
| Vision source / observer | Explicitly registered component that creates a hard vision polygon; its illumination policy declares whether legal illumination is required or bypassed |
| Legal illumination source | Explicitly registered component that creates a hard illumination polygon; it is independent of Unreal's rendered light contribution |
| Occluder | Explicit 2.5D line/polygon geometry with floor and height metadata that blocks a source |
| Live vision | Stable, authoritative area produced by unioning every gated vision source's source-compatible hard coverage with permitted illumination-bypass vision, then applying live suppression |
| Memory | Persistent player knowledge produced only by prior legal live vision |
| Subject | Actor/component whose live and remembered presentation is governed by policy |
| Subject Reveal Override | Temporary subject-only presentation permission, such as a damage-source reveal applied to an attacker after the Knowledge Owner receives a qualifying attack/damage event; it is separate from the knowledge state and never writes memory |
| Modifier | Region that clears, blocks writing, suppresses memory presentation, or optionally suppresses live vision |
| Floor | A 2.5D spatial layer identified by a stable ID plus `ZMin`/`ZMax` |
| Presentation mask | World-space raster of vision, illumination, effective live, or memory coverage used only for rendering; it is not gameplay authority |

## Required knowledge states

`REQ-STATE-001` Every queryable point/subject for one knowledge owner has exactly one effective state:

1. **Visible**: normal current presentation and current subject state are allowed.
2. **Remembered**: a gray, non-authoritative last-seen presentation is allowed.
3. **Unknown**: completely black; no geometry, subject, prompt, or threat information is presented.

These are knowledge states for the ordinary vision/memory pipeline. A Subject Reveal Override is an orthogonal, subject-only presentation channel and is never a fourth knowledge state.

`REQ-STATE-002` Live vision wins over ordinary remembered/unknown state unless an active `SuppressLiveVision` modifier applies.

`REQ-STATE-003` Memory may be created only by a legal source. Screen color, UE light contribution, camera frustum presence, proximity, sound, AI perception, or a softened post-process edge must never write memory by themselves.

`REQ-STATE-004` Gameplay visibility, memory-write eligibility, and presentation softness must be independently inspectable results.

`REQ-STATE-005` A `Subject Reveal Override` never changes the subject's effective `Visible`/`Remembered`/`Unknown` knowledge state. It is returned and debugged through a separate result channel and cannot grant normal visibility, interaction, threat-HUD eligibility, or memory writes.

## Observer requirements

`REQ-SOURCE-001` Sources must be explicit registered components with stable runtime handles. The plugin must not treat every UE light or pawn as a source.

`REQ-SOURCE-002` Version 1 must support:

- a directional cone with range and configurable near awareness region;
- a camera-style directional cone;
- a point/radial source where the game explicitly requests it;
- remote observation sources located away from the player;
- multiple simultaneous sources whose valid live areas are unioned for one knowledge owner;
- runtime activation, deactivation, transform/profile change, floor change, and owner reassignment.

`REQ-SOURCE-003` Every vision source explicitly declares `RequiresLegalIllumination` or `BypassLegalIllumination`. A generic filter may further restrict a source but cannot stand in for the first-class illumination system.

`REQ-SOURCE-004` DARKWELL always has a permanent, player-attached circular vision source with `BypassLegalIllumination`. It remains subject to explicit occlusion, floor/height rules, and `SuppressLiveVision`; darkness or loss of a rendered light cannot disable it.

`REQ-SOURCE-005` Source computation is world-space and independent of whether its area is inside the player's current screen.

`REQ-SOURCE-006` Source edge softness is presentation-only. A query point is inside or outside the authoritative polygon using stable geometric rules and documented boundary epsilon.

`REQ-SOURCE-007` Remote vision and illumination sources are activated and deactivated only by the DARKWELL Adapter. The core unions only sources whose explicit registered state is active; proximity, rendering, or component existence cannot activate them implicitly.

## Legal illumination requirements

`REQ-LIGHT-001` Legal illumination is an independent first-class system with an explicit registered illumination-source component, stable runtime handle, floor/height metadata, activation state, profile, and dirty revision.

`REQ-LIGHT-002` Each active legal illumination source produces one or more hard world-space illumination polygons against explicit occluders. Ordinary `ULightComponent` presence, brightness, attenuation pixels, shadows, emissive materials, exposure, Scene Color, or GBuffer luminance are never legal-illumination authority.

`REQ-LIGHT-003` The system provides exact point, bounds/multi-sample, and batched hard illumination queries. Results identify contributing illumination-source handles, floor, occlusion/rejection reason, and the immutable revision consumed.

`REQ-LIGHT-004` Compatibility is preserved per illumination-gated vision source. For every gated source `s`, `GatedCoverage(s) = VisionPolygon(s) intersect Union(IlluminationPolygon(l) for every legal illumination source l compatible with s)`. Authoritative effective live coverage is the union of every `GatedCoverage(s)` plus the union of every illumination-bypass vision polygon. `SuppressLiveVision` is applied only after those unions. A global union of all gated vision intersected with a global union of all illumination is forbidden because it loses source-to-illumination compatibility.

`REQ-LIGHT-005` An illumination polygon alone never reveals a point or subject and never writes memory. Only effective hard live coverage resulting from the vision/illumination rule may do so.

`REQ-LIGHT-006` CPU hard queries and GPU presentation consume the same registered compatibility configurations and immutable vision/illumination revision. Within the same Knowledge Owner and floor, the GPU may group gated sources by a full compatibility configuration `p`, but must compute `Union over p (VisionMask[p] intersect CompatibleIlluminationMask[p])`, then union `BypassVisionMask`. Here `p` represents the complete accepted illumination set or an exactly equivalent configuration, not merely one channel. A filtered lighting buffer or one unqualified global vision/illumination intersection cannot replace this data flow.

## Occlusion requirements

`REQ-OCC-001` The primary v1 occlusion representation is explicit 2.5D geometry: line segments/polygon edges, floor ID, `ZMin`, `ZMax`, enabled state, and static/dynamic classification.

`REQ-OCC-002` Static geometry must be held in a spatial index and reused until authoring or streaming changes it.

`REQ-OCC-003` Doors and other supported dynamic occluders must update their registered segments without rescanning the entire world. A door opening must alter vision deterministically and be visible in debug tools.

`REQ-OCC-004` Pawns and ordinary moving subjects do not occlude by default. A game may opt an actor into dynamic occlusion explicitly.

`REQ-OCC-005` The solution must preserve straight-wall boundaries in world space. Movement, turning, viewport resolution, and post-process interpolation must not create obvious moving waves along an unchanged straight wall.

`REQ-OCC-006` Doorways, corners, thin walls, adjacent collinear segments, source-on-boundary cases, and near-parallel rays require deterministic tolerances and automation coverage.

`REQ-OCC-007` UE `Visibility` collision may be imported or validated by editor tools, but it must not silently become the runtime source of truth for the general plugin.

## Live-vision and gameplay-query requirements

`REQ-LIVE-001` Authoritative live areas are computed from vision shape, explicit occluders, floor/height rules, compatible hard legal-illumination polygons when required, any additional hard source filters, illumination-bypass policy, and live-suppression modifiers.

`REQ-LIVE-002` Subject/HUD/gameplay visibility must query authoritative polygons or an equivalent exact geometric result. It must never read post-process color, filtered Render Targets, antialiased pixels, SceneDepth outlines, or memory masks.

`REQ-LIVE-003` Point, sphere/bounds, and multi-sample subject queries are required. Subject policy chooses “anchor visible”, “any sample visible”, “all required samples visible”, or adapter-provided samples.

`REQ-LIVE-004` Query output must include at least: effective knowledge state, contributing vision-source handle(s), contributing legal-illumination handle(s) or the explicit bypass reason, floor, whether occluded, whether blocked by illumination or a modifier, and a frame/revision identifier for debugging. Subject Reveal Overrides are never folded into this result.

`REQ-LIVE-005` Queries and render-mask generation consume the same immutable visibility snapshot, including the same compatibility configuration, registered source data, and revision. Any compatibility change creates a new revision and invalidates the affected CPU query data and GPU mask groups together. Presentation may lag by a bounded configured amount but may not feed back into authority.

## Memory requirements

`REQ-MEM-001` Live vision writes permanent memory only where memory writes are allowed.

`REQ-MEM-002` Fixed environment—including floor, walls, and explicitly fixed decoration—must preserve readable surface/material and spatial detail in gray memory. A SceneDepth/WorldNormal outline alone is insufficient.

`REQ-MEM-003` Memory presentation must not reveal current moving enemies, current dynamic-object state, current lights/shadows, hidden VFX, or other live changes that were not legally reacquired.

`REQ-MEM-004` The memory store is world-space, floor-aware, sparse/tiled or otherwise bounded, deterministic to serialize, and independent of current viewport resolution.

`REQ-MEM-005` Memory writes use the hard authoritative area. Visual feather/blur cannot expand memory, bridge an occluder, or make a subject remembered.

`REQ-MEM-006` Re-exploration after memory clearing must work with no special reset.

`REQ-MEM-007` Capacity exhaustion may not silently stop writes. The plugin must expose budgets/counters and a defined partition, eviction, or hard-error policy.

`REQ-MEM-008` CPU memory precision is a test parameter, not a settled 25 cm default. The implementation spike and acceptance evidence must compare 2.5 cm, 5 cm, 10 cm, and 25 cm using identical paths and report visual fidelity, boundary error, CPU cost, GPU upload cost, runtime memory, and compressed save size before selecting a value.

## Regional modifier requirements

Every modifier supports circle, oriented box, authored room volume, and 2D polygon footprints plus floor/`ZMin`/`ZMax`. Modifiers may be placed in the editor or created/updated through C++ and Blueprint.

`REQ-REGION-001` `ClearMemory(region, duration)` permanently removes gray memory in the target region, removes/invalidates remembered subject snapshots in the region, and permits later legal re-exploration. `duration == 0` is immediate; positive duration may animate presentation, but cleared knowledge must not remain queryable as remembered.

`REQ-REGION-002` `BlockMemoryWrites` prevents new memory writes while active. Live vision remains normal; when live vision leaves, the effective presentation is immediately black. Pre-existing underlying memory is preserved by default rather than cleared, so removing the blocker may restore that older memory.

`REQ-REGION-003` `BlockMemoryWrites` therefore combines a write gate with memory-presentation suppression while active. It does not imply `SuppressLiveVision` and it does not mutate/clear the underlying memory. An option may explicitly discard prior memory by composing a separate `ClearMemory` call.

`REQ-REGION-004` `SuppressMemoryPresentation` makes an area appear black while preserving underlying memory and subject snapshots. Removing the modifier restores them.

`REQ-REGION-005` `SuppressLiveVision` is separately configurable. Ordinary memory-write blockers do not suppress current vision. Special black fog/energy fields may suppress both live presentation and authoritative subject visibility.

`REQ-REGION-006` Overlap resolution is deterministic: clear is a mutation; write blocking is the union of active blockers; effective memory-presentation suppression is the union of active blockers and active presentation suppressors; live suppression is the union of active live suppressors. No modifier softness changes gameplay boundaries.

`REQ-REGION-007` Modifier state changes increment revisions and invalidate only affected spatial tiles/sources/subjects where practical.

`REQ-REGION-008` DARKWELL monster permanent-blackout behavior is composed explicitly: `ClearMemory` permanently removes existing memory/snapshots and `BlockMemoryWrites` prevents reacquisition while the blackout remains active. It is not a new implicit modifier semantic.

## Subject and last-seen requirements

`REQ-SUBJECT-001` The generic subject component supports at least these policies:

- `NeverRemember`: enemies, active monsters, and other moving threats; visible only in legal current vision;
- `StaticEnvironment`: immutable geometry/details may be shown wherever remembered;
- `LastSeenSnapshot`: current render state is shown while visible; a render-only cached presentation is shown in memory;
- `VisibleOnly`: transient object shown only while currently visible, with no remembered proxy;
- `Custom`: game adapter supplies samples, snapshot data, and presentation callbacks.

`REQ-SUBJECT-002` Enemies and active monsters must never remain in gray memory. The same current-vision query gates world rendering, interaction prompts, target UI, and threat HUD.

`REQ-SUBJECT-003` Doors, containers, and machines may use last-seen state. DARKWELL fixed, uncollected items use `LastSeenSnapshot` as the decided policy. The plugin provides generic snapshot identity/version/timestamp and render-proxy lifecycle; game-specific semantic state remains in the Adapter.

`REQ-SUBJECT-004` Memory proxies are render-only. They do not own collision, AI, interaction, audio, Niagara simulation, gameplay state, replication, or save-slot policy.

`REQ-SUBJECT-005` Reacquisition hides/removes the memory proxy and presents the current authoritative subject immediately. Losing vision captures at most one stable last-seen update per visibility transition unless the adapter requests otherwise.

`REQ-SUBJECT-006` Clearing memory or unloading a floor removes affected remembered proxies. Suppression may hide proxies without deleting their snapshots.

`REQ-SUBJECT-007` Damage-source reveal direction is explicit: a Knowledge Owner receives a qualifying attack/damage event, the event identifies an attack source or Instigator, the DARKWELL Adapter resolves that attacker to a Subject, and the Adapter applies a time-bounded `Subject Reveal Override` to the attacker Subject. The generic plugin does not listen to DARKWELL damage events or decide whether zero final damage, armor absorption, blocking, or similar cases qualify. The Knowledge Owner attacking that enemy does not trigger this rule unless project code separately and explicitly applies an override.

`REQ-SUBJECT-008` Reveal overrides have independent handles, Knowledge Owner and Subject identity, reasons, expiry/revocation, and a `RevealSpecification` that explicitly controls reveal primitives plus darkness, ordinary-occlusion, and suppression policy. The approved reveal follows only the specified attacker Subject as it moves, reveals no surrounding environment or nearby subjects, and does not alter actor transform or gameplay state. It cannot set `Visible`, refresh/create `LastSeenSnapshot`, set memory bits, illuminate the environment, or qualify ordinary HUD, interaction, targeting, or threat queries. When it ends, presentation returns to the unchanged underlying knowledge state without a remembered afterimage.

## Multi-floor and height requirements

`REQ-FLOOR-001` Version 1 is 2.5D, not a 3D voxel system.

`REQ-FLOOR-002` Every source, occluder, subject, memory tile, and modifier resolves to a stable `FloorId` (or equivalent handle) with `ZMin`/`ZMax`.

`REQ-FLOOR-003` A vision or legal-illumination source computes on its active floor and against occluders whose height range intersects its configured observer/target or illumination band.

`REQ-FLOOR-004` V1 presents and queries exactly one active floor for the local view at a time. Floor changes, streaming, stairs, and elevators must invalidate/reassign data predictably; inactive-floor memory remains stored separately and is not composited or merged into the active XY memory plane.

`REQ-FLOOR-005` Simultaneously visible stacked floors, atria, balconies, and inter-floor portals are outside v1 and must be rejected or clearly reported by validation; silent leakage is unacceptable.

## Rendering requirements

`REQ-RENDER-001` Hard live and memory masks are rasterized in stable world-space coordinates, preferably into floor-aware tiled R8 textures/Render Targets. Camera-relative half-resolution texture generation is not the world authority.

`REQ-RENDER-002` Hard gated vision, compatible legal illumination, illumination-bypass vision, effective live, memory, and modifier masks are separate or independently addressable resources/channels. For every distinct full compatibility configuration `p` within a Knowledge Owner/floor scope, the renderer derives `VisionMask[p]` and `CompatibleIlluminationMask[p]`; `EffectiveLiveMask = Union over p (VisionMask[p] intersect CompatibleIlluminationMask[p]) union BypassVisionMask`. Bypass vision is never assigned to a compatibility group. Live suppression is applied afterward, and presentation feathering is applied only to that final hard result.

`REQ-RENDER-003` Limited edge antialiasing/feather is allowed after hard rasterization. It may darken/soften presentation but cannot change hard queries or memory writes.

`REQ-RENDER-004` Remembered environment uses a neutral-gray material treatment that retains desaturated base-material texture/detail and stable spatial cues while excluding current or last-captured live illumination. Freezing the last lit color image is not supported. The first implementation must document supported material domains/features and provide a fallback for unsupported translucent/unlit/VFX surfaces.

`REQ-RENDER-005` Unknown and effectively suppressed areas output pure black before UI. UI itself remains legible and must not reveal hidden subject data.

`REQ-RENDER-006` Rendering supports DX12/SM6 as DARKWELL's primary configuration and must document any fallback behavior. Hardware ray tracing is not required.

## Save/persistence requirements

`REQ-SAVE-001` The plugin exposes a plain versioned snapshot structure and capture/restore API. It does not own DARKWELL's `USaveGame`, slot names, autosave timing, missions, or level-loading flow.

`REQ-SAVE-002` Snapshot data includes plugin schema version, floor identity/bounds metadata, compressed memory tiles, modifier mutations that are defined as persistent, and last-seen subject records keyed by adapter-provided persistent IDs.

`REQ-SAVE-003` Live vision, vision/illumination polygons, active source state, Subject Reveal Overrides, transient fades, GPU resources, and derived caches are recomputed or reactivated by the host after load and are not serialized as memory authority.

`REQ-SAVE-004` Snapshot ordering and serialization are deterministic. Corrupt, incompatible, oversized, duplicate-ID, missing-floor, and future-version data fail with explicit diagnostics and a documented fallback.

`REQ-SAVE-005` Plugin schema migrations are independent of the host game's save version. DARKWELL's old v6 fog-memory grid is not migrated into WorldVision; a WorldVision-enabled save begins with fresh WorldVision memory unless it already contains a valid WorldVision snapshot. Legacy v6 fog data remains relevant only to the legacy authority path while that path exists.

## Public API requirements

`REQ-API-001` Vision source, legal-illumination source, occluder, subject, Subject Reveal Override, floor, and modifier lifecycle operations are available in C++ and Blueprint where runtime authoring is safe.

`REQ-API-002` Query APIs are callable without accessing renderer internals and support one-shot and batched forms.

`REQ-API-003` Memory clear/block/suppress and live-suppress operations return handles/results and explicit errors; no string-based actor lookup is required.

`REQ-API-004` Events include source registration/state change, subject effective-state transition, memory changed/cleared, modifier changed, floor loaded/unloaded, and snapshot restored. Events are revisioned to avoid recursive duplicate work.

`REQ-API-005` APIs use generic plugin types and neutral names. Host-specific tags/classes are carried as opaque IDs, user data, or adapter callbacks.

`REQ-API-006` The generic reveal API is semantically equivalent to `ApplySubjectRevealOverride(KnowledgeOwner, Subject, RevealSpecification)`. It accepts an already resolved Knowledge Owner and Subject and does not depend on DARKWELL's damage system. Trigger qualification and attacker/Instigator resolution belong to the host Adapter.

## Editor and debugging requirements

`REQ-EDITOR-001` An Editor module provides occluder/polygon authoring, floor assignment, modifier visualization, validation, and optional conversion/bake assistance from selected meshes/collision.

`REQ-EDITOR-002` Editor tools never modify source assets silently. Generated/baked data has explicit ownership, rebuild commands, undo support, and stale-data diagnostics.

`REQ-DEBUG-001` Runtime debug views display vision and illumination source shapes, their separate polygons, per-source/per-compatibility-profile gated coverage, bypass-vision coverage, effective live area, occluder segments/endpoints/heights, active floor, hard vs feathered masks, memory tiles, modifier effects, subject samples/policy/knowledge result, independent Subject Reveal Overrides, and contributing/rejection reasons.

`REQ-DEBUG-002` Runtime stats expose source/segment/polygon/subject/tile counts, solve time, query time, raster/composite GPU time, dirty tiles, upload/readback bytes, memory allocation, and save size.

## Automation and acceptance requirements

`REQ-TEST-001` Pure C++ geometry tests cover cone clipping, visibility polygons, segment endpoints, collinear/duplicate edges, tangents, door openings, height bands, floor isolation, and deterministic epsilon behavior.

`REQ-TEST-002` Runtime tests cover vision/illumination registration and activation, polygon intersection, hard illumination queries, permanent body-circle bypass, multiple-source union, subject policies, independent no-memory Subject Reveal Overrides, modifier overlaps, monster `ClearMemory` plus `BlockMemoryWrites`, clear/re-explore, suppression restore, save round-trip, single-active-floor lifecycle, and authority switching.

`REQ-TEST-003` Rendering tests compare hard masks and selected visual reference images for straight walls, diagonal walls, corners, doorway motion, resolution changes, and memory detail. Image differences are presentation tests only and never define gameplay truth.

`REQ-TEST-004` Performance tests use declared vision-source/illumination-source/occluder/subject/floor workloads and record hardware, build type, RHI, resolution, frame statistics, and the complete 2.5/5/10/25 cm memory-precision comparison.

`REQ-TEST-005` Damage-source direction tests place Enemy A outside ordinary live vision and have Enemy A cause a qualifying attack/damage event on the player Knowledge Owner. The Adapter must apply a Subject Reveal Override to Enemy A, and only Enemy A's approved reveal primitive follows Enemy A temporarily. The test must also prove that the player shooting Enemy A does not trigger this rule by itself, nearby Enemy B and the surrounding environment remain unrevealed and unilluminated, environment memory is not written, no last-seen proxy is refreshed, and ordinary visibility/HUD/interaction/targeting results remain unchanged.

`REQ-TEST-006` Visible/infrared isolation tests use overlapping vision polygons where Source A accepts only visible illumination, Source B accepts only infrared illumination, and only infrared legal illumination covers the sample. Source A must remain not live, Source B must become live, and the CPU exact query must agree with the GPU hard-mask result.

`REQ-TEST-007` Multi-channel compatibility tests use Source C whose complete compatibility configuration accepts both visible and infrared legal illumination. Either accepted type must satisfy Source C, while an unrelated incompatible illumination type must not. CPU attribution and the matching GPU compatibility group must agree.

`REQ-TEST-008` Bypass tests run the player body-circle vision with no legal illumination. Its coverage must remain live while obeying walls, `FloorId`, height rules, and `SuppressLiveVision`; it must never be assigned to or satisfied through an illumination compatibility group.

`REQ-ACCEPT-001` Before DARKWELL integration, a standalone plugin test map must pass straight wall, wall corner, room, doorway, dynamic door, different heights, multiple vision/illumination sources, visible/infrared compatibility isolation, multi-channel accepted sets, permanent body-circle bypass, all modifier types, subject-memory/reveal policies, save/restore, and debug inspection.

`REQ-ACCEPT-002` A static straight wall must remain visually stable during a repeatable lateral-motion/rotation camera path at supported resolutions. Acceptance thresholds are set after a capture harness exists; subjective “looks better” is insufficient.

`REQ-ACCEPT-003` No enemy can be visible or listed by HUD outside hard current vision in automated/runtime checks, regardless of edge feather.

`REQ-ACCEPT-004` When a Knowledge Owner receives a qualifying attack/damage event, damage-source presentation may reveal only the resolved attacker Subject outside hard current vision and only through an active Subject Reveal Override. The reverse event—the Knowledge Owner attacking that Subject—does not activate this rule by itself. During and after the override, the attacker's knowledge query remains unchanged and no environment memory bit or last-seen record is created or refreshed.

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
- more than one active/presented floor per local view;
- AI perception, stealth/noise rules, lighting renderer replacement, Niagara, audio, quest, inventory, or mission logic;
- automatic semantic snapshots of arbitrary game actors with no adapter contract;
- copying DARKWELL source names or game-specific behavior into plugin core;
- third-party runtime libraries or external fog-of-war plugins;
- deleting or modifying the existing DARKWELL fog system during the design/prototype phase.

## Recorded human product decisions

- `WorldVision` is a temporary internal code name only; the public plugin/API name remains unset until source creation is separately approved.
- Legal illumination is an independent first-class system with explicit illumination-source components, hard illumination polygons/queries, and per-source or equivalently profile-grouped CPU/GPU compatibility intersections; incompatible illumination channels never share an unqualified global intersection.
- DARKWELL has a permanent player-attached circular vision source that bypasses illumination.
- Damage-source reveal is applied to the attacker Subject after the Knowledge Owner receives a qualifying attack/damage event; it is a non-memory-writing Subject Reveal Override and never a normal `Visible` state.
- Remembered geometry uses neutral-gray material detail, not a frozen last-lit image.
- DARKWELL fixed, uncollected items use `LastSeenSnapshot`.
- V1 is single-player and presents one active floor at a time.
- Old DARKWELL v6 fog memory is not migrated.
- Remote sources are activated only by the DARKWELL Adapter.
- Monster permanent blackout composes `ClearMemory` with `BlockMemoryWrites`.
- Memory precision remains unselected until the 2.5/5/10/25 cm comparison is complete.

Remaining architecture/acceptance gates are limited to implementation-facing choices such as public package naming, minimum hardware/reference workload, supported material domains, and measured selection of the memory precision. They are tracked in `VISION_SYSTEM_ARCHITECTURE.md`.

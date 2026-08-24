# Independent vision plugin architecture

Status: architecture revised with the latest human product decisions. No plugin source, shader, asset, or game Adapter is implemented or authorized by this documentation milestone.

Temporary internal code name: `WorldVision`. The public plugin/package/module name remains undecided and must be approved before source creation.

## Decision summary

Use a **hybrid architecture with explicit 2.5D geometry as authority**:

1. Explicit vision sources and explicit legal-illumination sources use height-banded occluder segments to produce separate stable vision and illumination polygons in CPU world space.
2. Gameplay subjects, HUD, interaction, legal-illumination checks, and memory-write eligibility use hard CPU queries. Effective live coverage is illumination-gated vision intersected with legal illumination, unioned with explicitly permitted illumination-bypass vision.
3. The render path rasterizes the same immutable revisions into separate world-space vision, illumination, bypass, and effective-live masks, then mirrors a CPU-owned persistent memory tile field into GPU masks.
4. A permanent player-attached circular source takes the illumination-bypass path. It still obeys occlusion, floor/height, and `SuppressLiveVision`.
5. A post-process material applies presentation-only antialiasing and composites live scene, neutral-gray detailed memory, and black unknown.
6. Immutable environment may use stable material attributes because it cannot change. Stateful/moving subjects use policy-controlled live render primitives and render-only last-seen proxies so current state cannot leak into memory.
7. Attacker hit feedback uses an independent, transient Subject Reveal Override presentation lane. It never changes the `Visible`/`Remembered`/`Unknown` knowledge state and never writes memory.

This keeps the stable-wall and deterministic-query strengths of explicit polygons while using the GPU for the workload it handles well: triangle rasterization, the vision-mask/illumination-mask intersection, tiled mask combination, and final full-screen composition.

## Why this is the recommended algorithm

The current repository provides direct engineering evidence against another radial/screen solution:

- a 1024-ray radial distance field still reconstructs unchanged straight walls from moving angular samples;
- half-resolution, 30 Hz screen texture generation makes the edge dependent on projection and cadence;
- a separate 100 cm grid makes object visibility disagree with the continuous mask;
- a screen-bounded memory writer cannot support off-screen remote observers;
- current GBuffer outline reconstruction is not last-seen state.

Explicit segments retain the actual wall line, endpoint, height, floor, and dynamic identity. Analytic ray/segment intersections place visibility-polygon vertices on that unchanged line. World-space rasterization may antialias the resulting edge, but camera movement does not change the underlying polygon.

The plugin should not compute a polygon union merely to answer gameplay queries. Each query tests the small set of contributing source polygons; the GPU obtains the union naturally by drawing all valid polygons with max/additive mask blending. This preserves source attribution and avoids a fragile polygon-boolean hot path.

## Algorithm comparison

| Dimension | Explicit 2.5D geometry + visibility polygon | Scene Capture/depth + GPU raymarch | Recommended hybrid |
| --- | --- | --- | --- |
| Straight-wall stability | Strong: analytic vertices remain on explicit segments; world-space raster is camera independent | Medium: depth texel/raymarch step, capture projection, bias, and temporal sampling can move edges | Strong: explicit polygon is hard authority; GPU only rasterizes/softens it |
| Dynamic doors | Strong: update one registered segment/shape and affected sources | Visually automatic if capture sees the door, but capture timing/bias and gameplay query agreement remain difficult | Strong: door component updates explicit geometry; render follows same revision |
| Multiple floors/heights | Natural 2.5D floor/height bands; simultaneous stacked views require a later extension | Better raw 3D representation, but one/more depth captures per source/floor and ambiguous saved XY memory | Explicit floor authority with exactly one active floor in v1; unsupported stacked views are validated |
| Multiple observers | CPU cost scales with relevant segments per source; polygons rasterize cheaply | Capture and raymarch cost scale sharply per source, face, and resolution | CPU source solves plus one union mask pass; remote sources work off-screen |
| Dynamic occluders | Good for a bounded explicit set; event-driven updates | Good visually for arbitrary renderable meshes | Explicit for gameplay-relevant occluders; optional visual-only depth detail cannot change authority |
| CPU cost | Spatial query + angular sweep/rays; deterministic and worker-friendly | Low visibility CPU but still needs a separate exact-query solution | Bounded CPU authority; no CPU screen-pixel evaluation/upload |
| GPU cost | Low polygon raster/composite; geometry extraction is not GPU | Potentially high captures, depth resources, and raymarch per source | Low polygon raster + tiled masks; optional capture is off by default |
| Memory | Segments, polygons, sparse bit tiles; predictable | Per-source depth/cube maps plus masks; resolution dependent | Sparse CPU memory plus compact GPU tile atlas; no save-time readback |
| Save | Deterministic CPU tiles and subject records | GPU visibility/history needs readback or duplicate CPU state | CPU snapshot is authoritative; GPU resources are derived |
| Fab setup cost | Requires occluder/floor authoring or bake tool | Low initial setup but complex tuning/capture exclusions later | Editor conversion/validation lowers setup cost while keeping explicit results |
| Debug difficulty | High observability: draw segments, rays/sweep events, polygon, query reason | Harder: inspect depth captures, steps, bias, per-source textures, and CPU/GPU mismatch | Explicit debug truth plus mask inspection |
| UE 5.8.1 compatibility | Uses ordinary components, subsystem, collision-independent geometry, render targets/RDG | Scene Capture and material paths exist but feature/material permutations are more fragile | Core stays on stable engine concepts; renderer implementation is isolated and version-tested |
| Gameplay determinism | Strong | Weak unless duplicated by CPU queries, which creates two authorities | Strong; presentation never feeds gameplay |
| Complex arbitrary 3D meshes | Requires simplified authoring | Strong visual fit | Simplified explicit blocker is authoritative; complex visuals are presentation-only |

Conclusion: use the hybrid column. A pure Scene Capture/GPU raymarch system would make the easiest visual prototype, but it does not satisfy deterministic enemy visibility, saveable memory, multi-source attribution, or Fab debugging without recreating a separate CPU authority.

## Plugin and host module boundaries

### Plugin descriptor

Plugin root after separate implementation and naming approval: `Plugins/<ApprovedName>/`. `WorldVision` must not be baked into public paths merely because it is the current code name.

| Module | Type | Owns | Must not own |
| --- | --- | --- | --- |
| `<ApprovedName>Runtime` | Runtime | subsystem, floor/vision-source/illumination-source/occluder/subject/modifier components, geometry solvers, hard queries, Subject Reveal Overrides, CPU memory tiles, save snapshot structs, render-resource bridge, public C++/Blueprint API | DARKWELL actors/tags, missions, save slots, HUD text, Niagara/audio, third-party libraries |
| `<ApprovedName>Editor` | Editor | component visualizers, occluder/polygon editing, mesh/collision conversion assistance, floor tools, validation, debug panels, example-map authoring helpers | runtime gameplay decisions or editor code in the Runtime module |
| `<ApprovedName>Tests` | Developer/Editor-only | pure geometry, runtime functional, rendering comparison, persistence, and performance harnesses | shipping runtime dependencies |

Rendering remains a private area inside the approved Runtime module for v1 so public users depend on one runtime module. If the render path later requires substantial alternate RHI/server packaging, it may split into a dedicated Rendering module without changing public authority/query types.

### Plugin content

After approval, plugin content may contain:

- a post-process material and material functions;
- debug materials/icons;
- authoring data assets/settings;
- standalone example/test maps and simple meshes.

No DARKWELL map, item, enemy, mission, or material belongs in plugin content.

### DARKWELL adapter boundary

Project-owned adapter code, later placed under `Source/Darkwell` (or a separate project plugin only if approved), maps:

- player facing/aim/equipment to illumination-gated vision-source profiles;
- the permanent player-attached circular awareness source to an illumination-bypass vision profile;
- DARKWELL legal lights to explicit illumination-source components/profiles rather than generic source filters;
- security cameras and all remote observation/illumination sources to Adapter-controlled activation;
- monster permanent blackout to an explicit `ClearMemory` mutation plus an active `BlockMemoryWrites` region;
- enemies, pickups, doors, containers, machines, and objective items to generic subject policies;
- designated attacker-subject damage-received events to time-bounded Subject Reveal Overrides;
- current-visibility events to HUD, interaction, Niagara, and audio;
- plugin snapshots into DARKWELL's versioned continuation save.

The Adapter may reference both DARKWELL and the eventual generic system. The generic system must never reference DARKWELL.

## Proposed runtime classes and responsibilities

Names reflect the current code name and are API drafts, not implementation commitments. They must be renamed consistently if the approved public name changes.

| Type | Responsibility |
| --- | --- |
| `UWorldVisionSubsystem : UWorldSubsystem` | registry, revisions, scheduling, immutable frame snapshots, queries, memory mutation, floor/stream lifecycle, debug/stats, snapshot capture/restore |
| `UVisionSourceComponent` | vision transform/profile, knowledge owner, active floor, activation, illumination requirement/bypass policy, dirty tracking, optional non-illumination filter/provider interface |
| `UVisionIlluminationSourceComponent` | explicit legal-illumination transform/profile, knowledge/channel compatibility, active floor, activation, hard polygon, dirty tracking; never inferred from render luminance |
| `UVisionOccluderComponent` | explicit local polygon/segments, height band, floor, static/dynamic mode, transform revision |
| `UVisionSubjectComponent` | visibility samples, knowledge policy, live/reveal primitive sets, last-seen provider/proxy, knowledge-state events, separate reveal-override events, persistent ID |
| `UVisionFloorComponent` or `AVisionFloorVolume` | stable floor ID, XY bounds, `ZMin`/`ZMax`, and the single active-presentation-floor state |
| `AMemoryModifierVolume` | editor/runtime region actor wrapping one or more modifier operations |
| `UVisionBlueprintLibrary` | safe subsystem lookup, point/bounds/batch queries, region builders, debug helpers |
| `UVisionSettings` | project/plugin defaults, tolerances, tile resolution, scheduling/budgets, debug and rendering configuration |
| `IVisionSubjectSnapshotProvider` | host callback for semantic last-seen capture, proxy construction/update, and snapshot serialization |
| `IVisionSourceFilterProvider` | optional host callback/profile for hard rules beyond vision and legal illumination; cannot replace illumination authority or read presentation masks |

Components self-register in `OnRegister`/world begin play and unregister safely. The subsystem stores handles and plain immutable solve data; workers never dereference gameplay UObjects.

## Core data structures

```text
FVisionKnowledgeId
    opaque local knowledge owner (single DARKWELL player in v1)

FVisionFloorId / FVisionFloorDefinition
    stable ID, XY bounds, ZMin, ZMax, transform/origin, revision

FVisionSourceHandle / FVisionSourceState
    generation-safe handle, knowledge ID, floor, origin/eye Z, facing,
    shape/profile, enabled/filter state, illumination requirement/bypass,
    transform/profile revisions

FVisionIlluminationSourceHandle / FVisionIlluminationSourceState
    generation-safe handle, compatible knowledge/channel profile, floor,
    origin/height, shape/profile, enabled state, transform/profile revisions

FVisionOccluderHandle / FVisionSegment2D
    generation-safe handle, endpoints A/B, floor, ZMin/ZMax,
    static/dynamic flags, source component ID, geometry revision

FVisionPolygon
    source handle, floor, ordered world-space vertices, bounds,
    hard-edge epsilon policy, source/occluder revision tuple

FVisionIlluminationPolygon
    illumination-source handle, floor, ordered world-space vertices, bounds,
    compatibility profile, hard-edge epsilon, source/occluder revision tuple

FVisionFrameSnapshot
    monotonically increasing revision, vision and illumination polygons,
    spatial lookup, active modifier query data; immutable during hard queries,
    memory writes, and render submission

FVisionSubjectSampleSet
    anchor/bounds/custom points plus any/all/required-count rule

FVisionQueryResult
    Unknown/Remembered/Visible, hard live boolean, remembered boolean,
    vision source handle(s), legal-illumination handle(s) or bypass reason,
    floor, rejection flags, snapshot revision; contains no reveal override

FVisionIlluminationQueryResult
    hard illuminated boolean, illumination-source handle(s), floor,
    occlusion/rejection flags, snapshot revision

FSubjectRevealOverrideHandle / FSubjectRevealOverrideState
    subject handle, Adapter-supplied reason, start/expiry/revocation,
    presentation policy, revision; never serialized as knowledge

FVisionSubjectPresentationResult
    unchanged FVisionQueryResult plus a separate list of active reveal overrides

FVisionRegionShape
    circle/oriented box/room-volume reference/polygon, floor, Z range

FVisionModifierHandle / FVisionModifierState
    operation flags, region, enabled, presentation fade data, revision

FVisionMemoryTileKey / FVisionMemoryTile
    knowledge ID, floor, integer tile coordinate, packed hard-memory bits,
    dirty rect/revision, optional presentation-transition state

FVisionSubjectSnapshotRecord
    stable subject ID, floor, last-seen transform/time/revision,
    provider type/version, render descriptor or opaque provider payload

FWorldVisionSaveSnapshot
    schema version, settings fingerprint, floor records, compressed tiles,
    persistent clear mutations if required, subject snapshot records
```

Handles include a generation number so stale Blueprint/C++ handles fail explicitly rather than targeting a recycled entry.

## Occluder authoring and spatial index

### Static occluders

The Editor module converts approved wall/room outlines or selected mesh/collision edges into a normalized authoring asset/component representation. Normalization:

- welds endpoints inside a configurable authoring tolerance;
- removes zero-length/duplicate edges;
- preserves deliberate openings;
- records floor and height range;
- optionally merges safe collinear runs while retaining source mapping for debug;
- validates winding, self-intersection, floor bounds, and stale source geometry.

At runtime static segments enter a floor-local uniform grid or BVH. A source queries only cells intersecting its range/bounds.

### Dynamic occluders

Doors and other approved dynamics register a small local segment set. Transform/state changes increment a geometry revision and update affected spatial entries. The subsystem marks only sources whose influence bounds overlap the old/new occluder bounds as dirty.

Dynamic skeletal meshes, mobs, cloth, particles, and arbitrary physics debris do not become blockers automatically. A simplified explicit component is required if their occlusion matters to gameplay.

## Vision- and illumination-polygon solve

### Correctness reference

The correctness implementation is shared by explicit vision and legal-illumination sources. It clips each source shape to its floor and evaluates candidate angles at:

- source cone boundary rays;
- every relevant segment endpoint angle;
- endpoint angle minus/plus a documented small angular epsilon;
- stable tessellation angles/events required by radial or curved source boundaries.

For each candidate ray, analytic ray/segment intersection selects the nearest valid segment whose height band blocks the configured source/target or illumination band. Intersections are sorted by angle and deduplicated by deterministic distance/segment-ID rules. Vertices remain in world coordinates and on the source boundary or the actual occluder segment. Vision polygons and illumination polygons retain distinct source handles and revisions even when they use the same solver implementation.

### Optimized solve

After reference tests pass, an angular sweep maintains active segments sorted by intersection distance and emits the same polygon topology in approximately `O((N + K) log N)` for N relevant segments and K events. The reference solver remains available in tests/debug to compare results.

### Epsilon policy

World weld, ray/segment parallel, endpoint angular, point-in-polygon, and boundary-visible epsilons live in one settings structure and are reported in debug output. Tolerances use world units and scale bounds, not viewport pixels. Stable segment IDs break exact ties.

### Vision/illumination combination and filters

Vision and illumination polygons stay separate. For a knowledge owner, floor, and point `p`, the authoritative hard-live rule is:

```text
Gated(p) = any gated vision polygon v containing p
           for which a legal-illumination polygon compatible with v contains p
Bypass(p) = any explicitly illumination-bypass vision polygon containing p
HardLive(p) = (Gated(p) OR Bypass(p)) AND NOT SuppressLiveVision(p)
```

Before contributing to `Gated` or `Bypass`, every vision polygon must pass:

1. floor/height membership;
2. source polygon containment;
3. its optional additional hard source-filter result.

Legal illumination must independently pass floor/height membership, illumination-polygon containment, and compatibility profile. The permanent player-attached circular source is an explicit `BypassLegalIllumination` source; it is not synthesized from proximity and is not disabled when rendered lights are absent. It still fails on occlusion, inactive floor, or live suppression.

The CPU exposes direct hard illumination queries and the combined hard-live query with both source attributions. A legal-illumination polygon alone has no reveal or memory effect. Additional DARKWELL-specific filters may further narrow a source but cannot implement legal illumination as an opaque callback, sample a blurred light buffer, or sample final scene color.

## Frame/update model

1. Game thread drains vision-source, illumination-source, occluder, floor, modifier, subject, and reveal-override events.
2. Plain solve inputs are copied into generation-safe arrays.
3. Worker tasks update dirty vision/illumination polygons and affected hard-memory tiles.
4. Game thread publishes one immutable `FVisionFrameSnapshot` at a controlled sync point.
5. Batched illumination/live/subject queries consume that snapshot and enqueue knowledge-state transitions; reveal overrides remain a separate presentation result.
6. Game-thread component callbacks apply live/proxy/reveal presentation and notify host Adapters without converting reveal into `Visible`.
7. Render thread rasterizes the same vision and illumination revisions into separate world-space masks, derives effective live coverage, and uploads only dirty memory tiles.
8. Post process samples effective hard masks, applies presentation-only feather, and composites the scene.

Source solve cadence can be capped, but a door/source transform change must carry explicit maximum latency. Gameplay and rendering always identify which snapshot revision they consumed.

## Live, memory, and modifier data flow

```text
registered illumination-gated vision source + floor/height
    + nearby explicit occluder segments
    -> per-source hard visibility polygon
    -> hard gated-vision coverage

registered legal-illumination source + floor/height
    + nearby explicit occluder segments
    -> per-source hard illumination polygon
    -> hard legal-illumination coverage

registered illumination-bypass vision source (including permanent body circle)
    + nearby explicit occluder segments
    -> hard bypass-vision coverage

CPU immutable FVisionFrameSnapshot
    -> ((gated vision INTERSECT legal illumination) UNION bypass vision)
    -> subtract hard SuppressLiveVision regions
    -> effective hard live coverage
       |-> exact illumination/live point/bounds/subject/HUD/gameplay queries
       |-> intersect with !BlockMemoryWrites (CPU hard regions)
           -> set persistent CPU memory bits
           -> invalidate/refresh last-seen subject snapshots
           -> upload dirty memory tiles to GPU

GPU from the same revisions
    -> rasterize hard VisionMask
    -> rasterize hard IlluminationMask
    -> rasterize hard BypassVisionMask
    -> EffectiveLiveMask = (VisionMask INTERSECT IlluminationMask)
                           UNION BypassVisionMask
    -> subtract SuppressLiveVisionMask

persistent memory bits
    - ClearMemory mutation
    -> hard remembered mask
    - (BlockMemoryWrites regions union SuppressMemoryPresentation regions) for display only
    -> effective memory presentation mask

effective live mask + effective memory mask + controlled live/memory subject primitives
    -> presentation-only edge AA/feather
    -> neutral-gray detailed memory composite
    -> pure black unknown/suppressed area

independent Subject Reveal Override (for example attacker hit feedback)
    -> approved temporary reveal primitives only
    -> no change to FVisionQueryResult, live masks, memory bits, or snapshots
```

### Modifier semantics

- `ClearMemory` clears CPU bits and subject snapshot records immediately. A visual fade may retain a transient darkening transition but queries report not remembered from the clear revision onward. Re-exploration sets new bits even during the fade; new knowledge wins.
- `BlockMemoryWrites` gates the bit-set operation and suppresses remembered presentation while active, so the area becomes black immediately after live vision leaves. It does not erase existing bits and does not hide live vision; removing it may restore the older underlying memory.
- `SuppressMemoryPresentation` masks GPU/display memory and memory proxies while leaving CPU bits/records intact.
- `SuppressLiveVision` removes samples/polygon coverage from hard queries as well as presentation.
- `SuppressMemoryPresentation` differs from `BlockMemoryWrites` because legal live vision may continue updating underlying memory while presentation is suppressed.

## CPU/GPU responsibility split

### CPU owns authority

- registration and generation-safe handles;
- floors/heights and segment spatial index;
- separate vision and legal-illumination polygons and exact containment/bounds queries;
- the hard “gated vision intersect illumination, union bypass vision” result and both source attributions;
- hard modifier membership;
- persistent memory bits;
- subject knowledge-state transitions, last-seen metadata, and a separate transient reveal-override registry;
- deterministic snapshot serialization/compression;
- debug reasons and counters.

### GPU owns presentation

- rasterizing already-approved gated-vision, legal-illumination, and illumination-bypass polygons to separate R8 mask tiles/atlases;
- intersecting hard gated-vision and illumination masks, unioning the bypass-vision mask, and applying live suppression to derive effective live presentation;
- mirroring dirty memory and modifier-presentation tiles;
- max/union of multiple polygons inside each mask class;
- distance/coverage-based edge antialiasing within a bounded presentation width;
- full-screen composition, independent subject-reveal presentation, and optional debug overlays.

The plugin does not read a blurred GPU mask—or a rendered-light buffer—back to answer illumination/live queries or save memory. Save capture never waits for a full GPU readback.

## World-space mask design

Use floor-local, stable XY-to-UV mapping. Two implementation candidates must be spiked after approval:

1. sparse/tiled `UTextureRenderTarget2D` atlas managed per active knowledge owner/floor;
2. `UTextureRenderTarget2DArray` or render-graph pooled texture layers where UE 5.8.1 platform support and material sampling are reliable.

The CPU memory store uses packed bits at a configurable world resolution. No default is selected yet: the same reference scenes and motion paths must compare 2.5 cm, 5 cm, 10 cm, and 25 cm for boundary fidelity, CPU update time, GPU dirty-upload cost, runtime bytes, and compressed save bytes before a shipping value is chosen. GPU memory tiles use R8 for filterable presentation. Vision and illumination masks may use a separately measured finer active-window resolution because they are not saved. Tile borders include a one/filter-radius gutter to avoid seams. World origin and tile indices, not camera position, determine UVs.

At minimum the renderer exposes separately inspectable `VisionMask` (illumination-gated vision only), `IlluminationMask`, `BypassVisionMask`, `SuppressLiveVisionMask`, and derived `EffectiveLiveMask`. When compatibility profiles require more than one illumination class, the renderer either keeps matching mask layers or rejects the unsupported configuration; it must not over-union incompatible illumination.

Floating-origin/world-partition shifts require an explicit floor-origin/rebase event; raw large-world floating coordinates must not be baked into irreversible save keys.

## Detailed gray-memory presentation

### Immutable environment path

Static environment remains rendered normally under the fog post process. In the remembered-only region, the decided treatment uses stable material attributes—principally desaturated Base Color with controlled contrast, plus stable normal/AO/depth cues—to produce neutral-gray detail under stable neutral shading. It does not use `PostProcessInput0` lighting and does not freeze the last lit image. Thus fixed texture details remain readable while current point lights, flashes, exposure, and moving shadows cannot update memory.

The style must retain more than a depth outline. Default tuning should preserve mid-frequency base-color texture contrast and use depth edges only as a secondary cue.

### Subject path

`UVisionSubjectComponent` separates:

- **live primitives**, enabled only in hard current vision for `VisibleOnly`, `NeverRemember`, and snapshot policies;
- **memory primitives/proxy**, render-only and enabled only when a valid remembered snapshot is effective;
- authoritative gameplay actor/collision, never owned or frozen by the plugin.

For simple static-mesh subjects, an optional generic descriptor may capture mesh/material slots, transform, visibility, and selected material parameters. DARKWELL fixed, uncollected items use `LastSeenSnapshot`; their Adapter must supply stable identity and any semantic snapshot data not covered by the descriptor. Complex skeletal pose, construction-script semantics, Niagara, audio, inventory state, or arbitrary MID state requires `IVisionSubjectSnapshotProvider` from the host Adapter.

The proxy contributes Base Color/normal/depth to the same detailed memory material path. It never ticks gameplay, casts gameplay collision, or emits live light/audio/VFX.

### Unsupported material/features

Translucent, unlit, emissive-only, dynamic decal, custom-depth-only, and Niagara content require explicit support policy. V1 default is conservative: exclude them from memory unless an adapter provides an opaque memory proxy. Debug validation reports unsupported remembered primitives.

### Optional capture fallback

An aligned Scene Capture may later render a curated memory-only scene for projects that cannot use GBuffer/proxies. It is optional presentation, disabled by default, and cannot affect queries or saved memory. It must have documented show-only lists, cost, resolution, material limitations, and multi-source/floor behavior before becoming a supported Fab feature.

## Subject policy state machine

| Effective knowledge | `NeverRemember` / `VisibleOnly` | `StaticEnvironment` | `LastSeenSnapshot` |
| --- | --- | --- | --- |
| Visible | live primitives on; proxy off | normal environment | live on; capture/refresh transition metadata; proxy off |
| Remembered | all subject primitives off | gray environment allowed | live off; last-seen proxy on if valid |
| Unknown/cleared/suppressed | off | blacked by mask | both off; clear deletes record, suppression retains hidden record |

Transitions are driven from hard snapshot results. A subject with multiple samples uses its configured any/all/threshold rule. The result includes source attribution so a host may distinguish player/camera/item observation without changing core state.

### Independent Subject Reveal Override lane

Subject Reveal Overrides are not another row or value in the knowledge-state table. The DARKWELL Adapter may add a time-bounded attacker-subject reveal after that subject receives the relevant hit/damage event; the subsystem returns it beside, never inside, the `FVisionQueryResult`. A reveal may enable only explicitly registered reveal primitives or a conservative reveal material. It does not:

- set `Visible`, `hard live`, or a contributing vision/illumination source;
- enable ordinary target selection, prompts, threat HUD, or interaction;
- set memory bits or create/refresh `LastSeenSnapshot` metadata;
- survive expiry as a proxy or remembered image;
- serialize into the memory snapshot.

Reveal handles carry subject, reason, start/expiry, revocation, presentation policy, and revision. Debug views show the underlying knowledge state and active reveal override simultaneously. Whether a specific region is allowed to suppress a reveal is an explicit reveal-policy check; it must never be implemented by falsifying the knowledge state.

## Multi-floor strategy

### V1 model

- each floor has a stable ID, XY bounds, `ZMin`, `ZMax`, and local mask origin;
- sources, occluders, subjects, memory/modifiers, and tiles belong to one floor at a time;
- an occluder blocks a source when its height interval crosses the configured eye/target band;
- the local view has exactly one active queried/presented floor, while other floor memories remain stored but not composited;
- stairs/elevators change floor assignment through volumes/adapter events;
- simultaneous inter-floor portals are outside v1.

### Non-silent limitations

Stacked floors visible in the same XY pixels, open atria, balconies, portals, and long vertical sight lines cannot be inferred safely by one 2D polygon. V1 validation marks simultaneous-floor presentation unsupported. The plugin must never merge floors into one unexplained memory bitfield.

## Public C++ API draft

Illustrative signatures only:

```cpp
// Components normally self-register; explicit handles support runtime-created data.
FVisionSourceHandle RegisterSource(const FVisionSourceRegistration& Registration);
FVisionIlluminationSourceHandle RegisterIlluminationSource(
    const FVisionIlluminationSourceRegistration& Registration);
FVisionOccluderHandle RegisterOccluder(const FVisionOccluderRegistration& Registration);
FVisionModifierHandle AddModifier(const FVisionModifierRegistration& Registration);
bool UpdateSource(FVisionSourceHandle Handle, const FVisionSourceUpdate& Update);
bool UpdateIlluminationSource(
    FVisionIlluminationSourceHandle Handle,
    const FVisionIlluminationSourceUpdate& Update);
bool UpdateOccluder(FVisionOccluderHandle Handle, const FTransform& Transform, bool bEnabled);
bool RemoveModifier(FVisionModifierHandle Handle);

FVisionQueryResult QueryPoint(
    FVisionKnowledgeId Knowledge,
    FVisionFloorId Floor,
    const FVector& WorldPoint,
    EVisionQueryFlags Flags) const;

FVisionQueryResult QuerySamples(
    FVisionKnowledgeId Knowledge,
    FVisionFloorId Floor,
    TConstArrayView<FVector> Samples,
    EVisionSampleRule Rule) const;

void QueryBatch(TConstArrayView<FVisionQueryRequest> Requests,
                TArray<FVisionQueryResult>& OutResults) const;

FVisionIlluminationQueryResult QueryIlluminationPoint(
    FVisionKnowledgeId Knowledge,
    FVisionFloorId Floor,
    const FVector& WorldPoint) const;

FSubjectRevealOverrideHandle AddSubjectRevealOverride(
    FVisionSubjectHandle Subject,
    const FSubjectRevealOverrideRegistration& Registration);
bool RemoveSubjectRevealOverride(FSubjectRevealOverrideHandle Handle);
FVisionSubjectPresentationResult QuerySubjectPresentation(
    FVisionSubjectHandle Subject) const;

FVisionMemoryMutationResult ClearMemory(
    FVisionKnowledgeId Knowledge,
    const FVisionRegionShape& Region,
    const FVisionClearOptions& Options);

FVisionModifierHandle BlockMemoryWrites(...);
FVisionModifierHandle SuppressMemoryPresentation(...);
FVisionModifierHandle SuppressLiveVision(...);

FWorldVisionSaveSnapshot CaptureMemorySnapshot(FVisionKnowledgeId Knowledge) const;
EVisionRestoreResult RestoreMemorySnapshot(
    FVisionKnowledgeId Knowledge,
    const FWorldVisionSaveSnapshot& Snapshot,
    const FVisionRestoreOptions& Options);
```

Public results use enums/structs rather than booleans so new reasons/states can be added compatibly. All mutating calls define game-thread requirements. Read-only batch queries may receive immutable snapshot tokens for worker use.

## Blueprint API draft

`UVisionSourceComponent`:

- `SetVisionSourceEnabled(bool)`
- `SetVisionSourceProfile(FVisionSourceProfile)`
- `SetVisionKnowledgeOwner(FVisionKnowledgeId)`
- `SetVisionFloor(FVisionFloorId)`
- `GetLastSolvedRevision()`

`UVisionIlluminationSourceComponent`:

- `SetIlluminationSourceEnabled(bool)`
- `SetIlluminationSourceProfile(FVisionIlluminationSourceProfile)`
- `SetIlluminationKnowledgeOwner(FVisionKnowledgeId)`
- `SetIlluminationFloor(FVisionFloorId)`
- `GetLastSolvedRevision()`

`UVisionSubjectComponent`:

- `SetVisionSubjectPolicy(EVisionSubjectPolicy)`
- `SetVisionSamples(FVisionSubjectSampleSet)`
- `GetEffectiveVisionState()`
- `OnVisionStateChanged(Result)`
- `GetActiveRevealOverrides()`
- `OnSubjectRevealOverrideChanged(Result)`
- `InvalidateLastSeenSnapshot()`

`AMemoryModifierVolume`:

- operation flags for block/presentation/live suppression;
- enabled state, floor/height, fade settings;
- explicit `ClearMemoryNow` editor/runtime action.

`UVisionBlueprintLibrary`:

- `GetWorldVisionSubsystem`
- `QueryVisionAtLocation`
- `QueryLegalIlluminationAtLocation`
- `QueryActorVision`
- `AddSubjectRevealOverride`
- `RemoveSubjectRevealOverride`
- circle/box/polygon region constructors;
- `ClearVisionMemory`
- runtime modifier create/update/remove;
- snapshot capture/restore wrappers where Blueprint serialization is appropriate.

Blueprint events carry generic IDs/results only. Knowledge-state events and Subject Reveal Override events are different types and delegates. DARKWELL Blueprint/C++ Adapters translate them into game tags, HUD, VFX, or audio without treating a reveal callback as `Visible`.

## Save schema and versioning

### Ownership

`FWorldVisionSaveSnapshot` is a serializable value embedded by the host. WorldVision never calls `SaveGameToSlot`, opens levels, or chooses autosave timing.

### V1 snapshot contents

- magic/schema version and plugin format GUID;
- knowledge owner stable ID supplied by host;
- settings fingerprint for tile size/floor coordinate scheme;
- sorted floor records;
- sorted memory tile keys with compressed packed bits;
- persistent clear/mutation records only if the approved policy requires them beyond cleared bits;
- sorted last-seen subject records keyed by host persistent ID;
- provider type/version and bounded payload sizes;
- integrity counts/checksum where appropriate.

### Restore

1. Validate version, limits, duplicates, floor mapping, settings compatibility, and provider availability.
2. Migrate into the current in-memory schema without partially applying invalid data.
3. Publish memory in one restore revision.
4. Recompute vision and legal-illumination polygons; never restore serialized live state.
5. Lazily create memory proxies as their floor streams in.
6. Mark GPU tiles dirty for derived upload.

Live vision/illumination polygons, active-source state, GPU masks, and Subject Reveal Overrides are not snapshot contents; the host reactivates sources after restore and transient reveals are discarded.

Plugin schema migrations remain independent from DARKWELL save v6. The decided DARKWELL policy is **no v6 fog-memory migration**: old v6 grid/cell data is never converted into WorldVision tiles. A save without a valid WorldVision snapshot starts with empty WorldVision memory. While legacy authority remains available, its old fog data may still be read only by that legacy path.

## Editor authoring workflow

1. Create/assign floor volumes and stable IDs.
2. Add explicit occluder components or select approved meshes/collision and run a preview conversion.
3. Inspect/weld/simplify generated outlines without closing deliberate gaps.
4. Mark dynamic doors with explicit local segments and height range.
5. Add modifier volumes/polygons and inspect operation/floor colors.
6. Add distinct vision-source/illumination-source, subject, and reveal-override examples in the plugin example map.
7. Run validation: missing/duplicate floor IDs, zero or multiple active floors, out-of-bounds data, self intersections, overlapping ambiguous floors, zero/duplicate segments, incompatible illumination profiles, unsupported remembered materials, missing subject persistent IDs, excessive dynamic blockers, stale baked data.
8. Save generated data explicitly with undo/source references.

Conversion is assistance, not hidden runtime magic. Fab users can author segments manually for small maps and use bake tools for larger static layouts.

## Debug views and instrumentation

Runtime views:

- vision-source profile/cone/range/eye height, illumination requirement/bypass, and activation reason;
- legal-illumination-source profile/range/height, Adapter activation reason, and hard polygon;
- nearby occluder spatial cells, segments, height bands, normals/endpoints;
- per-vision-source and per-illumination-source polygons and current revisions;
- separate gated-vision, illumination, bypass-vision, live-suppression, and derived effective-live masks;
- hard illumination query, hard live intersection, and feathered presentation comparison;
- loaded floors and the exactly one active queried/presented floor;
- memory tile state/dirty rect/revision;
- write-block, memory-suppress, and live-suppress masks in distinct colors;
- subject sample points, policy, underlying knowledge state, vision/illumination attribution or bypass reason, proxy state, and separate reveal-override handles/expiry;
- clear/fade transitions;
- save restore/provider diagnostics.

Stats:

- active/dirty vision and illumination sources, relevant segments, emitted vertices, solve queue/latency;
- illumination/live point, batch, and subject queries and time;
- active floors, allocated/dirty memory tiles, CPU/GPU bytes;
- polygon raster and composite GPU timing;
- modifier count and invalidations;
- snapshot raw/compressed bytes and restore time;
- warnings when configured caps or latency budgets are exceeded.

## Automation test matrix

| Layer | Required cases |
| --- | --- |
| Geometry unit | segment intersection; endpoint +/- epsilon; parallel/collinear/duplicate segments; source on edge/corner; cone/radial clip; deterministic stable IDs; very small/large coordinates |
| Polygon topology | straight wall, L/T corners, closed room, doorway, thin blocker, adjacent rooms, holes/openings, moving/rotating door, reference solver vs optimized sweep |
| Height/floor | occluder below/above eye, intersecting band, floor isolation, floor change, unloaded floor, duplicate/missing floor ID, unsupported stacked-floor validation |
| Sources | separate vision/illumination registration; Adapter-controlled remote activation; profile/transform revisions; multi-source union/attribution; permanent attached body circle; source-limit diagnostics; optional additional hard filter |
| Illumination | explicit illumination polygon; point/bounds/batch hard query; gated vision outside/inside illumination; illumination alone reveals/writes nothing; source compatibility; ordinary rendered light has no authority |
| Queries | point boundary epsilon; bounds any/all/threshold; gated intersection; bypass attribution; batch equivalence; immutable shared revisions; CPU/GPU agreement; no presentation feedback |
| Memory | legal effective-live write; illumination-only no-write; occluded no-write; off-screen Adapter-activated remote write; clear/re-explore; 2.5/5/10/25 cm comparison; capacity diagnostics; tile edge/gutter; world-origin mapping |
| Modifiers | all shapes, height/floor, each operation, overlap union, strict no-memory composition, monster `ClearMemory` plus `BlockMemoryWrites`, fade with re-exploration, enable/disable invalidation |
| Subjects | never-remember enemy; fixed uncollected item `LastSeenSnapshot`; immutable environment; last-seen simple proxy; custom provider; clear/suppress/reacquire; duplicate/missing persistent IDs; attacker-hit Subject Reveal Override remains separate and writes no bit/snapshot |
| Persistence | deterministic round trip, compression, corrupt/future/oversized data, floor remap, provider migration/missing provider, independent plugin schema migration, explicit no-v6-fog-import behavior, no transient reveal serialization |
| Rendering | CPU reference vs GPU vision/illumination/intersection/bypass masks, straight-wall motion capture, 1080p/1440p/window resize, tile seams, neutral-gray base detail, no live-light/enemy/dynamic-state leakage |
| Lifecycle | PIE restart, world teardown, streaming load/unload, source/subject destroyed during solve, authority switch, no stale callbacks/handles |
| Performance | declared 1/4/8-source workloads, static/dynamic segment mixes, 512 subjects, modifier stress, memory growth/save size, Development packaged and Editor samples |

The independent example map must include labeled test lanes for straight wall, diagonal wall, corner, closed/opening doorway, rotating door, rooms, multiple height bands, stacked-floor rejection, multiple vision and illumination sources, body-circle bypass, vision/illumination intersection, each modifier, each subject policy, attacker-hit reveal override, and save/reload.

## Performance architecture and budgets

The provisional budgets in `VISION_SYSTEM_REQUIREMENTS.md` are supported by these design choices:

- event-driven dirty source/occluder tracking, not repeated world-wide actor scans;
- floor-local spatial index and source-range culling;
- plain immutable worker data and reusable arrays;
- per-vision/illumination-source polygon caching and source-attribution preservation;
- no polygon union on CPU for normal queries;
- batched subject queries;
- packed CPU memory bits and dirty tiles;
- GPU triangle rasterization rather than CPU evaluation of every viewport pixel;
- no save-time GPU readback;
- bounded, opt-in dynamic occluders and snapshot proxies;
- stats/caps that fail visibly rather than silently dropping sources/memory.

Reference workload proposed for first measurement: 8 active vision sources, 8 active illumination sources, 4 floors loaded with exactly one active/presented, 4,096 relevant segments across dirty sources, 32 dynamic door segments, 512 subjects, 256 m x 256 m explored area, and 1920x1080 DX12/SM6 Development build. Run the workload independently at 2.5, 5, 10, and 25 cm memory precision; there is no selected default until the comparison is reviewed. The user must approve or replace the workload and minimum hardware before budgets become acceptance criteria.

## Main risks and mitigations

| Risk | Impact | Mitigation / fallback |
| --- | --- | --- |
| Robust endpoint/collinear geometry | cracks, false wedges, nondeterminism | central epsilon policy, stable IDs, normalized authoring, reference solver, adversarial tests |
| Dynamic door churn | repeated source solves | overlap-based invalidation, transform thresholds, bounded dynamic blocker budget, finish-state caching |
| Vision/illumination authority drift | CPU query, memory write, and GPU composition disagree | one immutable revision, explicit hard illumination queries, separate masks, CPU/GPU reference comparisons, no rendered-light sampling |
| Simplified 2.5D mismatch with authored meshes | visual gap between blocker and art | editor preview/validation, explicit author adjustment, complex visual depth remains presentation-only |
| Stacked floors/atria | XY ambiguity and leakage | exactly one active floor in v1; reject simultaneous-floor layouts instead of guessing |
| World-space tile seams/origin | visible seams or save drift | tile gutters, stable floor origins, integer keys, rebase events, render/reference tests |
| Memory proxy setup cost | poor Fab usability | simple automatic static-mesh descriptor, adapter templates, validators, example subjects; complex actors remain explicit |
| GBuffer/material limitations | missing translucent/unlit/decal detail | supported-material matrix, opaque proxy fallback, optional curated capture later |
| Multi-source CPU spike | missed frame budget | spatial index, cached polygons, worker scheduling, staggered solve with declared latency, caps/stats |
| GPU/RHI path on UE 5.8.1 | render target/array/shader variation | isolate private renderer, DX12/SM6 reference tests, fallback atlas path, no renderer dependency in queries/save |
| Save growth/provider incompatibility | large/corrupt saves or missing proxies | bit tiles, compression, size bounds, deterministic records, provider versions, atomic validation/restore |
| Old/new coexistence | double masking/hiding and divergent memory | separate lab map, single authority switch, adapter gates every consumer, explicit render-state reset |
| Scope expansion toward full 3D/multiplayer | delayed v1 | keep voxel, arbitrary vertical sight, multiple active floors, and multiplayer explicit non-goals until v1 evidence exists |

The largest technical risk is not polygon computation itself; it is preserving credible last-seen object/material presentation across arbitrary host content without leaking live state or imposing excessive adapter work. V1 must define a conservative supported subject/material set and make unsupported cases visible in validation.

## Alternatives and fallback decisions

1. **If optimized angular sweep proves fragile**, ship the tested endpoint-ray reference solver for v1 with authoring/source-count limits; optimize only after profile evidence.
2. **If texture-array support complicates UE 5.8.1 material/RHI compatibility**, use a floor/tile atlas with explicit UV metadata. Authority remains unchanged.
3. **If automatic subject proxies are unreliable**, narrow v1 to immutable environment, visible-only/never-remember subjects, and adapter-provided snapshot proxies. Do not show current dynamic state as memory.
4. **If GBuffer memory styling misses required materials**, provide opaque memory-proxy materials first. Evaluate curated Scene Capture only as optional presentation after cost/feature tests.
5. **If a post-v1 map requires simultaneous stacked-floor sight**, evaluate explicit portal/layer composition as a separately approved extension; do not weaken the v1 one-active-floor rule or jump to full voxel fog without evidence.

## Recorded human decisions and remaining gates

The following product decisions are settled for this architecture revision:

1. `WorldVision` is a temporary internal code name, not a committed plugin/API name.
2. Legal illumination is independent first-class authority with an explicit component, hard polygons/queries, and separate CPU/GPU data; gated live coverage is the vision/illumination intersection.
3. The permanent player-attached circular source bypasses illumination while still obeying occlusion, floor, and live suppression.
4. Remote vision and illumination sources are activated by the DARKWELL Adapter; the core never infers activation.
5. Remembered environment uses neutral-gray material detail and stable neutral shading, never a frozen last-lit image.
6. DARKWELL fixed, uncollected items use `LastSeenSnapshot`.
7. V1 is single-player with exactly one active queried/presented floor.
8. Old DARKWELL v6 fog memory is not migrated; WorldVision memory starts fresh when no WorldVision snapshot exists.
9. Monster permanent blackout is composed from `ClearMemory` plus `BlockMemoryWrites`.
10. Attacker hit feedback is an independent, non-memory-writing Subject Reveal Override and never becomes normal `Visible` state.
11. Memory precision is selected only after comparing 2.5, 5, 10, and 25 cm.

The remaining implementation gates are the public plugin/API name, approved minimum hardware and reference workload, exact supported remembered-material domains, automatic simple-proxy scope versus Adapter-provided snapshots, modifier persistence/fade details, the selected precision after measurement, and the approved reveal primitive/material and suppression policy. None of these remaining gates may be silently fixed by an implementation default.

## Approval gate

The decisions above revise the target architecture but do not authorize source, shaders, plugin content, Unreal assets, or deletion of the old fog system. A separate implementation approval must close or explicitly defer the remaining gates. Only then may the next implementation milestone create the approved-name skeleton, tests, and independent lab map described in `VISION_SYSTEM_MIGRATION_PLAN.md`.

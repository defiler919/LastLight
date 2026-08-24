# Independent vision plugin architecture

Status: proposed architecture for human approval. No plugin source, shader, asset, or game adapter is implemented in this milestone.

Working package/product name: `WorldVision` (provisional).

## Decision summary

Use a **hybrid architecture with explicit 2.5D geometry as authority**:

1. Runtime sources and explicit height-banded occluder segments produce stable per-source visibility polygons in CPU world space.
2. Gameplay subjects, HUD, interaction, and memory-write eligibility query those hard polygons directly.
3. The render path rasterizes the same polygons into floor-aware world-space live masks and mirrors a CPU-owned persistent memory tile field into GPU masks.
4. A post-process material applies presentation-only antialiasing and composites live scene, detailed gray memory, and black unknown.
5. Immutable environment may use stable current-frame material attributes because it cannot change. Stateful/moving subjects use policy-controlled live render primitives and render-only last-seen proxies so current state cannot leak into memory.

This keeps the stable-wall and deterministic-query strengths of visibility polygons while using the GPU for the workload it handles well: triangle rasterization, tiled mask combination, and final full-screen composition.

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
| Multiple floors/heights | Natural 2.5D floor/height bands; requires authored portals for stacked views | Better raw 3D representation, but one/more depth captures per source/floor and ambiguous saved XY memory | Explicit floor authority with optional presentation capture only for exceptional authored cases |
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

Provisional plugin root after approval: `Plugins/WorldVision/`.

| Module | Type | Owns | Must not own |
| --- | --- | --- | --- |
| `WorldVisionRuntime` | Runtime | subsystem, floor/source/occluder/subject/modifier components, geometry solver, queries, CPU memory tiles, save snapshot structs, render-resource bridge, public C++/Blueprint API | DARKWELL actors/tags, missions, save slots, HUD text, Niagara/audio, third-party libraries |
| `WorldVisionEditor` | Editor | component visualizers, occluder/polygon editing, mesh/collision conversion assistance, floor tools, validation, debug panels, example-map authoring helpers | runtime gameplay decisions or editor code in the Runtime module |
| `WorldVisionTests` | Developer/Editor-only | pure geometry, runtime functional, rendering comparison, persistence, and performance harnesses | shipping runtime dependencies |

Rendering remains a private area inside `WorldVisionRuntime` for v1 so public users depend on one runtime module. If the render path later requires substantial alternate RHI/server packaging, it may split into `WorldVisionRendering` without changing public authority/query types.

### Plugin content

After approval, plugin content may contain:

- a post-process material and material functions;
- debug materials/icons;
- authoring data assets/settings;
- standalone example/test maps and simple meshes.

No DARKWELL map, item, enemy, mission, or material belongs in plugin content.

### DARKWELL adapter boundary

Project-owned adapter code, later placed under `Source/Darkwell` (or a separate project plugin only if approved), maps:

- player facing/aim/equipment to source profiles;
- security cameras and remote observation items to source activation;
- DARKWELL illumination rules to optional source filtering;
- monster black fog to memory/live modifier regions;
- enemies, pickups, doors, containers, machines, and objective items to generic subject policies;
- current-visibility events to HUD, interaction, Niagara, and audio;
- plugin snapshots into DARKWELL's versioned continuation save.

The adapter may reference both DARKWELL and WorldVision. WorldVision must never reference DARKWELL.

## Proposed runtime classes and responsibilities

Names are API drafts, not implementation commitments.

| Type | Responsibility |
| --- | --- |
| `UWorldVisionSubsystem : UWorldSubsystem` | registry, revisions, scheduling, immutable frame snapshots, queries, memory mutation, floor/stream lifecycle, debug/stats, snapshot capture/restore |
| `UVisionSourceComponent` | source transform/profile, knowledge owner, active floor, activation, dirty tracking, optional filter/provider interface |
| `UVisionOccluderComponent` | explicit local polygon/segments, height band, floor, static/dynamic mode, transform revision |
| `UVisionSubjectComponent` | visibility samples, policy, live primitive set, last-seen provider/proxy, state-transition events, persistent ID |
| `UVisionFloorComponent` or `AVisionFloorVolume` | stable floor ID, XY bounds, `ZMin`/`ZMax`, active presentation layer, optional portal references |
| `AMemoryModifierVolume` | editor/runtime region actor wrapping one or more modifier operations |
| `UVisionBlueprintLibrary` | safe subsystem lookup, point/bounds/batch queries, region builders, debug helpers |
| `UVisionSettings` | project/plugin defaults, tolerances, tile resolution, scheduling/budgets, debug and rendering configuration |
| `IVisionSubjectSnapshotProvider` | host callback for semantic last-seen capture, proxy construction/update, and snapshot serialization |
| `IVisionSourceFilterProvider` | optional host callback/field profile for rules beyond a direct source polygon; cannot read presentation masks |

Components self-register in `OnRegister`/world begin play and unregister safely. The subsystem stores handles and plain immutable solve data; workers never dereference gameplay UObjects.

## Core data structures

```text
FVisionKnowledgeId
    opaque local knowledge owner (single DARKWELL player in v1)

FVisionFloorId / FVisionFloorDefinition
    stable ID, XY bounds, ZMin, ZMax, transform/origin, revision

FVisionSourceHandle / FVisionSourceState
    generation-safe handle, knowledge ID, floor, origin/eye Z, facing,
    shape/profile, enabled/filter state, transform/profile revisions

FVisionOccluderHandle / FVisionSegment2D
    generation-safe handle, endpoints A/B, floor, ZMin/ZMax,
    static/dynamic flags, source component ID, geometry revision

FVisionPolygon
    source handle, floor, ordered world-space vertices, bounds,
    hard-edge epsilon policy, source/occluder revision tuple

FVisionFrameSnapshot
    monotonically increasing revision, source polygons, spatial lookup,
    active modifier query data; immutable during queries/render submission

FVisionSubjectSampleSet
    anchor/bounds/custom points plus any/all/required-count rule

FVisionQueryResult
    Unknown/Remembered/Visible, hard live boolean, remembered boolean,
    source handle(s), floor, rejection flags, snapshot revision

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

## Visibility-polygon solve

### Correctness reference

The correctness implementation clips the source shape to its floor and evaluates candidate angles at:

- source cone boundary rays;
- every relevant segment endpoint angle;
- endpoint angle minus/plus a documented small angular epsilon;
- stable tessellation angles for radial/curved source boundaries;
- optional authored portal boundaries.

For each candidate ray, analytic ray/segment intersection selects the nearest valid segment whose height band blocks the configured source/target band. Intersections are sorted by angle and deduplicated by deterministic distance/segment-ID rules. Vertices remain in world coordinates and on the source boundary or the actual occluder segment.

### Optimized solve

After reference tests pass, an angular sweep maintains active segments sorted by intersection distance and emits the same polygon topology in approximately `O((N + K) log N)` for N relevant segments and K events. The reference solver remains available in tests/debug to compare results.

### Epsilon policy

World weld, ray/segment parallel, endpoint angular, point-in-polygon, and boundary-visible epsilons live in one settings structure and are reported in debug output. Tolerances use world units and scale bounds, not viewport pixels. Stable segment IDs break exact ties.

### Source union and filters

Per-source polygons stay separate. A point/subject is hard-visible if any active legal source for the knowledge owner passes:

1. floor/height membership;
2. source polygon containment;
3. optional hard source-filter result;
4. no hard live-suppression modifier at the sample.

The GPU draws all passing polygons into the same live mask. Optional DARKWELL illumination filtering must either produce hard world-space field geometry/query results or configure the source profile; it may not sample a blurred light buffer or final scene color.

## Frame/update model

1. Game thread drains registrations and source/occluder/floor/modifier/subject dirty events.
2. Plain solve inputs are copied into generation-safe arrays.
3. Worker tasks update dirty source polygons and affected hard-memory tiles.
4. Game thread publishes one immutable `FVisionFrameSnapshot` at a controlled sync point.
5. Batched subject queries consume that snapshot and enqueue state transitions.
6. Game-thread component callbacks apply live/proxy presentation and notify host adapters.
7. Render thread rasterizes the same polygon revision into world-space masks and uploads only dirty memory tiles.
8. Post process samples hard masks, applies presentation-only feather, and composites the scene.

Source solve cadence can be capped, but a door/source transform change must carry explicit maximum latency. Gameplay and rendering always identify which snapshot revision they consumed.

## Live, memory, and modifier data flow

```text
registered source profile + floor/height
    + nearby explicit occluder segments
    -> per-source hard visibility polygon
    -> optional hard source filter
    -> subtract hard SuppressLiveVision regions
    -> FVisionFrameSnapshot
       |-> exact point/bounds/subject/HUD/gameplay queries
       |-> rasterize live mask (GPU world-space floor tiles)
       |-> intersect with !BlockMemoryWrites (CPU hard regions)
           -> set persistent CPU memory bits
           -> invalidate/refresh last-seen subject snapshots
           -> upload dirty memory tiles to GPU

persistent memory bits
    - ClearMemory mutation
    -> hard remembered mask
    - (BlockMemoryWrites regions union SuppressMemoryPresentation regions) for display only
    -> effective memory presentation mask

live mask + effective memory mask + controlled live/memory subject primitives
    -> presentation-only edge AA/feather
    -> detailed gray memory composite
    -> pure black unknown/suppressed area
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
- source polygons and exact containment/bounds queries;
- hard modifier membership;
- persistent memory bits;
- subject state transitions and last-seen metadata;
- deterministic snapshot serialization/compression;
- debug reasons and counters.

### GPU owns presentation

- rasterizing already-approved polygons to live R8 mask tiles/atlas;
- mirroring dirty memory and modifier-presentation tiles;
- max/union of multiple source polygons;
- distance/coverage-based edge antialiasing within a bounded presentation width;
- full-screen composition and optional debug overlays.

The plugin does not read a blurred GPU mask back to answer queries or save memory. Save capture never waits for a full GPU readback.

## World-space mask design

Use floor-local, stable XY-to-UV mapping. Two implementation candidates must be spiked after approval:

1. sparse/tiled `UTextureRenderTarget2D` atlas managed per active knowledge owner/floor;
2. `UTextureRenderTarget2DArray` or render-graph pooled texture layers where UE 5.8.1 platform support and material sampling are reliable.

The CPU memory store uses packed bits at a configurable world resolution (provisional default 25 cm). GPU tiles use R8 for filterable presentation. Live masks may use a finer active-window resolution (provisional 10-12.5 cm) because they are not saved. Tile borders include a one/filter-radius gutter to avoid seams. World origin and tile indices, not camera position, determine UVs.

Floating-origin/world-partition shifts require an explicit floor-origin/rebase event; raw large-world floating coordinates must not be baked into irreversible save keys.

## Detailed gray-memory presentation

### Immutable environment path

Static environment remains rendered normally under the fog post process. In the remembered-only region, the material uses stable material attributes—principally Base Color with controlled desaturation/contrast, plus stable normal/AO/depth cues—to produce gray detail. It does not use `PostProcessInput0` lighting for memory. Thus fixed texture details remain readable while current point lights, flashes, exposure, and moving shadows cannot update memory.

The style must retain more than a depth outline. Default tuning should preserve mid-frequency base-color texture contrast and use depth edges only as a secondary cue.

### Subject path

`UVisionSubjectComponent` separates:

- **live primitives**, enabled only in hard current vision for `VisibleOnly`, `NeverRemember`, and snapshot policies;
- **memory primitives/proxy**, render-only and enabled only when a valid remembered snapshot is effective;
- authoritative gameplay actor/collision, never owned or frozen by the plugin.

For simple static-mesh subjects, an optional generic descriptor may capture mesh/material slots, transform, visibility, and selected material parameters. Complex skeletal pose, construction-script semantics, Niagara, audio, inventory state, or arbitrary MID state requires `IVisionSubjectSnapshotProvider` from the host adapter.

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

## Multi-floor strategy

### V1 model

- each floor has a stable ID, XY bounds, `ZMin`, `ZMax`, and local mask origin;
- sources, occluders, subjects, memory/modifiers, and tiles belong to one floor at a time;
- an occluder blocks a source when its height interval crosses the configured eye/target band;
- the local view normally selects one presentation floor, while other floor memories remain stored but not composited;
- stairs/elevators change floor assignment through volumes/adapter events;
- explicitly authored portals may project visibility between floors as a bounded advanced feature after the single-floor solver is accepted.

### Non-silent limitations

Stacked floors visible in the same XY pixels, open atria, balconies, and long vertical sight lines cannot be inferred safely by one 2D polygon. V1 validation either requires authored portal/layer rules or marks the arrangement unsupported. The plugin must never merge those floors into one unexplained memory bitfield.

## Public C++ API draft

Illustrative signatures only:

```cpp
// Components normally self-register; explicit handles support runtime-created data.
FVisionSourceHandle RegisterSource(const FVisionSourceRegistration& Registration);
FVisionOccluderHandle RegisterOccluder(const FVisionOccluderRegistration& Registration);
FVisionModifierHandle AddModifier(const FVisionModifierRegistration& Registration);
bool UpdateSource(FVisionSourceHandle Handle, const FVisionSourceUpdate& Update);
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

`UVisionSubjectComponent`:

- `SetVisionSubjectPolicy(EVisionSubjectPolicy)`
- `SetVisionSamples(FVisionSubjectSampleSet)`
- `GetEffectiveVisionState()`
- `OnVisionStateChanged(Result)`
- `InvalidateLastSeenSnapshot()`

`AMemoryModifierVolume`:

- operation flags for block/presentation/live suppression;
- enabled state, floor/height, fade settings;
- explicit `ClearMemoryNow` editor/runtime action.

`UVisionBlueprintLibrary`:

- `GetWorldVisionSubsystem`
- `QueryVisionAtLocation`
- `QueryActorVision`
- circle/box/polygon region constructors;
- `ClearVisionMemory`
- runtime modifier create/update/remove;
- snapshot capture/restore wrappers where Blueprint serialization is appropriate.

Blueprint events carry generic IDs/results only. DARKWELL Blueprint/C++ adapters translate them into game tags, HUD, VFX, or audio.

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
4. Recompute live source polygons; never restore serialized live state.
5. Lazily create memory proxies as their floor streams in.
6. Mark GPU tiles dirty for derived upload.

Plugin migrations remain independent from DARKWELL save v6. A later DARKWELL adapter may convert old 100/10 cm cells into WorldVision memory tiles once, but old fields remain readable until a separately approved campaign-save migration.

## Editor authoring workflow

1. Create/assign floor volumes and stable IDs.
2. Add explicit occluder components or select approved meshes/collision and run a preview conversion.
3. Inspect/weld/simplify generated outlines without closing deliberate gaps.
4. Mark dynamic doors with explicit local segments and height range.
5. Add modifier volumes/polygons and inspect operation/floor colors.
6. Add source/subject components in the plugin example map.
7. Run validation: missing/duplicate floor IDs, out-of-bounds data, self intersections, overlapping ambiguous floors, zero/duplicate segments, unsupported remembered materials, missing subject persistent IDs, excessive dynamic blockers, stale baked data.
8. Save generated data explicitly with undo/source references.

Conversion is assistance, not hidden runtime magic. Fab users can author segments manually for small maps and use bake tools for larger static layouts.

## Debug views and instrumentation

Runtime views:

- source profile/cone/range/eye height and activation reason;
- nearby occluder spatial cells, segments, height bands, normals/endpoints;
- per-source polygon and current polygon revision;
- union hard live mask versus feathered presentation mask;
- floor/layer selection and portal links;
- memory tile state/dirty rect/revision;
- write-block, memory-suppress, and live-suppress masks in distinct colors;
- subject sample points, policy, effective state, source attribution, proxy state;
- clear/fade transitions;
- save restore/provider diagnostics.

Stats:

- active/dirty sources, relevant segments, emitted vertices, solve queue/latency;
- point/batch/subject queries and time;
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
| Sources | activation/profile/transform revisions, off-screen remote source, multi-source union/attribution, source limit diagnostics, optional hard filter |
| Queries | point boundary epsilon, bounds any/all/threshold, batch equivalence, immutable revision, no presentation feedback |
| Memory | legal write, occluded no-write, off-screen remote write, clear/re-explore, capacity diagnostics, tile edge/gutter, world-origin mapping |
| Modifiers | all shapes, height/floor, each operation, overlap union, strict no-memory composition, fade with re-exploration, enable/disable invalidation |
| Subjects | never-remember enemy, visible-only pickup, immutable environment, last-seen simple proxy, custom provider, clear/suppress/reacquire, duplicate/missing persistent IDs |
| Persistence | deterministic round trip, compression, corrupt/future/oversized data, floor remap, provider migration/missing provider, independent schema migration |
| Rendering | hard-mask CPU reference vs GPU raster, straight-wall motion capture, 1080p/1440p/window resize, tile seams, gray base-detail, no live-light/enemy/dynamic-state leakage |
| Lifecycle | PIE restart, world teardown, streaming load/unload, source/subject destroyed during solve, authority switch, no stale callbacks/handles |
| Performance | declared 1/4/8-source workloads, static/dynamic segment mixes, 512 subjects, modifier stress, memory growth/save size, Development packaged and Editor samples |

The independent example map must include labeled test lanes for straight wall, diagonal wall, corner, closed/opening doorway, rotating door, rooms, multiple height bands, stacked-floor rejection/portal case, multiple observers, each modifier, each subject policy, and save/reload.

## Performance architecture and budgets

The provisional budgets in `VISION_SYSTEM_REQUIREMENTS.md` are supported by these design choices:

- event-driven dirty source/occluder tracking, not repeated world-wide actor scans;
- floor-local spatial index and source-range culling;
- plain immutable worker data and reusable arrays;
- per-source polygon caching and source-attribution preservation;
- no polygon union on CPU for normal queries;
- batched subject queries;
- packed CPU memory bits and dirty tiles;
- GPU triangle rasterization rather than CPU evaluation of every viewport pixel;
- no save-time GPU readback;
- bounded, opt-in dynamic occluders and snapshot proxies;
- stats/caps that fail visibly rather than silently dropping sources/memory.

Reference workload proposed for first measurement: 8 active sources, 4 floors loaded (one presented), 4,096 relevant segments across dirty sources, 32 dynamic door segments, 512 subjects, 256 m x 256 m explored area at 25 cm memory resolution, 1920x1080 DX12/SM6 Development build. The user must approve or replace this workload and minimum hardware before budgets become acceptance criteria.

## Main risks and mitigations

| Risk | Impact | Mitigation / fallback |
| --- | --- | --- |
| Robust endpoint/collinear geometry | cracks, false wedges, nondeterminism | central epsilon policy, stable IDs, normalized authoring, reference solver, adversarial tests |
| Dynamic door churn | repeated source solves | overlap-based invalidation, transform thresholds, bounded dynamic blocker budget, finish-state caching |
| Simplified 2.5D mismatch with authored meshes | visual gap between blocker and art | editor preview/validation, explicit author adjustment, complex visual depth remains presentation-only |
| Stacked floors/atria | XY ambiguity and leakage | one active floor v1, explicit portals/layers, reject unsupported layouts instead of guessing |
| World-space tile seams/origin | visible seams or save drift | tile gutters, stable floor origins, integer keys, rebase events, render/reference tests |
| Memory proxy setup cost | poor Fab usability | simple automatic static-mesh descriptor, adapter templates, validators, example subjects; complex actors remain explicit |
| GBuffer/material limitations | missing translucent/unlit/decal detail | supported-material matrix, opaque proxy fallback, optional curated capture later |
| Multi-source CPU spike | missed frame budget | spatial index, cached polygons, worker scheduling, staggered solve with declared latency, caps/stats |
| GPU/RHI path on UE 5.8.1 | render target/array/shader variation | isolate private renderer, DX12/SM6 reference tests, fallback atlas path, no renderer dependency in queries/save |
| Save growth/provider incompatibility | large/corrupt saves or missing proxies | bit tiles, compression, size bounds, deterministic records, provider versions, atomic validation/restore |
| Old/new coexistence | double masking/hiding and divergent memory | separate lab map, single authority switch, adapter gates every consumer, explicit render-state reset |
| Scope expansion toward full 3D/multiplayer | delayed Fab-quality v1 | keep voxel, arbitrary vertical sight, and multiplayer explicit non-goals until v1 evidence exists |

The largest technical risk is not polygon computation itself; it is preserving credible last-seen object/material presentation across arbitrary host content without leaking live state or imposing excessive adapter work. V1 must define a conservative supported subject/material set and make unsupported cases visible in validation.

## Alternatives and fallback decisions

1. **If optimized angular sweep proves fragile**, ship the tested endpoint-ray reference solver for v1 with authoring/source-count limits; optimize only after profile evidence.
2. **If texture-array support complicates UE 5.8.1 material/RHI compatibility**, use a floor/tile atlas with explicit UV metadata. Authority remains unchanged.
3. **If automatic subject proxies are unreliable**, narrow v1 to immutable environment, visible-only/never-remember subjects, and adapter-provided snapshot proxies. Do not show current dynamic state as memory.
4. **If GBuffer memory styling misses required materials**, provide opaque memory-proxy materials first. Evaluate curated Scene Capture only as optional presentation after cost/feature tests.
5. **If the approved map requires simultaneous stacked-floor sight**, add explicit portal/layer composition as a separate milestone; do not jump to full voxel fog without evidence.

## Questions requiring user decisions

Architecture approval should answer these before formal implementation. Recommended defaults are included.

1. **Product/API name:** approve provisional `WorldVision`, or provide the desired plugin/module prefix? Recommended: choose a distinctive Fab-searchable name before source creation so public types do not need redirects.
2. **Minimum hardware and workload:** what CPU/GPU, resolution, expected active sources, map extent, simultaneous floors, dynamic doors, and subjects define acceptance? Recommended interim workload is listed above; budgets cannot be final without this.
3. **DARKWELL illumination semantics:** should the player cone directly grant vision, or must it still intersect explicit illumination fields as the old system does? Recommended: plugin sources grant vision directly; a DARKWELL adapter adds a hard illumination filter only if the game design still requires darkness-gated sight.
4. **Remote observation activation:** do security-camera/remote-item sources always share knowledge when active, or only while the player is connected/using them? Recommended: adapter-controlled activation; the plugin unions only active sources.
5. **Memory look:** should gray memory preserve desaturated Base Color/material detail with stable neutral shading, or freeze the last lit color image? Recommended: material detail with neutral shading; do not store live lighting/shadows.
6. **V1 subject snapshots:** is automatic static-mesh snapshot plus adapter-provided complex snapshots sufficient? Recommended: yes; exclude arbitrary skeletal/Niagara/translucent snapshots from v1.
7. **Pickup memory policy:** the request says uncollected items need last-seen support while moving/nonrememberable objects do not. Should DARKWELL pickups default to last-seen or visible-only? Recommended: important stationary placed items may be `LastSeenSnapshot`; loose/removable loot remains `VisibleOnly` unless explicitly opted in.
8. **Floor scope:** can v1 display one active floor at a time with authored floor transitions, treating atria/stacked simultaneous views as a later portal feature? Recommended: yes.
9. **`BlockMemoryWrites` and old memory:** after the blocker is removed, should memory written before it appeared return? Recommended core semantic: yes; while the blocker is active the area is still black outside live vision, and `ClearMemory` is available when prior memory must be destroyed.
10. **Clear fade semantics:** approve immediate knowledge deletion with presentation-only fade and new exploration winning during the fade? Recommended: yes.
11. **Modifier persistence:** should placed/runtime suppression/block volumes persist only through game state, while `ClearMemory` changes are naturally present in saved memory bits? Recommended: yes; plugin snapshot stores resulting memory and subject records, host saves dynamic modifier actors/state separately unless explicitly requested.
12. **Supported materials:** is opaque/masked static environment plus opaque memory proxies an acceptable v1 baseline, with translucent/unlit/dynamic decal/Niagara remembered detail deferred? Recommended: yes.
13. **Fab scope:** keep v1 single-player/local-knowledge only and defer multiplayer/replication? Recommended: yes, consistent with repository constraints.
14. **Old-grid migration:** after plugin acceptance, should existing DARKWELL v6 exploration be converted once or should plugin-enabled saves begin with fresh memory? Recommended: provide a one-way v6 grid-to-plugin migration in the DARKWELL adapter, then retain backward load support for at least one product save-version cycle.

## Approval gate

Approval means agreeing to the explicit-geometry/hybrid authority model, module boundary, memory-proxy strategy, v1 floor scope, and answers to the questions above. Approval does not authorize deleting the old fog system. The next implementation milestone should create only the plugin skeleton, tests, and independent lab map described in `VISION_SYSTEM_MIGRATION_PLAN.md`.

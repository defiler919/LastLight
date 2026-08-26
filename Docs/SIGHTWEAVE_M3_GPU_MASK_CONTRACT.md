# SightWeave M3 GPU world-space mask contract

Status: **FROZEN FOR M3 IMPLEMENTATION**

Architecture freeze: M3.0

Authority baseline: M2P.5 at `d98440f656a13a8ab396ba2bd93637c6d8e2b15c`

Engine reference: Unreal Engine 5.8.1, changelist `56057345`

## Scope

This document freezes the contract between the authoritative SightWeave CPU snapshot and the derived GPU live-vision mask. It does not implement shaders, render modules, materials, assets, maps, memory persistence, subject proxies, or DARKWELL integration.

The words **MUST**, **MUST NOT**, **SHOULD**, and **MAY** are normative. Labels mean:

- **Approved fact**: already required by the vision requirements or closed M1/M2/M2P work.
- **Frozen M3.0 decision**: selected by this architecture freeze and required of M3 implementation.
- **Proposed default**: an implementation starting value that remains tunable or needs measured confirmation.
- **User decision pending**: product or hardware input that M3.0 cannot truthfully settle.

## Non-negotiable authority boundary

**Approved facts**

1. CPU geometry, compatibility, source attribution, floor/height rules, and hard suppression remain gameplay and memory-write authority.
2. GPU resources are presentation-only derivatives. They MUST NOT feed gameplay queries, memory writes, save capture, subject state, AI, interaction, or HUD eligibility.
3. A softened or filtered pixel MUST NOT expand hard live coverage.
4. Legal illumination is explicit registered geometry, not rendered light, Scene Color, GBuffer luminance, exposure, shadows, emissive output, or a `ULightComponent` sample.
5. The permanent body-circle bypass source remains occluded, floor-bound, and suppression-bound, but never enters an illumination compatibility group.
6. M2P.5 CPU contracts and evidence remain unchanged. M3 work MUST NOT rename a CPU timing result, substitute wall time for intrinsic CPU time, or reopen closed CPU gates without new CPU production changes.

For one Knowledge Owner `o`, floor `f`, and immutable revision `r`, the hard presentation reference is:

```text
Gated[p] = VisionMask[p] AND CompatibleIlluminationMask[p]
PreSuppressionLive = OR over every complete compatibility profile p of Gated[p]
                     OR BypassVisionMask
EffectiveLiveMask = PreSuppressionLive AND NOT SuppressLiveVisionMask
```

An unqualified global vision/illumination intersection is forbidden.

## Frozen representation

**Frozen M3.0 decisions**

- The GPU consumes CPU-produced deterministic polygons and CPU-produced deterministic triangle indices. The GPU rasterizes and combines them; it does not solve visibility.
- The persistent output is a stable, floor-local, sparse/tiled world-space atlas. It is not camera-relative and not a monolithic map-sized render target.
- Persistent resources are partitioned by `(WorldInstance, KnowledgeOwnerId, FloorId)`. Different owners or floors never share logical mask contents.
- Compatibility intermediates are transient dirty-tile scratch resources. There is no persistent texture per source and no persistent full atlas per compatibility profile.
- The persistent hard effective-live mask is one linear single-channel UNorm resource, with `PF_G8`/R8 as the required reference format. Its baseline path needs SRV and render-target support; UAV support is optional and MUST NOT be assumed without an RHI capability gate.
- The renderer derives one final hard mask before presentation feathering. Debug builds may expose transient profile, bypass, suppression, and effective stages without making them persistent shipping allocations.
- Camera motion alone changes only world-to-screen sampling. It never dirties or regenerates world-space mask tiles.

## Immutable GT-to-RT packet

The Runtime module will expose a renderer-neutral immutable packet built from one published `FSightWeaveFrameSnapshot`. The packet is self-contained for every included `(owner, floor)` scope so the render queue may safely coalesce older pending revisions. It contains no `UObject*`, actor/component pointer, `TArrayView` into mutable state, raw pointer into Game Thread storage, callback into gameplay state, or RHI object.

The render packet is conceptually:

```text
FSightWeaveRenderPacket
  SchemaVersion
  SnapshotRevision
  WorldInstanceSerial
  ActivePresentedFloorId
  Scopes[]

FSightWeaveRenderScopePacket
  KnowledgeOwnerId
  FloorId
  FloorOriginXY
  FloorBoundsXY
  PrecisionTier
  WorldUnitsPerTexel
  TileInteriorTexels
  TileGutterTexels
  LogicalTileRange
  CanonicalProfiles[]
  Sources[]
  PolygonVertices[]
  TriangleIndices[]
  DirtySourceIds[]
  DirtyPolygonIds[]
  DirtyWorldBounds[]
  DirtyLogicalTiles[]
  FullRebuildReason
```

The exact C++ layout may pack arrays differently, but it MUST preserve the fields and semantics below.

| Field | Required semantics |
| --- | --- |
| `SchemaVersion` | Reject an unknown packet schema; never reinterpret it heuristically. |
| `SnapshotRevision` | Exact CPU snapshot revision from which every item in the packet was derived. |
| `WorldInstanceSerial` | Monotonic identity that prevents a packet from an old PIE/world lifetime entering a replacement world. Pointer value alone is insufficient. |
| `KnowledgeOwnerId`, `FloorId` | Required on every scope; sources cannot migrate between scopes implicitly. |
| `ActivePresentedFloorId` | Exactly one active/presented floor per local view in v1. Inactive floors may remain resident but cannot be composited. |
| `SourceId` | Stable generation-aware vision, illumination, bypass, or suppression source identity; source type is explicit. |
| `SourceRevision` | CPU revision for content comparison and diagnostics; it does not replace packet revision ordering. |
| `ProfileIndex` | Packet-local index into a canonical full accepted-capability sequence. Bypass uses no profile. |
| `PolygonId` | Stable within source generation and packet schema; carries source type and source identity. |
| vertices/indices | Owned, finite, floor-local `float2` data and valid triangle indices. No implicit fan triangulation on the Render Thread. |
| `VertexOffset/Count`, `IndexOffset/Count` | Checked ranges into owned packed arrays. Degenerate or out-of-range geometry invalidates the affected scope. |
| polygon bounds | Conservative finite floor-local XY bounds used for tile selection and dirty expansion. |
| world-to-texel | Derived only from floor origin, tier, tile coordinate, and world-units-per-texel as specified below. |
| dirty metadata | Hints derived from old and new CPU-owned state. They may narrow work, never narrow correctness. |
| full-rebuild reason | Explicit enum: initial, origin/bounds change, tier change, capacity/layout change, RHI recreation, unknown delta, or validation recovery. |

All arrays become immutable before enqueue. The render command captures an owning `TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>` or an equivalent move-owned immutable value. Enqueueing a pointer/reference to `USightWeaveWorldSubsystem` containers is forbidden.

## Coordinate and texel contract

Floor-local XY is the precision domain:

```text
LocalXY = WorldXY - FloorOriginXY
LogicalTexel = floor(LocalXY / WorldUnitsPerTexel)
LogicalTile = floor_div(LogicalTexel, TileInteriorTexels)
TileTexel = positive_mod(LogicalTexel, TileInteriorTexels) + TileGutterTexels
```

`floor_div` and `positive_mod` MUST be defined for negative coordinates; C++ truncation toward zero is not acceptable. The CPU reference and shader constants MUST use the same definitions.

Atlas slot placement is a residency detail, not identity. A logical tile key is `(WorldInstanceSerial, KnowledgeOwnerId, FloorId, PrecisionTier, TileX, TileY)`. Evicting or relocating a physical slot cannot change the world-to-texel result.

The floor origin and precision tier are immutable for a resident scope. A change requires a new revision, invalidates the whole scope, and cannot reuse old pixels. World-origin rebasing supplies an explicit origin change; silently baking large absolute coordinates into `float` UV math is forbidden.

## Polygon and boundary contract

- CPU polygon construction and compatibility remain authoritative.
- Packet polygons MUST be simple, finite, consistently wound, and deterministically triangulated on CPU. Empty polygons are valid no-coverage entries. Invalid non-empty geometry fails the affected scope closed.
- A hard texel represents its world-space texel-center sample. The reference result is the existing CPU inclusive point-in-polygon rule evaluated at that sample for the same revision.
- GPU triangle top-left fill rules alone are not a correctness oracle at polygon boundaries. M3 implementation MUST validate edge and vertex samples and document its conservative/inclusive treatment.
- Exact CPU-to-GPU agreement is required away from polygon boundaries. A boundary exception may cover only texels whose centers lie within `max(CpuBoundaryEpsilon, 0.5 * texelDiagonal)` of an edge; it cannot bridge an occluder or exceed one texel before presentation feathering.
- Hard atlas writes use point semantics. Linear filtering/feathering occurs only in the presentation sample and is bounded by the configured presentation radius.
- Tile gutters MUST be at least `ceil(PresentationFeatherTexels) + 1` texels. Gutter pixels are regenerated from neighboring logical coverage, not clamped from the tile edge.

## Complete compatibility profile identity

The CPU source of truth already removes `None`, sorts by `FName::LexicalLess`, and removes duplicates for both accepted and emitted capability arrays. M3 reuses that exact canonicalization.

A profile is the entire canonical `AcceptedCapabilities` sequence. A 64-bit/128-bit fingerprint MAY accelerate lookup but MUST NOT define equality. On fingerprint collision, canonical sequence equality decides. Profile indices are packet-local, sorted deterministically by canonical sequence, and have no persistence meaning.

For profile `p`, `CompatibleIlluminationMask[p]` unions every illumination polygon whose canonical emitted set contains at least one capability accepted by `p`, matching the CPU `Accepts` semantics. A source accepting `{Infrared, Visible}` is one complete profile and is satisfied by either capability. It is not split into two independent vision sources. An unrelated capability never satisfies it.

Proposed implementation limits are not semantic shortcuts. If the active profile cap is exceeded, the scope fails closed and reports capacity exhaustion; profiles are never merged by hash, first element, or “closest” capability.

## Revision, coalescing, and stale rejection

Render state tracks per world and scope:

```text
LastAcceptedRevision
DesiredRevision
AppliedRevision
ResourceGeneration
Status
FailureReason
```

Rules:

1. A packet from another `WorldInstanceSerial` is rejected.
2. `Revision < LastAcceptedRevision` is stale and rejected with a counter.
3. `Revision == LastAcceptedRevision` is a duplicate. Identical packet identity is a no-op; contradictory content is a validation failure and fails the scope closed.
4. A newer pending packet may replace older pending packets because every packet is self-contained.
5. Revision gaps are legal only through self-contained replacement. A delta that needs an absent base is rejected.
6. `DesiredRevision > AppliedRevision` means the affected scope is not eligible for normal sampling until all required dirty work for `DesiredRevision` is ordered in the same RDG graph before composition.
7. Partial atlas contents are never published. If a revision must span frames, the scope composites black until the entire revision is complete; no partially updated or stale mask is sampled.
8. A successful RDG ordering point promotes `AppliedRevision = DesiredRevision`. A later GPU/RHI failure invalidates the resource generation and returns the scope to fail-closed state.

Presentation lag is measured as CPU snapshot revision/time to `AppliedRevision`. **Proposed default:** at most 2 rendered frames in ordinary operation. The acceptance limit remains a user/product decision until frame-rate and minimum hardware are approved.

## Dirty invalidation contract

Dirty metadata is produced by comparison against the prior immutable packet/render mirror and MUST include old and new coverage:

| Change | Minimum dirty region |
| --- | --- |
| source added | new bounds plus gutter |
| source removed | old bounds plus gutter |
| transform/polygon/source revision changed | union of old and new bounds plus gutter |
| compatibility profile changed | old and new bounds, plus overlapping illumination-dependent tiles for old and new profiles |
| illumination changed | old/new illumination bounds intersected with tiles occupied by profiles it could satisfy; full overlapping scope if uncertain |
| bypass changed | old/new bypass bounds plus gutter; no compatibility layer |
| suppression changed | old/new suppression bounds plus gutter, applied after all live unions |
| dynamic door/occluder changed | old/new bounds of every CPU polygon actually rebuilt by that snapshot revision |
| camera moved/resolution changed | no mask raster work; composite only |
| floor origin/bounds/tier/layout/RHI generation changed | full scope rebuild |

Each dirty logical tile is cleared and redrawn from every self-contained contributor overlapping that tile. Incremental additive painting without removal/recomposition is forbidden. If old bounds, contributor lookup, or profile impact is uncertain, full scope rebuild is the only valid fallback.

A revision with no semantic or polygon change performs no tile raster/combine work. It may update diagnostics only.

## Resource lifecycle and failure contract

The following conditions return a black hard mask for the affected owner/floor and emit a stable reason/counter:

- no published packet or no active floor;
- NullRHI, unsupported feature level, unavailable global shader, or unsupported required pixel-format capabilities;
- invalid schema, world identity, owner/floor identity, transform, packed range, triangle, polygon, bounds, profile, or revision;
- atlas/tile/profile/source/polygon/vertex/index capacity exhaustion;
- allocation, device removal, RHI recreation, or resource generation mismatch;
- partial, stale, contradictory, or dependency-missing update;
- world teardown or Scene View Extension unavailable.

Failure MUST never substitute white, reuse a different owner/floor, sample an old world, silently drop contributors, or fall back to Scene Capture/rendered-light inference. CPU queries remain available even when rendering is unavailable.

On world teardown the Game Thread stops publication, invalidates `WorldInstanceSerial`, releases its view-extension reference, and enqueues Render Thread resource release. Late commands carry the old serial and are rejected. RHI loss/recreation invalidates persistent pooled targets; the next valid packet performs a full rebuild. Shutdown and hot reload MUST NOT synchronously dereference destroyed Game Thread state from the Render Thread.

## Debug and test readback

Full-frame GPU readback is forbidden in Shipping gameplay, normal presentation, memory writing, and save capture.

Development/test builds MAY request bounded asynchronous readback of named hard stages, atlas pages, or selected tiles through RDG `AddEnqueueCopyPass` and `FRHIGPUTextureReadback`. The request records world/owner/floor/revision/resource generation/tile coordinates. Results are accepted only after `IsReady`, correct row-pitch handling, and an exact metadata match; otherwise they are discarded. No Game Thread stall, `FlushRenderingCommands`, or blocking readback is permitted in the normal test loop.

Required inspectable counters include accepted/duplicate/stale/rejected packets, desired/applied revision, lag frames, full rebuild reason, dirty tiles, rasterized polygons/triangles, profile count, atlas pages/bytes, scratch high-water bytes, evictions, black-fallback frames, failure reason, and readback bytes/latency.

## Contract categories and open product decisions

| Item | Category | M3.0 outcome |
| --- | --- | --- |
| CPU authority and compatibility formula | Approved fact | unchanged |
| CPU polygons -> GPU rasterization | Frozen M3.0 decision | selected |
| sparse floor-local atlas | Frozen M3.0 decision | selected |
| one persistent R8 effective-live mask | Frozen M3.0 decision | selected |
| transient profile scratch | Frozen M3.0 decision | selected |
| one active/presented floor per view | Approved fact | unchanged |
| 2.5/5/10/25 cm comparison | Approved requirement | all four remain required; no shipping tier selected |
| minimum hardware | User decision pending | required before provisional GPU milliseconds become acceptance gates |
| map/floor extents | User decision pending | required for final residency caps |
| simultaneous Knowledge Owners | User decision pending | proposed defaults are documented in the architecture |
| resident inactive floors | User decision pending | v1 composition remains exactly one floor |
| maximum active compatibility profiles | Proposed default | must be validated at 1/4/8 and overflow |
| presentation lag limit | Proposed default | 2 frames until approved/measured |

## M3.0 exit statement

This contract is complete enough to start a rendering implementation without inventing authority, ownership, revision, compatibility, boundary, dirtying, lifecycle, or failure semantics. It does not claim GPU correctness or performance evidence; those belong to the M3 validation plan and later implementation evidence.

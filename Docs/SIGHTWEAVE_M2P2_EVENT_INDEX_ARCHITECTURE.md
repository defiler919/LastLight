# SightWeave M2P.2 prepared event index architecture

## Decision status

Status: **ACCEPTED FOR INCREMENTAL IMPLEMENTATION**.

The selected production direction is a bounded, world-owned, two-level prepared geometry index:

1. stable floor/segment metadata that is independent of any observer;
2. exact observer-origin entries containing source-independent endpoint preparation, canonical world-angle order, and angular interval preparation;
3. source-owned views and final snapshot entries that retain handle, revision, owner, capability, illumination policy, shape, cone, range, and attribution isolation.

This is a hybrid of the three investigated families. Floor metadata alone and final-polygon sharing are rejected as complete solutions. Unbounded per-source or origin caches are also rejected. The implementation must remain exact and may use an uncached/full-rebuild fallback whenever a bounded entry cannot be acquired or an incremental ordering budget is exceeded.

## Evidence used for the decision

The decision uses checked-in M2P.1 stage profiles and the corrected M2P.2 motion trace, not a synthetic estimate detached from the runtime pipeline.

The final M2P.1 4,096-segments/source profile measured these eight-source aggregate medians:

| Stage | Median us | Reusable at unchanged origin/geometry? |
|---|---:|---|
| candidate, height, endpoint preparation | 2,222.903 | yes |
| exact event sort/merge and direction preparation | 3,662.098 | yes |
| angular acceleration build | 1,127.899 | yes |
| active sweep, exact nearest intersection, output | 5,496.405 | no for a changed cone/range; yes for pure-radial rotation |
| local topology parity guard | 1,142.796 | no after new output; yes when the output is reused unchanged |
| total CPU | 13,656.195 | baseline |

Retaining the first three exact stages removes a measured 7,012.900 us from the eight-source aggregate. The residual measured work is about 6,643.295 us, or 830.412 us/source before index lookup and view materialization. This gives enough measured headroom to make the strict sub-1 ms median plausible. Floor metadata alone can remove at most the 2,222.903 us preparation stage; the residual is about 1,429.162 us/source and cannot meet the gate by itself.

The corrected M2P.2 trace further establishes:

- no-change is already a true no-publish hit: 0.201/0.298/0.399/5.800 us median/p95/p99/max, 0 rebuilds, and 39,592 retained target events observed across 101 samples;
- a 0.5-degree radial rotation still rebuilds 39,592 events and costs 39.298 us median even though radial visibility geometry is orientation-independent;
- 1/5/20 cm translations all perform 101 full target event rebuilds;
- four compatible-origin source updates perform 404 revisions and 404 independent source rebuilds, costing 76.097/157.502/180.699/232.700 us total and 44.301/88.401/92.898/97.003 us in exact target solve CPU;
- the small-scene cold source registration costs 141.598 us and produces a 76,680-byte published snapshot. Cold construction remains visible and must not be relabeled as warm work.

The current optimized cache is insufficient because its invariant key contains both origin and forward, it retains only prepared segment slots/candidates, and it still reconstructs endpoint events, exact ordering, directions, intervals, rays, and output on every solve. The subsystem also destroys both its candidate array and shared prepared-cache control block on every ordinary source update.

## Candidate comparison

| Candidate family | Complexity | Normal motion | 4,096/source | Cold cost | Warm memory | Invalidation | Correctness/lifecycle risk | Fab suitability | Decision |
|---|---|---|---|---|---|---|---|---|---|
| Floor/static-scene prepared metadata only | low | small saving | measured ceiling still about 1.43 ms/source | low | proportional to authoritative segments | floor/static revision | low | high | retain as level 1, reject as full solution |
| Per-source persistent exact event index | medium/high | strong for rotation; kinetic option for translation | measured residual about 0.83 ms/source | one full build/source | duplicates same-origin data | source plus geometry key | medium; lifecycle simple | acceptable but wasteful | retain source view, reject duplicated preparation as sole ownership |
| Shared observer-origin occlusion preparation | high | strongest for co-located player/light/camera groups | same measured stage reuse plus cross-source amortization | one full build/group | lowest when compatible; unsafe if unbounded | origin/floor/height/candidate/geometry revisions | highest key/isolation risk | acceptable only with hard limits and diagnostics | selected as bounded level 2 |

### Rejected complete designs

**Floor metadata only** is safe and useful, but the measured Amdahl ceiling misses the 1 ms gate. It remains the immutable input layer for the chosen index.

**One unbounded exact cache per source** avoids cross-source key risk but duplicates the dominant endpoint/order/interval data for co-located sources and grows directly with source count and high-water geometry. Source-owned final result buffers remain, but duplicated origin preparation is rejected.

**Shared final polygons or snapshot entries** are rejected. Different range, cone, near-awareness, capability, illumination policy, knowledge owner, source handle, source revision, and attribution require independent final values. Even two radial sources can have different range and candidate events. Only plain occlusion preparation may be shared.

**Approximate small-motion ordering** is rejected. No displacement epsilon may silently preserve an old order. The kinetic path must compute exact new keys, prove the exact order, or fall back to the established radix rebuild.

**A larger unbounded snapshot/publication cache** is rejected as an allocation workaround. Publication storage and prepared-event storage have separate bounds and ownership rules.

## Data model

### Level 1: prepared floor segment metadata

Each world subsystem owns floor-partitioned segment records keyed by stable segment ID. A record contains only exact/plain values derived without an observer: endpoints, vector, squared/actual length as required by the established intersection math, fraction epsilon inputs, height range, floor, dynamic/static identity, occluder handle, and stable tie-break ID.

This storage follows the authoritative occluder registry. It is not an extra historical cache, so its bound is the currently registered segment count. Static and dynamic records remain distinguishable. UObject references are forbidden.

### Level 2: bounded prepared origin entries

An origin entry retains:

- exact candidate stable-ID sequence and a collision-checked fingerprint;
- observer-relative offsets and distance lower bounds;
- absolute endpoint angles and deterministic endpoint identity;
- canonical sorted world-angle endpoint order;
- exact base directions where the current algorithm requires trigonometric construction;
- absolute angular intervals in deterministic start/stable-ID order;
- static/dynamic revision stamps;
- retained arrays and their allocated-byte accounting;
- entry ordinal, generation, last-used revision, hit/miss/rebuild counters, and bound source count.

Entries live in a pre-reserved slot array, not a `TMap<Key, TSharedPtr<...>>` created on every motion update. A slot generation prevents stale source bindings after reuse. Inner arrays reset without shrinking. An uncached exact solve uses the existing thread-local scratch if no cache slot can be safely reused.

### Level 3: source binding and final view

Every active source owns a small binding `(entry index, entry generation)` and its existing result/snapshot buffers. A source view selects/merges exact events for its shape, cone, near-awareness, and range, performs the active sweep when needed, and materializes its own polygon, candidate arrays, handle, revision, compatibility, and attribution.

The final snapshot never points into mutable prepared-entry arrays. Published arrays are value-owned by the immutable snapshot buffer. Reusing geometry means copying or regenerating into the new unpublished entry, never mutating a published reader.

## Exact keys and equality

Hashing is an accelerator only. A hit requires full field equality and exact stable-ID sequence equality.

`FPreparedOriginKey` contains:

- subsystem identity implicitly through world ownership; entries never cross worlds;
- floor ID;
- exact normalized origin XY bit patterns;
- exact normalized height range;
- geometry tolerance fingerprint;
- floor static geometry revision;
- floor dynamic geometry revision;
- candidate stable-ID sequence fingerprint plus full sequence equality;
- candidate segment value revision/fingerprint, so stable IDs with changed endpoints cannot hit.

Forward, shape, cone angle, range, near-awareness, owner, capability, and illumination policy do not alter the source-independent absolute endpoint preparation and are intentionally excluded from this level. Candidate discovery still uses the source's exact query bounds/range. Sources with different ranges share an origin entry only when their exact candidate stable-ID sequences and segment values are equal; otherwise they use separate entries while still sharing level-1 metadata.

`FPreparedSourceViewKey` contains:

- origin entry index and generation;
- source kind (vision or illumination);
- handle and source revision;
- shape;
- exact normalized forward bit patterns, except that pure-radial geometry marks forward as non-geometric;
- range, half-angle, and near-awareness;
- floor and height range repeated as a defensive invariant;
- knowledge owner;
- illumination policy;
- normalized accepted/emitted capability sequence fingerprint plus full equality.

Source views are not globally shared. Including semantic fields prevents a geometry hit from becoming an owner/capability/attribution leak.

## Motion strategies

### No change

The normalized description compares equal, so no revision, dirty state, solve, or publication occurs. This remains the fastest path and records a no-change hit separately from a prepared-index hit.

### Pure-radial rotation only

Forward does not affect radial world geometry. The source description and public source revision still advance. The prepared origin entry and exact world-space polygon can be reused. The unpublished source entry receives the new metadata while query acceleration retains a declared canonical world-angle representation. Tests must prove the point-query and debug representation across 180/360-degree wrap; orientation changes are never swallowed.

### Cone/camera/illumination rotation only

The origin entry remains valid because absolute endpoint order and absolute intervals do not change. The source view converts the cone boundaries to world angles, selects the exact circular endpoint ranges, merges the required boundary/arc events deterministically, and sweeps the cached absolute intervals. Large turns and the `-PI/PI` seam use the same circular selection; there is no incremental-angle assumption.

### Small translation

Candidate discovery runs with retained spatial-query scratch. If the exact stable-ID sequence changes, the path performs a full exact origin rebuild.

When candidates are unchanged, every observer-dependent value is recomputed exactly. The previous endpoint identities seed an insertion/kinetic sort. Each new IEEE/radix order key is compared, adjacent swaps are counted, and the resulting order is validated monotonically with the established deterministic tie fields. Seam changes are ordinary key changes. If the swap budget is exceeded, an identity is missing, an interval order is unsafe, or validation fails, the path deterministically invokes the full radix sort and increments `KineticFallbackCount`. No old angle or old nearest hit is retained.

The initial proposed small-motion distance is 25 cm, configurable and clamped. It selects an optimization attempt only; it never selects different correctness semantics.

### Large translation and teleport

A displacement above the configured small-motion attempt threshold, camera switch, or unavailable prior binding performs exact candidate discovery and full origin-entry build. A teleport may hit an already resident exact key, but is never assumed to do so.

### Range change

Candidate discovery is repeated. An identical candidate sequence may reuse the origin base, but the source view is rebuilt because maximum distance and boundary points change. A changed candidate sequence uses a different/full entry. A farther-range superset is never silently used as an exact shorter-range event list.

### Height or floor change

The old binding is released and a different key/full build is required. Exact height is part of the key. Floor change never shares an origin entry even if coordinates and segment IDs appear similar.

### Dynamic occluder change

Static metadata remains resident. Dynamic records receive deterministic per-floor revisions. Entries whose exact floor/height and query bounds intersect the union of old/new dynamic bounds are invalidated in entry-ordinal order. The changed dynamic prepared records are recomputed and merged with the static base. If exact candidate membership or merge invariants differ, the view performs a full rebuild. All affected sources rebuild synchronously before `PublishSnapshot`; no old-snapshot window is introduced.

### Disable, delete, and unregister

Inactive/deleted sources release their binding before publication. Removing one binding cannot mutate a shared entry used by another source. Removing an occluder advances the corresponding geometry revision and follows the same affected-entry invalidation path as movement. Disabled sources retain no required prepared entry.

## Rotation and cone correctness notes

Endpoint order around a fixed origin is invariant under source rotation when represented in absolute world angles. The current cache misses this fact because it subtracts the forward angle into each prepared segment and keys on forward.

A cone cannot generally reuse a radial final polygon by clipping its vertices. Cone arc samples and exact cone-boundary `-epsilon/zero/+epsilon` events can occur at angles absent from the radial result, and range/near-awareness semantics differ. The safe reuse unit is the absolute endpoint/interval preparation. The cone view must generate its own event merge, exact hits, output, and topology guard.

Different ranges may share nearest-hit *preparation* only for an identical candidate sequence. They cannot blindly share final nearest distances: a longer source can hit geometry beyond the shorter source's range, and extra far endpoint events alter the exact candidate-ray contract even when the geometric boundary appears equivalent.

## Invalidation table

| Change | Floor metadata | Origin entry | Source view/final output |
|---|---|---|---|
| identical normalized update | retain | retain | retain; no revision |
| radial forward only | retain | hit | metadata revision; radial geometry reuse |
| cone forward only | retain | hit | exact recut/sweep |
| small translation, candidates/order safe | retain | exact in-place rebuild/kinetic reorder | rebuild |
| translation candidate change or kinetic fallback | retain | full rebuild | rebuild |
| teleport | retain | lookup or full rebuild | rebuild |
| range | retain | hit only after exact candidate equality | rebuild |
| height/floor | retain | miss | rebuild |
| static segment add/update/remove | update affected floor | invalidate affected floor keys | synchronous rebuild |
| dynamic segment add/update/remove | update dynamic record/revision | invalidate overlapping keys | synchronous rebuild |
| tolerance change | rebuild metadata-dependent values | miss/purge | rebuild |
| source unregister | retain | unbind/eligible for eviction | remove |
| floor unload | purge floor | purge floor entries in ordinal order | remove/disable affected outputs |
| world teardown | destroy all | destroy all | destroy all |

## Bounds, eviction, and high-water behavior

Initial production limits are explicit settings, subject to benchmark tuning before finalization:

- maximum prepared origin entries: 32, clamped to `[1, 1024]`;
- maximum retained prepared-origin bytes: 64 MiB, clamped to `[1 MiB, 1 GiB]`;
- small-translation kinetic swap budget: bounded per entry and never allowed to grow scratch;
- per-source view state: at most one retained view/binding per registered source;
- floor metadata: exactly current registered geometry, with no historical generations.

Insertion first reclaims invalid and unbound slots. Then it evicts the lowest `LastUsedRevision`; ties use entry ordinal. Bound entries are not mutated or evicted. If every slot is bound or one entry alone exceeds the byte cap, the request runs the exact uncached solver and records `CapacityFallbackCount`/`OversizedEntryCount`. Authority never waits for, skips, or reuses stale geometry.

Byte accounting includes every retained inner-array capacity, not only live element bytes. Reclamation occurs on insertion pressure, source/occluder deletion, floor unload, settings reinitialization, and world teardown. Revision-based idle reclamation may be added only with deterministic ordering. High-water bytes, live bytes, slots, evictions, and fallback counts are observable.

The fixed slot array is pre-reserved. In-place rebuilds retain capacity. This is required for warmed motion allocation proof; a hash map or shared-pointer control block must not be created per transform.

## Snapshot and reader safety

Prepared entries contain plain mutable game-thread data and are never published. Final snapshot entries own their arrays. Publication writes only to an unpublished reusable buffer, atomically swaps it, and does not modify the previous shared snapshot.

The production API currently returns snapshots by value; runtime queries hold the internal shared pointer only during a synchronous call. The non-Shipping testing hook can deliberately hold an internal immutable snapshot. Tests must hold at least one old reader across rotation, translation, dynamic invalidation, source deletion, and eviction and compare all old fields byte/value-wise after new revisions.

If a held reader prevents standby reuse, allocating or selecting another publication buffer is permitted outside the strict ordinary warm-transform allocation case; mutating the held buffer is never permitted. Publication-buffer policy must remain bounded by production-access semantics and must be reported separately from the prepared-index bound.

## Lifecycle and concurrency

- The index is an instance member of `USightWeaveWorldSubsystem`; no static world cache exists.
- Source/occluder/floor bindings use stable IDs plus slot generations, never raw pointers into reallocatable arrays.
- World teardown clears source bindings, index slots, floor metadata, scratch, and published buffers in that order.
- Multiworld tests must prove identical IDs and coordinates do not share counters, entries, or memory.
- Source mutation and index writes remain on the game thread. Solver scratch remains thread-local for concurrent independent solves. A future worker path must lease immutable prepared data or copy a versioned view; this design does not introduce worker mutation.
- Deterministic traces compare hit/miss/fallback/eviction sequences as well as polygon/query results.

## Reference and Shipping behavior

Reference remains unchanged. In non-Shipping `Verify`, the optimized cached result is compared with the established Reference result using the existing exact contract and tolerances. A mismatch records the cache key, motion strategy, stable candidate IDs, revision stamps, fallback path, and seed, then fails; it never changes Reference or widens epsilon.

Shipping continues to compile and execute Optimized only. Prepared-index runtime types belong to `SightWeaveRuntime`, hold no Tests/Editor dependency, and use no editor-only automation. Debug counters may be compiled in Shipping when they are plain numeric state, but Reference verification, detailed mismatch capture, and test hooks remain non-Shipping.

## Diagnostics for Fab content

The subsystem exposes aggregate prepared-index stats through normal debug data: hits, misses, radial geometry reuses, cone recuts, kinetic attempts/swaps/fallbacks, full rebuilds, dynamic invalidations, capacity/oversized fallbacks, evictions, live/high-water bytes, and entry/source-binding counts.

Configuration normalization emits one rate-limited world diagnostic for invalid entry/byte/swap limits. Repeated capacity fallback, oversized single entries, or high eviction/miss ratios produce rate-limited actionable warnings naming the floor, candidate count, requested/allowed bytes, and relevant setting. Diagnostics never log per-frame success and never disable strict synchronous updates.

## Implementation sequence and keep/revert gates

1. Add allocation attribution for motion workloads before changing ownership.
2. Add transform-specific mutation APIs and retained spatial-query/publication storage; require 0/0/0 warmed allocation/reallocation/bytes and p99 at or below 0.10 ms.
3. Refactor prepared segment angles to absolute world representation and add per-source view keys without changing output. Run all differential seeds.
4. Retain exact endpoint order and intervals at unchanged origin. Keep only if 4,096/source median falls below 1 ms, p99 remains below 2 ms, and normal 8x64/4,096-total gates do not regress.
5. Add radial rotation reuse and cone recut independently, each behind exact counters and differential tests.
6. Add bounded shared-origin slots. Keep only if the four-source trace reduces repeated preparation and all range/height/floor/owner/capability isolation tests pass.
7. Add kinetic translation as a separate iteration. Keep only if exact crossing/fallback tests pass and warmed translation improves without allocation regression.
8. Add static/dynamic revision partition and local invalidation. Strict dynamic-door authority and latency remain hard gates.

Any failed iteration is reverted or left disabled with its negative result documented. Performance acceptance never substitutes worker wall time for source CPU and never hides cold/full-rebuild cost.

## Remaining proof obligations

- startup allocation callstacks for each motion class;
- measured retained bytes for 64, 512, and 4,096 candidate entries;
- cold, rotation, translation, teleport, and dynamic-invalidation distributions after implementation;
- endpoint-order swap and `-PI/PI` seam differential proof;
- two-range/two-cone shared-origin behavior;
- multiple dynamic doors and local invalidation;
- capacity fallback, deterministic eviction, high-water reclaim, source/occluder/floor deletion;
- held reader, restart, multiworld, teardown, and concurrent scratch/index tests;
- BuildPlugin, clean host, Game Development/Shipping, and runtime dependency isolation.

Until those obligations pass, this document authorizes the implementation direction but does not claim M2P.2 completion.

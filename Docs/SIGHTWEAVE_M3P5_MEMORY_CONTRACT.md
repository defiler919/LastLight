# SightWeave M3.5 exploration-memory contract

Status: **FROZEN / PRECISION EXPERIMENT COMPLETE**

Branch: `codex/m3p5-sightweave-static-environment-memory`

Frozen baseline: `22f55b1e744cea37ad5d3c7beb618be0509fbf99`

## 1. Authority and three-state ordering

`HardLive` is the existing current-frame CPU EffectiveLive result. It is binary, world-space, source-compatible, suppression-aware, and derived from an immutable CPU snapshot. The hard GPU atlas is only a point-sampled mirror. M3.4 inward feather is presentation-only and can only reduce Scene Color after the HardLive gate.

`HardMemory` is the binary record that a world texel has previously passed CPU EffectiveLive and the memory-write gates. CPU packed tiles are its only authority. Bits are monotonic between explicit `ClearMemory` mutations. GPU pages are disposable derived mirrors; gameplay, save, restore, and authority never use GPU readback.

`StaticEnvironmentEligibility` is independent of HardMemory. It identifies explicitly authored immutable environment attributes that may be presented in the remembered branch. It is not inferred from actor name, object color, mobility observed at runtime, velocity, Scene Color, current GBuffer appearance, or a hash. Missing, unsupported, or uncertain eligibility fails black.

The final ordering is exact:

```text
HardLive == 1:
    SceneColor * M3.4InwardFeather
else HardMemory == 1
     and StaticEnvironmentEligibility == 1
     and MemoryPresentationSuppressed == 0:
    StableNeutralEnvironmentAttribute
else:
    float4(0, 0, 0, 0)
```

HardLive always wins. Memory never fills or alters the live branch. Unknown RGB is byte-zero. If the Memory mirror or eligibility mirror is invalid, only the valid M3.4 live branch may render; all non-live pixels are black. If the base world/presentation scope is invalid, the entire view follows the existing fail-black contract.

## 2. Scope, identity, and revision contract

Every CPU tile, immutable packet, GPU page-table entry, presentation binding, modifier, and eligibility record carries or is resolved against all applicable fields below:

- world lifetime identity (`FSightWeaveRenderWorldIdentity::Serial`);
- explicit world generation, advanced on initialization/restart and never reused by delayed commands;
- Knowledge Owner ID;
- Floor ID and floor-local origin/generation;
- the sorted full sequence of canonical compatibility-profile identities, comparing every capability name rather than only `StableHash`;
- memory precision tier and exact centimeters-per-texel;
- CPU memory revision;
- published CPU snapshot revision;
- memory packet revision;
- Memory atlas resource generation;
- Memory residency generation;
- presentation revision;
- StaticEnvironmentEligibility revision and attribute generation.

Stable hashes accelerate lookup only. Scope equality always compares the complete canonical profile sequence and every non-hash field. A forced collision must not alias memory, modifiers, eligibility, residency, or presentation.

Stale or mismatched world, generation, owner, floor, origin, profile sequence, precision, revision, atlas generation, residency generation, presentation revision, or eligibility revision is rejected without mutating either CPU authority or GPU resources.

## 3. Packed CPU Memory Tiles

The authoritative logical tile has a `248 x 248` interior matching the frozen sparse-atlas logical layout. There is no authoritative gutter. Rows are tightly packed little-bit-first along +X:

```text
rowBytes = 248 / 8 = 31
packedBytesPerTile = 31 * 248 = 7,688
bit(x, y) = bytes[y * 31 + (x >> 3)] & (1 << (x & 7))
```

The tile key contains the complete memory scope plus signed 32-bit logical X/Y. Mapping uses double-precision floor arithmetic:

```text
worldTexel = floor((WorldXY - FloorOrigin) / CentimetersPerTexel)
logicalTile = floor_div(worldTexel, 248)
interiorTexel = worldTexel - logicalTile * 248       // always 0..247
```

This definition covers negative coordinates without truncation toward zero. Inputs that cannot map to signed 32-bit logical tiles fail explicitly. Sorting is scope fields, logical X, then logical Y; deterministic replay of the same normalized snapshot/modifier sequence produces byte-identical tiles, revisions, dirty keys, and packet payloads.

Tiles are allocated only when at least one bit becomes one. Repeated writes to set bits do not allocate, dirty, or advance revisions. Empty tiles produced by ClearMemory are removed. Capacity failure is explicit and fails the affected memory scope closed; it cannot silently drop writes.

## 4. CPU write and publication rules

Memory update consumes the immutable published CPU visibility snapshot, never a camera or render callback. Candidate tiles come from changed effective-live source polygons plus changed modifier bounds. Per logical texel the packed effective result is built from complete compatibility groups:

```text
EffectiveLive = Union_p(Vision[p] AND CompatibleIllumination[p])
                OR BypassVision
EffectiveLive = EffectiveLive AND NOT SuppressLiveVision
WritableLive = EffectiveLive AND NOT BlockMemoryWrites
HardMemory |= WritableLive
```

The implementation may use deterministic packed scan conversion and word-wise boolean operations, but must match CPU point-query semantics at texel centers and the existing documented boundary tolerance. VisualFeather, screen visibility, Scene Color, GBuffer values, lighting, shadows, post-process state, camera position/rotation/FOV/OrthoWidth, viewport dimensions, resolution scale, and temporal history are absent from the write input.

Therefore active offscreen and remote sources write memory, and memory can update with no active camera or viewport. Camera and viewport changes schedule zero CPU memory work and zero Memory-atlas work when the authoritative snapshot and modifiers are unchanged.

One update transaction applies deterministic mutations, source writes, tile removal, dirty tracking, and revision advancement on the game thread. A background or render command cannot mutate CPU tiles. The memory revision advances once only when authoritative bytes actually change. No-change publication may advance snapshot provenance in a packet but cannot allocate, expand, dirty, upload, or advance memory revision.

The published `FSightWeaveMemoryPacket` is an owned, immutable, thread-safe value containing the complete identity, memory/snapshot/packet revisions, sorted dirty tile payloads, sorted removed tile keys, presentation-suppression data needed by the mirror, and diagnostic byte/count data. Replacing a pending packet cannot discard unapplied work: the replacement is self-contained or explicitly forces a full memory-mirror rebuild.

## 5. Modifier semantics and ordering

All modifier shapes are normalized into immutable world-space circle, axis-aligned box, rotated box, or polygon footprints with Floor ID and inclusive height range. Degenerate, nonfinite, self-intersecting, unsupported, or scope-mismatched shapes fail validation. Stable handles and canonical shape data determine ordering; Actor traversal order is never semantic.

- `ClearMemory` is an explicit authoritative mutation. It clears every intersecting HardMemory bit and removes empty tiles. Re-exploration can set the bits again unless a write blocker remains.
- `BlockMemoryWrites` is the union of active blockers. It prevents new bits but does not clear existing authority. In accordance with the requirements contract, an active blocker also suppresses remembered presentation in its region; live presentation remains normal.
- `SuppressMemoryPresentation` preserves HardMemory and only hides the remembered branch. Removing it reveals existing memory again.
- `SuppressLiveVision` remains the final hard-live suppression and therefore also prevents new memory writes.

Within one transaction, explicit ClearMemory is applied first, then current EffectiveLive writes are evaluated through the unioned write blockers. This makes simultaneous clear plus block deterministic: the region is cleared and cannot be reacquired until unblocked. Presentation suppression is the union of active BlockMemoryWrites and SuppressMemoryPresentation regions. Modifier movement dirties the union of old and new covered tiles; teardown dirties the old bounds and removes all effect. Owner, floor, height, world, profile, and precision mismatch contributes nothing.

## 6. Persistent sparse GPU Memory Mirror

Memory uses a distinct persistent sparse R8 resource family. Logical tiles and physical slots remain separate. Physical pages are `2048 x 2048`, physical slots are `256 x 256`, interiors are `248 x 248`, and gutters are four texels. Signed logical adjacency is resolved through the sorted Memory page table; adjacent physical slots are never assumed to be adjacent in world space.

Each dirty CPU packed tile is expanded into hard R8 interior/gutter data and incrementally uploaded. Gutter texels sample the corresponding neighboring logical CPU bit or zero when that logical neighbor is absent. Four-tile corners use the diagonal logical neighbor. Removed, evicted, or reused slots are black-cleared before they can be rebound. No-change packets perform no raster, upload, page-table upload, allocation, reallocation, or generation advance.

Residency accepts only a packet whose full identity and revisions match the active CPU publication. Any stale revision, missing/incomplete dirty payload, missing page table/page, resource failure, generation mismatch, capacity overflow, slot-reuse hazard, teardown, or NullRHI resource prohibition invalidates Memory presentation. Old Memory pages are never substituted. NullRHI retains CPU authority and tests but creates no RHI resources.

At the frozen 128-tile ceiling, one PF_G8 Memory family is at most `128 * 256 * 256 = 8,388,608` bytes plus a small page table. Static attributes are separately budgeted. The existing measured M3.4 persistent live presentation (`18,697,216` bytes maximum) remains under its independent 32 MiB cap; total SightWeave runtime persistent CPU plus GPU memory must remain at or below 64 MiB.

## 7. Static-environment route and leakage boundary

The production route is an explicit authored static-environment eligibility plus derived neutral environment-attribute atlas. Eligible components must be explicitly classified immutable and provide stable 2.5D footprint/material attributes. The derived attribute buffer contains only fixed neutral intensity/material cues and never samples current rendered lighting, shadow, reflection, emissive animation, WPO, decal/VFX state, destruction state, or dynamic subject state.

Remembered presentation reconstructs the active floor-plane position from the view ray and samples this world-stable attribute atlas, rather than using the current dynamic object's SceneDepth as remembered geometry. This prevents a moving enemy, mesh, door pose, particle, or pickup from becoming a remembered silhouette or occluder. Unsupported translucent, unlit, WPO, animated, dynamic-decal, particle, skeletal, movable, destructible, and stateful materials/objects contribute no eligibility and therefore fail black outside HardLive.

Routes compared and rejected:

| Route | Decision |
| --- | --- |
| post-process GBuffer BaseColor/stable material cues | rejected as the sole route: current surface/depth can expose dynamic occlusion/state; only future explicitly classified static cues could supplement the authored attribute path |
| explicit static-environment mask | required for eligibility, but insufficient alone for detail |
| static proxy/simplified material | viable future extension; not required when the derived attribute atlas supplies the fixed neutral cue |
| derived environment attribute buffer | selected; world-stable, explicit, camera-independent data with no current lighting/state input |
| Scene Color grayscale | rejected: leaks current lighting, shadow, reflection, VFX, and dynamic objects |
| frozen screenshot or SceneCapture | prohibited and rejected: temporal/current-state capture, second visibility source, cost, and lifecycle leakage |

## 8. Precision experiment contract

The comparison runs 2.5, 5, 10, and 25 cm/texel independently. Every tier uses the same `24.8 m x 24.8 m` floor-local test region, source definitions, modifier geometry, deterministic update order, timing warmup, exploration path, static attribute fixtures, and screenshot cameras. The region contains a 5 cm wall cue, 25/50/100 cm wall/corridor bands, straight/diagonal seams, a four-tile corner at each tier's logical grid, negative coordinates, ClearMemory, write block, and presentation suppression.

For each tier the experiment records:

- allocated CPU tile count and exact packed bytes;
- resident GPU tile/page count and exact mirror/attribute/page-table bytes;
- dirty and expanded-dirty tile counts and upload bytes;
- GT packed write and complete authority-update p50/p95/max;
- RT packet consume/page-table/upload p50/p95/max;
- GPU mirror update p50/p95/max;
- maximum boundary error in centimeters, horizontal/vertical/diagonal and four-corner seams;
- preservation of the small wall and narrow corridors;
- ClearMemory boundary/re-exploration behavior;
- deterministic packed snapshot-size estimate only (raw and a named test compression estimate), without implementing persistence;
- Lab screenshots inspected as automated captures and directly by the agent.

The raw full-region upper-bound estimate below excludes sparse-map/container overhead and assumes all touched logical tiles are allocated. Actual experiment values replace estimates:

| Precision | Interior span | Tiles across 24.8 m | Packed CPU upper bound | R8 Memory mirror upper bound | R8 static attribute upper bound |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 2.5 cm | 6.2 m | 4 x 4 = 16 | 123,008 B | 1,048,576 B | 1,048,576 B |
| 5 cm | 12.4 m | 2 x 2 = 4 | 30,752 B | 262,144 B | 262,144 B |
| 10 cm | 24.8 m | 1 x 1 = 1 | 7,688 B | 65,536 B | 65,536 B |
| 25 cm | 62.0 m | 1 x 1 = 1 | 7,688 B | 65,536 B | 65,536 B |

Timing samples are separated into cold creation, warmed dirty update, warmed no-change, and teardown/restart. Reference dirty-update acceptance is CPU memory p95 below 0.25 ms and GPU memory p95 below 0.25 ms. Warmed no-change must upload/raster/allocate zero times. Total plugin runtime memory must be at most 64 MiB and the existing live presentation cap remains 32 MiB.

Selection is mechanical:

1. Reject a tier that fails any authority, modifier, seam, leakage, lifecycle, deterministic replay, NullRHI, D3D12/SM6, or packaging test.
2. Reject a tier that misses either 0.25 ms memory-update p95 or the 64 MiB total budget.
3. Among remaining tiers, choose the numerically smallest centimeters-per-texel value (highest world precision).
4. If no tier passes, M3.5 is `PARTIAL`; thresholds are not weakened and no untested lower precision is substituted.
5. If evidence cannot objectively distinguish passing tiers, record a recommendation and leave the production default unset.

The experiment completed on the recorded RTX 4060 machine without changing any threshold. `Coarse` (25 cm/texel) is the production Memory precision default. It was the only tier that repeatedly passed every frozen CPU, GT, RT, GPU, and memory gate in the combined four-tier run. The existing M3 live-mask Standard tier remains unchanged and was not silently reused as the Memory choice.

## 9. Frozen precision decision

The final combined run used 8 declared GPU/RT warmup updates followed by 56 warmed GPU samples, 63 warmed CPU dirty samples excluding cold index zero, and 16 no-change samples per tier. Candidate benchmarks remain successful automation entries because their purpose is to record selection data; only the selected production tier is a regression budget gate. `eligible_this_run` and each individual gate are emitted in the report so rejection cannot be hidden.

| Tier | CPU packed | Tiles | CPU dirty p95 | GT packet p95 | RT total p95 | GPU dirty p95 | Boundary bound | Eligible |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Ultra 2.5 cm | 123,008 B | 16 | 1,391.299 us | 26.498 us | 742.000 us | 246.000 us | 1.25 cm | no: CPU and RT |
| Fine 5 cm | 30,752 B | 4 | 458.401 us | 5.398 us | 351.101 us | 249.000 us | 2.50 cm | no: CPU and RT |
| Standard 10 cm | 7,688 B | 1 | 82.098 us | 2.399 us | 71.898 us | 335.000 us | 5.00 cm | no: GPU |
| **Coarse 25 cm** | **7,688 B** | **1** | **73.601 us** | **2.302 us** | **97.401 us** | **33.000 us** | **12.50 cm** | **yes / selected** |

All tiers produced zero warmed no-change mirror work. The selected Coarse run also passed independently with CPU dirty p95 63.300 us, RT total p95 79.699 us, GPU dirty p95 43.000 us, and 36,462,592 B frozen worst-case plugin runtime memory. The full results, scale data, cold values, and evidence paths are recorded in `SIGHTWEAVE_M3P5_PERFORMANCE.md`.

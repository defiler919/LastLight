# SightWeave M3 GPU mask validation plan

Status: **FROZEN PLAN; NO M3 GPU IMPLEMENTATION EVIDENCE YET**

This plan defines the evidence required after the M3 world-space mask is implemented. It does not claim that a shader, render module, test map, GPU timing, or package result currently exists.

Normative behavior comes from `SIGHTWEAVE_M3_GPU_MASK_CONTRACT.md`; topology and proposed limits come from `SIGHTWEAVE_M3_GPU_MASK_ARCHITECTURE.md`.

## Evidence rules

1. CPU and GPU comparisons use the same immutable snapshot revision, World instance serial, Knowledge Owner, floor, precision tier, and hard-mask stage.
2. CPU hard point/polygon queries are the oracle. A screenshot, softened edge, Scene Color, wall time, draw-call count, or shader clock estimate cannot replace the oracle.
3. GPU comparisons use asynchronous selected-tile/stage readback in Development/Editor. Shipping contains no pixel-readback path.
4. Every result records engine changelist, full repository SHA, dirty state, machine/GPU/driver, RHI, feature level/shader model, build configuration, screen resolution, screen percentage, VSync/frame cap, precision tier, owner/floor/profile/source/tile counts, packet/applied revision, resource generation, and test seed.
5. Every timed distribution reports sample count plus p50/p95/p99/max. Average-only and best-run-only evidence is invalid.
6. Failed, slow, unsupported, lost, stale, and crash runs are retained. Gates are not split, renamed, or relaxed to discard failures.
7. A GPU or driver failure is not classified as plugin CPU. CPU wall time/raw cycles are not GPU milliseconds, and GPU event duration is not intrinsic CPU time.
8. Any unknown ownership/revision, missing marker, incomplete readback, profiler loss, or contradictory result fails the affected claim closed.
9. GPU validation does not reopen M2P.5. If M3 changes CPU Runtime production code, only affected CPU regression/allocation/performance evidence is rerun; unchanged closed M2 evidence remains the baseline.

## Required artifact layout

Later implementation runs SHOULD write immutable evidence under:

```text
Saved/SightWeaveM3/
  Build/<timestamp>/
  Automation/<timestamp>/
  CpuGpuDiff/<timestamp>/
  Lifecycle/<timestamp>/
  Precision/<timestamp>/
  Performance/<timestamp>/
  Soak/<timestamp>/
  Packaging/<timestamp>/
  CleanHost/<timestamp>/
```

Each root contains a manifest with command line, environment, SHA-256 for produced CSV/JSON/readback files, start/end time, process exit code, discovered/pass/fail counts, and an explicit `COMPLETE`, `PARTIAL`, `BLOCKED`, or `UNSUPPORTED` verdict. `Saved` artifacts remain untracked.

## Layer 1: packet and pure contract automation

These tests run without relying on a rendered frame where possible.

| Area | Cases | Pass condition |
| --- | --- | --- |
| canonical profiles | empty/None removal, duplicate removal, lexical order, `{Visible}`, `{Infrared}`, `{Infrared, Visible}`, hash collision fixture | exact canonical sequence matches Runtime semantics; hash never decides equality |
| packet ownership | source snapshot released/recycled after enqueue, worker completion after newer revision, world teardown during build | packet arrays remain valid/immutable; old work discarded |
| packed ranges | zero polygons, max valid offsets/counts, overflow, invalid triangle index, NaN/Inf, degenerate triangle, wrong winding | valid packets deterministic; invalid scope fails black with reason |
| coordinates | positive/negative XY, origin exactly on tile boundary, large floor origin, floor-div/mod reference | CPU and shader constants produce identical logical tile/texel keys |
| dirty comparison | add/delete/move/profile change/bypass/suppression/old-new bounds/no-change | exact expected dirty tile set or conservative full rebuild; never under-dirty |
| revision state machine | first, duplicate-identical, duplicate-contradictory, older, gap with self-contained packet, gap delta, resource generation change | contract-defined accept/no-op/reject/rebuild result and counter |
| capacity | every cap at `limit-1`, `limit`, `limit+1`; byte-budget overflow | no truncation/merge; over-limit scope black and diagnostic |
| determinism | repeated packet build from identical snapshot, different source registration order with same canonical state | byte-stable canonical arrays/keys/dirty result where schema promises ordering |

Required runs: Debug/Development automation where supported, repeated in at least three fresh processes. Packet tests also run under NullRHI to prove no renderer dependency enters CPU authority.

## Layer 2: hard CPU/GPU differential matrix

### Sampling rule

For each selected logical tile, read back the hard stage before presentation filtering. For every interior texel, evaluate the CPU oracle at that texel's world-space center and compare bits.

- Away from an edge: exact equality is required.
- Within `max(CpuBoundaryEpsilon, 0.5 * texelDiagonal)` of a polygon edge: differences are counted separately as boundary-tolerance samples and may occupy at most one hard texel.
- No accepted boundary difference may cross to the far side of an occluder, join disconnected regions, leak into another floor/owner/profile, or exceed the declared tolerance.
- Gutter texels compare against the neighboring logical world samples, not against clamped interior pixels.
- Point-on-edge and point-on-vertex fixtures additionally require the implementation's documented inclusive treatment; generic raster top-left behavior is not accepted without the reference comparison.

Every report contains total/exact/boundary/mismatch counts, maximum world-space error, mismatching coordinates, contributing CPU source/light/bypass/suppression handles, profile, revision, and a visual diff image for diagnosis. Any non-boundary mismatch fails.

### Geometry matrix

Run every row at 25, 10, 5, and 2.5 cm/texel and at logical tiles around `(0,0)`, negative coordinates, and physical atlas slot/page boundaries.

| Fixture | Required assertions |
| --- | --- |
| empty and single triangle | black empty; triangle interior, all edges, and all vertices follow the hard reference |
| radial/cone polygons | center, range edge, cone edges, near-awareness circle, outside samples |
| straight wall | no coverage behind wall; vertices stay on CPU line within tier tolerance |
| diagonal wall | no stair-step leak beyond allowed boundary texel |
| inside/outside corner | no crack or bridge at shared endpoint |
| L/T junctions | no endpoint crack, false wedge, or branch-side leakage |
| closed/opening/open door | old pixels removed, new opening appears only at applied revision |
| cone around `-PI/+PI` | ordered polygon/raster is continuous across the angular seam |
| overlapping polygons | boolean union is idempotent and independent of source ordering |
| thin wedge/sliver/collinear/duplicate inputs | either valid bounded coverage or explicit CPU packet rejection; never undefined pixels |
| tile seam and four-tile corner | interior and gutter comparison is continuous under camera motion |
| height-band exclusion | polygons/sources rejected by CPU floor/height rules produce no GPU contribution |
| floor origin/rebase | old generation not sampled; rebuilt mask maps identically in rebased local coordinates |

### Compatibility matrix

Use overlapping polygons and explicit sample bands:

| Case | Profiles | Illumination | Expected hard result |
| --- | --- | --- | --- |
| A visible isolation | Source A `{Visible}` | Infrared only | A remains black |
| B infrared isolation | Source B `{Infrared}` | Infrared only | B becomes live |
| A+B overlap | 2 complete profiles | Infrared only | only B contribution appears |
| C multi-capability | Source C `{Infrared, Visible}` | either one independently | C appears for either accepted type |
| unrelated channel | C as above | Ultraviolet only | C remains black |
| illumination only | no vision source | any legal illumination | no live pixels |
| gated vision without compatible light | gated source | none or incompatible only | no live pixels |
| emitted multi-capability | A and B | one light emits both | both matching profiles appear |
| same set, different order/duplicates | equivalent C sources | either accepted light | one canonical profile, identical result |
| forced fingerprint collision | two unequal canonical sets | matching one only | no cross-talk; sequence comparison separates them |
| bypass body circle | no profiles | no illumination | bypass coverage live, still wall/floor/suppression constrained |
| suppression overlap | any live case | matching light | suppression is applied last and removes hard coverage |
| Subject Reveal Override | any ordinary hard state | independent moving subject reveal | world mask is byte-identical and no environment pixel is added |

Run active complete-profile counts `1`, `4`, and `8`, with the same source polygons shuffled across at least three deterministic registration orders. Run `32` as the proposed hard ceiling and `33` as fail-closed overflow; the 32-profile case is a capacity stress, not the reference performance workload.

### Owner and floor isolation

| Configuration | Required result |
| --- | --- |
| same floor, owners A/B with disjoint knowledge | each view/readback sees only its requested owner; unsupported second-owner composition is black, not owner A |
| owner reassignment | old owner tiles cleared/invalidated and new owner scope rebuilt at new revision |
| floors A/B with overlapping XY | only active floor appears; inactive floor never leaks |
| active floor transition | old scope stops composition at switch; new scope is black until fully applied |
| world A/world B PIE with identical IDs | World serial prevents any cross-world packet/resource/readback acceptance |

The approved v1 product path still presents one floor. Multi-owner and resident transition cases validate isolation/failure behavior, not a commitment to render them simultaneously.

## Layer 3: dirty-update and revision matrix

For each case capture accepted/applied revision, dirty logical tiles, rasterized polygons/triangles, bytes uploaded, full-rebuild reason, and a before/after hard readback.

| Mutation | Expected work |
| --- | --- |
| camera lateral move, rotation, FOV change | zero packet/tile raster when CPU revision is unchanged; composite only |
| resize 1080p -> 1440p -> windowed -> 1080p | zero world-mask raster; result samples same world coverage |
| duplicate identical packet | zero upload/raster/combine; duplicate counter only |
| source move | only union of old/new expanded tiles; deleted old coverage proven black |
| add/remove source | new/old overlap tiles only |
| compatibility change | old and new dependent profile tiles; no unrelated profile work |
| illumination move/capability change | only conservatively affected dependent tiles; exact final reference |
| bypass move | bypass tiles only; no profile allocation |
| suppression move | old/new effective tiles with suppression last |
| narrow dynamic door | tiles touched by affected rebuilt polygons only |
| broad 4V/2L 24-state door sequence | every state agrees with full CPU reference; no stale state or cumulative painting |
| no semantic `bDynamic`-only change | no changed hard pixels; only explicitly documented metadata work |
| precision/origin/layout change | full-scope rebuild and new generation |
| floor/map bounds change | full-scope rebuild; pixels outside new legal bounds are unavailable/black |
| more dirty tiles than one scratch batch | multiple batches in one graph or black across frames; never partial visible result |
| new revision while prior is pending | newest self-contained packet wins; prior cannot later overwrite it |

A parallel full-redraw reference path is required in Development automation. Every incremental result must byte-match the full redraw outside the declared edge band. Full redraw is diagnosis/oracle, not the normal shipping update policy.

## Layer 4: lifecycle and fail-closed matrix

| Event | Required behavior |
| --- | --- |
| renderer disabled/module unavailable | CPU queries pass; presentation status unavailable/black |
| plugin disable/re-enable in a supported Editor flow | old SVE/resources/world serial are gone; re-enable starts black and rebuilds from a new valid packet |
| NullRHI | no GPU resource work/readback; CPU suites pass; black/Unavailable status |
| unsupported feature level or R8 required flags | explicit unsupported reason; black; no white/fallback authority |
| shader compile/permutation unavailable | explicit shader reason; black |
| atlas allocation/byte cap failure | whole affected scope black; no contributor truncation |
| malformed/stale/contradictory packet | reject with stable counter/reason; no resource mutation from rejected packet |
| PIE start/stop repeated 20 times | no stale SVE, cross-world resource, crash, ensure, or growing allocation count |
| map/world transition | old world serial invalidated before new composition |
| module shutdown/hot reload in supported Editor flow | Render Thread releases resources without dereferencing destroyed GT objects |
| RHI resource recreation/feature-level change | generation increments, applied revision invalidates, full rebuild before sampling |
| device removed/DXGI/GPU crash | run retained as failure; no claim of mask correctness/performance |
| readback completing after teardown/generation change | discarded by metadata; no test callback into dead world |

Severe-log scanning covers `ensure`, assertion, fatal, critical error, unhandled exception, device removed, DXGI error, GPU crash, shader compile failure, RDG validation error, and RHI validation error. Known engine self-diagnostics must be identified by exact message/source; broad text suppression is forbidden.

## Layer 5: presentation and visual stability

Hard-mask differential evidence precedes visual approval. Then use deterministic camera paths for straight wall, diagonal wall, corner, doorway, seam, and floor transition at:

- 1920x1080 and 2560x1440;
- full-screen, windowed, and resize transitions;
- screen percentage values selected for supported product settings;
- stationary, lateral translation, forward/back motion, and rotation;
- temporal AA/upscaler modes selected by DARKWELL later.

Capture hard point-sampled mask and final feathered output separately. The hard world edge must not move when only camera/resolution changes. Feather may change screen sampling but cannot exceed its world/texel bound, bridge occluders, reveal another floor/owner, or alter CPU/memory results.

M3.0 validates strict black versus hard live presentation only. Neutral-gray memory, subject proxies, materials, and DARKWELL GPU-mask integration are later M3 work and cannot be declared covered by these captures.

### Lab test-region plan

A later implementation task may extend `/SightWeave/Maps/L_SightWeave_Lab` only through Unreal Editor or Editor Python APIs. Planned labeled, spatially separated regions are:

1. hard polygon basics: empty/triangle/radial/cone, edge and vertex probes;
2. straight/diagonal walls plus L/T/corner and `-PI/+PI` seam paths;
3. narrow/rotating/broad dynamic-door lanes with deterministic 24-state control;
4. Visible/Infrared/multi-capability/incompatible illumination lanes;
5. bypass and suppression ordering lane;
6. negative-coordinate, tile seam/page seam, origin/rebase, and bounds lane;
7. overlapping XY floor/height-band isolation lane;
8. Knowledge Owner isolation and PIE multi-world lifecycle lane;
9. camera/resolution/screen-percentage stability path;
10. failure/capacity/debug-readback controls that default to black.

Subject Reveal Override receives a separate subject-only lane proving no world-mask delta. Neutral-gray memory and last-seen proxy regions are not created by M3.1.

## Layer 6: performance and memory

### Metric categories

| Metric | Category now | Threshold/status |
| --- | --- | --- |
| M2 Batch512 authoritative intrinsic CPU | **closed formal contract** | on-CPU p50 `<=150 us`, p95 `<=180 us`, p99 `<=200 us`; unchanged |
| M2 broad 4V/2L dynamic door intrinsic CPU | **closed formal contract** | on-CPU p99 `<250 us`; engineering `<225 us`, ideal `<200 us`; unchanged |
| M2 Prepared 4096/source | **closed formal contract** | warmed p50 `<1 ms`, p99 `<2 ms`; unchanged |
| M2 warmed hot-path allocation/correctness | **closed formal contract** | zero asserted allocator activity/capacity failures and exact parity for declared workloads; unchanged |
| M3 GT render-packet build/dispatch | **proposed engineering target** | p95 `<0.25 ms` at reference workload; no unchanged-frame packet build |
| M3 RT packet accept/RDG setup, dirty reference update | **proposed engineering target** | p95 `<0.20 ms`; unchanged p95 `<0.05 ms` with zero raster/upload passes |
| M3 packet upload + tile clear/raster/gutter | **proposed engineering share** | p95 `<0.45 ms` for the reference dirty-door workload, independent of screen resolution |
| M3 profile intersection | **proposed engineering share** | p95 `<0.15 ms` at 8 complete profiles in the reference dirty-door workload |
| M3 bypass + suppression merge | **proposed engineering share** | combined p95 `<0.10 ms` in the reference dirty-door workload |
| M3 composite only | **proposed engineering share** | p95 `<0.30 ms` at 1080p and `<0.50 ms` at 1440p |
| M3 raster + final composite at 1080p | **provisional requirement target** | proposed acceptance interpretation: p95 `<1.0 ms`, report p99/max |
| M3 raster + final composite at 1440p | **provisional requirement target** | proposed acceptance interpretation: p95 `<1.5 ms`, report p99/max |
| M3 live-mask GPU budget | **proposed architecture limit** | `32 MiB` including 2 MiB scratch, plus report measured driver/RDG allocation |
| whole plugin runtime memory | **provisional requirement target** | `<64 MiB` at approved reference map/floor set; not yet a gate |
| 1080p -> 1440p composite pixel ratio | **arithmetic estimate** | `3,686,400 / 2,073,600 = 1.7778x`; not a timing result |
| per-tier atlas bytes | **arithmetic estimate** | 4/8/12/16 MiB per scope for proposed 64/128/192/256 tile caps |
| actual GPU p50/p95/p99/max | **later measurement** | no evidence exists until implementation runs on declared hardware |

The per-pass shares are diagnostic guardrails, not independent excuses to waive the total. They apply to the reference dirty-door workload, not a full-cap rebuild. The provisional GPU targets cannot become formal acceptance contracts until the user approves minimum hardware, map/floor extents, simultaneous owners, resident floors, source/profile workload, screen percentage/upscaler, and build/RHI settings. Until then a run may meet or miss an engineering target, but M3.0 must not state that product performance passed.

### Timing workload matrix

Use D3D12/SM6 Development with VSync/frame-rate smoothing disabled and record clocks/power mode/background conditions. Each row has a warmup, at least 10,000 measured frames or enough repetitions for stable p99, and at least three fresh processes. The soak uses 2,400 warmup frames and 36,000 measured frames to remain comparable with M2 evidence.

The office RTX 4060 is the primary development measurement GPU, not an approved minimum specification. The home Turing 8 GB GPU remains recorded only as an unidentified `RTX 2070 ...`-truncated device until its full model is verified; it MUST NOT be relabeled as an RTX 2060 Super or used as a formal minimum by assumption.

| Workload | Profiles | Dirty tiles/frame | Sources | Resolutions | Purpose |
| --- | ---: | ---: | ---: | --- | --- |
| unchanged steady state | 1/4/8 | 0 | 8V/8L | 1080p/1440p | composite-only cost; assert zero raster/upload |
| one narrow door | 1/4/8 | actual narrow set | 4V/2L and 8V/8L | 1080p/1440p | normal dirty update tail |
| broad 24-state door | 1/4/8 | measured | 4V/2L | 1080p/1440p | CPU/GPU coupled churn and stale rejection |
| one dirty tile | 1/4/8 | 1 | controlled polygons | 1080p/1440p | per-profile/tile marginal cost |
| one scratch batch | 1/4/8 | 8 | controlled polygons | 1080p/1440p | proposed scratch high-water |
| full active scope rebuild | 1/4/8 | tier cap | reference sources | 1080p/1440p | spike/failure behavior; not expected per-frame |
| camera-only motion | 8 | 0 | 8V/8L | 1080p/1440p | world stability and composite scaling |
| tier sweep | 1/4/8 | same world mutation | 8V/8L | 1080p/1440p | 25/10/5/2.5 cm fidelity/cost/bytes |
| source-count sweep | 1/4/8 | 0/1/8 plus door set | 2/8/32 total, plus 8V/8L reference | 1080p/1440p | scaling; 32-source stress is reported separately |
| owner/floor residency stress | 1/4/8 | controlled | 8V/8L per scope | 1080p/1440p | one active scope, attempted second scope, cap/byte-budget failure and isolation |

GPU events MUST separate `PacketUpload`, `TileClear`, `VisionRaster`, `IlluminationRaster`, `ProfileCombine`, `Bypass`, `Suppression`, `Gutter`, and `Composite`, plus a total SightWeave event. Use UE 5.8 `RDG_EVENT_SCOPE_STAT` and RHI breadcrumbs. Do not use deprecated no-op `RDG_GPU_STAT_SCOPE` as evidence.

Record CPU GT/RT time separately using Unreal Insights named CPU scopes and thread identity. If CPU tail attribution becomes necessary, collect scheduler-aware ETW with the same fail-closed ownership/loss rules used by M2P.5; GPU duration still remains a separate metric.

The 2-source and 8-source sweeps plus the 8V/8L reference are expected to meet the provisional total targets. The 32-total-source row is a scale stress with all percentiles reported; **proposed scale ceiling**, pending workload/hardware approval, is p95 `<2.0 ms` at 1080p and `<3.0 ms` at 1440p. Missing that scale ceiling cannot be hidden by reducing sources or profiles.

### Memory/allocation evidence

- measured pooled atlas bytes by owner/floor/page/tier and resource generation;
- transient RDG peak/high-water, upload buffer bytes, packet owned bytes, and driver-reported budget where available;
- no per-frame allocation/capacity growth in unchanged warmed presentation;
- expected bounded allocation on new revision/page creation explicitly separated from leaks;
- 36,000-frame D3D12 soak has stable active page/tile/scratch/packet counts after warmup;
- capacity overflow tests prove black failure and recovery after load decreases;
- teardown returns all module-owned persistent RHI resources and view extensions to baseline.

## Layer 7: build, package, and clean-host matrix

After any M3 production C++/shader/module change, required evidence is:

1. `Scripts/BuildEditor.ps1` full `DarkwellEditor Win64 Development` success.
2. Focused M3 automation, then `SightWeave.M2P5`, `SightWeave.M2P4`, `SightWeave.M2P3`, `SightWeave.M2P2`, `SightWeave.M2`, full `SightWeave`, and `Darkwell` regression with every failure retained.
3. NullRHI CPU/lifecycle smoke and D3D12/SM6 render smoke.
4. Win64 Game Development and Shipping build/module audit: Runtime and Render present as intended; Editor/Tests absent.
5. fresh `RunUAT BuildPlugin` into a new external temporary directory; package contains plugin shader sources/metadata and expected binaries only.
6. install packaged plugin into a clean UE 5.8.1 host project, build/cook/package Development and Shipping, run the D3D12 smoke, and verify no workspace/DerivedDataCache dependency.
7. Shipping binary/source/import scan for DARKWELL, Editor, Tests, Automation, debug readback, and unintended Engine-private dependencies.
8. Git LFS status/fsck and repository checks; generated artifacts remain untracked.

No ordinary filesystem operation may move/rewrite `.uasset` or `.umap`. A later lab map change must use Unreal Editor or Editor Python APIs and must be committed as an explicit asset change with Git LFS verification.

## Final implementation verdict rules

M3 GPU-mask implementation may be marked **COMPLETED** only when:

- every normative packet/authority/revision/failure contract passes;
- CPU/GPU hard differentials have zero non-boundary mismatches across all required tiers, profiles, owner/floor isolation, dynamic-door, and dirty/full-redraw cases;
- lifecycle/NullRHI/Shipping/BuildPlugin/clean-host checks pass;
- no partial or stale mask is presented and every capacity/resource failure is black and diagnosed;
- performance is truthfully labeled against approved or still-provisional targets with all distributions retained;
- affected M2 CPU regressions remain closed and no new production-code evidence is borrowed from a pre-change binary.

If correctness or lifecycle is unresolved, verdict is **PARTIAL**. If required hardware/RHI/user workload approval prevents a formal performance verdict while correctness is complete, report **PARTIAL — performance acceptance pending**, not a guessed pass. A real external/toolchain blocker is **BLOCKED** with the exact missing evidence. Unsupported RHI behavior is acceptable only when explicitly outside the approved support matrix and demonstrably fail-closed.

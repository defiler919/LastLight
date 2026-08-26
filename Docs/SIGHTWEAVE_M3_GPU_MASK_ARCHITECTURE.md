# SightWeave M3 GPU mask architecture

Status: **FROZEN FOR M3 IMPLEMENTATION**

This document selects the M3 live-mask architecture for Unreal Engine 5.8.1. The normative data and failure semantics are in `SIGHTWEAVE_M3_GPU_MASK_CONTRACT.md`. Nothing in this document implements or authorizes DARKWELL gameplay, memory presentation, materials, assets, or maps.

## Decision summary

SightWeave will use CPU visibility polygons and deterministic CPU triangle indices, transferred as immutable revisioned packets to a new `SightWeaveRender` Runtime module. A world-scoped Scene View Extension will rasterize only dirty logical tiles into transient profile scratch resources, combine them into a persistent R8 effective-live atlas partitioned by Knowledge Owner and floor, and expose the result to a post-tonemap presentation pass. The CPU Runtime remains independent of Renderer/RHI and remains usable with NullRHI.

This is a stable world-space tiled system. It is neither screen-space fog nor GPU visibility solving.

## Alternatives reviewed

| Candidate | Stability and correctness | Scaling/lifecycle | Decision |
| --- | --- | --- | --- |
| one fixed map-sized world RT | world-stable, but forces an approved map bound and wastes empty space; resize/rebase is a destructive full rebuild | memory grows with maximum map extent, not used coverage | rejected as primary |
| sparse/tiled world atlas | stable integer world keys, bounded residency, dirty updates and explicit overflow | predictable pages; works with one active floor and off-screen sources | **selected** |
| camera-centered clipmap | bounded and good for huge terrain, but camera movement causes tile churn and makes off-screen remote coverage/history awkward | needs level blending, origin rollover, and temporal policy before M3 needs it | rejected for v1; future scale extension only |
| texture array per logical tile | clean slice addressing, but sampling/material/RHI limits and array resizing become product constraints | viable internal fallback after platform evidence | not baseline |
| one resource per floor | preserves isolation but does not bound large floors | selected only as a logical partition; each floor uses sparse pages |
| one resource per Knowledge Owner | preserves private knowledge | selected as a logical partition; no cross-owner pixels |
| persistent resource per source | simple draw ownership but memory/pass count scale with sources | unnecessary because dirty tiles can redraw contributors | rejected |
| persistent resource per compatibility profile | preserves semantics but VRAM grows with profiles and floors | transient per-tile profile scratch preserves semantics at bounded memory | rejected as persistent layout |
| screen-space half/full-resolution mask | cheap prototype, but camera dependent, off-screen sources vanish, edges swim, and resolution changes alter the hard mask | violates world-space contract | rejected |
| Scene Capture/depth/raymarch | rendered geometry can look convenient, but duplicates CPU truth and introduces capture bias/current-light coupling | cost/resources scale per source/capture | rejected as authority and baseline |
| CPU triangulation -> GPU rasterization | deterministic CPU truth, modest packet size, natural RDG work | CPU packet cost and GPU tile cost are independently measurable | **selected** |
| GPU visibility solve | could move angular work to GPU but creates a second authority, readback/debug complexity, and platform exposure | reopens closed M2 solver work | rejected |

## Module boundary

### Planned plugin modules

| Module | Type/loading | Responsibility | Dependencies |
| --- | --- | --- | --- |
| `SightWeaveRuntime` | existing Runtime / `Default` | CPU authority, immutable snapshot, renderer-neutral render packet types/builder interface | remains free of `RHI`, `RenderCore`, and `Renderer` |
| `SightWeaveRender` | new Runtime / `PostConfigInit` | shader mapping, global shaders, world render subsystem, Scene View Extension, RDG atlas/update/composite path | private: `Core`, `CoreUObject`, `Engine`, `Projects`, `RHI`, `RenderCore`, `Renderer`, `SightWeaveRuntime` |
| `SightWeaveTests` | existing Editor / `Default` | CPU tests plus Development/Editor-only render/readback automation | add private `SightWeaveRender` only when render tests arrive |
| `SightWeaveEditor` | existing Editor / `Default` | authoring and future lab automation; no render authority | no change in M3.0 |

`SightWeaveRuntime` MUST NOT depend on `SightWeaveRender`. The render module discovers and consumes the Runtime world subsystem. Disabling or failing to load `SightWeaveRender` leaves CPU registration, solves, queries, and diagnostics functional.

### Planned source and shader layout

```text
Plugins/SightWeave/
  Shaders/
    Private/
      SightWeaveMaskRaster.usf
      SightWeaveMaskCombine.usf
      SightWeaveMaskComposite.usf
  Source/
    SightWeaveRuntime/
      Public/SightWeaveRenderPacket.h
      Private/SightWeaveRenderPacket.cpp
    SightWeaveRender/
      SightWeaveRender.Build.cs
      Private/SightWeaveRenderModule.cpp
      Private/SightWeaveRenderWorldSubsystem.*
      Private/SightWeaveSceneViewExtension.*
      Private/SightWeaveMaskShaders.*
      Private/SightWeaveMaskRenderState.*
```

Names are frozen as an implementation plan, not existing files. M3.0 creates none of them.

`SightWeaveRender::StartupModule` resolves the plugin base directory through `IPluginManager`, maps `<PluginBase>/Shaders` to `/Plugin/SightWeave`, and registers all global shader types from that virtual path. `PostConfigInit` is required so mapping and global shader registration occur before shader compilation. `ShutdownModule` removes only module-owned callbacks/state; it cannot assume RHI availability.

UE 5.8.1 evidence for this plan:

- `ShaderCore.h` declares `AddShaderSourceDirectoryMapping` and engine plugins use `/Plugin/<Name>` mappings from `PostConfigInit` modules.
- `ViewportWidgetOverlay` and `GeometryMask` demonstrate Runtime plugin global shaders and world-scoped Scene View Extensions.
- `BuildPluginCommand.Automation.cs` includes `/Shaders/...` in packaged plugin filters; a later BuildPlugin/clean-host test still remains mandatory.
- `SceneViewExtension.h` exposes RDG-aware `PreRenderViewFamily_RenderThread` and post-processing callbacks.

Global shader `ShouldCompilePermutation` MUST gate the actually supported feature level/platform. Unsupported permutations do not trigger a different authority; the corresponding render scope is black/Unavailable. The DX12/SM6 path is the required first evidence target, not a claim that every RHI is supported.

## Ownership and data flow

```text
Game Thread / worker CPU authority
  USightWeaveWorldSubsystem publishes immutable FSightWeaveFrameSnapshot r
      -> production const shared-snapshot acquisition keeps r alive
      -> renderer-neutral packet builder canonicalizes profiles,
         triangulates polygons, calculates old/new dirty bounds and owns arrays
      -> immutable FSightWeaveRenderPacket r
      -> ENQUEUE_RENDER_COMMAND captures owning const shared pointer

Render Thread
  FWorldSceneViewExtension receives/coalesces packet r
      -> validates world serial, scope, schema, caps, revision
      -> PreRenderViewFamily_RenderThread registers persistent atlas pages in RDG
      -> redraws dirty tiles from the self-contained contributor set
      -> transient profile/bypass/suppression scratch -> hard effective R8 atlas
      -> promotes AppliedRevision after all work is ordered

Post process
  SubscribeToPostProcessingPass(Tonemap)
      -> after-tonemap callback samples active owner/floor hard atlas
      -> applies bounded presentation-only feather/composite
      -> honors FPostProcessMaterialInputs::OverrideOutput
      -> missing/invalid/not-applied scope produces black
```

The existing snapshot pool is compatible with a production const acquisition API: a published snapshot is reused as standby only when its shared reference count returns to one. The render bridge MUST acquire a const shared reference, never call the Blueprint copy-return API per frame, and never mutate the snapshot.

Packet construction happens only for a new relevant revision, not every view or frame. It may run from immutable data on a worker if profiling requires it, but final publication to the render queue is ordered on the Game Thread and keeps only the newest completed self-contained revision. Any asynchronous builder must carry the source revision and discard completion after world teardown or a newer revision.

## Scene View Extension plan

`USightWeaveRenderWorldSubsystem` is private to `SightWeaveRender` and initializes after `USightWeaveWorldSubsystem`. It owns a `TSharedPtr<FSceneViewExtension, ESPMode::ThreadSafe>` created with `FSceneViewExtensions::NewExtension`. The extension derives from `FWorldSceneViewExtension`, so a PIE/editor world cannot draw another world's masks.

- Game Thread setup selects the local Knowledge Owner and exactly one active floor and supplies view-independent world metadata.
- `PreRenderViewFamily_RenderThread(FRDGBuilder&, FSceneViewFamily&)` applies pending packet/resource changes once per view family before any SightWeave sampling.
- `SubscribeToPostProcessingPass(EPostProcessingPass::Tonemap, ...)` installs the after-tonemap compositor. UE 5.8.1 maps this callback to `BL_SceneColorAfterTonemapping`.
- The callback consumes `SceneColor`, writes `OverrideOutput` when valid, otherwise returns a correctly allocated screen-pass output, and preserves view rectangles/slices.
- Split-screen/multiple views may sample the same owner/floor atlas. Views requesting different owners are unsupported under the proposed v1 default and fail black until a multi-owner product budget is approved.

The post-tonemap choice is frozen for strict black/live-mask presentation because it is independent of viewport resolution and preserves pure black after tone mapping. Neutral-gray remembered-environment composition is deliberately outside M3.0; if it later requires pre-tonemap material attributes, it may add an earlier pass without changing live-mask authority, identity, or atlas generation.

## RDG and RHI resource model

Persistent atlas pages are Render Thread-owned `TRefCountPtr<IPooledRenderTarget>` objects or the equivalent UE 5.8.1 external pooled target. Each graph registers them using `FRDGBuilder::RegisterExternalTexture`; transient tile scratch and upload buffers use `CreateTexture`/RDG buffers. `ConvertToExternalTexture` is not the default because UE documents it as increasing memory pressure and primarily easing legacy ports.

Reference atlas description:

```text
Extent: 2048 x 2048
Format: PF_G8 (linear R8 UNorm, one channel, one byte/texel)
Flags: TexCreate_ShaderResource | TexCreate_RenderTargetable
Clear: black
Mips: 1
Samples: 1
```

The baseline is a graphics global-shader path:

1. clear the dirty atlas slot and transient scratch;
2. rasterize CPU triangles for one complete profile into `VisionScratch` and compatible illumination into `IlluminationScratch`;
3. pixel-combine `VisionScratch AND IlluminationScratch` into an effective accumulator with max/OR semantics;
4. union bypass scratch;
5. apply live-suppression scratch last;
6. write the hard result into the persistent atlas slot and regenerate gutters.

Profile scratch is cleared and reused serially. A future compute/UAV implementation is an optimization only after RHI capability and equivalence evidence; R8 UAV support is not a baseline dependency.

Every pass uses `RDG_EVENT_SCOPE_STAT`/named RHI breadcrumbs and separates packet upload, raster, combine, gutter, and composite events. UE 5.8 deprecates the old `RDG_GPU_STAT_SCOPE`; it MUST NOT be used as timing evidence.

### Resource generations

Render state owns a monotonically increasing `ResourceGeneration`. Allocation/layout change, RHI reinitialization, device removal, world teardown, feature-level change, or atlas recreation increments it and invalidates `AppliedRevision`. A readback or queued result from another generation is discarded.

All creation, registration, mutation, and release of atlas resources occurs on the Render Thread/RHI path. Game Thread teardown only invalidates the world serial and enqueues release. No resource destructor depends on a live `UWorld` or Runtime subsystem.

## Tiling and precision tiers

The four tiers below are **proposed defaults required for comparison**, not an approved shipping precision. They intentionally share a physical layout so fidelity and cost can be compared without changing shader topology.

Common layout:

- one physical tile slot: `256 x 256` R8 = `65,536 B` (`64 KiB`);
- usable interior: `248 x 248` texels;
- gutter: `4` texels on each edge;
- one atlas page: `2048 x 2048`, `8 x 8 = 64` slots, `4,194,304 B` (`4 MiB`);
- maximum presentation feather: proposed `2.0` texels; the 4-texel gutter covers filter radius plus neighbor regeneration;
- transient scratch: proposed maximum eight in-flight tiles x four R8 scratch surfaces = `2 MiB`, shared by the renderer rather than multiplied by scopes.

| Tier | World cm/texel | Interior world span/tile | Proposed active tiles/scope | Pages | Persistent R8/scope | Max non-overlap area represented |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Coarse | 25.0 | 62.0 m | 64 | 1 | 4 MiB | 246,016 m2 (about 496 m square) |
| Standard | 10.0 | 24.8 m | 128 | 2 | 8 MiB | 78,725 m2 (about 281 m square) |
| Fine | 5.0 | 12.4 m | 192 | 3 | 12 MiB | 29,522 m2 (about 172 m square) |
| Ultra | 2.5 | 6.2 m | 256 | 4 | 16 MiB | 9,841 m2 (99.2 m square) |

The area number is a capacity illustration, not a contiguous square allocation guarantee. Sparse logical tiles may be disjoint. Gutter bytes are already included because the entire 256-square physical slot is counted.

Persistent bytes per scope are:

```text
ceil(MaxActiveTiles / 64) * 2048 * 2048 * 1 byte
```

There is no full-atlas double buffer and no persistent profile multiplier. RDG ordering lets a revision update before its later composite; if an update must span frames, that scope is sampled as black until completion rather than exposing partial/stale contents.

### Proposed residency and safety caps

| Limit | Proposed default | Safety behavior |
| --- | ---: | --- |
| composited Knowledge Owners per local view | 1 | other owner requests black + diagnostic |
| resident `(owner, floor)` scopes | 1 | optional transition scope requires byte-budget preflight |
| total live-mask GPU budget including scratch | 32 MiB | allocation refused before partial residency |
| complete profiles/scope | 8 | validation at 1/4/8; configurable hard ceiling 32; overflow fails scope closed |
| active vision sources/scope | 64 | overflow fails scope closed |
| active illumination sources/scope | 64 | overflow fails scope closed |
| packed polygon vertices/packet | 262,144 | checked reject, never truncate |
| packed triangle indices/packet | 786,432 | checked reject, never truncate |
| dirty logical tiles/revision | tier active-tile cap | larger/unknown change becomes full rebuild within same cap |

The existing reference workload remains 8 vision sources, 8 illumination sources, one active floor, 4,096 relevant segments, and 512 subjects. The caps above are defensive proposals, not workload approval. Minimum hardware, actual floor/map extents, required simultaneous owners, resident-floor transition behavior, and the shipping tier remain **user decisions pending**.

At the 32 MiB proposed mask budget, examples are:

| Configuration | Persistent | Shared scratch | Total live-mask estimate | Default verdict |
| --- | ---: | ---: | ---: | --- |
| 1 Standard scope | 8 MiB | 2 MiB | 10 MiB plus small buffers | allowed |
| 1 Ultra scope | 16 MiB | 2 MiB | 18 MiB plus small buffers | allowed |
| 2 Standard scopes | 16 MiB | 2 MiB | 18 MiB plus small buffers | optional after product need |
| 2 Ultra scopes | 32 MiB | 2 MiB | 34 MiB plus small buffers | rejected by default budget |
| 4 Ultra scopes | 64 MiB | 2 MiB | 66 MiB plus small buffers | rejected; also exceeds the provisional whole-plugin 64 MiB target before CPU data |

Small buffers, pooled-resource alignment, driver metadata, and RDG aliasing are not included in arithmetic estimates and MUST be reported from runtime allocation instrumentation.

## Dirty-update architecture

The packet builder compares stable source IDs/generations and source revisions against the prior immutable render mirror. It records old and new bounds before replacing entries. The renderer maps conservative expanded bounds to logical tile keys, then redraws every contributor in each dirty tile from the new self-contained packet.

Dynamic doors have no render-specific semantic. A door segment change causes the CPU snapshot to rebuild affected vision/illumination polygons; old/new bounds of those changed polygons dirty the corresponding tiles. M2P.4/M2P.5 dynamic-sector optimization remains entirely CPU-side.

| Event | Raster/update work |
| --- | --- |
| identical published revision | none |
| source activation/add | new-overlap tiles |
| source deactivation/delete | old-overlap tiles |
| polygon transform/shape | union of old/new overlap tiles |
| compatibility change | old/new profile dependencies and overlap tiles |
| illumination change | dependent profiles in overlapping tiles |
| bypass change | bypass overlap only |
| suppression change | affected effective tiles after union |
| camera motion/turn/FOV/resolution | no world mask update; full-screen sample only |
| active floor/owner switch | select valid resident scope or black while building it |
| capacity/layout/origin/RHI change | full-scope rebuild |

An LRU MAY select a physical slot only among tiles not needed by the current desired scope. Eviction increments counters. Requested live coverage that cannot be resident under the cap does not silently disappear: the whole affected scope fails black/CapacityExceeded. Later architecture may introduce an approved camera-independent interest partition, but M3.0 does not invent one.

## Presentation and resolution behavior

The atlas precision is independent of 1920x1080, 2560x1440, window resizing, screen percentage, and temporal upscaling. For each scene pixel the composite reconstructs/uses world position and active floor metadata, maps it to a logical tile, resolves the atlas slot table, point-samples the hard mask, then optionally applies bounded presentation filtering.

The composite must preserve ViewRect, stereo/slice semantics, `OverrideOutput`, and UE's screen-pass conventions. Missing depth/world reconstruction, missing tile mapping, wrong floor, stale revision, or failed resource returns black. The compositor cannot sample a neighboring owner/floor as fallback.

1920x1080 processes `2,073,600` pixels; 2560x1440 processes `3,686,400`, exactly `1.7778x` as many. This ratio is an arithmetic workload estimate, not a GPU timing prediction. Measured GPU time must come from RDG/RHI profiler events on the declared hardware/RHI/build.

## Shipping, Fab, and NullRHI behavior

- `SightWeaveRender` is a normal Runtime module and shaders reside under the plugin `Shaders` directory so BuildPlugin packages them.
- No shader source depends on project paths, DARKWELL defines, Editor-only modules, Engine private headers, or uncooked assets.
- Shipping keeps fail-closed status/counters needed for support but excludes pixel readback and expensive debug visualization.
- Development/Editor may expose RDG captures, selected-tile readback, atlas/profile debug views, and validation console commands.
- NullRHI or server-like execution never loads a usable GPU path. CPU Runtime stays authoritative and operational; the render status is `Unavailable/NullRHI` and any presentation consumer receives black.
- Clean-host packaging must prove the plugin does not depend on local DerivedDataCache, project shader mappings, developer files, or precompiled binaries from this workspace.

## Implementation sequence implied by the freeze

1. Add renderer-neutral immutable packet/acquisition support without changing query authority or M2 contracts.
2. Add `SightWeaveRender` with startup mapping and a compile-only global shader smoke test.
3. Add world subsystem/SVE lifecycle and black fallback before live rasterization.
4. Add one owner/floor, one profile, one tile hard raster/readback.
5. Add compatibility, bypass, suppression, dirty tile recomposition, stale rejection, and tier/cap handling.
6. Add post-tonemap sampling and presentation-only feather.
7. Run the validation matrix before any DARKWELL or memory-presentation integration.

Each implementation step that changes production C++ requires the repository's full editor build and the affected automation/regression evidence. Shader/module additions also require BuildPlugin, packaged Development, clean-host, NullRHI, and post-change GPU evidence.

# SightWeave M3.1 Final Validation

Status: **COMPLETED**

Branch: `codex/m3p1-sightweave-render-single-tile`

Frozen baseline: `2cb3f82ab44e810a09f18bed036fa1e4d36db4aa`

Validated implementation and test checkpoint: `c426283bd2e1cadec6778f0fba40426a5ea766ce`

Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`

Formal GPU path: Windows D3D12 SM6 on NVIDIA GeForce RTX 4060, Studio Driver 610.88

## Disposition

M3.1 closes the first executable GPU presentation slice of the frozen M3 contract:

`CPU-authoritative polygons -> deterministic CPU triangles -> immutable packet -> SightWeaveRender -> RDG single-tile raster -> asynchronous test readback`

The implementation is intentionally limited to one world, one knowledge owner, one floor, one compatibility profile, and one 256 x 256 `PF_G8` physical tile with a 248 x 248 interior, four-texel gutter, and Standard 10 cm/texel precision. CPU Authority remains the only gameplay truth. GPU results are presentation-only and never feed AI, HUD, interaction, memory, Last-Seen, damage-reveal rules, or other gameplay decisions.

M3.1 is marked COMPLETED because the immutable contract, deterministic triangulation, world-scoped publication/SVE lifecycle, PF_G8 capability gate, D3D12 raster formula, asynchronous readback, texel parity, NullRHI fail-closed path, packaging, clean-host matrix, and Shipping isolation are all closed by retained evidence. Two old M2P2 wall-clock performance tests remain noisy in the full NullRHI suite; their failures are retained and isolated because M2 intrinsic CPU acceptance was already closed by administrator ContextSwitch ETW. No threshold was changed and no M2 algorithm was reopened.

## Recovery record

- The post-power-loss audit found the expected branch and remote checkpoint, no truncated or zero-byte source, no conflict marker, no active Git process, no stale Git lock, and no repository corruption.
- Recovery path A was used: reliable local work was audited, compiled, tested, committed, and pushed without reset, restore, clean, rebase, or merge.
- The local `Darkwell.uproject` EngineAssociation GUID difference was preserved throughout and excluded from every commit.
- Generated `Binaries`, `Intermediate`, `Saved`, DDC, reports, captures, temporary packages, and readback data were not tracked.
- All development failures and slow samples remain in their original `Saved` reports/logs or temporary BuildPlugin directories.

## Implementation contract

### Modules and shader registration

- `SightWeaveRender` is a Runtime module loaded at `PostConfigInit` so its global shader types and `/Plugin/SightWeave` source mapping exist before normal runtime shader use.
- `SightWeaveRender` depends only on `Core`, `CoreUObject`, `Engine`, `Projects`, `RHI`, `RenderCore`, `Renderer`, and `SightWeaveRuntime`.
- `SightWeaveRuntime` does not depend on `SightWeaveRender`.
- The render module has no dependency on Darkwell, UnrealEd, SightWeaveEditor, SightWeaveTests, AutomationTest, or Windows ETW libraries.
- The formal shader is `Plugins/SightWeave/Shaders/Private/SightWeaveSingleTile.usf`. It explicitly includes `/Engine/Public/Platform.ush` as required by UE 5.8.1 and contains the raster, fullscreen, and combine entry points.
- Shader mapping is registered once by the module. World state is owned by world subsystems and the SVE, not by module-global mutable gameplay state.

### Immutable packet and deterministic triangles

The compact packet owns and privately exposes only immutable render data through a thread-safe `TSharedPtr<const FSightWeaveRenderPacket>`:

- monotonic world instance serial;
- knowledge owner, floor, and canonical compatibility-profile identity;
- packet, registry, and published-snapshot revisions;
- tile coordinate, physical bounds, world-to-tile UV scale/bias, cm/texel, and gutter;
- dirty reason and full-tile semantics;
- Vision, Illumination, Bypass, and Suppression triangle ranges;
- owned deterministic vertex/index buffers;
- validity/fail-closed classification and deterministic content hash.

The builder validates scope, profile, revisions, tile geometry, finite input, source identity, bounds, size caps, and index ranges. Invalid input returns a black/fail-closed packet instead of retaining an older mask. Source order is canonicalized by layer and stable source ID. Polygons are normalized for duplicate endpoints and collinear vertices, winding is canonicalized, simple-polygon validity is checked, and a bounded deterministic ear-clipping pass handles convex and concave polygons. Degenerate, non-simple, non-finite, over-capacity, or untriangulable input fails closed. Completely outside polygons are omitted; crossing and gutter geometry remains in world space and is clipped by raster coverage.

The revision gate classifies Accepted, Duplicate, Stale, RevisionConflict, WorldMismatch, and Invalid. Equal revision/equal hash is a no-work duplicate. Equal revision/different hash is a conflict and fails black. Older revisions are rejected.

### Publication and world lifecycle

- `USightWeaveWorldSubsystem` publishes immutable ordinary-data snapshots through an event; the render subsystem subscribes instead of polling gameplay on Tick.
- `USightWeaveRenderWorldSubsystem` allocates a new monotonic world serial for each lifetime, builds compact packets from the published snapshot, and owns one `FWorldSceneViewExtension`.
- The SVE consumes only matching-world immutable packets and enqueues render-thread ownership transfers.
- Deinitialize removes the publication delegate, shuts down the SVE, enqueues world-identity-checked resource release, and invalidates the world serial.
- Delayed commands and readbacks are bound to world, owner, floor, profile, and revision. Old-world writes and stale readback results are rejected.
- No-change publications emit no replacement GPU work. Camera movement alone does not rebuild a world-space tile.

### PF_G8 capability gate and RDG data flow

The formal resource path verifies `PF_G8` support for Texture2D, render target, and shader resource use. D3D12 readback confirms real row pitch and 256 x 256 binary texels. `PF_G8` UAV capability is reported as available on the acceptance hardware, but the implementation uses an RTV raster/combine path and does not require UAV support. NullRHI and unsupported capability/resource states are classified Unavailable/Black and perform no illegal RHI work.

Persistent allocation:

- EffectiveLive: one 256 x 256 `PF_G8` texture, 65,536 bytes.

Transient dirty-tile scratch:

- Vision, Illumination, Bypass, and Suppression: four 256 x 256 `PF_G8` textures, 262,144 bytes total.
- Peak mask bytes on a non-empty redraw: 327,680 bytes, excluding compact transient packet vertex/index buffers.
- Empty/clear-only work uses no scratch masks and peaks at 65,536 mask bytes.

RDG events are:

- `SightWeave.ClearTile`
- `SightWeave.RasterVision`
- `SightWeave.RasterIllumination`
- `SightWeave.RasterBypass`
- `SightWeave.RasterSuppression`
- `SightWeave.CombineEffectiveLive`

The combine shader executes the frozen single-profile formula exactly:

- `Gated = Vision intersection CompatibleIllumination`
- `EffectiveLive = Gated union Bypass`
- `Final = EffectiveLive minus Suppression`

Scratch and result clears are black. The hard raster path emits only 0 or 255. Vision-only, illumination-only, disjoint vision/light, stale input, teardown, deletion, and explicit clear cases are black. Suppression is applied last. Damage reveal is excluded from the world mask.

## CPU/GPU differential and lifecycle evidence

The final successful repository D3D12 run executed 29 M3.1 tests: 19 asynchronous D3D12 readback cases plus 10 packet, triangulation, lifecycle, NullRHI-compatible, packaging, and preliminary-performance tests.

- Every readback is bound to world/scope/revision and reports dimensions, row pitch, hash, 0/255 counts, non-binary count, dispatch count, duplicate/stale counts, and async status.
- Each completed mask compares all 65,536 texel centers against the CPU formula using the frozen world-to-texel transform.
- All final cases reported zero non-boundary mismatches and zero boundary-class mismatches. No tolerance was widened.
- All final cases reported zero non-binary texels; row pitch was 256 pixels on the accepted adapter.
- Fixed-seed repeated packets produced identical hashes.
- Duplicate revision counted one duplicate and did not redispatch; stale revision counted one stale packet and retained the accepted current result; stale asynchronous readback was discarded and released its pixels.
- Source deletion and teardown/restart produced black before accepting the new-world packet. No old-world white texel leaked.
- All sampled no-change consumes reported `no_change_mask_work=false`, meaning no mask pass was produced.
- NullRHI returned an explicit failed/unavailable readback with zero pixels and left CPU Authority operational; it was never treated as a successful black GPU result.

Final focused evidence:

- NullRHI: `Saved/AutomationReports/SightWeaveM3P1_NullRHI_PackagingFinal`, 10 passed, 0 failed.
- D3D12/SM6 authoritative retry: `Saved/AutomationReports/SightWeaveM3P1_D3D12_PackagingFinal_Retry1`, 29 passed, 0 failed.
- D3D12 log: `Saved/Logs/SightWeaveM3P1_D3D12_PackagingFinal_Retry1.log`.
- Packaged-host D3D12: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM3P1_PackagedHost_Final3\Reports\M3P1D3D12`, 29 passed, 0 failed.

## Preliminary measured performance and capacity

These are **preliminary measurements**, not the final M3 GPU performance contract. GPU absolute timestamps exclude asynchronous readback. Readback end-to-end time is reported separately.

Packet builder, warmup 256 and 2,048 samples per source count:

| Sources | p50 us | p95 us | p99 us | max us | Vertices/indices | Buffer bytes | Capacity growth |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 2 | 1.300 | 2.300 | 2.700 | 107.200 | 8 / 12 | 112 | 0 |
| 8 | 3.100 | 3.300 | 5.000 | 405.000 | 32 / 48 | 448 | 0 |
| 32 | 14.700 | 19.100 | 25.000 | 225.700 | 128 / 192 | 1,792 | 0 |

All packet-builder p95 values are below the suggested 250 us GT target and deterministic hashes remained stable. The retained report is `Saved/AutomationReports/SightWeaveM3P1_PreliminaryPacketPerformance`.

Final successful D3D12 run, 21 measured readback samples, nearest-rank percentiles:

| Metric | p50 us | p95 us | p99 us | max us |
|---|---:|---:|---:|---:|
| GT submit | 3.099 | 9.701 | 16.402 | 16.402 |
| RT consume | 0.600 | 1.099 | 1.300 | 1.300 |
| RT RDG setup | 381.801 | 542.302 | 684.101 | 684.101 |
| Clear setup | 24.103 | 61.400 | 68.199 | 68.199 |
| Vision raster setup | 0.402 | 6.400 | 6.698 | 6.698 |
| Illumination raster setup | 0.101 | 1.401 | 5.700 | 5.700 |
| Bypass raster setup | 2.999 | 8.397 | 45.098 | 45.098 |
| Suppression raster setup | 0.000 | 0.101 | 0.902 | 0.902 |
| Combine setup | 2.600 | 15.501 | 48.701 | 48.701 |
| GPU mask graph | 79.000 | 375.000 | 4,278.000 | 4,278.000 |
| Async readback end-to-end | 9,186.998 | 10,365.102 | 11,035.901 | 11,035.901 |

The cold first BypassUnion sample accounts for the 4.278 ms GPU maximum. GT submit satisfies its suggested target. RT RDG setup p95 does **not** satisfy the suggested 200 us target. The measurement constructs a fresh one-shot render state and resources for each test case, so it is not a warmed persistent-update benchmark; nevertheless the miss is retained as an explicit risk. M3.1 therefore makes no final GPU-performance conclusion and does not lower the suggested target.

## Regression and smoke matrix

- `Scripts/BuildEditor.ps1`: `DarkwellEditor Win64 Development` succeeded after the final production include fix and after the packaging test was added.
- Focused M3.1 NullRHI: 10/10 passed.
- Focused M3.1 D3D12/SM6: authoritative retry 29/29 passed on RTX 4060 / Studio 610.88.
- Full SightWeave NullRHI: 125 passed, 2 failed, 0 not run in `Saved/AutomationReports/SightWeaveM3P1_FullSightWeave_PackagingFinal`.
- DARKWELL regression: 24/24 passed in `Saved/AutomationReports/SightWeaveM3P1_Darkwell24_PackagingFinal`.
- Lab NullRHI smoke: map loaded and exited cleanly; debug query reported `status=0 authoritative=1 live=1 vision=1 light=0 bypass=1 snapshot=58 visionSources=1 illuminationSources=0`.
- Lab D3D12/SM6 smoke: the same authoritative query passed on RTX 4060 / 610.88 and teardown was clean.

The two full-suite failures are retained old M2P2 wall-clock tests:

- `SightWeave.M2P2.Performance.Batch512Gate`: final run worst p99 337.899 us; distribution 4 and 8 p99 exceeded 200 us, with zero steady capacity growth.
- `SightWeave.M2P2.Performance.PreparedEventIndex4096`: total median 1,144.800 us and p99 2,048.202 us exceeded the existing 1 ms / 2 ms wall thresholds.

These are wall-clock observations, not new intrinsic CPU attribution. The prior administrator ContextSwitch ETW remains authoritative for M2 closure. The failures were not deleted, split, waived, or used to change production Runtime.

## BuildPlugin, clean host, and Shipping isolation

Authoritative BuildPlugin package:

`C:\Users\defiler919\AppData\Local\Temp\SightWeaveM3P1_BuildPlugin_Final3`

- UAT `BuildPlugin -TargetPlatforms=Win64` succeeded from the committed source state.
- UAT performed real source builds for UnrealEditor Win64 Development, UnrealGame Win64 Development, and UnrealGame Win64 Shipping.
- The only compiler warnings were the non-preferred VS 14.51 toolchain notice and C4996 warnings originating in UE 5.8 engine headers; no SightWeave source warning or error was emitted.
- The package contains `Shaders/Private/SightWeaveSingleTile.usf` at 3,304 bytes.
- Full SHA-256 comparison of packaged `Source` and `Shaders` against the repository returned zero differences.

Independent packaged host:

`C:\Users\defiler919\AppData\Local\Temp\SightWeaveM3P1_PackagedHost_Final3`

- UnrealEditor Win64 Development rebuilt all four packaged modules from source and succeeded.
- UnrealGame Win64 Development consumed the packaged precompiled Game products and succeeded.
- UnrealGame Win64 Shipping consumed the packaged precompiled Shipping products and succeeded.
- D3D12/SM6 loaded and compiled the packaged shader mapping, then passed all 29 M3.1 tests.

Shipping/package audit:

- Shipping intermediate module directories are exactly `SightWeaveRuntime` and `SightWeaveRender`; Editor and Tests modules are absent.
- Runtime/Render Build.cs dependency audit found no forbidden dependency and confirmed Runtime has no reverse Render dependency.
- Binary-string and COFF-symbol scans across Shipping objects returned zero matches for `FSightWeaveRenderTestReadback`, `FRHIGPUTextureReadback`, `AutomationTest`, `SightWeaveTests`, `SightWeaveEditor`, `Darkwell`, `UnrealEd`, and `Advapi32`.
- The test readback translation unit is present as an empty guarded Shipping object, but its `WITH_DEV_AUTOMATION_TESTS` API and implementation symbols are compiled out.
- Runtime and Render DLL import audits returned zero forbidden imports. Render imports only Core/CoreUObject/Engine/Projects/RHI/RenderCore/SightWeaveRuntime plus platform CRT/kernel libraries; Runtime imports only its allowed engine dependencies plus platform CRT/kernel libraries.
- Project PCH or Darkwell dependencies are not used to hide missing public includes; the final BuildPlugin Game builds validated self-contained headers.

## Retained failures and warning classification

Retained development diagnostics include:

- the first shader compile failure before adding the UE 5.8-required `Platform.ush` include;
- early readback/RDG failures in the `SightWeaveM3P1_D3D12_EmptyReadback*` reports while the one-shot bridge, pooled texture ownership, and raster assumptions were corrected;
- `SightWeaveM3P1_NullRHI_CompleteFocused`, which caught an abstract UObject lifecycle test and event-count assumptions;
- timestamp query validation failures while the RDG pass flags and render-query lifetime were corrected;
- the initial BuildPlugin Game compile failure caused by two non-self-contained public headers, retained at `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM3P1_BuildPlugin_Final`;
- `Saved/AutomationReports/SightWeaveM3P1_D3D12_PackagingFinal`, where the first cold BypassUnion absolute timestamp was unavailable once; the independent retry passed 29/29 and the packaged-host run also passed 29/29.

No retained development failure is classified as a performance failure. Final-log scans found no ensure, assert, fatal, critical error, unhandled exception, GPU crash, Device Removed, DXGI error, shader compile error, RDG validation error, resource leak, stale-world access, or teardown crash. UE's informational `TDR settings OK` summary is present in D3D logs and is not a TDR event.

## Git and LFS closure

Reliable implementation checkpoints through `c426283bd2e1cadec6778f0fba40426a5ea766ce` were individually reviewed, explicitly staged, committed, pushed, and verified against the current remote branch. The final documentation commit is the only later tracked change. Final handoff requires local HEAD, upstream, and `git ls-remote` to match, `git diff --check` and `git diff --cached --check` to pass, Git LFS status to contain no pending object, and both `git lfs fsck` and `git fsck --no-reflogs` to pass.

`Darkwell.uproject` remains intentionally modified and unstaged only for the local EngineAssociation GUID.

## Deferred scope and next step

Not validated or implemented in M3.1:

- sparse multi-tile atlas allocation/residency and dirty-tile scheduling;
- simultaneous multi-owner, multi-floor, or multi-profile residency;
- final post-process presentation, point-sampled consumer material, feathering, or visual polish;
- memory/Last-Seen proxy layers or damage-reveal presentation;
- DARKWELL gameplay or `L_Prototype` integration;
- D3D11/Vulkan support;
- warmed persistent-update RT/GPU performance contract.

The precise recommended M3.2 scope is sparse multi-tile allocation/residency and dirty-tile scheduling while preserving the M3.1 immutable packet, world identity, fail-black behavior, single-profile formula, and CPU Authority boundary. Before scaling capacity, add a warmed persistent-resource benchmark that separates resource creation from ordinary dirty-tile updates and investigates the retained RT RDG setup p95 risk.

## Final conclusion

M3.1 is **COMPLETED**. Functional GPU correctness, fail-closed behavior, packaging, clean-host execution, and Shipping isolation are closed. Final GPU performance remains deliberately open for a later M3 performance milestone; the preliminary RT setup miss and cold GPU outlier are explicitly retained rather than hidden.

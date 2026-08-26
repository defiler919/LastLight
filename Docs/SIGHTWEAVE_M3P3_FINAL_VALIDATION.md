# SightWeave M3.3 final validation

## 1. Status

**COMPLETED**

The hard-mask post-process path, correctness/readback matrix, lifecycle isolation, Lab visualization, performance budgets, packaging boundaries, source-only rebuilds, Shipping scans, and Git/LFS closure pass. The only failing automation entries are the two explicitly retained M2P2 wall-time gates.

## 2. Branch, baseline, and final revision

- Branch: `codex/m3p3-sightweave-hard-mask-composite`
- Frozen M3.2 baseline: `ccb4c02c7a0bcbd9295847f16da17985bd8fd39c`
- Final revision: terminal `docs: record SightWeave M3P3 validation` commit; exact SHA is recorded in the final task response and remote-closure check.
- Engine: Unreal Engine 5.8.1, changelist `56057345`, `D:\UE_5.8`.

## 3. Commits

1. `df5ff5f` `docs: start SightWeave M3P3 hard mask presentation`
2. `81f406a` `feat: bind SightWeave presentation scopes`
3. `7292a63` `feat: composite SightWeave hard masks`
4. `6962da1` `test: validate SightWeave screen-space presentation`
5. `4adec58` `test: add SightWeave M3P3 lab coverage`
6. `875e7b4` `perf: measure SightWeave hard mask composite`
7. `98aec3d` `test: validate SightWeave M3P3 packaging boundaries`
8. `docs: record SightWeave M3P3 validation` (this checkpoint)

Every reliable checkpoint was pushed. There was no merge, rebase, force-push, `Darkwell.uproject` change, generated-directory commit, or prototype-map edit.

## 4. RTX 2070 SUPER identity

All M3.3 GPU evidence is from NVIDIA GeForce RTX 2070 SUPER (Turing), 8192 MiB, Studio Driver 610.88. UE selected D3D12 adapter 0, feature level 12_2, shader model 6.7, with SM6 forced. No RTX 4060 data is mixed into these distributions.

## 5. Post-process injection and UE 5.8 evidence

`FSightWeaveSceneViewExtension::SubscribeToPostProcessingPass` registers an after-pass callback only for `ISceneViewExtension::EPostProcessingPass::Tonemap` and only while presentation is enabled.

UE 5.8.1 `Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp` maps the public extension pass to `EPass::Tonemap` at line 561, executes `AddAfterPass(EPass::Tonemap, SceneColor)` at line 1647 after `AddTonemapPass`, then executes the `BL_SceneColorAfterTonemapping` material chain at lines 1667-1676. The callback therefore consumes post-tonemap Scene Color at the supported engine extension point immediately before the named after-tonemapping material chain. No Engine shader or SceneCapture is modified.

## 6. Immutable presentation-scope contract

The selection/binding validates world lifetime serial, knowledge owner, floor ID/origin, precision, the complete canonical compatibility-profile sequence, explicit already-unioned EffectiveLiveMask semantics, atlas resource and residency generations, packet/registry/published-snapshot revisions, and presentation revision. Canonical profile equality is authoritative. Artificial hash-collision tests prove an equal hash cannot substitute for the full profile sequence. Each view can bind only its exact world/owner/floor/precision scope.

## 7. Screen, world, tile, slot, and page mapping

The production shader maps output to Scene Color and depth rectangles, integer-loads SceneDepth, reconstructs translated world position with `SvPositionToTranslatedWorld`, subtracts translated floor origin, and uses `floor(LocalPosition / InteriorSpan)` for negative coordinates. It binary-searches `StructuredBuffer<int4>`, decodes page/slot, computes the 8x8 slot origin in a 2048x2048 page, maps the 248x248 interior, adds the 4-texel gutter, and integer-loads PF_G8. Tests cover origin, negative/large coordinates, camera translation/zoom, orthographic view, 0/45/90 rotation, FOV/OrthoWidth, 1080p/1440p, resize, and resolution scale. Eight repeated camera-only setups left page-table uploads at one.

## 8. Hard composite rule

```text
HardLive >= 0.5: post-tonemap Scene Color
otherwise:        float4(0, 0, 0, 0)
```

PF_G8 contains binary 0/1 (0/255 storage semantics). No bilinear eligibility, feather, gray memory, BaseColor/Normal reconstruction, temporal history, minimum brightness, noise, dithering, or blur participates.

## 9. Fail-closed behavior

An enabled view clears black for unsupported RHI/platform/PF_G8, invalid shader/resource state, missing selection/binding/page table/atlas, incomplete residency, capacity uncertainty, wrong world/owner/floor/profile, stale/conflicting revision, generation mismatch, eviction, nonresident tile, and teardown. It never reuses old world/scope data or SceneCapture. A disabled presentation registers no callback and is distinct from enabled-but-invalid fail-black.

## 10. Lifecycle and teardown

Automation covers SVE registration/unregistration, PIE create/destroy, restart under a new world serial, delayed render commands, stale bindings, old-world rejection, module shutdown, and matching-world release. M3.3 NullRHI 7/7 and D3D12/SM6 19/19 completed without teardown crash, stale-world access, or leaked state.

## 11. Seam, gutter, slot, and page results

CPU mapping and GPU readback cover tile center/edges/corners, horizontal/vertical/diagonal seams, four-tile corner, page 0/1, slot 63/64, 4-texel gutter, negative logical tiles, nonresident/evicted/capacity failure, and stale generations. `PageBoundarySlot63And64` and `TileSeamsAndFourTileCorner` passed with no bright seam or neighboring-slot contamination.

## 12. Asynchronous GPU final-output readback

| Case | Samples | GPU us | Result |
| --- | ---: | ---: | --- |
| BasicHardComposite | 3 | 5 | live color retained; black RGB zero |
| NegativeLogicalTiles | 3 | 6 | negative mapping correct |
| OldWorldFailsBlack | 1 | 1 | old world rejected black |
| PageBoundarySlot63And64 | 4 | 6 | page/slot boundary correct |
| TileSeamsAndFourTileCorner | 4 | 4 | no seam/corner leakage |
| WrongScopeFailsBlack | 1 | 0 | wrong owner rejected black |

Readback and timestamp helpers exist only under `WITH_DEV_AUTOMATION_TESTS`.

## 13. Lab visual inspection

`/SightWeave/Maps/L_SightWeave_Lab` was updated only through idempotent Unreal Editor Python. The LFS map has a separate M3.3 area with straight, L, T, diagonal, multi-tile seam, page 0/1 and slot 63/64 fixtures plus cameras. `L_Prototype` is untouched.

The final Game run used D3D12/SM6 at 1920x1080, entered play, reported authoritative live bypass state, saved `Saved/Screenshots/WindowsEditor/HighresScreenshot00001.png`, and exited cleanly. The agent visually inspected the actual render and observed strict black masking, retained visible Scene Color line work, no gray/soft transition, and no obvious bright seam. This is not a claim that the user operated an interactive viewport.

## 14. Cold and warmed performance

Unrelated experimental toolset plugins were disabled. No remote desktop, NVIDIA overlay, or recorder was intentionally active. Warmed distributions use 64 full-frame samples; GT binding uses 2,048. Cold GPU total includes initial mask raster/resource work plus composite; warmed GPU is composite-only no-change work.

| Resolution | Sources | Tiles | GT binding p95 us | GT submit us | RT bind us | Cold RT setup us | Cold GPU total us | Warm view p95 us | Warm composite setup p95 us | Warm GPU p95 us |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1920x1080 | 2 | 1 | 0.201 | 1.099 | 0.998 | 746.898 | 57 | 1.002 | 3.599 | 41 |
| 1920x1080 | 8 | 8 | 0.201 | 1.401 | 1.099 | 786.200 | 83 | 1.300 | 4.496 | 62 |
| 1920x1080 | 32 | 128 | 0.201 | 1.200 | 1.200 | 1675.799 | 366 | 1.200 | 4.400 | 327 |
| 2560x1440 | 2 | 1 | 0.201 | 1.300 | 0.998 | 926.100 | 87 | 1.203 | 3.900 | 319 |
| 2560x1440 | 8 | 8 | 0.201 | 1.702 | 0.902 | 708.800 | 122 | 1.200 | 4.299 | 97 |
| 2560x1440 | 32 | 128 | 0.201 | 1.200 | 0.902 | 1357.000 | 426 | 1.099 | 4.198 | 399 |

All warmed results pass the 1.0 ms 1080p, 1.5 ms 1440p, 2.0/3.0 ms pressure, 0.25 ms GT, and 0.20 ms RT setup targets. The deterministic full-frame harness reuses the production page-table/atlas hard-mask kernel; the real Lab render separately proves production SceneDepth/view integration. Unchanged M3.2 packet-build p95 is 4.701/8.103/24.799 us for 2/8/32 sources; immutable packet enqueue p95 remains below 0.5 us.

## 15. GPU memory and allocation stability

| Tiles | Pages | Persistent bytes | Transient 1080p | Transient 1440p |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 1 | 4,390,928 | 8,294,400 | 14,745,600 |
| 8 | 1 | 4,391,040 | 8,294,400 | 14,745,600 |
| 128 | 2 | 8,587,264 | 8,294,400 | 14,745,600 |

Persistent bytes include lazy PF_G8 pages, three 256x256 PF_G8 scratch textures, and `int4` page table. The 128-tile high-water is 8.19 MiB, below 32 MiB. Every 64-sample warmed series held page-table uploads at 1, page/scratch allocations at 1-or-2/3, and resource generation at 2-or-3: no post-warmup growth occurred.

## 16. Exact automation counts

| Suite | Result |
| --- | --- |
| M3.3 NullRHI | 7/7 |
| M3.3 D3D12/SM6 | 19/19 |
| M3.3 GPU readback subset | 6/6 |
| M3.3 performance | 6/6 |
| M3.3 packaging | 1/1 |
| M3.3/M2/M1 Lab fixtures | 1/1 each |
| M3.1 D3D12/SM6 | 29/29 |
| M3.2 D3D12/SM6 | 22/22 |
| Full SightWeave NullRHI | 140/142; two retained M2P2 wall-time failures |
| DARKWELL | 24/24 |
| Clean-host M3.3 D3D12/SM6 | 12/12 |

## 17. BuildPlugin, clean host, and Shipping

UAT output: `Saved/SightWeaveM3P3_BuildPlugin_20260826`.

- `BUILD SUCCESSFUL` after Editor Development, Game Development, and Game Shipping.
- 94 Source/Shader files: zero package missing, zero SHA-256 mismatch.

Source-only host: `Saved/SightWeaveM3P3_CleanHost_20260826`.

- Began with zero Binary/Intermediate directories and zero assets.
- Editor rebuilt 80 actions/all four modules; explicit-plugin Development and Shipping rebuilt 26 Runtime/Render actions each.
- M3.3 D3D12/SM6 readback/performance passed 12/12.

Shipping has exactly `SightWeaveRuntime,SightWeaveRender`; Editor/Tests are absent. Twenty-six objects and 28 response/dependency/precompiled files were inspected. Precise binary-string and COFF-symbol matches are zero for presentation readback, benchmark, GPU readback, test shader entry points, and M3.3 automation names. Runtime imports Core/CoreUObject/Engine/DeveloperSettings plus platform libraries; Render imports Projects/RHI/RenderCore/Renderer/Core/CoreUObject/Engine/SightWeaveRuntime plus platform libraries. Build/source checks exclude Tests, Editor, UnrealEd, Darkwell, AutomationTest, ETW, and SceneCapture dependencies.

## 18. Retained failures and warnings

No threshold was lowered and no sample was deleted.

- `Batch512Gate`: worst median/p95/p99/max 154.600/174.101/196.002/205.901 us; all ten distributions exceed the frozen 150 us median wall-time gate; steady capacity growth is zero.
- `PreparedEventIndex4096`: median/p95/p99/max total 1503.401/1526.900/1844.097/2134.398 us, above 1 ms. Candidate/sort/acceleration medians are 86.300/269.700/97.401 us. Prior administrator ContextSwitch ETW remains intrinsic authority.
- The first performance run had two fixture failures because integer partitioning crossed negative tile -1/0. The fixture was corrected to reuse tile 0; the complete authoritative rerun passed 6/6. The failed report remains in `Saved/Automation/M3P3PerformanceD3D12`.
- Initial clean-host Game calls wrote only metadata without explicit plugin forcing and are not counted. Explicit `-plugin` reruns performed 26 actions each.
- MSVC 14.51.36256 is newer than UE's preferred 14.50.35717. C4996 warnings originate in UE headers. Non-Win64 SDK notices are incidental.
- Severe authoritative-log scans found zero assertion, ensure, fatal/critical error, unhandled exception, GPU crash, device removal, DXGI error, shader error, RDG validation failure, resource leak, or stale-world access.

## 19. Unvalidated items

There is no user-operated interactive viewport, RTX 4060 rerun, D3D11/Vulkan run, Shipping-frame GPU capture, DARKWELL `L_Prototype` integration, or Fab validation. These are outside or prohibited by M3.3. The agent did directly inspect the actual final D3D12/SM6 Game screenshot.

## 20. Remaining risks

- Development timestamps are not a substitute for representative Shipping gameplay-frame capture after gameplay integration is authorized.
- The full-frame benchmark uses deterministic world positions with the production lookup/composite kernel rather than timing real depth reconstruction separately; production reconstruction is covered by source contract tests and the Lab render.
- Inward-only feather and memory/Last-Seen remain deferred and must preserve hard eligibility.
- UE minor-version changes must revalidate Tonemap callback ordering.

## 21. Git and LFS closure

Final checks require a clean worktree, `git diff --check`, `git lfs status`, `git lfs fsck`, `git fsck --no-reflogs`, unchanged `Darkwell.uproject`, local HEAD equal to upstream, and `git ls-remote` equal to the documentation commit. The Lab `.umap` remains LFS-managed and checkpoint `4adec58` uploaded its object.

## 22. Documents

- `Docs/SIGHTWEAVE_M3P3_HANDOFF.md`
- `Docs/SIGHTWEAVE_M3P3_FINAL_VALIDATION.md`
- Frozen references: `Docs/SIGHTWEAVE_M3_GPU_MASK_CONTRACT.md`, `Docs/SIGHTWEAVE_M3_GPU_MASK_ARCHITECTURE.md`, `Docs/SIGHTWEAVE_M3_GPU_MASK_VALIDATION_PLAN.md`

## 23. Recovery command

```powershell
git switch codex/m3p3-sightweave-hard-mask-composite
git pull --ff-only
```

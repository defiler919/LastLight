# SightWeave M3.3 hard-mask presentation handoff

Status: **COMPLETED**

Branch: `codex/m3p3-sightweave-hard-mask-composite`

Frozen M3.2 baseline: `ccb4c02c7a0bcbd9295847f16da17985bd8fd39c`

Engine: Unreal Engine 5.8.1, changelist `56057345`, at `D:\UE_5.8`

Validated GPU: NVIDIA GeForce RTX 2070 SUPER, Turing, 8192 MiB, Studio Driver 610.88

## Outcome

M3.3 closes the requested path:

`persistent sparse PF_G8 EffectiveLiveMask -> SceneDepth world reconstruction -> floor-relative logical tile -> resident page/slot -> post-tonemap hard composite`

Live pixels retain post-tonemap Scene Color; non-live, nonresident, stale, mismatched, unsupported, or otherwise invalid presentation pixels produce exact black. GPU data remains presentation-only and is not gameplay, AI, HUD, interaction, memory, Last-Seen, reveal, or save authority.

The world-scoped `FWorldSceneViewExtension` subscribes to `EPostProcessingPass::Tonemap`. UE 5.8.1 `PostProcessing.cpp` maps that public pass to the renderer Tonemap pass, executes `AddAfterPass(EPass::Tonemap, SceneColor)`, and only afterwards begins `BL_SceneColorAfterTonemapping`. No global Engine shader, SceneCapture, second visibility solve, or camera-driven atlas update was introduced.

## Contract and implementation

`FSightWeaveViewPresentationSelection` and `FSightWeaveViewPresentationBinding` are immutable value contracts. A valid binding carries world lifetime identity, owner, floor and origin, precision, the complete canonical compatibility-profile sequence, explicit effective-union semantics, resource/residency generations, packet/registry/snapshot revisions, and presentation revision. Full profile equality is authoritative; a hash is never accepted as identity.

The shader maps output to SceneColor/depth rectangles, loads SceneDepth, calls `SvPositionToTranslatedWorld`, subtracts the translated floor origin, uses `floor` for negative-safe logical coordinates, binary-searches a persistent `int4` page table, maps 64 slots per 2048-square page, adds the 4-texel gutter, and performs integer `Texture2D.Load`. No bilinear authority, temporal history, feather, gray memory, minimum brightness, noise, or blur is present.

An enabled but invalid presentation fails black. A disabled presentation registers no Tonemap callback and remains normal Scene Color pass-through. World teardown releases only the matching world serial; stale delayed commands and old-world selections cannot bind to a restarted world.

## Validation summary

- `DarkwellEditor Win64 Development`: passed after final C++ changes.
- M3.3 NullRHI 7/7; M3.3 D3D12/SM6 19/19; deterministic final-output GPU readback 6/6.
- M3.1 D3D12/SM6 29/29; M3.2 D3D12/SM6 22/22.
- Full SightWeave NullRHI 140/142, with only the two frozen M2P2 wall-time failures retained; DARKWELL 24/24.
- Lab M1 load, M2 component, and M3.3 fixture tests passed. A final D3D12/SM6 Game run loaded the Lab, entered play, produced a 1920x1080 screenshot, and exited cleanly. The agent visually inspected the actual rendered image; no user-operated interactive viewport was claimed.
- RTX 2070 SUPER warmed composite p95: 41/62/327 us at 1080p and 319/97/399 us at 1440p for 1/8/128 resident tiles and 2/8/32 sources. All pass the frozen targets.
- Maximum persistent mask allocation: 8,587,264 bytes at 128 tiles/two pages. Warmed view changes caused zero page-table upload, atlas/scratch allocation, or resource-generation growth.
- UAT BuildPlugin passed Editor Development, Game Development, and Game Shipping. Package/source comparison covered 94 Source/Shader files with zero missing or mismatched SHA-256 values.
- Independent source-only host rebuilt Editor (80 actions), Game Development (26), and Game Shipping (26), then passed M3.3 D3D12/SM6 12/12.
- Shipping contains only Runtime and Render module directories. Precise test/readback/benchmark binary-string and COFF-symbol matches are zero in 26 objects; imports contain only declared engine/runtime and platform libraries.

## Retained warnings and exclusions

The full SightWeave run retains `Batch512Gate` and `PreparedEventIndex4096`; no threshold or sample changed. MSVC 14.51.36256 is newer than UE's preferred 14.50.35717, and UAT C4996 warnings originate in UE 5.8 headers. Non-Win64 SDK notices are irrelevant to this Win64-only milestone. Severe final-log scans found no assertion, ensure, fatal error, unhandled exception, GPU crash, device removal, DXGI error, shader compile failure, RDG validation failure, resource leak, or stale-world access.

M3.3 excludes gray memory, Last-Seen, subject proxies, damage reveal, formal feathering, noise/fog animation, final art grading, DARKWELL gameplay or `L_Prototype` integration, SceneCapture, GPU visibility solving, D3D11, Vulkan, and Fab packaging.

Detailed evidence: `Docs/SIGHTWEAVE_M3P3_FINAL_VALIDATION.md`.

## Recovery

```powershell
git switch codex/m3p3-sightweave-hard-mask-composite
git pull --ff-only
```

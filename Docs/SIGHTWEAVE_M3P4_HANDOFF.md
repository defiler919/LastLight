# SightWeave M3.4 inward-feather handoff

Status: **COMPLETED — integrated Lab repair validated on Windows D3D12/SM6**

Repair branch: `codex/m3p4-sightweave-lab-repair`

Original implementation branch: `codex/m3p4-sightweave-inward-feather`

Frozen M3.3 baseline: `35411db9b193ec6a669278af6e43e38fa72f9d9a`

Engine: Unreal Engine 5.8.1, changelist `56057345`, at `D:\UE_5.8`

Validation GPU: NVIDIA GeForce RTX 2070 SUPER, Turing, 8192 MiB, Studio Driver 610.88

## 2026-08-27 integrated Lab correction

Company-machine interactive PIE exposed that the committed Lab could render almost entirely black even with `VisualFeatherWidthCentimeters=0`. This invalidates the previous `COMPLETED` disposition for the integrated Lab presentation path. The shader-level readback, performance, and packaging evidence remains useful, but byte-identical black screenshots are not proof of a working scene composite.

Root cause: `SW_M3P3_PageBoundaryVision` was centered at Y=11000 cm. Its 160000 cm, 0.2-degree cone straddled the logical-row boundary at Y=10860 cm. The polygon AABB therefore requested approximately 65 columns by 2 rows (130 tiles), exceeding the frozen Standard active-tile capacity of 128. The render packet correctly failed closed. M3.4 also shared `Local/Ground` with every historical M2/M3 fixture, so unisolated PIE accumulated unrelated source bounds.

The repair:

- keeps the immutable 128-tile capacity and all authority contracts unchanged;
- moves the page-boundary strip to Y=12100 cm, the center of logical row 7;
- applies editor-only Lab milestone isolation in PIE, defaulting to M3P4;
- preserves only the M3P4 fixtures plus the shared M3P3 page-boundary fixture;
- binds the selected milestone overview camera once a PIE controller exists;
- explicitly registers `Project Settings > Plugins > SightWeave`;
- logs desired tile count, capacity, packet failure, recovery, and presentation state;
- adds regression coverage for the old 130-tile failure, corrected bounded footprint, fixture routing, and settings registration.

The first integrated repair exposed two additional production issues that were then closed on this branch:

- render-thread pending packets could overwrite the first full-dirty packet before it was consumed, leaving an incremental packet with no atlas resources; coalescing now forces a full rebuild whenever an unapplied pending packet is replaced or no applied packet exists;
- the first real post-process submission used a screen-pass vertex input layout with a vertex-ID-only shader and D3D12 rejected the PSO with `E_INVALIDARG`; Hard and Feather composites now use an explicit `SV_VertexID` fullscreen triangle with an empty vertex declaration.

The company UE 5.8.1 retest now closes the former `PARTIAL` gate: Editor Development rebuilt successfully, the Lab published 113/128 resident tiles, the post-process submitted both Hard and Feather paths with `bindingFailure=0`, user-operated PIE visibly distinguished 0/50/100 cm, and the page-boundary and rotated cameras showed no bright seam or outward SceneColor leak.

## Objective

M3.4 adds world-space inward-only visual feathering above the immutable M3.3 hard presentation gate:

```text
FinalPresentationWeight = HardLivePointSample * VisualFeatherWeight
OutputColor = SceneColor * FinalPresentationWeight
```

HardLive remains the sole presentation-eligibility authority. VisualFeather is presentation-only, may only reduce already-live Scene Color, and cannot feed gameplay, AI, HUD, interaction, lock-on, memory, Last-Seen, or save state.

## Starting disposition

Local HEAD, upstream, and the remote M3.3 branch all matched the frozen baseline. The worktree was clean, `git diff --check` passed, Git LFS had no pending objects, `git lfs fsck` passed, and `Darkwell.uproject` was unchanged.

Algorithm selection, rejected routes, resource policy, failure behavior, implementation evidence, retained failures, and recovery instructions will be recorded as reliable checkpoints complete.

## Frozen M3.4 algorithm contract

The selected route is a world-texel, dirty-tile-derived `VisualFeather` atlas produced by a bounded jump-flood distance transform. Seed generation resolves every sampled logical texel through the hard atlas page table; physical slot adjacency is never interpreted as world adjacency. Two fixed-size transient seed textures are reused serially. The final derived PF_G8 page mirrors hard residency addresses but remains a distinct presentation-only resource family.

For each affected tile, seed initialization covers the 248x248 interior plus a halo sized from the bounded maximum world width. Hard-zero texels are distance seeds. Fixed-count jump-flood passes propagate the nearest seed. A final pass writes only the resident tile interior, clears the complete physical slot before reuse, clamps weight to `[0,1]`, and writes zero wherever the authoritative point-sampled HardLive is zero.

Presentation uses manual world-logical bilinear reconstruction of the continuous VisualFeather value, then reapplies the exact M3.3 point gate:

```text
HardLivePointSample == 0: FinalPresentationWeight = 0
HardLivePointSample == 1: FinalPresentationWeight = saturate(VisualFeather)
OutputColor = SceneColor * FinalPresentationWeight
```

`FeatherWidthCm == 0` selects the unchanged M3.3 shader path, does not allocate VisualFeather pages or transform scratch, and schedules no feather update.

### Alternatives reviewed

| Candidate | Safety/stability | Cost/update behavior | Decision |
| --- | --- | --- | --- |
| screen-space neighborhood blur | can bleed nonzero values into hard-black pixels; width varies with depth, resolution, and camera/TAA motion | full-screen multi-tap every frame | rejected |
| direct world-space multi-tap in composite | hard re-gating is safe and world-stable, but high quality requires many bounded page-table lookups per output pixel | cost scales with full-screen pixels even when HardLive is unchanged | rejected as production route; retained as conceptual oracle only |
| dirty-tile derived VisualFeather atlas | world-stable, hard-re-gated, logical-neighbor aware | local work only after hard/settings changes; cheap warmed composite | selected |
| bounded distance transform | produces monotonic inward distance but exact serial transforms are awkward across sparse physical slots | suitable when scoped to an expanded logical work tile | selected through bounded jump flood |
| separable blur/erosion | axis-biased corners and repeated passes; blur alone can illuminate hard-black pixels unless re-gated | radius-dependent pass count | rejected |
| jump flood / approximate distance field | bounded pass count, supports corners and wide radii; approximation needs monotonic/seam tests | fixed transient scratch and dirty-tile passes | selected |

### Frozen bounds and quality

- width is configured in world centimeters and clamped to `0..100 cm`;
- the standard development value is `50 cm`;
- quality may alter transform resolution/pass refinement only, never HardLive or the final point gate;
- the work halo is derived from the maximum safe width and active precision tier;
- all loops and pass counts are compile-time or configuration-bounded;
- camera motion, FOV, resize, screen percentage, and temporal jitter schedule zero hard/feather atlas work.

### Dirtying, identity, and failure

Hard dirty and removed logical tiles expand by `ceil(FeatherWidthCm / InteriorWorldSpan)` in every logical direction. A width/quality change fully re-derives only the selected resident scope. No-change packets may advance matching provenance without GPU work only when hard content and residency are unchanged.

VisualFeather binding includes world serial, owner, floor, precision, full canonical profiles, packet/registry/snapshot/presentation revisions, hard resource/residency generations, feather resource generation, feather applied packet revision, and feather settings revision. Any mismatch, incomplete dirty work, unavailable shader/format/resource, teardown, stale command, or uncertain logical-neighbor state makes the enabled view fail black. Old feather is never substituted.

At the Standard 128-tile ceiling, hard pages (8 MiB) plus matching PF_G8 feather pages (8 MiB), existing mask scratch, page tables, and two bounded jump-flood scratch surfaces remain below the frozen 32 MiB persistent-live-presentation budget. Exact measured allocation remains an implementation gate.

## Completed implementation

- A distinct PF_G8 VisualFeather atlas mirrors hard residency addresses without becoming authority.
- Two reusable 328x328 PF_G32R32F scratch textures run the bounded transform serially.
- The jump sequence starts at the next power of two for the active world-width radius (50 cm Standard: 32; 100 cm: 64), rather than the maximum work-surface dimension.
- Hard dirty/removed tiles mark logical neighbors; derived work resolves all cross-tile samples through the page table.
- Slots are cleared before derive/reuse. Eviction, teardown, incomplete work, and provenance mismatch invalidate Feather and fail black.
- The post-tonemap shader point-samples HardLive first, then manually bilinearly reconstructs VisualFeather in logical world texels.
- Width zero releases/avoids all Feather resources and selects the unchanged M3.3 composite shader.

## Validation disposition

Authoritative results and exact logs are recorded in `Docs/SIGHTWEAVE_M3P4_FINAL_VALIDATION.md`. Highlights:

- Original M3.4 NullRHI: 5/5; D3D12/SM6: 29/29; clean-host D3D12/SM6: 29/29.
- Post-repair company rerun: M3.4 D3D12/SM6 37/37 and NullRHI 8/8, with reports and logs recorded in the final validation document.
- GPU safety readback: 8/8, hard-zero RGB leaks 0, nonfinite 0, monotonic violations 0, seam discontinuities 0, width-zero mismatch 0.
- Performance matrix: 21/21. Maximum warmed total GPU p95 669 us; maximum RT Feather setup p95 80.802 us; maximum persistent GPU memory 18,697,216 bytes.
- M3.1/M3.2/M3.3: 29/29, 22/22, 19/19. DARKWELL: 24/24.
- Full SightWeave NullRHI: 145/147; only the two frozen M2P2 wall-time gates remain failed.
- BuildPlugin and independent source-only Editor Development, Game Development, and Game Shipping builds succeeded.
- Shipping contains only Runtime/Render modules; exact development readback/benchmark/test shader strings and COFF symbols are absent.

The seven original automated D3D12 screenshots remain retained evidence. The subsequent user-operated company PIE session added readable close-up comparisons at 0/50/100 cm and fixed-camera page-boundary/45-degree views. Width zero was a hard edge, 50 cm produced a narrow inward ramp, and 100 cm produced a visibly wider inward ramp. Black SceneColor outside HardLive remained black; cyan/green/thin fixture lines are development debug overlays rendered after the presentation pass. GPU readback remains the authoritative zero-leak and monotonicity proof.

## Resume

```powershell
cd D:\UE_projects\LastLight
git fetch origin
git switch codex/m3p4-sightweave-lab-repair
git status --short --branch
git pull --ff-only
```

Do not integrate with `L_Prototype`, gameplay visibility, memory/Last-Seen, D3D11, Vulkan, or SceneCapture without a separately authorized milestone.

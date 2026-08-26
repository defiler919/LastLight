# SightWeave M3.4 inward-feather handoff

Status: **IN_PROGRESS**

Branch: `codex/m3p4-sightweave-inward-feather`

Frozen M3.3 baseline: `35411db9b193ec6a669278af6e43e38fa72f9d9a`

Engine: Unreal Engine 5.8.1, changelist `56057345`, at `D:\UE_5.8`

Validation GPU: NVIDIA GeForce RTX 2070 SUPER, Turing, 8192 MiB, Studio Driver 610.88

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

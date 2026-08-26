# SightWeave M3.4 final validation

## 1. Status

**IN_PROGRESS**

M3.4 validation evidence will be added without deleting slow samples, weakening thresholds, or borrowing results from pre-change binaries.

## 2. Frozen baseline

- Branch: `codex/m3p4-sightweave-inward-feather`
- Frozen M3.3 baseline: `35411db9b193ec6a669278af6e43e38fa72f9d9a`
- Engine: Unreal Engine 5.8.1, changelist `56057345`
- GPU: NVIDIA GeForce RTX 2070 SUPER, 8192 MiB, Studio Driver 610.88

## 3. Safety invariant

```text
FinalPresentationWeight = HardLivePointSample * VisualFeatherWeight
OutputColor = SceneColor * FinalPresentationWeight
```

HardLive zero must always produce exact black. Feather width zero must remain pixel-identical to M3.3 and require no feather resource or work.

## 4. Frozen algorithm selection

M3.4 selects a dirty-tile-derived world-texel VisualFeather atlas using a bounded jump-flood distance transform. It rejects screen-space blur, full-screen direct multi-tap, separable blur/erosion, and any route that treats physical slots as logical neighbors, rebuilds the whole atlas every frame, samples SceneCapture/GBuffer color as authority, or can present a weight outside HardLive.

The complete comparison, world-width definition, dirty expansion, resource estimate, provenance requirements, and failure rules are frozen in `Docs/SIGHTWEAVE_M3P4_HANDOFF.md` before implementation begins.

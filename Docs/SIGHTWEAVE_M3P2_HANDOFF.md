# SightWeave M3.2 sparse-atlas handoff

Status: **IN_PROGRESS**

Branch: `codex/m3p2-sightweave-sparse-atlas-residency`

Frozen M3.1 baseline: `a96e85f9c0ac32400abf81c87f75e2db15cdb36f`

Engine: Unreal Engine 5.8.1, changelist `56057345`, at `D:\UE_5.8`

Home validation GPU: NVIDIA GeForce RTX 2070 SUPER, Turing, 8192 MiB, Studio Driver 610.88

## Starting disposition

The home-machine M3.1 admission gate passed before this branch was created. Local HEAD, upstream, and the remote M3.1 branch all matched the frozen baseline. The worktree was clean, `git diff --check` passed, Git LFS had no pending objects, and `git lfs fsck` passed. `Darkwell.uproject` had no local difference and was not modified.

- `DarkwellEditor Win64 Development`: passed using UE's bundled .NET 10 SDK.
- M3.1 NullRHI: 10/10 passed.
- M3.1 D3D12/SM6 cold process: 29/29 passed.
- M3.1 D3D12/SM6 warmed process: 29/29 passed.
- D3D12 selected the RTX 2070 SUPER with Feature Level SM6, shader platform `PCD3D_SM6`, maximum hardware feature level 12_2, and shader model 6.7.
- The cold process compiled all four SightWeave global shader types once.
- Each D3D process completed 21 asynchronous 256 x 256 `PF_G8` readbacks. Every completed readback used row pitch 256, contained only 0/255 texels, and produced zero CPU/GPU non-boundary or boundary-class mismatches.
- Duplicate, stale revision, explicit clear, source deletion, world restart/teardown, suppression-last, gutter-edge, and asynchronous stale-readback rejection passed.
- Final severe-log scans found no ensure, assertion, fatal, unhandled exception, device removal, DXGI error, GPU crash, shader compile error, RDG validation error, RHI validation error, or automation failure. UE's startup `UnifiedErrorTest` messages remain classified as engine self-diagnostics outside the selected test results.

Retained ignored evidence:

- `Saved/AutomationReports/SightWeaveM3P1_Home2070_NullRHI_20260826`
- `Saved/AutomationReports/SightWeaveM3P1_Home2070_D3D12_Cold_20260826`
- `Saved/AutomationReports/SightWeaveM3P1_Home2070_D3D12_Warmed_20260826`
- `Saved/Logs/SightWeaveM3P1_Home2070_NullRHI_20260826.log`
- `Saved/Logs/SightWeaveM3P1_Home2070_D3D12_Cold_20260826.log`
- `Saved/Logs/SightWeaveM3P1_Home2070_D3D12_Warmed_20260826.log`

## Independent Turing measurements

These samples are RTX 2070 SUPER admission evidence only and are not mixed with the RTX 4060 distribution. The M3.1 readback harness creates a fresh one-shot render state and resources per case; its second process is process/DDC-warmed but is not a persistent-resource update benchmark.

| Metric, 21 samples | Cold p50 / p95 / p99 / max us | Warmed-process p50 / p95 / p99 / max us |
| --- | ---: | ---: |
| GT submit | 4.400 / 10.300 / 26.599 / 26.599 | 5.599 / 7.398 / 23.600 / 23.600 |
| RT consume | 0.600 / 0.902 / 1.002 / 1.002 | 0.402 / 0.801 / 0.902 / 0.902 |
| RT RDG setup | 358.302 / 493.500 / 585.202 / 585.202 | 327.203 / 433.501 / 464.499 / 464.499 |
| GPU mask graph | 19.000 / 23.000 / 25.000 / 25.000 | 18.000 / 21.000 / 23.000 / 23.000 |
| Async readback end-to-end | 8519.202 / 9348.202 / 27881.898 / 27881.898 | 8480.299 / 9383.198 / 16317.002 / 16317.002 |

Warmed-process packet-builder p50/p95/p99/max microseconds were:

| Sources | p50 | p95 | p99 | max |
| ---: | ---: | ---: | ---: | ---: |
| 2 | 1.900 | 2.500 | 2.800 | 10.400 |
| 8 | 4.600 | 4.900 | 5.900 | 23.500 |
| 32 | 15.000 | 15.100 | 18.500 | 19.300 |

The one-shot RT RDG setup p95 remains above the proposed 200 us engineering target. This is retained as an M3.2 investigation item, not relabeled as warmed persistent performance.

## Frozen M3.2 boundary

M3.2 preserves:

`CPU-authoritative polygons -> deterministic CPU triangles -> immutable revisioned packet -> SightWeaveRender -> world-scoped persistent sparse PF_G8 atlas -> EffectiveLiveMask`

The hard-mask formula remains complete-profile vision/compatible-illumination intersections, then bypass union, then suppression last. GPU data remains presentation-only and cannot become gameplay, AI, HUD, interaction, memory, Last-Seen, or save authority.

M3.2 implements sparse multi-tile layout, persistent atlas pages, deterministic residency/allocation/reuse/eviction, dirty-tile scheduling from old and new bounds, gutter/seam correctness, scope/revision/world isolation, asynchronous selected-tile readback, and cold-versus-warmed performance/allocation evidence.

M3.2 does not implement final post-process composition, feathering, memory/Last-Seen, last-seen proxies, damage-reveal presentation, DARKWELL gameplay integration, Scene Capture, GPU visibility solving, D3D11, Vulkan, or Fab final-product packaging.

## Frozen layout and starting limits

- format: `PF_G8`, linear R8 UNorm;
- physical tile: 256 x 256 texels;
- interior: 248 x 248 texels;
- gutter: four texels on every side;
- page: 2048 x 2048 texels, 8 x 8 physical slots, 64 slots, 4 MiB;
- Standard comparison tier: 10 cm/texel and 128 active tiles / two pages per resident owner/floor scope;
- logical identity includes world, owner, floor, full canonical profile identity, precision tier, logical tile coordinate, and revision provenance;
- physical slot location is residency state, never logical identity;
- allocation/resource uncertainty and capacity exhaustion fail the affected scope black.

## Implementation checkpoints

1. `docs: start SightWeave M3P2 sparse atlas work`
2. `feat: add persistent SightWeave atlas residency`
3. `feat: schedule SightWeave dirty tiles`
4. `test: validate SightWeave multi-tile GPU parity`
5. `perf: measure warmed SightWeave atlas updates`
6. `test: validate SightWeave packaging boundaries`
7. `docs: record SightWeave M3P2 validation`

Reliable checkpoints are pushed immediately. No merge, rebase, force-push, gameplay change, ordinary-filesystem asset operation, or `Darkwell.uproject` staging is permitted.

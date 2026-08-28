# SightWeave M4P2 final validation

## 1. Status

**COMPLETED.** The frozen M4P1 behavior now has reliable BuildPlugin delivery, independent clean-host builds, clean Shipping boundaries, Cooked/Staged Win64 Shipping evidence, full D3D12/SM6 and NullRHI regression, lifecycle closure, and passing frozen performance gates.

Authoritative execution detail is in `SIGHTWEAVE_M4P2_EXECUTION_REPORT.md`; recovery and compact evidence are in `SIGHTWEAVE_M4P2_HANDOFF.md`.

## 2. Identity and scope

- Milestone: M4P2 — SightWeave Packaging / Shipping / Clean-Host / Performance Closure
- Branch: `codex/m4p2-sightweave-packaging-performance-closure`
- Frozen baseline: M4P1 `93f156f552aa85ee9d30891508d439011c57c479`
- Engine: UE 5.8.1 at `D:\UE_5.8`
- GPU path: RTX 4060, D3D12 / SM6
- Scope: closure only; no gameplay, visual, public API, serialization, or production-asset change

`Darkwell.uproject` retains only the user's unstaged EngineAssociation GUID difference. It was never staged, restored, rewritten, or committed.

## 3. Build and packaging gates

| Gate | Result |
| --- | --- |
| Repository Editor target | passed after relevant C++ changes |
| BuildPlugin `-Rocket -TargetPlatforms=Win64` | passed; UAT exit 0 |
| BuildPlugin Editor / Development Game / Shipping Game | 102/102, 32/32, 32/32 |
| Delivery inventory | 128/128 Source + Shaders + Content + Engine.ini; no missing/extra files |
| Fresh source-isolated clean host | Editor 102/102, Development 32/32, Shipping 32/32 |
| Shipping implementation | exactly Runtime 19 + Render 13 objects |
| Shipping compile definitions | dev automation/editor/editor-only data off; Shipping on |
| COFF/dependency/import/string scans | no forbidden implementation or host dependency |
| Cooked/Staged Shipping | passed BuildCookRun, Pak + IoStore, 494 cooked packages |
| Staged runtime | D3D12/SM6, 56/56 checks, clean teardown and natural exit |

Final BuildPlugin: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_BuildPlugin_Closure_95d59f2_20260828_1748`.

Final clean host: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_CleanHost_Closure_95d59f2_20260828_1805`.

Final archive: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_StagedShipping_Closure_95d59f2_20260828_1955_FinalFix2`.

The archive contains 30 files and 673,112,704 bytes. Main executable SHA-256: `AD2FA409148448002EFA1BF368592E14BA5D974D105A24BF8AD48DB288FB6AAA`.

## 4. Final regression matrix

| Gate | RHI | Result |
| --- | --- | ---: |
| Full SightWeave | NullRHI | 175/175 |
| Full SightWeave | D3D12/SM6 | 267/267 |
| Full DARKWELL | NullRHI | 24/24 |
| M3.4 presentation | NullRHI | 21/21 |
| M3.4 presentation | D3D12/SM6 | 37/37 |
| M3.5 | D3D12/SM6 | 26/26 |
| M4P1 | NullRHI | 9/9 |
| M4P1 | D3D12/SM6 | 12/12 |

Readback lifecycle and LastSeen real-view lifecycle pass in the same process. Final D3D12 full closure produced no fatal, assert, ensure, RDG/shader error, GPU crash, device removal, nonfinite result, stale callback, or cumulative resource-reservation warning.

## 5. Frozen performance gates

| Prepared4096 RHI | Minimum | p50 | p95 | p99 |
| --- | ---: | ---: | ---: | ---: |
| NullRHI | 84.102 us | 119.098 us | 138.599 us | 191.499 us |
| D3D12/SM6 | 68.597 us | 74.700 us | 78.201 us | 79.699 us |

Both pass the unchanged p50 `<1 ms` and p99 `<2 ms` gates.

Batch512 final ten-distribution results pass the unchanged p50 `<=150 us`, p95 `<=180 us`, and p99 `<=200 us` gates. Worst NullRHI p95/p99 was `139.002/145.901 us`; worst D3D12 p95/p99 was `144.098/151.899 us`. Every row reported zero capacity growth. Three physical-core-isolated runs also passed independently.

The exact M3.4 `Width0.1080p.Tiles1Sources2` row passed three independent focused processes with GPU p95 `296 us`, `188 us`, and `950 us`, all below 1 ms. Raw GPU samples are retained. Selected M3.5 10 cm and 25 cm gates, no-change zero-work rules, 32-source pressure limits, persistent-memory limits, and correctness counters also pass.

M4P1 LastSeen operation timing remains baseline-only because its authority defines correctness but no independent frame-time threshold. All baseline runs remained correctness-clean.

## 6. Staged Shipping lifecycle

The final runtime JSON at `Saved/Logs/M4P2/M4P2_ShippingSmoke_FinalFix2.json` records:

- `success=true`, `teardown_complete=true`, `ready_for_screenshot=true`;
- D3D12, SM6 true, visual feather width 50 cm;
- 56 passes and zero failures;
- Runtime/Render subsystem and real GameViewport/camera availability;
- M3.4 presentation, M3.5 static memory/no-change/dirty/clear, and all required M4P1 falling-edge/proxy/suppression/reacquire/identity/tile/unknown paths;
- unregister, render-command drain, resource release, and clean natural process exit.

The visible staged window was also inspected and showed PASS. Its screenshot contains an unrelated Windows Firewall prompt raised by the earlier UAT `UnrealPak` process; no network permission was granted, and deterministic JSON is authoritative.

## 7. Retained attempts

All meaningful failed evidence remains retained. Early performance misses were superseded by isolated core validation and full final closure without changing thresholds. Two Zen staging attempts failed at the UE 5.8 oplog HTTP attachment; the final run used the correct Game packaging config and `-SkipZenStore` while preserving Pak/IoStore/SM6. The first staged fixture excluded its own floor Z from its static-description range; only the ignored neutral fixture was corrected, and FinalFix2 passed without production-code changes.

## 8. Disposition

All M4P2 completion rules are met. No optimization lacking compilation or validation was committed. Generated evidence remains ignored. Final Git/LFS/object integrity and local/upstream/remote equality are performed after the documentation checkpoint and reported with the task handoff.

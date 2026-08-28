# SightWeave M4P2 execution report

## 1. Verdict

**COMPLETED.** Independent plugin packaging, clean-host compilation, Shipping isolation, Cooked/Staged D3D12/SM6 execution, full automation, render/readback lifecycle, frozen performance thresholds, and repository integrity all closed without weakening a gate or changing a frozen product contract.

## 2. Identity and repository discipline

- Branch: `codex/m4p2-sightweave-packaging-performance-closure`
- Frozen M4P1 baseline: `93f156f552aa85ee9d30891508d439011c57c479`
- Engine: UE 5.8.1, `D:\UE_5.8`
- Hardware: RTX 4060 D3D12/SM6, i5-13500, 32 GiB
- Scope: packaging, Shipping, lifecycle, regression, and performance closure only

The only persistent local project-descriptor difference is the user's `Darkwell.uproject` EngineAssociation GUID. That file was never staged, restored, overwritten, formatted, or committed. Generated outputs remain ignored and untracked.

## 3. Reliable implementation checkpoints

| Commit | SHA | State |
| --- | --- | --- |
| docs: start SightWeave M4P2 packaging closure | `f246ce5` | pushed |
| build: close SightWeave plugin packaging boundaries | `f3bb4c7` | pushed |
| test: refresh SightWeave packaging assertions | `f33dd3f` | pushed |
| perf: expand SightWeave closure matrices | `c57b774` | pushed |
| docs: record SightWeave M4P2 final validation | `ac1ece8` | pushed; later superseded |
| fix: close SightWeave automation resource lifetime | `1abafb6` | pushed |
| perf: stabilize SightWeave prepared and batch gates | `0809a1c` | pushed |
| test: retain SightWeave presentation GPU samples | `4dae900` | pushed |
| test: prove SightWeave render lifecycle closure | `a6a5c83` | pushed |
| test: isolate SightWeave batch performance core | `95d59f2` | pushed |

The final documentation commit follows this ledger and is pushed before exact final Git/LFS closure.

## 4. Production-package change

The only production delivery correction placed the already-required CustomDepth invariants in portable plugin config and included that config in BuildPlugin output: `r.CustomDepth=3` and `r.CustomDepthTemporalAAJitter=0`.

This repaired independent-host portability; it did not change SightWeave runtime semantics. Remaining edits were Development-only assertion, measurement, lifecycle, and isolation coverage. No public API, serialization, memory/presentation policy, or production asset changed.

## 5. Final BuildPlugin

Output: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_BuildPlugin_Closure_95d59f2_20260828_1748`

| Target | Actions | Result |
| --- | ---: | --- |
| UnrealEditor Development | 102 | pass |
| UnrealGame Development | 32 | pass |
| UnrealGame Shipping | 32 | pass |
| AutomationTool | — | exit 0, 4m06s |

The package contains 281 files and 626,582,253 bytes. Repository/package delivery mapping across Source, Shaders, Content, and `Engine.ini` is 128/128 with no missing or extra file. Portable `Engine.ini` SHA-256: `20B5D72E3EA4E1409E9997182F140B9E1C45B74AEEF82E2E1E42F1B26F4A54DC`.

Descriptor differences are only standard UAT transforms: installed flag, normalized EngineVersion, and blank packaging metadata.

## 6. Final clean host

Host: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_CleanHost_Closure_95d59f2_20260828_1805`

The host began without generated directories, reparse points, DARKWELL source, or repository path references. It contains only the packaged SightWeave plugin.

| Target | Actions | Time | Result |
| --- | ---: | ---: | --- |
| Editor Development | 102 | 148.63s | pass |
| Game Development | 32 | 49.18s | pass |
| Game Shipping | 32 | 40.12s | pass |

## 7. Shipping isolation

Shipping compiled exactly 32 implementation objects: SightWeaveRuntime 19 and SightWeaveRender 13. Both module definitions have `WITH_DEV_AUTOMATION_TESTS 0`, `WITH_EDITOR 0`, `WITH_EDITORONLY_DATA 0`, and `UE_BUILD_SHIPPING 1`.

Six guarded readback/benchmark objects contain zero forbidden COFF symbol rows. Final source, dependency, object-name, binary-string, content, and PE-import scans found:

- zero forbidden readback/benchmark implementation types;
- zero repository path or DARKWELL references;
- zero `SightWeaveTests` or `SightWeaveEditor` strings in the Shipping executable;
- zero UnrealEd/test/DARKWELL PE imports;
- zero severe UAT log results.

The plugin descriptor inside Pak names the available Editor/Test modules as metadata, but no corresponding Shipping binary, implementation, symbol, or import exists. Generic AutomationTest strings in the monolithic executable are engine metadata, not SightWeave test implementation.

## 8. Full automation closure

| Gate | RHI | Result | Evidence |
| --- | --- | ---: | --- |
| Full SightWeave | NullRHI | 175/175 | `Saved/AutomationReports/M4P2/M4P2_FullSightWeave_NullRHI_Closure_Retry1` |
| Full SightWeave | D3D12/SM6 | 267/267 | `Saved/AutomationReports/M4P2/M4P2_FullSightWeave_D3D12_Closure` |
| Full DARKWELL | NullRHI | 24/24 | retained M4P2 DARKWELL report |
| M3.4 | NullRHI | 21/21 | retained M4P2 M3.4 report |
| M3.4 | D3D12/SM6 | 37/37 | final D3D12 prefix |
| M3.5 | D3D12/SM6 | 26/26 | final D3D12 prefix |
| M4P1 | NullRHI | 9/9 | retained M4P2 M4P1 report |
| M4P1 | D3D12/SM6 | 12/12 | final D3D12 prefix |

The D3D12 full closure ran for 129.24 seconds. Real GameViewport/game views and RHI readbacks were used; SceneCapture was not authority. Readback lifecycle and LastSeen real-view lifecycle pass in one process, and final teardown reports no stale callback or virtual-resource reservation warning.

## 9. Prepared4096 closure

Final 101-repeat measurements:

| RHI | min | p50 | p95 | p99 |
| --- | ---: | ---: | ---: | ---: |
| NullRHI | 84.102 us | 119.098 us | 138.599 us | 191.499 us |
| D3D12/SM6 | 68.597 us | 74.700 us | 78.201 us | 79.699 us |

Both pass the unchanged p50 `<1 ms` and p99 `<2 ms` gates.

## 10. Batch512 closure

All ten distributions pass under both RHIs with zero capacity growth:

- NullRHI median range approximately 89.9–90.4 us; worst p95 139.002 us; worst p99 145.901 us.
- D3D12/SM6 median range 88.599–90.398 us; worst p95 144.098 us; worst p99 151.899 us.

These pass unchanged p50 `<=150 us`, p95 `<=180 us`, and p99 `<=200 us` gates. Three additional independent physical-core-isolated focused processes pass. One representative third run pinned physical core 10 and recorded distribution 0 median 89.899 us, p95 147.302 us, p99 158.802 us, capacity growth zero.

## 11. M3.4 presentation closure

The exact formerly variable row `Width0.1080p.Tiles1Sources2` passed three independent focused processes. GPU p95 results were 296 us, 188 us, and 950 us, all below the unchanged 1 ms gate. Raw samples are retained; GT and RT distributions remained tightly stable. Sparse p99 spikes are retained as diagnostic baselines because this authority defines a p95 gate, not a p99 gate.

All 32-source pressure rows pass the existing 1080p `<2 ms` and 1440p `<3 ms` gates. Width=50 inward feather, hard-zero, nonfinite, seam, stale-command, and binding correctness counters pass.

## 12. M3.5 and LastSeen results

Selected 10 cm and 25 cm memory precision rows pass their frozen CPU, GT, RT, GPU, persistent-memory, and no-change zero-work gates. Expanded resident/dirty scaling rows remain declared pressure baselines where their authority supplies no formal budget.

M4P1 remembered-nochange, reacquire, block/suppress, clear, identity reuse, and page/tile-boundary timings remain baseline-only because M4P1 specifies correctness, not an independent frame-time gate. All associated pixel, nonfinite, ordering, resource, identity, and lifecycle checks pass.

## 13. Cooked/Staged Shipping build

Final archive: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_StagedShipping_Closure_95d59f2_20260828_1955_FinalFix2`

The successful BuildCookRun rebuilt current Editor and Shipping source (7/7 actions), cooked 494 packages, skipped seven platform-inapplicable entries from 501, staged Pak/IoStore, archived, and exited 0 after 287.42 seconds.

UE 5.8's default Zen staging attempted to retrieve an oplog from `[::1]:8558` and failed in two retained earlier attempts. `UProjectPackagingSettings` is Game config, so the final neutral host fixture correctly used `DefaultGame.ini` with `bUseZenStore=False` plus `-AdditionalCookerOptions=-SkipZenStore`. This disabled only the failing Zen staging transport; Shipping, Pak, IoStore, and SM6 remained required and enabled.

Final IoStore evidence: 494 packages, 1,838 chunks, 192.11 MiB, `Zen: 0`, loose-file input 321,842,106 bytes, and both SM6 shader archives with 2,065 host shaders. The archive has 30 files and 673,112,704 bytes. Main executable SHA-256: `AD2FA409148448002EFA1BF368592E14BA5D974D105A24BF8AD48DB288FB6AAA`.

## 14. Staged runtime smoke

Authoritative ignored JSON copied to `Saved/Logs/M4P2/M4P2_ShippingSmoke_FinalFix2.json` records:

- `success=true`;
- `teardown_complete=true`;
- `ready_for_screenshot=true`;
- D3D12 and SM6 true;
- Width=50 cm;
- 56 passes, zero failures;
- Runtime/Render subsystem, real GameViewport, and real camera;
- M3.4 presentation;
- M3.5 packet/static/no-change/dirty/clear;
- M4P1 falling edge, frozen proxy, suppress/restore, reacquire, identity, tile/page, clear, and unknown paths;
- subsystem unregister and render-command drain.

The process exited naturally. The visible window screenshot at `Saved/Screenshots/M4P2/SightWeaveShipping_Closure_95d59f2.jpg` shows PASS, D3D12, SM6 true, Width=50 cm, and its visual check count. A Windows Firewall prompt raised by an earlier UAT `UnrealPak` process overlays the screenshot center; no network access was granted. Deterministic JSON is authoritative.

## 15. Retained failed attempts

Failures were preserved and repaired without hiding evidence:

1. Early independent-host visual failures identified missing portable CustomDepth config. The package fix preserved product behavior and final M4P1 passed.
2. Early Prepared4096 and Batch512 misses were traced through prepared-state and batch-core isolation; final full matrices and three independent focused processes pass unchanged gates.
3. The retained M3.4 1.169 ms p95 process was followed by three independent exact-row passes; no gate changed.
4. Two valid cooks initially failed only at Zen oplog attachment during stage. Correct Game packaging config plus `-SkipZenStore` produced Pak/IoStore Shipping successfully.
5. The first valid staged runtime reported two fixture failures because the neutral fixture's static-description range `-100..250` excluded its configured `FloorPlaneZ=-200`. Production filtering was correct. Only the ignored fixture range was corrected; FinalFix2 passed all 56 checks.
6. Pre-authorization sandbox launches that could not write required UE local state are not product evidence. Authorized serialized runs supersede them.

Engine C4996 and newer-than-preferred MSVC diagnostics remain informational. No fatal, assert, ensure, shader/RDG failure, GPU crash, device removal, or production-code failure remains.

## 16. Final repository closure

The final documentation checkpoint is committed and pushed normally. Closure then runs the exact requested status, diff, SHA, remote, LFS, and Git object-integrity commands. The task handoff reports their exact results and confirms local/upstream/remote equality. Only `Darkwell.uproject` remains modified locally; no generated output is tracked.

## 17. Home recovery

```powershell
git fetch origin
git switch codex/m4p2-sightweave-packaging-performance-closure
git pull --ff-only
```

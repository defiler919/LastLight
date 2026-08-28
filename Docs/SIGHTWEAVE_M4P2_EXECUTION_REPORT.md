# SightWeave M4P2 execution report

## 1. Verdict

**PARTIAL.** Independent packaging, source-isolated builds, Shipping boundaries, D3D12/SM6 correctness, Lab coverage, and expanded baselines are reliable. Completion is withheld because existing performance gates remain unresolved and no Cooked/Staged Shipping game smoke was produced.

## 2. Identity

- Branch: `codex/m4p2-sightweave-packaging-performance-closure`
- Frozen M4P1 baseline: `93f156f552aa85ee9d30891508d439011c57c479`
- Engine: UE 5.8.1, `D:\UE_5.8`
- Hardware: RTX 4060, driver 610.88, D3D12/SM6; i5-13500; 32 GiB
- Scope: packaging/Shipping/clean-host/performance closure only; no new capability or asset change

`Darkwell.uproject` retained only the user's unstaged EngineAssociation GUID difference and was never staged, restored, overwritten, or formatted.

## 3. Changes

The only production-package fix moved the already-required CustomDepth invariant into portable plugin config:

- `Config/Engine.ini`: `r.CustomDepth=3`, `r.CustomDepthTemporalAAJitter=0`;
- `Config/FilterPlugin.ini`: includes `/Config/Engine.ini` in BuildPlugin output.

Other source edits are Development-only test assertion refreshes and p50/p95/p99/performance matrices. No Runtime/Render policy, API, serialization, or visual behavior changed.

## 4. Checkpoints

| Commit | SHA | Remote |
| --- | --- | --- |
| docs: start SightWeave M4P2 packaging closure | `f246ce5` | pushed |
| build: close SightWeave plugin packaging boundaries | `f3bb4c7` | pushed |
| test: refresh SightWeave packaging assertions | `f33dd3f` | pushed |
| perf: expand SightWeave closure matrices | `c57b774` | pushed |
| docs: record SightWeave M4P2 final validation | pending | pending |

## 5. Retained package-boundary failure

The first clean host ran M4P1 D3D12 at 9/12. Screenshots showed sparse jittered dots instead of solid Last-Seen proxies because the host-only temporal-jitter CVar was absent from BuildPlugin output. After plugin-owned config was added, the single repair retry passed 12/12: Camera1 exact proxy 1936, Camera3 1448, Camera4 514, nonfinite 0. Both attempts are retained.

## 6. Final BuildPlugin

Output: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_BuildPlugin_Final_c57b774_20260828_153400`

| Target | Actions | Result |
| --- | ---: | --- |
| UnrealEditor Development | 102 | pass |
| UnrealGame Development | 32 | pass |
| UnrealGame Shipping | 32 | pass |
| AutomationTool | — | exit 0, 4m02s |

Package inventory: 281 files; Source 123/123, Shaders 1/1, Content 2/2. Released `Engine.ini` SHA-256 matches source: `20B5D72E3EA4E1409E9997182F140B9E1C45B74AEEF82E2E1E42F1B26F4A54DC`.

## 7. Final independent host

Host: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_CleanHost_Final_c57b774_20260828_154000`

It contains the final package as its sole plugin, no DARKWELL source, zero reparse points, zero repository absolute-path references, and no reused generated directories.

| Target | Actions | Time | Result |
| --- | ---: | ---: | --- |
| Editor Development | 102 | 137.15s | pass |
| Game Development | 32 | 54.72s | pass |
| Game Shipping | 32 | 43.55s | pass |

## 8. Repository build

Final `Scripts/BuildEditor.ps1`: DarkwellEditor Win64 Development, 5/5 current incremental actions, exit 0. BuildPlugin and the clean host also compiled all current plugin TUs from source.

## 9. Shipping inventory

The clean-host Shipping tree has 32 objects, 34 response files, 2 precompiled headers, 32 dependency JSON files, and 32 SARIF files. Modules are exactly SightWeaveRender (13 objects) and SightWeaveRuntime (19 objects). Tests, Editor, UnrealEd, AutomationTest, and DARKWELL metadata hits are zero.

Both module `Definitions.h` files contain `WITH_DEV_AUTOMATION_TESTS 0`, `WITH_EDITOR 0`, `WITH_EDITORONLY_DATA 0`, and `UE_BUILD_SHIPPING 1`.

## 10. COFF and dependency isolation

The six guarded test-readback/benchmark TUs produce empty Shipping objects. `dumpbin /symbols` found zero actual symbol rows containing FSightWeave, Automation, Benchmark, Readback, RenderGraph, or RHI. Runtime/Render source scans found zero host/test/editor dependencies.

## 11. Package-after-install D3D12 smoke

The final package, installed as the independent host's sole plugin, ran the complete SightWeave prefix on RTX 4060 D3D12/SM6: 263 tests performed. M3.4 was 37/37, M3.5 26/26, M4P1 12/12, and M4P2 LastSeen frame coverage 1/1. Real PIE/game views and RHI readbacks were used; SceneCapture was not authority.

Severe-log scan: no fatal, assert, ensure, shader/RDG failure, GPU crash, device removal, or nonfinite visual result. A direct generic Shipping executable launch was attempted but the blank host was not Cooked/Staged and exited 1 immediately; it is not counted as a valid smoke or plugin regression. A staged Shipping game remains follow-up work.

## 12. Automation matrix

| Gate | RHI | Success | Warning | Fail | Total |
| --- | --- | ---: | ---: | ---: | ---: |
| Full SightWeave | NullRHI | 173 | 0 | 2 | 175 |
| Full SightWeave | D3D12/SM6 | 261 | 1 | 1 | 263 |
| Full DARKWELL | NullRHI | 24 | 0 | 0 | 24 |
| M3.5 focused | NullRHI | 16 | 0 | 0 | 16 |
| M3.5 focused | D3D12/SM6 | 26 | 0 | 0 | 26 |
| M4P1 focused | NullRHI | 9 | 0 | 0 | 9 |
| M4P1 repaired | D3D12/SM6 | 12 | 0 | 0 | 12 |
| M4P1 Lab | NullRHI / D3D12 | 2 / 2 | 0 | 0 | 2 / 2 |
| Continuous transition | D3D12/SM6 | 1 | 0 | 0 | 1 |

## 13. M3.4 presentation performance

The first dedicated matrix was 19/21. After an 8-sample GPU warmup and applying the already-authoritative 32-source pressure gates, the permitted retry was 20/21. Width0, 1080p, 1 tile/2 sources retained the failure: GT binding p95 0.201us, RT p95 2.798us, GPU p50/p95/p99 0.348/1.169/1.942ms against p95 `<1.0ms`.

The same row later passed in the final full D3D12 prefix at 0.041/0.046/0.048ms. The variance is not erased or resolved by another retry. All 32-source rows met the existing 1080p `<2ms` and 1440p `<3ms` pressure gates.

## 14. Selected M3.5 precision

Targeted memory performance passed 7/7. Final full D3D12 production-eligible rows:

| Precision | CPU dirty p95/p99 | GT p95/p99 | RT p95/p99 | GPU p95/p99 | Worst persistent |
| --- | ---: | ---: | ---: | ---: | ---: |
| 10cm | 59.500/69.898us | 1.200/1.799us | 59.698/63.799us | 54/190us | 36,462,592 B |
| 25cm | 51.901/63.602us | 3.599/4.701us | 57.898/73.798us | 42/193us | 36,462,592 B |

All formal selected-precision gates pass; no-change work is zero. The 5cm/2.5cm rows remain candidate baselines, not new gates.

## 15. Memory scaling baselines

| Resident/dirty | CPU clear p95/p99 | CPU write p95/p99 | RT write p95/p99 | GPU p95/p99 | CPU/GPU persistent bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1/1 | 45.300/47.598us | 59.500/59.500us | 51.998/57.500us | 203/292us | 7,688 / 4,194,320 |
| 8/8 | 379.700/392.199us | 493.299/621.501us | 493.404/629.801us | 77/81us | 61,504 / 4,194,432 |
| 128/32 | 1972.400/2381.399us | 2430.599/2530.400us | 3555.700/5676.102us | 407/484us | 984,064 / 8,390,656 |

The last two are pressure baselines; existing authority does not assign the one-dirty formal budget to them.

## 16. LastSeen frame baselines

Each operation used 8 warmup + 64 samples at 1009x340; all nonfinite counts were zero.

| Operation | GT p50/p95/p99 | RT p50/p95/p99 | GPU p50/p95/p99 |
| --- | --- | --- | --- |
| remembered_nochange | 8.576/10.101/10.688ms | 9.721/11.451/12.328ms | 3.161/3.566/3.743ms |
| reacquire | 7.433/12.278/16.368ms | 3.999/6.431/6.773ms | 1.644/1.964/2.093ms |
| block_suppress | 8.326/16.278/17.412ms | 4.337/6.931/14.814ms | 1.860/2.315/2.551ms |
| clear | 5.919/7.359/8.528ms | 3.257/4.367/4.659ms | 1.866/2.940/3.454ms |
| identity_reuse | 6.178/7.974/9.671ms | 3.466/4.482/5.078ms | 2.021/2.688/2.800ms |
| page_tile_boundary | 5.820/7.252/7.694ms | 3.240/3.925/4.040ms | 2.036/2.724/2.878ms |

These are baseline-only because M4P1 defines correctness, not an independent frame-time threshold.

## 17. Visual correctness

Final M4P1 pixels: Camera0 exact proxy 183; Camera1 LastSeen 1936; reacquired proxy 0/live 2988; Camera2 leak 0; Camera3 page boundary 1448; clear suppression 928; Camera4 yaw45 514; identity reuse 264. All nonfinite counts were zero. Continuous transition, proxy-before-live reacquire, clear/suppression/unknown, identity reuse, and page/tile boundaries passed.

## 18. Retained failures and warning

- NullRHI Batch512 distribution 7 p99 271.499us >200us; D3D12 later passed with worst p99 184.599us.
- Prepared4096 failed both full runs: Null p50 1005.203us and D3D12 p50 1017.399us against `<1000us`; both p99 values remained below 2ms.
- Dedicated M3.4 Width0 1080p 1/2 p95 1.169ms >1ms, despite later full-prefix pass.
- Full D3D12 emitted one engine RHI warning: reserved virtual resource size 258 GiB exceeded its 256 GiB budget during a passing M2 query test. No device removal or allocation failure occurred; cumulative reservation growth remains a risk.
- UE warns that MSVC 14.51 is newer than preferred 14.50 and emits engine-header C4996 warnings; all targets passed.
- Pre-queue sandboxed UE launches failed because LocalAppData Zen/DDC was not writable. The valid authorized run passed; no automation result was overwritten.

## 19. Evidence

- Final host reports: `<final-host>\Saved\AutomationReports\M4P2_FullSightWeave_{NullRHI,D3D12}_Final`
- Final host logs: `<final-host>\Saved\Logs\M4P2_FullSightWeave_*.log`
- DARKWELL: `Saved\AutomationReports\M4P2\M4P2_FullDarkwell_NullRHI_Final`
- M3.4 retry: `Saved\AutomationReports\M4P2\M4P2_M3P4_PresentationPerformance_D3D12_Retry1`
- M3.5 performance: `Saved\AutomationReports\M4P2\M4P2_M3P5_MemoryPerformance_D3D12`
- LastSeen: `Saved\AutomationReports\M4P2\M4P2_LastSeenFrameMatrix_D3D12_Valid1`

Generated evidence is ignored and uncommitted.

## 20. Git/LFS closure

Final documentation commit must record local/upstream/remote equality, `git diff --check`, `git lfs status`, `git lfs fsck`, `git fsck --no-reflogs`, and proof that only the known unstaged project descriptor remains. No generated package, host, report, screenshot, Binaries, Intermediate, Saved, or DDC path may be tracked.

## 21. Resume

Remain on M4P2. Investigate Prepared4096/Batch512 without weakening gates; stabilize M3.4 under controlled clocks; Cook/Stage a blank Win64 Shipping host for D3D12 lifecycle smoke; and investigate cumulative RHI virtual reservations.

```powershell
cd D:\UE_projects\LastLight
git switch codex/m4p2-sightweave-packaging-performance-closure
git status --short --branch
```

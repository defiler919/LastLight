# SightWeave M4P3 execution report

## 1. Verdict

**COMPLETED.** Deterministic persistence, atomic restore, provider fallback, rollback, world isolation, actual D3D12 rebuild/readback, the frozen Batch512 performance gate, the complete timing/resource matrix, all regressions, BuildPlugin, clean-host builds, Shipping isolation, and Git/LFS closure passed.

No threshold, frozen contract, persistence rule, or test count was weakened. The implementation changes only C++ and documentation; no `.uasset`, `.umap`, `.uproject`, `.uplugin`, shader, module-loading, or packaged-resource source file changed.

## 2. Identity and implementation

- Branch: `codex/m4p3-sightweave-persistence-restore-closure`
- Frozen base: `902b192c2acc52d8817ccca0ee13cdb377eb600e`
- Final validated implementation/test head before closure documentation: `ad10f3ac8dc1443a8738a5f71f7b9f7c244d0f02`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Platform: Win64, offline, single-player
- Forced graphics path: D3D12/SM6 on NVIDIA GeForce RTX 2070 SUPER

The Runtime module owns a bounded `SWPERSV1` envelope with checked little-endian serialization, deterministic canonical ordering, BLAKE3 integrity, raw/Zlib storage, 64 MiB canonical/blob limits, persistent stable modifier IDs, provider versioning, validate-before-commit atomic restore, localized missing-provider fail-black, and post-commit derived publication. UObject, world, actor, render, RDG, RHI, GPU, runtime handle, address, random, timestamp, and current-frame visibility state is never serialized.

## 3. Pushed checkpoints

| Commit | Purpose |
| --- | --- |
| `a02ed54`, `7f74127` | freeze and normalize the M4P3 contract |
| `70fea46` | deterministic V1 snapshot format |
| `208b2fb` | atomic authority/provider restore |
| `3f9d114` | malformed-input and rollback coverage |
| `ac893da` | world isolation and D3D12 derived rebuild |
| `b114c4b` | initial validation report with two open gates |
| `55fb14f` | restore Batch512 gate margin |
| `b502dbc` | instrument timing and resource closure matrices |
| `ad10f3a` | isolate lifecycle garbage collection safely |

## 4. Batch512 A/B and final gate

The exact frozen test was launched as an independent process five times for NullRHI and five times for D3D12/SM6 at each tested commit. Every process emitted ten distributions, each containing 1,001 raw untrimmed samples and zero steady-state capacity growth.

| State | NullRHI mean process-worst p50 | D3D12 mean process-worst p50 | Processes | Raw samples | Result |
| --- | ---: | ---: | ---: | ---: | --- |
| frozen baseline `902b192c` | 150.861 us | 150.921 us | 10 | 100,100 | stable fail |
| pre-optimization `b114c4b` | 155.300 us | 152.501 us | 10 | 100,100 | stable fail |
| optimized `55fb14f+` | 137.280 us | 136.901 us | 10 | 100,100 | 10/10 pass |

Final per-process worst p50/p95/p99/max values:

| RHI | Run 1 | Run 2 | Run 3 | Run 4 | Run 5 |
| --- | --- | --- | --- | --- | --- |
| NullRHI | 136.901/142.302/146.799/201.903 | 138.499/142.299/146.799/160.802 | 136.901/141.501/144.698/169.300 | 137.199/141.598/145.800/198.599 | 136.901/142.001/146.199/183.601 |
| D3D12 | 136.502/141.300/145.700/182.100 | 136.800/141.501/157.200/204.399 | 136.700/140.801/146.601/199.702 | 136.700/141.900/145.901/176.199 | 137.802/144.001/149.898/170.600 |

The frozen limits are p50 <= 150 us, p95 <= 180 us, and p99 <= 200 us. The overall worst final p50/p95/p99 is 138.499/144.001/157.200 us, leaving 11.501/35.999/42.800 us of margin. A/B plus final evidence contains 300,300 exact samples; no trimming or outlier removal was used.

Root cause was duplicate exact polar-geometry containment when illumination and a vision source shared identical geometry. Snapshot preparation now selects the deterministic lowest identical vision index, and the batch query reuses that evaluated containment. Scalar behavior and owner/capability/policy authority remain unchanged. A 449,923,721-byte Unreal Insights trace under CPU/frame/memory/bookmark tracing also passed at 136.502/140.600/145.301 us.

## 5. M4P3 timing matrix

Each fixture used five warmups and 100 formal capture+restore samples. Every cell is p50/p95/p99/max in microseconds; all 3,900 phase samples are retained raw in the final NullRHI log.

| Phase | Small 1/1/0/0 | Typical 8/8/4/2 | Maximum 128/32/16/8 |
| --- | ---: | ---: | ---: |
| canonical snapshot build | 1.900/2.000/2.302/2.403 | 17.598/17.799/17.900/19.901 | 158.597/181.999/193.499/258.300 |
| ordering + serialization | 3.699/4.001/4.701/10.602 | 30.600/31.397/34.600/60.700 | 247.501/273.898/319.302/337.400 |
| checksum | 9.000/9.499/10.200/10.200 | 33.300/35.498/35.800/35.901 | 425.100/437.405/447.400/452.701 |
| compression or raw | 41.399/43.403/46.398/47.002 | 270.199/277.400/282.802/283.100 | 4666.500/4719.499/5148.903/5386.502 |
| envelope parse | 0.101/0.101/0.201/0.201 | 0.101/0.201/0.201/0.201 | 0.201/0.499/0.700/0.700 |
| decompression | 18.902/19.401/21.499/25.399 | 127.699/130.501/135.098/135.299 | 1922.000/1938.596/1950.201/1962.300 |
| core validate | 4.001/4.403/5.800/6.400 | 30.302/30.700/32.801/32.801 | 172.198/208.501/224.601/237.498 |
| provider prepare + validate | 0.000/0.101/0.101/0.101 | 0.298/0.302/0.302/0.402 | 0.902/1.602/1.997/2.000 |
| atomic commit | 0.197/0.201/0.201/0.201 | 0.998/1.099/1.103/1.103 | 6.203/6.400/8.401/8.803 |
| generation/revision update | 0.000/0.101/0.101/0.101 | 0.000/0.101/0.101/0.101 | 0.101/0.201/0.201/0.399 |
| derived CPU rebuild | 0.000/0.101/0.101/0.101 | 0.000/0.101/0.101/0.101 | 0.101/0.201/0.399/0.600 |
| render/GPU sync boundary | 0/0/0/0 | 0/0/0/0 | 0/0/0/0 |
| total restore | 31.501/34.001/34.999/40.799 | 197.601/200.700/205.301/207.502 | 2530.202/2608.500/2696.402/2789.501 |

Canonical/stored bytes were 8,115/311, 64,825/732, and 997,801/3,517. Prepared capacity was 392/456/744 bytes. All three fixtures restored the exact selected authority and published derived CPU state; NullRHI correctly has no render/GPU rebuild boundary.

## 6. Six 100-loop resource matrices

1. Capture: 100 samples for each fixture with stable canonical authority and bounded prepared capacities.
2. Successful restore: 100 per fixture; final tiles/modifiers/subjects remained exactly 1/0/1, 8/4/8, and 128/16/32.
3. Invalid restore rollback: 100 per fixture; guard revisions and authority remained unchanged.
4. Missing-provider fallback: 100 for provider-bearing typical and maximum fixtures; localization remained fail-black and provider registrations did not accumulate. The provider-free small fixture correctly performs zero fallback loops.
5. World teardown/recreate: 100 cycles; SightWeave objects, rooted objects, worlds, Runtime subsystems, and Render subsystems were stable. In the final full suite a foreign Lab world was present, so the test used explicit pending-kill and render flush without global GC to avoid collecting another test's initialized subsystem.
6. D3D12 full derived rebuild/readback: 100 packets; p50/p95/p99/max 3201.998/3434.197/3482.999/3613.702 us, one resident tile/page, resource generation 2->2, residency generation 3->201, and persistent GPU bytes exactly 4,194,320.

The isolated world fixture also ran global GC every 20 cycles and at completion: total live UObjects 51,685->51,651, SightWeave 12->12, worlds/Runtime/Render 2->2, rooted 44,915->44,915.

## 7. Final automation and build matrix

| Gate | NullRHI | D3D12/SM6 |
| --- | ---: | ---: |
| M4P3 | 15/15 | 16/16 |
| M3P5 | 16/16 | 26/26 |
| M4P1 | 9/9 | 12/12 |
| full SightWeave | 191/191 | 283/283 |
| DARKWELL | 24/24 | not required |

The final full SightWeave runs contain zero failed tests and zero test warnings. The final Editor build after all Runtime/test changes succeeded. Severe scans across the 19 authoritative automation logs plus BuildPlugin and three successful clean-host logs found zero fatal errors, assertions, ensures, RDG/RHI/renderer/shader errors, GPU crashes, device removals, or out-of-memory conditions.

## 8. BuildPlugin and isolated clean host

BuildPlugin package:

`C:\Users\defiler\AppData\Local\Temp\SightWeaveM4P3_Final_BuildPlugin_ad10f3a_20260829_0050`

- UAT exit 0, `BUILD SUCCESSFUL`
- Editor 106/106, UnrealGame Development 33/33, UnrealGame Shipping 33/33
- 288 files, 639,946,716 bytes
- package Source 128/128, Shader 1/1, and Content 2/2 files match repository SHA-256 hashes; UAT intentionally omits `FilterPlugin.ini` and transforms the installed descriptor

Fresh source-isolated host:

`C:\Users\defiler\AppData\Local\Temp\SightWeaveM4P3_Final_CleanHost_ad10f3a_20260829_0100`

It began with zero Binaries/Intermediate/Saved/DDC directories, zero reparse points, zero repository path references, and only the BuildPlugin delivery content.

| Target | Actions | Result |
| --- | ---: | --- |
| Editor Development | 112/112 | pass |
| Game Development | 45/45 | pass |
| Game Shipping, fully clean retry | 45/45 | pass |

The final Shipping build graph contains only SightWeaveRuntime and SightWeaveRender, with 20 and 13 implementation objects. There are no Editor/Tests Shipping module directories. Shared definitions are `WITH_DEV_AUTOMATION_TESTS=0`, `WITH_EDITOR=0`, `WITH_EDITORONLY_DATA=0`, `UE_BUILD_SHIPPING=1`; six guarded readback/benchmark objects have zero SightWeave/readback/benchmark/automation external implementation symbols. PE imports and binary scans contain zero `SightWeaveEditor`, `SightWeaveTests`, `DARKWELL`, or repository paths and zero UnrealEd/Automation imports. Generic engine reflection strings named `/Script/UnrealEd` and `AutomationTestmap` remain in the monolithic engine executable but have no associated module import or SightWeave symbol.

Cook/Stage/Package was not triggered because no descriptor source, loading phase, Shipping condition, shader, Config delivery rule, asset, map, or packaged-resource declaration changed.

## 9. Preserved failed-attempt ledger

No meaningful failed evidence was overwritten. The retained ledger includes:

1. early fixture/build errors (self-`TArray::Add`, misplaced persistence getters, scope matching, shadowed flags) fixed before the first reliable implementation checkpoint;
2. an initial all-white D3D expectation corrected to exact suppression/fail-black texels;
3. the first M4P1 D3D run cropped two ROIs because forced resolution was omitted; the complete forced 1920x1080 retry passed 12/12;
4. the first final full NullRHI run exposed unsafe global GC in the new lifecycle fixture; targeted lifetime cleanup fixed the test and the complete retry passed 191/191;
5. one full D3D run lacked an absolute FourTileCorner GPU timestamp although pixel parity passed; its isolated retry produced four valid timestamps;
6. one full D3D retry with extra `TestExit` hit a frozen M4P2 viewport uniqueness assertion before M4P3; the exact historical command without that addition passed 283/283;
7. the first clean-host Shipping launch exhausted C: while writing a PCH/UBA trace (exit 112); its immediate retry correctly rejected the corrupt PCH (exit 6); generated outputs were preserved to ignored `Saved` archives, 17 GB was made available, and a source-only Shipping rebuild passed 45/45;
8. the UAT/UBT compiler warnings are UE 5.8 engine-header deprecations plus a newer-than-preferred MSVC warning; no SightWeave compile warning or product failure remains.

No merge, rebase, reset, clean, stash, force push, asset rewrite, threshold relaxation, or destructive recovery was used.

## 10. Evidence locations

- Raw A/B and final performance: `Saved/Logs/SIGHTWEAVE_M4P3_{AB_*,FINAL_BATCH512_*}.log`
- Timing/resource and final suites: `Saved/Logs/SIGHTWEAVE_M4P3_FINAL_*.log`
- Reports: `Saved/AutomationReports/M4P3_Closure_Final_*`
- Insights: `Saved/Profiling/SIGHTWEAVE_M4P3_BATCH512_OPTIMIZED_NULLRHI.utrace`
- BuildPlugin and clean host: paths above
- Final contract/validation/handoff: `Docs/SIGHTWEAVE_M4P3_*.md`

Generated evidence is intentionally ignored and untracked.

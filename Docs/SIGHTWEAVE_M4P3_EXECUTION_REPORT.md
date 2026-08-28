# SightWeave M4P3 execution report

## 1. Verdict

**PARTIAL.** The deterministic persistence format, atomic restore, provider fallback, rollback, world isolation, derived D3D12 rebuild, plugin packaging, clean-host builds, and Shipping isolation are implemented and validated. Two required closure gates remain open: a frozen pre-existing M2P2 performance test fails on this host, and the requested complete resource/timing evidence matrix is only partially instrumented.

This is a validation disposition, not an M4P3 correctness failure. No M4P3 test failed in the final focused or full runs.

## 2. Identity and scope

- Branch: `codex/m4p3-sightweave-persistence-restore-closure`
- Frozen base: `902b192c2acc52d8817ccca0ee13cdb377eb600e`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Primary target: Win64, offline, single-player
- D3D12 adapter: NVIDIA GeForce RTX 2070 SUPER, driver 610.88, Feature Level 12_2, Shader Model 6.7 support
- Forced validation RHI: D3D12 with SM6

The implementation changes only C++ and documentation. There are no `.uasset`, `.umap`, `.uproject`, `.uplugin`, shader, config, module-loading, or packaged-resource changes from the frozen base. DARKWELL save slots and product maps are not integrated.

## 3. Pushed implementation checkpoints

| Commit | Purpose |
| --- | --- |
| `a02ed54` | define the frozen M4P3 persistence contract |
| `7f74127` | normalize contract whitespace |
| `70fea46` | add deterministic V1 snapshot format |
| `208b2fb` | add atomic authority/provider restore |
| `3f9d114` | cover persistence and rollback failures |
| `ac893da` | validate world isolation and D3D12 derived rebuild |

All checkpoints were pushed to the branch. The first push of `3f9d114` hit a transient TLS `SSL_ERROR_SYSCALL`; an ordinary non-force retry succeeded.

## 4. Implemented contract

The Runtime module now owns a deterministic `SWPERSV1` envelope with an 80-byte little-endian header, BLAKE3 canonical-payload integrity, raw/Zlib storage selection, a 4 KiB compression threshold, and checked 64 MiB canonical/blob limits. Counts, lengths, arithmetic, allocations, array growth, decompression destinations, trailing bytes, compression ratios, versions, flags, and checksums are validated before state mutation.

Canonical ordering covers worlds/scopes, memory tiles, modifiers, subject records, and provider payloads. Runtime handles, process addresses, UObjects, world pointers, render/RHI/GPU resources, current-frame visibility, live sources, random values, and timestamps are excluded.

Restore is game-thread synchronous and split into decode, provider/authority prepare, lifetime/revision validation, atomic commit, and derived publication. Memory and SubjectMemory replacement states are owned plain data. Missing providers fail black only for their declared domains. Duplicate providers, provider prepare/commit failures, stale targets, malformed input, and all tested limit failures publish nothing and retain prior authority revisions/state.

WorldSubsystem restore republishes a full Memory packet and already-authored static environment only after successful commit. Render/GPU state is rebuilt from the restored CPU authority; it is never deserialized from the snapshot.

## 5. M4P3 focused validation

| Gate | NullRHI | D3D12/SM6 |
| --- | ---: | ---: |
| `SightWeave.M4P3` | 14 success + 1 expected-warning success | 15 success + 1 expected-warning success |
| M3P5 regression | 16/16 | 26/26 |
| M4P1 regression | 9/9 | 12/12 after resolution retry |

The M4P3 warning is the deliberate corrupt-Zlib fixture returning `Z_DATA_ERROR`; the test itself succeeds and proves rollback. The D3D12-only derived test reads the actual memory mirror produced by the full-rebuild packet and verifies suppression and fail-black atlas gutters rather than accepting an all-white approximation.

Final full-rebuild diagnostic:

`prepare_us=37.100 validate_us=0.101 commit_us=0.197 publish_us=2.202 packet_revision=2 memory_revision=1 full_rebuild=1 authority_tiles=1`

The world test covers two independent UWorlds/scopes, distinct identities/generations, restore, clear, reacquire, suppression, teardown, and world reconstruction.

## 6. Full regression

| Gate | Result | Disposition |
| --- | ---: | --- |
| Full SightWeave NullRHI | 188 success, 2 warning-success, 1 fail; 191 total | only `M2P2.Performance.Batch512Gate` failed |
| Full SightWeave D3D12/SM6 | 280 success, 2 warning-success, 1 fail; 283 total | only `M2P2.Performance.Batch512Gate` failed |
| Full DARKWELL NullRHI | 24/24 | pass |

The other warning-success entry is the retained M2P2 MotionTrace diagnostic. No M3P5, M4P1, M4P3, or DARKWELL test failed.

The frozen M2P2 batch gate requires every one of ten distributions to have a median no greater than 150 us. It failed in the full NullRHI run (`worst median 155.799 us`), full D3D12 run (`158.399 us`), and an isolated NullRHI retry. Affinity, elevated thread/process priority, and physical-core isolation were reported active. This test does not call the M4P3 persistence path. Its threshold and implementation were left unchanged.

## 7. Determinism, size, timing, and resource evidence

Independent-process fixture identity was stable across launches:

- BLAKE3: `b8334f227e5589965d427c70b3787baeefa9cbb408a196d0493badfaf1e857d8`
- canonical bytes: 61,665
- stored bytes: 315

The 128-tile fixture under final NullRHI produced:

- canonical bytes: 985,259
- stored bytes: 2,488
- stored/canonical ratio: 0.002525
- capture: 5,811.300 us
- restore total p50/p95/p99/max: 2,334.304 / 2,417.497 / 2,444.498 / 2,516.501 us

The final D3D12 run produced capture 5,639.501 us and restore total p50/p95/p99/max 2,317.302 / 2,421.297 / 2,504.200 / 2,604.600 us.

One hundred successful restores advanced the guard revision exactly 100 times and retained exactly 128 tiles and one persistent modifier. One hundred malformed restores advanced no guard revision and retained 128 tiles. Provider registrations are non-owning and call-local; no provider accumulation was observed by the authority tests.

Evidence limitation: current tests do not report a complete small/typical/maximum fixture matrix of p50/p95/p99/max for each of prepare, validate, commit, and derived publication, nor direct before/after UObject, Render-resource, and GPU-resource counts across all 100 loops. The D3D12 derived path is measured once and its GPU readback succeeds, but that is not a substitute for the requested full resource matrix. This is the second reason for `PARTIAL`.

## 8. Visual evidence

M3P5 D3D12 generated and was directly inspected at 1920x1080 request resolution:

- `Saved/Screenshots/M3P5_PIE_Camera0_Overview.png`
- `Saved/Screenshots/M3P5_PIE_Camera1_Remembered.png`
- `Saved/Screenshots/M3P5_PIE_Camera2_DynamicLeak.png`
- `Saved/Screenshots/M3P5_PIE_Camera3_PageBoundary.png`
- `Saved/Screenshots/M3P5_PIE_Camera4_Rotated45.png`

All eight M4P1 images under `Saved/Screenshots/M4P1` were directly inspected. The no-dynamic-leak frame is black, clear suppression removes the intended page-boundary region, Last-Seen transitions to live on reacquire, and rotated/identity-reuse proxies occupy the expected regions. Automated pixel metrics reported zero non-finite pixels.

The first M4P1 D3D12 launch used a 1009x315 offscreen window and failed two ROI-based visual tests because the requested regions were cropped. A focused retry and the complete retry used `-ResX=1920 -ResY=1080 -ForceRes`, generated 1526x554 captures, and passed 12/12. No product code or asset changed.

## 9. BuildPlugin and clean host

BuildPlugin output:

`C:\Users\defiler\AppData\Local\Temp\SightWeaveM4P3_BuildPlugin_Closure_ac893da_20260828_2317`

- UAT `BuildPlugin -Rocket -TargetPlatforms=Win64`: exit 0, approximately 4 minutes
- Editor Development: 106/106 actions
- UnrealGame Development: 33/33 actions
- UnrealGame Shipping: 33/33 actions
- final package: 288 files, 639,738,398 bytes

Final independent source clean host:

`C:\Users\defiler\AppData\Local\Temp\SightWeaveM4P3_CleanHost_Closure_ac893da_20260828_2324_Retry1`

It began with no host-generated directories, reparse points, DARKWELL source, or repository path references and consumed only the BuildPlugin package.

| Target | Actions | Result |
| --- | ---: | --- |
| Editor Development | 112/112 | pass |
| Game Development | 45/45 | pass |
| Game Shipping | 40/40 | pass |

The first real clean-host attempt used `BuildSettingsVersion.V6`; UE 5.8.1 rejected its shared Editor environment before plugin compilation. A new host with V7 was created rather than mutating or deleting the failed evidence.

## 10. Shipping isolation

The clean-host Shipping target compiled exactly 20 SightWeaveRuntime implementation objects and 13 SightWeaveRender objects. Runtime increased from M4P2's 19 objects only because `SightWeavePersistence.cpp` is new. No SightWeaveEditor or SightWeaveTests Shipping directory exists.

The shared compile definitions are `WITH_DEV_AUTOMATION_TESTS 0`, `WITH_EDITOR 0`, `WITH_EDITORONLY_DATA 0`, and `UE_BUILD_SHIPPING 1`. Six guarded readback/benchmark objects contain zero forbidden COFF implementation symbols. Binary-string and PE-import scans found zero `SightWeaveTests`, `SightWeaveEditor`, `DARKWELL`, repository-path, UnrealEd, or AutomationTest imports/strings.

Runtime dependencies remain exactly Core, CoreUObject, Engine, and DeveloperSettings. Render dependencies remain runtime-safe. Full Cook/Stage/Package was not repeated because no descriptor, module-loading, shader, config, packaged-resource, asset, map, or Shipping-conditional change crossed the task's escalation triggers.

## 11. Severe-log classification

Across final focused, full, and DARKWELL logs there were zero fatal errors, assertions, ensures, shader errors, RDG errors, RHI errors, GPU crashes, or device removals. Each Unreal launch emits 13 startup `LogAutomationTest: Error: Condition failed` diagnostics before test execution; they are engine discovery diagnostics and occurred equally in all successful runs. The corrupt-Zlib rollback fixture emits the expected `Z_DATA_ERROR` warning. The only final failed-test record is the frozen M2P2 performance gate described above.

## 12. Preserved failed-attempt ledger

Meaningful failed evidence was not overwritten:

1. an initial command used the wrong log switch, produced an empty report, and exited 3;
2. an early fixture used `TArray::Add` with a self element and crashed; the fixture now copies the element first;
3. an early compressed-corruption test caused an unavoidable Zlib engine error; checksum-header corruption was used for the quiet format test, while the rollback matrix intentionally retains the Zlib warning;
4. one build placed persistence getters on the packet rather than the authority; they were moved and the full build passed;
5. initial scope matching included the full stable hash and rejected valid restore targets; matching now uses canonical capabilities;
6. one test local named `Flags` shadowed the automation flags namespace; it was renamed;
7. the first D3D12 derived test assumed the entire tile was white; exact expected texels now include persistent suppression and fail-black gutters;
8. first M4P1 D3D12 visual run was cropped; forced-resolution retries passed;
9. full NullRHI, full D3D12, and isolated M2P2 retry retained the same old performance-gate miss;
10. the first clean-host orchestration used PowerShell's read-only `$Host` name; its generated copy was moved intact outside the repository, then a task-specific variable was used;
11. clean-host V6 build settings were rejected before compilation; the fresh V7 retry passed all targets.

No reset, clean, rebase, force push, asset rewrite, threshold relaxation, or destructive recovery was used.

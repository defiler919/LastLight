# SightWeave M3.2 final validation

Status: **PARTIAL**

Branch: `codex/m3p2-sightweave-sparse-atlas-residency`

Frozen M3.1 baseline: `a96e85f9c0ac32400abf81c87f75e2db15cdb36f`

Validated implementation/test checkpoint: `c35fb54` (`test: validate SightWeave packaging boundaries`)

Engine: Unreal Engine 5.8.1, changelist `56057345`, at `D:\UE_5.8`

Primary home evidence: Windows D3D12 SM6, `PCD3D_SM6`, NVIDIA GeForce RTX 2070 SUPER (Turing), 8192 MiB, Studio Driver 610.88. The adapter reports maximum feature level 12_2 and shader model 6.7.

## Disposition

The M3.2 implementation, functional verification, performance gate, BuildPlugin, independent clean-host builds, clean-host D3D12/SM6 execution, and Shipping isolation all pass. The status is `PARTIAL` only because remote Git closure is intentionally paused: the local branch contains reliable checkpoints after the already-pushed documentation start, but those later commits have not been pushed pending explicit approval for the configured `origin`. No correctness, lifecycle, M3.2 performance, packaging, or Shipping failure remains unexplained.

M3.2 preserves the frozen authority flow:

`CPU-authoritative polygons -> deterministic CPU triangles -> immutable revisioned packet -> SightWeaveRender -> world-scoped persistent sparse PF_G8 atlas -> EffectiveLiveMask`

GPU state remains presentation-only. No DARKWELL gameplay rule, AI query, HUD rule, memory/Last-Seen state, damage reveal, or save data reads the atlas.

## M3.1 home admission and final regression

Admission was completed before the M3.2 branch was created:

- Editor Development build passed with UE's bundled .NET 10.
- NullRHI passed 10/10.
- Cold D3D12/SM6 passed 29/29 and compiled all four SightWeave global shader types.
- A second process passed 29/29.
- Each D3D process completed 21 asynchronous 256 x 256 `PF_G8` readbacks containing only 0/255 and zero CPU/GPU mismatch.
- Stale/duplicate revision, explicit clear, source deletion, world teardown/restart, suppression-last, gutter edge, and stale async-readback rejection passed.

After all M3.2 changes, authoritative final regressions passed again:

- `Saved/AutomationReports/SightWeaveM3P1_FinalRegression_NullRHI_20260826`: 10 passed, 0 failed.
- `Saved/AutomationReports/SightWeaveM3P1_FinalRegression_D3D12_20260826`: 29 passed, 0 failed.

The Turing data is independent from the office RTX 4060 evidence and is not merged into the earlier distribution.

## Atlas, page, and tile layout

- Format: `PF_G8`, hard binary R8 UNorm.
- Physical tile: 256 x 256 texels.
- Interior: 248 x 248 texels.
- Gutter: four texels on each edge.
- Page: 2048 x 2048 texels, 8 x 8 physical slots, 64 slots, 4 MiB.
- Standard precision: 10 cm/texel, 128 active tiles, at most two pages per owner/floor/precision scope.
- Logical tile identity contains world lifetime, knowledge owner, floor, precision/floor origin, logical coordinate, and the complete canonical compatibility-profile sequence. Stable hashes are accelerators only.
- Physical page/slot addresses are residency state and are never logical identity.

Pages are allocated lazily per scope. A Standard scope therefore uses zero atlas bytes before its first dirty tile, 4 MiB for slots 0-63, and 8 MiB after crossing into slots 64-127. Three persistent 256 x 256 scratch textures (Vision, Illumination, Suppression) use 192 KiB per render state; Bypass unions directly into the selected atlas slot. Cold page creation clears the complete new page black. Ordinary updates clear only dirty 256 x 256 slot rectangles, never the complete resident atlas.

## Residency, reuse, eviction, and fail-black behavior

Residency is deterministic and bounded:

1. An equivalent complete identity reuses its current slot.
2. A free slot uses the lowest preassigned linear address.
3. If full, the least-recently-used unprotected slot is selected; equal use serials break by linear address.
4. Pinned, in-flight, or readback-referenced slots cannot be evicted or released.
5. Reused or newly allocated slots require a black clear before application.
6. If every candidate is protected, allocation returns capacity exceeded rather than exposing an older mask.

Removed identities are released before new dirty acquisitions. Page creation, scratch creation, invalid packet state, scope-capacity mismatch, unsupported RHI/pixel format, or protected-release uncertainty fails the affected scope black and drops its page/residency state. World teardown releases all pages, scratch resources, pending packet ownership, and the old world serial.

Automation covers first allocation, existing reuse, deterministic LRU reuse, pin protection, all-protected failure, complete-profile hash collision, page boundary 65, 128 active tiles, capacity+1 fail-black, and a GPU slot-reuse readback with exactly 65,536 black texels.

## Dirty scheduling

The game-thread builder canonicalizes scopes, profiles, sources, tile coordinates, vertices, and indices into a self-contained immutable packet. Current polygon bounds enumerate the active logical tile set with bounded floor semantics, including negative coordinates and large coordinates. Each tile's complete geometry/profile content hash is compared with the previous packet:

- new or changed content emits one dirty tile index;
- moved geometry dirties its new tile and records the absent old identity for black release;
- deletion records the old identity for release;
- profile, illumination, bypass, suppression, and explicit/full-scope changes alter the complete tile hash;
- duplicate tile requests are coalesced by the canonical tile set;
- unchanged content emits no dirty indices and no render mask passes;
- unknown/invalid scope effects fail or fully rebuild only that owner/floor scope.

The render thread consumes only dirty indices. The persistent test proves revisions `1/2/3` dispatch `1/0/1` tiles, with the no-change revision producing no clear, raster, publication, page allocation, scratch allocation, or resource-generation growth. One dirty tile never redraws the whole atlas.

## Formula, gutter, seams, and parity

For every complete equivalent profile, the render path computes `Vision[p] intersection CompatibleIllumination[p]`, max-unions profiles, unions Bypass, and applies Suppression last. The shader uses the physical slot viewport origin while raster coordinates remain tile-local at the frozen centimeters-per-texel scale. There is no feather or blurred tolerance.

Final M3.2 D3D12 evidence contains 22 selected-slot readbacks. Every 65,536-texel result was compared at CPU world-space texel centers, contained only 0/255, and reported zero non-boundary and zero boundary-class mismatch. Cases cover horizontal, vertical, diagonal, four-tile corner, negative coordinates, multi-profile Visible/IR, bypass/suppression, page boundary, capacity, persistent updates, slot reuse, stale/duplicate packets, and async stale rejection. Existing M2/M3.1 regressions retain straight-wall, L/T/vertex, angular `+/-pi`, gutter-edge, clear, and teardown coverage.

Final reports:

- `Saved/AutomationReports/SightWeaveM3P2_Final_NullRHI_20260826`: 8 passed, 0 failed.
- `Saved/AutomationReports/SightWeaveM3P2_Final_D3D12_20260826`: 22 passed, 0 failed.
- `Saved/Logs/SightWeaveM3P2_Final_D3D12_20260826.log`: 22 parity readbacks, all binary and zero mismatch.

## Scope, revision, and world isolation

- Multi-owner and multi-floor scopes occupy separate render-state partitions.
- Visible and Infrared remain separate complete profiles even when a test forces their stable hashes to collide.
- Tile equality compares the complete canonical capability sequence.
- Duplicate packet revision/equal hash is no work; stale revision is rejected; equal revision/different hash forces black invalid state.
- Readback identity binds world, owner, floor, profile sequence, logical coordinate, and packet revision.
- World mismatch is rejected by the builder and render state.
- M3.1 lifecycle regression creates/destroys real test worlds, proves monotonic new world serials and new SVE ownership, and prevents prior diagnostics or delayed commands from entering the restarted world.
- Sparse stale async readback releases its pixels rather than publishing a result for a newer expectation.

## Cold and warmed performance on RTX 2070 SUPER

All M3.2 numbers below are from the home RTX 2070 SUPER only. Nearest-rank distributions use 256 warmups plus 2,048 GT samples, or 64 updates after one cold persistent-resource sample for RT. No slow sample was deleted.

GT sparse packet build:

| Sources | p50 us | p95 us | p99 us | max us |
| ---: | ---: | ---: | ---: | ---: |
| 2 | 3.599 | 4.701 | 5.998 | 40.799 |
| 8 | 7.998 | 8.103 | 9.898 | 59.001 |
| 32 | 24.103 | 24.799 | 30.097 | 36.299 |

GT immutable ownership capture and render-command enqueue:

| Packet | p50 us | p95 us | p99 us | max us |
| --- | ---: | ---: | ---: | ---: |
| No change | 0.201 | 0.399 | 1.200 | 8.900 |
| One dirty tile | 0.201 | 0.402 | 1.103 | 6.702 |
| Eight dirty tiles | 0.201 | 0.499 | 1.103 | 7.201 |

Warmed render-thread update distributions:

| Dirty tiles / metric | p50 us | p95 us | p99 us | max us |
| --- | ---: | ---: | ---: | ---: |
| 1 / packet consume | 0.101 | 0.201 | 0.201 | 0.201 |
| 1 / dirty scheduling | 0.402 | 0.600 | 0.998 | 0.998 |
| 1 / persistent RDG setup | 7.097 | 9.600 | 58.703 | 58.703 |
| 1 / slot clear setup | 2.202 | 2.500 | 3.099 | 3.099 |
| 1 / raster setup | 2.999 | 3.703 | 54.400 | 54.400 |
| 1 / publication | 0.000 | 0.101 | 0.101 | 0.101 |
| 8 / packet consume | 0.201 | 0.298 | 0.399 | 0.399 |
| 8 / dirty scheduling | 1.401 | 1.900 | 2.202 | 2.202 |
| 8 / persistent RDG setup | 30.100 | 42.800 | 88.301 | 88.301 |
| 8 / slot clear setup | 12.800 | 18.202 | 53.700 | 53.700 |
| 8 / raster setup | 12.800 | 17.501 | 27.202 | 27.202 |
| 8 / publication | 0.000 | 0.101 | 0.101 | 0.101 |

The required warmed one-dirty-tile RT RDG p95 is 9.600 us, below the frozen 200 us target. During all 64 warmed one-tile and eight-tile samples, page allocations, scratch allocations, and resource generation remained fixed at `1/3/2`.

Cold versus warmed examples are deliberately separate:

- Cold one-tile persistent creation: RT RDG setup 745.401 us; warmed final update 7.000 us.
- Cold eight-tile persistent creation: RT RDG setup 791.300 us; warmed final update 30.000 us.
- Warmed final GPU mask graph: 7 us for one dirty tile and 16 us for eight dirty tiles.
- Cold/page-capacity GPU stress: 65 tiles crossing page 1 took 139 us; 128 tiles ending at page 1 slot 63 took 263 us.
- Persistent no-change RT RDG setup was 2.101 us and emitted zero mask work.

These are Development automation measurements, not a replacement for the later RTX 4060 production workload or a final shipping-frame GPU budget.

## Allocation and capacity evidence

The cold persistent sample allocates one 4 MiB page and three 64 KiB scratch textures. No-change and warmed updates retain page allocation count 1, scratch allocation count 3, and resource generation 2. The eight-tile update remains in one page. The 65-tile case allocates exactly two pages and places tile 64 at page 1 slot 0. The 128-tile case allocates exactly two pages and places tile 127 at page 1 slot 63. Capacity+1 returns a valid packet with the affected scope classified `CapacityExceeded` and no white-capable tile geometry.

## Complete validation matrix

| Validation | Result |
| --- | --- |
| `DarkwellEditor Win64 Development` | Passed after final source/test changes |
| M3.1 NullRHI | 10/10 passed |
| M3.1 D3D12/SM6 | 29/29 passed |
| M3.2 NullRHI | 8/8 passed |
| M3.2 D3D12/SM6 | 22/22 passed |
| Full SightWeave NullRHI | 133 passed, 2 retained M2P2 wall-time failures |
| DARKWELL regression | 24/24 passed |
| Lab NullRHI | Loaded, authoritative query passed, clean teardown |
| Lab D3D12/SM6 | Loaded on RTX 2070 SUPER, authoritative query passed, clean teardown |
| UAT BuildPlugin Win64 | Passed Editor Development, Game Development, Game Shipping |
| Independent source-only clean host Editor Development | Passed; all four modules rebuilt |
| Independent source-only clean host Game Development | Passed; Runtime/Render rebuilt |
| Independent source-only clean host Game Shipping | Passed; Runtime/Render rebuilt |
| Independent clean-host M3.2 D3D12/SM6 | Authoritative retry 22/22 passed |
| Shipping dependency/string/COFF/import scans | Passed |
| Severe-log scan | Zero severe hits in eight authoritative final logs |

Lab logs report `status=0 authoritative=1 live=1 vision=1 light=0 bypass=1 snapshot=58 visionSources=1 illuminationSources=0` in both RHI modes.

## BuildPlugin, clean host, and Shipping isolation

Authoritative UAT package:

`C:\Users\defiler\AppData\Local\Temp\SightWeaveM3P2_BuildPlugin_20260826`

- UAT reported `BUILD SUCCESSFUL` after real UnrealEditor Development, UnrealGame Development, and UnrealGame Shipping source builds.
- Package/source SHA-256 comparison covered 83 `Source` and `Shaders` files with zero missing files and zero mismatches.
- Packaged shader size is 4,277 bytes; SHA-256 is `FFCA93217F1AC1263E2922502DB41F14092853FF717FA620AB4FDA5E42120D4A`.

Independent source-only clean host:

`D:\UE_pro\Darkwell\Saved\SightWeaveM3P2_CleanHost_20260826`

It began without plugin `Binaries` or `Intermediate`, rebuilt all required modules, and passed M3.2 D3D12/SM6 22/22 in `Saved/AutomationReports/M3P2_D3D12_SM6_Retry2`.

Shipping audit:

- Shipping intermediate module directories are exactly `SightWeaveRuntime` and `SightWeaveRender`; Editor and Tests modules are absent.
- 23 Shipping objects and 27 response/precompiled dependency files were scanned.
- Exact binary-string and COFF-symbol matches were zero for both test-readback APIs, `FRHIGPUTextureReadback`, AutomationTest, Tests, Editor, UnrealEd, ETW libraries, and host-game binary/module names.
- The readback translation units exist as guarded empty Shipping objects; their `WITH_DEV_AUTOMATION_TESTS` implementation symbols are absent.
- Runtime DLL imports only Core, CoreUObject, Engine, DeveloperSettings, and platform CRT/kernel libraries.
- Render DLL imports only Projects, RHI, RenderCore, Core, CoreUObject, Engine, SightWeaveRuntime, and platform CRT/kernel libraries.
- Runtime Build.cs has no reverse Render dependency; Render has no Tests, Editor, UnrealEd, Darkwell, AutomationTest, or ETW dependency.

## Retained failures and warnings

No threshold was lowered and no retained failure was deleted.

- Full SightWeave NullRHI retained the two known M2P2 wall-clock failures: `Batch512Gate` (worst median/p95/p99/max 154.898/187.699/225.101/238.203 us, zero steady capacity growth) and `PreparedEventIndex4096` (median/p95/p99/max total 1521.599/1564.600/2241.600/2316.300 us). Prior administrator ContextSwitch ETW remains the intrinsic M2 authority.
- The first independent clean-host D3D run intentionally demonstrated the unsupported-SM5 fail-black path because the empty host defaulted to D3D12/SM5. Explicit `-sm6` selected `PCD3D_SM6` as required.
- The first explicit-SM6 clean-host run passed 21/22; `Capacity128` had one unavailable absolute GPU timestamp. A fresh-process authoritative retry passed 22/22. Functional readback/parity was otherwise intact.
- An earlier main-project M3.1-after-bridge run had the same one-time absolute timestamp absence in `BypassUnion`; the final M3.1 D3D12 regression passed 29/29.
- Build warnings are the non-preferred Visual Studio 14.51 toolchain notice and C4996 deprecations originating in UE 5.8 engine headers. No SightWeave warning or error was emitted.
- Severe scans found no ensure, assertion, fatal/critical error, unhandled exception, GPU crash, Device Removed, DXGI error, shader compile error, RDG validation failure, resource leak, stale-world access, or teardown crash in authoritative final logs.

## Git checkpoints and closure status

Local checkpoints after the frozen baseline are:

1. `b1ed646` — `docs: start SightWeave M3P2 sparse atlas work`
2. `3cdc06a` — `feat: add persistent SightWeave atlas residency`
3. `0b33e2a` — `feat: schedule SightWeave dirty tiles`
4. `456e747` — `test: validate SightWeave multi-tile GPU parity`
5. `4635dc4` — `perf: measure warmed SightWeave atlas updates`
6. `c35fb54` — `test: validate SightWeave packaging boundaries`
7. `docs: record SightWeave M3P2 validation` — this documentation checkpoint

Checkpoint 1 is present on the configured upstream. Checkpoints 2-7 remain local pending explicit authorization to push to the configured external `origin`; therefore Git/remote closure is the only reason for `PARTIAL`. `Darkwell.uproject`, generated build/test data, temporary packages, DDC, reports, and binary assets are not part of the commits.

## Unvalidated scope and remaining risks

- RTX 4060 final repetition remains outstanding; Turing data does not replace it.
- D3D11, Vulkan, final post-process composition, point-sampled consumer material, feathering, memory/Last-Seen, damage reveal, SceneCapture, gameplay integration, and Fab final packaging remain outside M3.2.
- The GPU timestamps are Development automation samples, not a full-frame shipping workload.
- Remote durability is pending the explicit push approval described above.

## Recovery command

After explicit approval for the configured `origin`, resume with:

```powershell
git status --short --branch
git log -7 --oneline --decorate
git push origin codex/m3p2-sightweave-sparse-atlas-residency
```

Then verify local HEAD, upstream, and `git ls-remote` match and change this document from `PARTIAL` to `COMPLETED`. Do not merge, rebase, force-push, modify `Darkwell.uproject`, or shut down the computer.

## Final verdict

**PARTIAL** — every local technical acceptance gate passes; only explicitly authorized remote Git closure remains.

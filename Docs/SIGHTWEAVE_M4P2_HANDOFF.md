# SightWeave M4P2 packaging and performance closure handoff

Status: **COMPLETED**

- Branch: `codex/m4p2-sightweave-packaging-performance-closure`
- Frozen M4P1 baseline: `93f156f552aa85ee9d30891508d439011c57c479`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Validation hardware: NVIDIA GeForce RTX 4060, D3D12 / SM6
- Host project: `D:\UE_projects\LastLight`

## Outcome

M4P2 closed the packaging, Shipping-isolation, clean-host, automation, lifecycle, and performance work deferred by M4P1. It added no gameplay or visual feature and changed no frozen subject-policy, memory, presentation, serialization, or public runtime contract.

The released topology was proved end to end:

```text
repository plugin
  -> UE 5.8 BuildPlugin -Rocket
  -> fresh external package
  -> fresh source-isolated host
  -> Editor Development / Game Development / Game Shipping
  -> Cooked/Staged Win64 Shipping, Pak + IoStore, D3D12/SM6
  -> public-interface lifecycle smoke and natural process exit
```

Shipping contains only `SightWeaveRuntime` and `SightWeaveRender`. Final object, dependency, macro, COFF-symbol, filename, content-string, PE-import, and severe-log scans found no SightWeave Editor/Test implementation, readback/benchmark implementation, DARKWELL dependency, repository path, or forbidden Shipping binary.

## Reliable checkpoints

| Checkpoint | SHA | State |
| --- | --- | --- |
| `docs: start SightWeave M4P2 packaging closure` | `f246ce5` | pushed |
| `build: close SightWeave plugin packaging boundaries` | `f3bb4c7` | pushed |
| `test: refresh SightWeave packaging assertions` | `f33dd3f` | pushed |
| `perf: expand SightWeave closure matrices` | `c57b774` | pushed |
| `docs: record SightWeave M4P2 final validation` | `ac1ece8` | pushed; superseded by later closure evidence |
| `fix: close SightWeave automation resource lifetime` | `1abafb6` | pushed |
| `perf: stabilize SightWeave prepared and batch gates` | `0809a1c` | pushed |
| `test: retain SightWeave presentation GPU samples` | `4dae900` | pushed |
| `test: prove SightWeave render lifecycle closure` | `a6a5c83` | pushed |
| `test: isolate SightWeave batch performance core` | `95d59f2` | pushed |

The final documentation checkpoint is the branch tip created after this file was updated. Its exact local/upstream/remote SHA is reported by the final closure commands and task handoff.

## Final evidence

- Full SightWeave NullRHI closure: **175/175**.
- Full SightWeave D3D12/SM6 closure: **267/267**, 129.24 seconds.
- Full DARKWELL NullRHI: **24/24**.
- M3.4: NullRHI **21/21**, D3D12/SM6 **37/37**.
- M3.5: D3D12/SM6 **26/26**.
- M4P1: NullRHI **9/9**, D3D12/SM6 **12/12**.
- Prepared4096 final: NullRHI p50/p95/p99 `119.098/138.599/191.499 us`; D3D12 p50/p95/p99 `74.700/78.201/79.699 us`.
- Batch512 final ten distributions: worst NullRHI p95/p99 `139.002/145.901 us`; worst D3D12 p95/p99 `144.098/151.899 us`; no capacity growth.
- Exact M3.4 `Width0.1080p.Tiles1Sources2` passed in three independent processes. GPU p95 was `296 us`, `188 us`, and `950 us`, all below the frozen 1 ms gate.
- Readback and LastSeen real-view lifecycle tests pass in the same process; the final full closure emitted no cumulative virtual-reservation warning.

BuildPlugin closure: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_BuildPlugin_Closure_95d59f2_20260828_1748`.

It passed Editor 102/102, Game Development 32/32, Game Shipping 32/32, and UAT exit 0. The delivery map was 128/128 with no missing or extra source/shader/content/config files. Portable `Engine.ini` SHA-256: `20B5D72E3EA4E1409E9997182F140B9E1C45B74AEEF82E2E1E42F1B26F4A54DC`.

Clean-host closure: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_CleanHost_Closure_95d59f2_20260828_1805`.

It passed Editor 102/102, Development Game 32/32, and Shipping Game 32/32. Shipping compiled exactly Runtime 19 plus Render 13 objects with `WITH_DEV_AUTOMATION_TESTS=0`, `WITH_EDITOR=0`, `WITH_EDITORONLY_DATA=0`, and `UE_BUILD_SHIPPING=1`.

Final staged Shipping archive: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM4P2_StagedShipping_Closure_95d59f2_20260828_1955_FinalFix2`.

BuildCookRun rebuilt current source, cooked 494 packages, staged Pak/IoStore, archived 30 files totaling 673,112,704 bytes, and exited 0. Main executable SHA-256: `AD2FA409148448002EFA1BF368592E14BA5D974D105A24BF8AD48DB288FB6AAA`.

The staged D3D12/SM6 executable reported `success=true`, `teardown_complete=true`, `ready_for_screenshot=true`, Width=50 cm, 56 passes, zero failures, then exited naturally. Authoritative JSON: `Saved/Logs/M4P2/M4P2_ShippingSmoke_FinalFix2.json`. The ignored screenshot visibly shows PASS; a Windows Firewall prompt from the earlier UAT `UnrealPak` process overlays its center, and no network permission was granted.

## Retained failures and classification

All failed attempts remain as ignored evidence and are superseded, not erased:

- Early Prepared4096/Batch512 misses were traced to automation setup/scheduling variance; isolated core coverage and final full matrices pass unchanged frozen gates.
- The earlier M3.4 1.169 ms p95 measurement is retained. Three independent focused processes now pass the same exact row without changing the gate.
- Two early stage attempts used UE 5.8 Zen staging and failed to attach the staging oplog at `[::1]:8558`. Source inspection established that `UProjectPackagingSettings` is Game config; the final fixture used `DefaultGame.ini` with `bUseZenStore=False` plus `-AdditionalCookerOptions=-SkipZenStore`. Shipping, Pak, IoStore, and SM6 requirements remained unchanged.
- The first valid staged runtime correctly rejected two fixture checks because the fixture's static-description Z range excluded its own `FloorPlaneZ=-200`. Only the ignored neutral fixture was corrected; production code was unchanged. FinalFix2 passed all checks.
- Engine C4996 and non-preferred MSVC toolchain diagnostics remain informational; all required builds pass.

No threshold was weakened and no failed or unverified optimization was committed.

## Repository state and recovery

`Darkwell.uproject` was never staged, restored, overwritten, or committed. Its only local difference remains the user's EngineAssociation GUID. Generated reports, binaries, Intermediate, DDC, package/archive output, screenshots, and temporary hosts are ignored and untracked.

Resume from home:

```powershell
git fetch origin
git switch codex/m4p2-sightweave-packaging-performance-closure
git pull --ff-only
```

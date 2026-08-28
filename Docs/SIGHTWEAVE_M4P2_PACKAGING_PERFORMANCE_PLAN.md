# SightWeave M4P2 packaging and performance plan

Status: **FROZEN, EXECUTED, AND COMPLETED**

Frozen product baseline: M4P1 `93f156f552aa85ee9d30891508d439011c57c479`

## 1. Authority and purpose

M4P2 closed the work explicitly deferred by `SIGHTWEAVE_M4P1_FINAL_VALIDATION.md`: BuildPlugin delivery, source-isolated Editor/Game builds, Shipping isolation, Cooked/Staged D3D12/SM6 lifecycle smoke, full regression, render/readback lifecycle, and expanded performance evidence. It added no capability.

The normative M4P1 subject-memory, M3.5 memory, M3 GPU-mask, requirements, and architecture contracts remained frozen. Historical M2/M3 performance documents supplied the thresholds. No threshold was weakened, renamed, or inferred from a baseline-only row.

## 2. Frozen exclusions

M4P2 excluded new gameplay or visual behavior, public policy/API/serialization changes, DARKWELL adapter or persistence work, SceneCapture authority, skeletal/VFX/light/audio/material snapshot expansion, reveal overrides, D3D11/Vulkan expansion, art/content changes, legacy-fog deletion, and `/Game/Maps/L_Prototype` changes. No `.uasset` or `.umap` was moved, renamed, deleted, or rewritten.

## 3. Executed delivery topology

```text
repository SightWeave plugin
  -> RunUAT BuildPlugin -TargetPlatforms=Win64 -Rocket
  -> new external package directory
  -> new external source-isolated host
  -> Editor Development
  -> Game Development
  -> Game Shipping
  -> BuildCookRun Win64 Shipping, Pak + IoStore, D3D12/SM6
  -> public-interface staged lifecycle smoke
```

All UBT, UAT, editor automation, package, cook, stage, and runtime processes were serialized. Failed evidence was retained and classified before bounded retries.

## 4. Packaging and Shipping gates

BuildPlugin had to compile Editor Development, Game Development, and Game Shipping, exit zero, and preserve the repository Source/Shaders/Content/Config delivery map. A fresh host could contain no repository linkage, reparse point, DARKWELL source, or reused generated directory.

Shipping modules were restricted to `SightWeaveRuntime` and `SightWeaveRender`. Editor, Tests, UnrealEd, automation registration, test readback/benchmark implementations, development Lab/screenshot controls, and host/DARKWELL dependencies were forbidden. Source/build metadata, objects, imports, exact strings, and COFF symbols were scanned and classified.

These gates passed. Final detail is in `SIGHTWEAVE_M4P2_EXECUTION_REPORT.md`.

## 5. Staged D3D12/SM6 gate

The staged Win64 Shipping game had to prove plugin/module load, real GameViewport and camera, Runtime/Render subsystem creation, RHI resources, M3.4 Width=50 inward feather, M3.5 static memory dirty/no-change/clear, all required M4P1 LastSeen transitions, resource release, render-command drain, world teardown, and natural process exit. SceneCapture and test-only Shipping symbols were not permitted substitutes.

The final Pak/IoStore archive passed 56/56 deterministic public-interface checks under D3D12/SM6 and exited naturally.

## 6. Automation gate

Exact discovered/performed results were required for full SightWeave under NullRHI and D3D12/SM6, full DARKWELL NullRHI, M3.4, M3.5, M4P1, Lab/focused coverage, and lifecycle tests. Final closure is SightWeave 175/175 NullRHI and 267/267 D3D12/SM6, DARKWELL 24/24, M3.4 21/21 NullRHI and 37/37 D3D12, M3.5 26/26 D3D12, and M4P1 9/9 NullRHI and 12/12 D3D12.

## 7. Performance gates

| Metric | Frozen gate |
| --- | --- |
| Batch512 | p50 `<=150 us`, p95 `<=180 us`, p99 `<=200 us` |
| Prepared4096 | p50 `<1 ms`, p99 `<2 ms` |
| Selected M3.5 Memory CPU dirty | p95 `<250 us` |
| Selected M3.5 GT packet | p95 `<250 us` |
| Selected M3.5 RT dirty/setup | p95 `<200 us` |
| Selected M3.5 GPU dirty | p95 `<250 us` |
| Warm no-change | zero mirror/upload/raster/allocation work |
| Live 1080p, 2/8 sources | GPU p95 `<1 ms` |
| Live 1440p, 2/8 sources | GPU p95 `<1.5 ms` |
| 32-source pressure | GPU p95 `<2 ms` at 1080p, `<3 ms` at 1440p |
| Persistent live presentation | `<32 MiB` |
| Plugin runtime persistent memory | `<=64 MiB` |
| hard-zero/nonfinite/seam/stale/binding | zero unexplained failures |

Final Prepared4096 p50/p99 was 119.098/191.499 us under NullRHI and 74.700/79.699 us under D3D12. Final Batch512 worst p95/p99 was 139.002/145.901 us under NullRHI and 144.098/151.899 us under D3D12, with zero capacity growth. The formerly variable exact M3.4 row passed three independent processes with GPU p95 at or below 950 us. All other frozen gates passed.

Expanded resident/dirty pressure and M4P1 transition timings remain baselines wherever their authority specifies no independent performance limit; correctness gates still pass.

## 8. Evidence and severe-log policy

Generated reports, logs, screenshots, package outputs, clean hosts, staged archives, Binaries, Intermediate, Saved, and DDC remain ignored and uncommitted. Every final log was checked for shader/RDG errors, D3D12/RHI errors, device removal, GPU crash/hang, fatal, assertion, ensure, unhandled exception, stale callback, binding failure, and automation failure. Final evidence contains no severe result.

Retained failures are not deleted or reclassified as passes. The execution report records early performance variance, Zen staging transport failures, and the neutral fixture range error together with their validated resolution.

## 9. Git discipline

Every reliable non-empty checkpoint was built or tested in proportion to risk, committed, and pushed normally. No force push, merge, rebase, reset, clean, generated output, or `Darkwell.uproject` change was allowed. Final closure runs the exact requested Git/LFS/object commands and verifies local, upstream, and remote equality.

## 10. Applied verdict

**COMPLETED.** BuildPlugin, clean-host Editor/Game Development/Game Shipping, Shipping scans, Cooked/Staged D3D12/SM6 runtime smoke, complete automation, lifecycle closure, all frozen thresholds, clean severe logs, and Git/LFS closure passed. No unresolved M4P2 item remains.

# SightWeave M4P2 packaging and performance plan

Status: **FROZEN FOR M4P2 EXECUTION**

Frozen product baseline: M4P1 `93f156f552aa85ee9d30891508d439011c57c479`

## 1. Purpose and authority

M4P2 closes the P2 packaging work explicitly deferred by `SIGHTWEAVE_M4P1_FINAL_VALIDATION.md`: independent BuildPlugin delivery, source-isolated clean-host builds, Shipping isolation, packaged D3D12/SM6 lifecycle/presentation smoke, complete regression, and the expanded performance matrix. It does not add capability.

Normative product contracts remain:

- `SIGHTWEAVE_M4P1_SUBJECT_MEMORY_CONTRACT.md`
- `SIGHTWEAVE_M3P5_MEMORY_CONTRACT.md`
- `SIGHTWEAVE_M3_GPU_MASK_CONTRACT.md`
- `SIGHTWEAVE_M3_GPU_MASK_VALIDATION_PLAN.md`
- `VISION_SYSTEM_REQUIREMENTS.md`
- `VISION_SYSTEM_ARCHITECTURE.md`

Historical measurements and threshold sources are `SIGHTWEAVE_M3P4_FINAL_VALIDATION.md`, `SIGHTWEAVE_M3P5_FINAL_VALIDATION.md`, `SIGHTWEAVE_M3P5_PERFORMANCE.md`, and the M2P2/M2P3 performance contracts. No threshold may be weakened, renamed, or inferred from a baseline-only measurement.

## 2. Hard exclusions

The following are outside M4P2: new gameplay/visual behavior; policy/API/serialization changes; DARKWELL adapter integration; persistence implementation; SceneCapture authority; skeletal, Niagara, VFX, light, audio, dynamic-material, or Blueprint-assembly snapshot support; reveal override; D3D11/Vulkan expansion; art/content changes; legacy fog removal; `/Game/Maps/L_Prototype`; and any ordinary-filesystem mutation of Unreal assets.

Development-only validation may be reorganized only when the production Runtime/Render behavior remains byte/semantic equivalent. A required public or runtime semantic change stops as an architecture blocker.

## 3. Serialized execution discipline

All UBT, dotnet/UBT, BuildEditor, BuildPlugin, UnrealEditor-Cmd, cook, package, and automation processes run strictly one at a time. Before each launch, inspect for residual `UnrealBuildTool`, UBT `dotnet`, `UnrealEditor-Cmd`, AutomationTool/BuildPlugin, and Live Coding processes.

On failure: preserve the log/report/package directory; wait for the related process to exit; classify the root cause; repair; retry once serially; retain both attempts. No failed evidence directory is deleted and no concurrent retry is allowed.

## 4. BuildPlugin topology and gate

Use the installed UE 5.8 AutomationTool:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin `
  '-Plugin=D:\UE_projects\LastLight\Plugins\SightWeave\SightWeave.uplugin' `
  '-Package=<new external temp directory>' `
  '-TargetPlatforms=Win64' -Rocket
```

The output directory is newly timestamped and outside the repository. It must contain the descriptor, Runtime/Render/Editor/Tests source as permitted by BuildPlugin, shader source/metadata, content, and the expected precompiled products. UAT must perform and pass UnrealEditor Win64 Development, UnrealGame Win64 Development, and UnrealGame Win64 Shipping. Package inventories and SHA-256 comparisons cover at least `Source`, `Shaders`, `Content`, `Config` when present, and the descriptor.

The descriptor gate checks module names/types/loading phases, Win64 support, `CanContainContent`, and absence of an unintended host-project dependency. No generated default filter file is committed.

## 5. Independent clean-host topology

Create a fresh blank UE 5.8 C++ host under a new external temporary directory. Install only the BuildPlugin package at `<CleanHost>/Plugins/SightWeave`. Do not use a junction, symlink, include path, plugin search path, or copy from `D:\UE_projects\LastLight\Plugins\SightWeave` after installation. The host must not contain DARKWELL source.

Delete no evidence on failure. Begin without plugin `Binaries`/`Intermediate` when testing source portability, or consume only BuildPlugin precompiled products when validating package consumption; record which topology each command exercises. Required clean-host targets are:

1. `UnrealEditor Win64 Development`
2. `UnrealGame Win64 Development`
3. `UnrealGame Win64 Shipping`

Any minimal smoke fixture uses only public SightWeave interfaces and neutral host code/content. Repository/package/host source inventories and SHA-256 values must prove no missing, mismatched, or extra plugin source/shader/content file.

## 6. Shipping dependency boundary and scans

Expected Shipping modules are exactly `SightWeaveRuntime` and `SightWeaveRender`.

Allowed UE dependencies:

- Runtime: Core, CoreUObject, Engine, DeveloperSettings
- Render: Core, CoreUObject, Engine, SightWeaveRuntime, Projects, RHI, RenderCore, Renderer

Forbidden in Shipping source/build products/imports/strings/COFF symbols include:

- `SightWeaveTests`, `SightWeaveEditor`, UnrealEd, AutomationTest and automation registration
- test readback types, `FRHIGPUTextureReadback`, test shader entry points, test-only console/Lab control paths, screenshot capture, benchmark/readback APIs
- DARKWELL/private host modules and paths
- development-only ETW/test libraries such as Advapi32, Psapi, and PowrProf when not a platform dependency of permitted engine modules
- SceneCapture used as SightWeave authority or packaged-smoke substitution

Scans cover Build.cs/source guards, UBT response/dependency/precompiled files, Shipping object file paths, DLL import tables, exact binary strings, and `dumpbin /symbols` COFF output. Generic engine metadata hits are classified by source/symbol and are not waived by broad substring suppression. A real forbidden dependency is fixed at the boundary, never hidden by renaming.

## 7. Packaged D3D12/SM6 smoke

Run on the RTX 4060 with real D3D12/SM6 and a real game view, not SceneCapture. The smoke must prove:

1. packaged plugin/module load and neutral host startup;
2. world create/destroy and at least two restart/transition cycles;
3. Render/Runtime world subsystem creation, packet publication, RHI resource initialization, applied revision, and resource release;
4. M3.4 Width=50 inward feather with hard-zero pixels remaining black;
5. M3.5 explicit static-environment memory, dirty/no-change/clear behavior, and neutral attribute path;
6. M4P1 legal falling-edge Last-Seen, frozen neutral proxy, suppression/clear/unknown, reacquire, identity reuse, and page/tile-boundary path through public interfaces;
7. proxy hide before live reacquire, no duplicate/residue/black handoff;
8. render-command drain, world teardown, module/process exit, and no late stale command/readback callback.

Capture exact logs and where practical deterministic screenshots/readbacks. Development readback may validate Development, but Shipping smoke must not depend on test-only symbols.

## 8. Automation matrix

Each run records exact discovered, performed, succeeded, succeeded-with-warning, failed, skipped/not-run, duration, process exit code, report path, and log path. Counts are discovered from the current binary and never assumed from M4P1/M3.5 reports.

| Gate | RHI/configuration | Filter |
| --- | --- | --- |
| M4P1 complete | NullRHI | `SightWeave.M4P1` |
| M4P1 complete | D3D12/SM6 | `SightWeave.M4P1` |
| continuous transition | D3D12/SM6 | `SightWeave.M4P1.Visual.ContinuousTransition` |
| M3.4 presentation | D3D12/SM6 | current presentation prefix discovered under M3P4 |
| M3.5 complete | NullRHI and D3D12/SM6 as supported by discovered tests | `SightWeave.M3P5` |
| full SightWeave | NullRHI | `SightWeave` |
| full SightWeave | D3D12/SM6 | `SightWeave` |
| full DARKWELL | NullRHI | `Darkwell` |
| Lab | NullRHI | current SightWeave Lab filters, including M4P1 |
| Lab | D3D12/SM6 | current SightWeave Lab filters, including M4P1 |
| clean-host focused/full | D3D12/SM6 | package-supported M3.4/M3.5/M4P1 filters |

Historical M2P2 wall-time failures remain governed by their frozen contracts. A full-suite failure is retained and may receive one isolated confirmation without changing the original result or threshold.

## 9. Performance workload matrix

Run D3D12/SM6 Development with VSync and smoothing disabled, stable power/clock conditions, declared warmup, retained samples, and p50/p95/p99. Cold creation is separate. If existing tests do not cover the complete Cartesian matrix, add bounded Development automation/harness coverage without production semantic changes.

Axes:

- resolution: 1920x1080 and 2560x1440;
- source count: 2, 8, 32 total sources;
- resident memory tiles: 1, 8, 128;
- dirty memory tiles: 1, 8, 32, including gutter expansion;
- operations/states: dirty, warmed no-change, clear, block, suppress, Last-Seen remembered, reacquire, identity reuse, and page/tile boundary;
- live presentation: unchanged/composite-only, controlled dirty updates, Width=50 inward feather, and the 32-source pressure row.

At minimum every axis value must participate in a declared paired/scaling matrix matching the frozen M3.5 topology (2/1, 8/8, 32/128 plus dirty 1/8/32) and every operation/state must have a measured row. Do not imply an unmeasured full Cartesian product.

For each row record:

- Game Thread p50/p95/p99 and sample count;
- Render Thread p50/p95/p99 and sample count;
- total/stage GPU p50/p95/p99 and sample count;
- GPU Memory mirror/update p50/p95/p99;
- CPU snapshot/packed authority bytes;
- GPU persistent live, memory, static-attribute, page-table, and total bytes;
- worst plugin persistent runtime memory and transient 1080p/1440p output separately;
- upload bytes, requested/expanded dirty counts, allocations, and no-change work;
- nonfinite, hard-black leak, stale-command/readback, binding-failure, capacity, and lifecycle counters.

## 10. Existing threshold sources

Formal/frozen gates are not invented by M4P2:

| Metric | Existing gate/source |
| --- | --- |
| selected Memory CPU dirty p95 | `<250 us`, M3.5 memory contract/performance |
| selected Memory GT packet p95 | `<250 us`, M3.5 performance |
| selected Memory RT dirty/setup p95 | `<200 us`, M3.5 performance |
| selected Memory GPU dirty p95 | `<250 us`, M3.5 memory contract/performance |
| warmed no-change | zero mirror/upload/raster/allocation work; M3.5 contract |
| live total GPU p95 at 1080p 2/8 sources | `<1.0 ms`; M3 validation plan/M3.5 |
| live total GPU p95 at 1440p 2/8 sources | `<1.5 ms`; M3 validation plan/M3.5 |
| 32-source pressure p95 | `<2.0 ms` at 1080p, `<3.0 ms` at 1440p; M3 validation plan/M3.5 |
| persistent live presentation | `<32 MiB`; M3.4/M3.5 |
| worst plugin runtime persistent memory | `<=64 MiB`; M3.5 memory contract |
| hard-zero/nonfinite/seam/stale/binding failures | zero unexplained occurrences; M3/M3.4/M3.5 contracts |
| Width=50 | frozen standard development behavior; M3.4 |

The resident 8/dirty 8 and resident 128/dirty 32 CPU/RT values were historically pressure data above the one-dirty update budgets; they remain reported baselines unless an authority document explicitly gives them a gate. Likewise M4P1 transition/identity operations have correctness gates but no existing independent timing threshold. M4P2 records their baselines and risks rather than fabricating pass criteria.

M2 intrinsic gates remain unchanged when exercised by the full prefix: Batch512 p50 `<=150 us`, p95 `<=180 us`, p99 `<=200 us`; broad door p99 `<250 us`; Prepared 4096 p50 `<1 ms`, p99 `<2 ms`; and frozen hot-path allocation/parity requirements.

## 11. Evidence layout and severe-log scan

Generated evidence is ignored and uncommitted:

```text
Saved/
  AutomationReports/M4P2/<gate>-<rhi>-<timestamp>/
  Logs/M4P2/<gate>-<rhi>-<timestamp>.log
  Screenshots/M4P2/<gate>-<timestamp>/
<system-temp>/SightWeaveM4P2_BuildPlugin_<timestamp>/
<system-temp>/SightWeaveM4P2_CleanHost_<timestamp>/
```

Every final log is scanned for shader compiler/ShaderCompileWorker errors, Renderer/RenderCore errors, RDG validation, D3D12/RHI errors, DXGI/device removal, GPU crash/hang, fatal, critical error, assertion, ensure, unhandled exception, stale command/readback, binding failure, and failed tests. Known engine diagnostics are identified by exact message and context; broad exclusion is forbidden.

## 12. Git and LFS checkpoints

Before every commit: inspect `git status --short --branch`, `git diff --check`, staged paths, and `git diff -- Darkwell.uproject`. Source/build checkpoints require the repository-standard full Editor build before commit. After relevant validation, commit non-empty results and push normally immediately.

Never commit generated directories, package/host outputs, reports, screenshots, temporary filter templates, `Darkwell.uproject`, or `/Game/Maps/L_Prototype`. Never force-push, merge, rebase, reset, or clean this milestone branch.

Final closure runs the exact Git/LFS/object commands required by the task and records local/upstream/remote SHA equality plus the sole approved EngineAssociation difference.

## 13. Verdict rules

**COMPLETED** requires every BuildPlugin, clean-host, Shipping, packaged-smoke, automation, lifecycle, severe-log, threshold, Git, and LFS condition in this plan. Existing baseline-only rows may remain baseline-only only where the authority documents contain no threshold, and the final report must say so.

**PARTIAL** is required when reliable plugin packaging exists but a manual/external packaged smoke, required environment, full matrix row, or existing gate remains unresolved or failed. **BLOCKED** is required for an unresolved architecture/roadmap conflict, true forbidden Shipping dependency, external authority that prevents progress, or a gate that can only be met by weakening a frozen contract.

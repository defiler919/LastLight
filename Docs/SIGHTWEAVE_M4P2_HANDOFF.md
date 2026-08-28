# SightWeave M4P2 packaging and performance closure handoff

Status: **PARTIAL — reliable package/Shipping boundary; retained performance gates and staged-game smoke remain**

- Branch: `codex/m4p2-sightweave-packaging-performance-closure`
- Frozen M4P1 baseline: `93f156f552aa85ee9d30891508d439011c57c479`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Primary validation hardware: NVIDIA GeForce RTX 4060, D3D12 / SM6
- Host project: `D:\UE_projects\LastLight`

## Authoritative scope

The current repository does not assign a different formal name to the post-M4P1 milestone. `SIGHTWEAVE_M4P1_FINAL_VALIDATION.md` explicitly defers BuildPlugin, clean-host Editor/Game Development/Game Shipping, Shipping dependency/import/string/COFF scans, full SightWeave and DARKWELL regression, and the expanded performance matrix to **P2 packaging closure**. This milestone therefore uses the approved task name **M4P2 — SightWeave Packaging / Shipping / Clean-Host / Performance Closure**.

M4P2 adds no gameplay or visual feature. It proves that the frozen M4P1 behavior can ship as an independent plugin without host-project private source or Editor/Test leakage, and closes the deferred packaged D3D12/SM6, regression, lifecycle, and performance evidence.

The historical `VISION_SYSTEM_MIGRATION_PLAN.md` recommendation to begin at M0 predates the completed M1-M4 implementation branches and is retained as design history. It does not supersede the current M4P1 freeze and P2 deferral.

## Frozen product contracts

M4P2 must not change the five subject-policy semantics, legal falling-edge capture, immutable snapshot descriptors, render-only proxies, Custom fail-closed behavior, clear/suppression/unknown ordering, world/owner/floor/profile/revision/generation isolation, M3.4 Width=50 inward feather, M3.5 static environment memory, or the accepted Camera 0-4 PIE behavior. Any packaging fix that requires a public API, serialization, or runtime semantic change is an architecture blocker and is not made silently.

No DARKWELL gameplay integration, persistence implementation, skeletal/VFX/light/audio capture, SceneCapture authority, reveal override, production content, D3D11/Vulkan expansion, legacy fog deletion, `/Game/Maps/L_Prototype` modification, or new art/gameplay behavior belongs to this milestone.

## Delivery topology

The authoritative chain is:

```text
repository plugin source
  -> UE 5.8 RunUAT BuildPlugin -TargetPlatforms=Win64 -Rocket
  -> fresh repository-external package directory
  -> fresh repository-external blank host project
  -> install only the packaged SightWeave plugin
  -> clean Editor Development / Game Development / Game Shipping
  -> packaged D3D12/SM6 smoke through public plugin interfaces
```

The clean host must not link, junction, or include the repository plugin source or DARKWELL private source. BuildPlugin output, clean-host projects, reports, screenshots, binaries, Intermediate, Saved, DDC, and packaging products remain outside Git.

Shipping contains only `SightWeaveRuntime` and `SightWeaveRender`. Runtime may depend only on Core, CoreUObject, Engine, and DeveloperSettings. Render may depend on Runtime plus Projects, RHI, RenderCore, and Renderer. Shipping must contain no `SightWeaveEditor`, `SightWeaveTests`, UnrealEd, AutomationTest, test readback/benchmark symbols, Lab controls, development screenshot route, or DARKWELL dependency.

## Validation and evidence

The complete matrix, thresholds, topology, scan rules, evidence paths, and verdict rules are frozen in `SIGHTWEAVE_M4P2_PACKAGING_PERFORMANCE_PLAN.md`. Results accumulate in `SIGHTWEAVE_M4P2_FINAL_VALIDATION.md`. Generated evidence uses timestamped children of:

- `Saved/AutomationReports/M4P2/`
- `Saved/Logs/M4P2/`
- `Saved/Screenshots/M4P2/`
- a new timestamped BuildPlugin directory under the system temporary directory;
- a separate new timestamped clean-host directory under the system temporary directory.

Authoritative execution detail is now in `SIGHTWEAVE_M4P2_EXECUTION_REPORT.md`. Final BuildPlugin passed Editor 102, Game Development 32, Game Shipping 32, and UAT exit 0. A fresh source-isolated host passed the same three targets. Shipping contains exactly Runtime 19 + Render 13 objects with correct Shipping macros and zero forbidden COFF symbol rows.

Final automation was SightWeave NullRHI 173/175, SightWeave D3D12/SM6 261 success + 1 warning + 1 failure out of 263, and DARKWELL NullRHI 24/24. M3.4 37/37, M3.5 26/26, and M4P1 12/12 passed in the final D3D12 prefix. M4P1 visual proxy counts remained 1936/1448/514 for the three formal proxy views, with nonfinite 0.

## Git checkpoint policy

Every non-empty reliable checkpoint is built or validated in proportion to its content, committed, and immediately pushed normally. No force push, merge, rebase, reset, clean, empty commit, generated output, or `Darkwell.uproject` change is permitted. Planned checkpoints are:

1. `docs: start SightWeave M4P2 packaging closure`
2. `build: close SightWeave plugin packaging boundaries` when source/build changes are actually required and verified
3. `test: validate SightWeave clean-host shipping`
4. `perf: close SightWeave M4P2 performance matrix`
5. `docs: record SightWeave M4P2 final validation`

## Verdict

- **COMPLETED** requires successful BuildPlugin; independent clean-host Editor Development, Game Development, and Game Shipping; clean Shipping scans; packaged RTX 4060 D3D12/SM6 smoke; all requested regressions with exact discovered/performed counts; the full performance matrix meeting existing gates or explicitly identified baseline-only rows; clean teardown/severe logs; pushed Git/LFS closure; and only the local EngineAssociation GUID difference.
- **PARTIAL** applies when core packaging is reliable but an honest external/manual packaged smoke or environment matrix remains, a retained run misses an existing threshold, or a required item has only baseline evidence.
- **BLOCKED** applies to a roadmap conflict, an irreducible Shipping dependency boundary, unavailable required external authority/environment, or a performance gate that cannot be met without changing the frozen contract.

Applied verdict: **PARTIAL**. Prepared4096 missed its `<1 ms` median gate under both final RHIs; Batch512 distribution 7 missed p99 under NullRHI; and the dedicated M3.4 repair retry retained a 1.169 ms p95 result against 1 ms even though the final full prefix later passed. A Cooked/Staged blank Shipping host must also replace the invalid direct launch of the non-staged BuildPlugin host. No threshold may be weakened to close these items.

## Resume

```powershell
cd D:\UE_projects\LastLight
git switch codex/m4p2-sightweave-packaging-performance-closure
git status --short --branch
```

Next actions, in order: investigate Prepared4096/Batch512 variance; stabilize and re-authorize the M3.4 measurement under controlled clocks; Cook/Stage a blank Win64 Shipping host for D3D12/SM6 lifecycle smoke; then investigate the one cumulative 258/256 GiB RHI virtual-reservation warning from the full prefix.

# SightWeave M4P2 final validation

## 1. Status

**IN PROGRESS — documentation checkpoint only**

No BuildPlugin, clean-host, packaged smoke, automation, Shipping scan, or M4P2 performance result is claimed by this initial document.

## 2. Identity and audit baseline

- Formal milestone: **M4P2 — SightWeave Packaging / Shipping / Clean-Host / Performance Closure**
- Branch: `codex/m4p2-sightweave-packaging-performance-closure`
- Frozen M4P1 baseline: `93f156f552aa85ee9d30891508d439011c57c479`
- Scope source: `SIGHTWEAVE_M4P1_FINAL_VALIDATION.md`, section 11, which assigns the omitted packaging, Shipping, full-regression, and expanded-performance work to P2 packaging closure
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Required GPU path: NVIDIA GeForce RTX 4060, D3D12 / SM6

Start audit:

- local HEAD: `93f156f552aa85ee9d30891508d439011c57c479`
- upstream M4P1: `93f156f552aa85ee9d30891508d439011c57c479`
- remote M4P1: `93f156f552aa85ee9d30891508d439011c57c479`
- worktree: only `Darkwell.uproject` EngineAssociation differs (`5.8` to the local GUID)
- `git diff --check`: passed
- `git lfs status`: no objects to push or commit; only the known unstaged project descriptor
- `git lfs fsck`: `Git LFS fsck OK`

The local project descriptor will not be staged, restored, overwritten, or formatted.

## 3. Frozen scope and exclusions

The exact scope, matrices, thresholds, and verdict rules are in `SIGHTWEAVE_M4P2_PACKAGING_PERFORMANCE_PLAN.md`. M4P1 subject policies and presentation behavior, M3.4 inward feather, and M3.5 memory/static-environment semantics are immutable inputs to this validation.

## 4. Checkpoint ledger

| Checkpoint | SHA | Remote | Evidence |
| --- | --- | --- | --- |
| `docs: start SightWeave M4P2 packaging closure` | pending this commit | pending push | baseline/roadmap audit and three planning documents |

## 5. BuildPlugin

Pending. Record exact command, output directory, start/end time, action counts, warnings, result, package inventory, descriptor validation, and retained failures.

## 6. Clean host

Pending. Record independent topology and Editor Development, Game Development, and Game Shipping results, including proof that only packaged-plugin output was installed.

## 7. Shipping isolation

Pending. Record module/object inventory and exact dependency, import, binary-string, source-string, response-file, and COFF-symbol scan queries and hit classification.

## 8. Packaged D3D12/SM6 smoke and lifecycle

Pending. Record plugin load, world create/destroy, render-resource init/release, M3.4 feather, M3.5 static memory, M4P1 Last-Seen/reacquire, command draining, teardown, and exit evidence. SceneCapture is prohibited as the formal view.

## 9. Automation matrix

Pending. Counts will be recorded from current discovery and execution, never copied from historical reports.

## 10. Performance and memory

Pending. Record p50/p95/p99 for Game Thread, Render Thread, GPU, and GPU mirror/update; CPU snapshot memory, GPU persistent memory, worst plugin runtime memory; plus nonfinite, black-leak, stale-command, and binding-failure counts.

## 11. Severe logs, warnings, and retained failures

Pending. MSVC 14.51 versus preferred 14.50 and engine-header C4996 warnings may be retained only when they remain source-independent. Every new compiler, linker, shader, RDG, D3D12, RHI, GPU, fatal, assert, ensure, crash, or test failure is preserved and classified.

## 12. Final Git/LFS closure

Pending. Final evidence must show local HEAD, upstream, and remote M4P2 SHA equality; no unuploaded LFS object; `git diff --check` and `git fsck --no-reflogs` success; only the known EngineAssociation GUID difference; and no tracked temporary output.

## 13. Final disposition and recovery

Pending. The final report will contain the authoritative status, all commits, unverified items, residual risks, evidence paths, and the next exact resume command.

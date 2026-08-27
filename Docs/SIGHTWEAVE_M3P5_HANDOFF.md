# SightWeave M3.5 static-environment memory handoff

Status: **PARTIAL**

Branch: `codex/m3p5-sightweave-static-environment-memory`

Frozen M3.4 baseline: `22f55b1e744cea37ad5d3c7beb618be0509fbf99`

Final implementation checkpoint before documentation closure: `b6102fa`

Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`

Validation machine: NVIDIA GeForce RTX 4060, driver 610.88, 8188 MiB

## Outcome

M3.5 now provides world-scoped exploration memory for explicitly eligible immutable static environment. The CPU-packed authority is the only HardMemory truth; the R8 GPU memory/static mirrors are derived presentation resources. Final ordering is exact HardLive Scene Color, remembered eligible neutral static environment, then strict-black Unknown. Clear, write-block, presentation-suppress, scope, generation, teardown, fail-black, and Width=0/50 compatibility behavior are covered in native automation.

All implementation, automated performance/memory work, M3.1-M3.5 regressions, M3.5 NullRHI and D3D12/SM6 gates, DARKWELL 24, automated Lab capture, BuildPlugin, clean-host Editor/Game Development/Game Shipping, Shipping isolation, and repository closure work are complete. The single remaining gate is user-operated interactive PIE. Automated PIE and agent screenshot inspection passed, but are not claimed as the user's manual check; therefore status is `PARTIAL`.

## Exact headline evidence

- M3.5: NullRHI 14/14; D3D12/SM6 24/24.
- Full SightWeave: NullRHI 164/164. Full D3D12 performed 248: 246 clean successes, one success with an engine RHI warning, and one historical M2P2 wall-time failure.
- The retained M2P2 full-suite median was 1,015.197 us against `< 1,000 us`; the one isolated confirmation passed at 985.600 us median. No gate or sample was changed.
- DARKWELL: 24/24 NullRHI.
- Precision: 4/4; Coarse 25 cm/texel selected. Resident/dirty scale: 3/3. Frozen M3.4 Width=50 live matrix: 9/9.
- Lab authority record: `remembered:1 live:1 clear:0 block:0 suppressed:1/1 unknown:0`.
- Final BuildPlugin: Editor 94, Game Development 30, Game Shipping 30 actions; all succeeded.
- Clean host: initial 94/30/30 clean actions succeeded; final test-only sync rebuilt Editor 6 actions, Game Development/Shipping remained up-to-date and succeeded.
- Repo/package/clean-host `Source`+`Shaders`+`Content`: 115 files each, zero missing/mismatch/extra.
- Shipping: 30 objects, only Runtime/Render modules, zero forbidden string/import/COFF symbol hits. Five generic `SceneCapture` metadata occurrences are engine types/fields, not SightWeave code or authority.
- Severe D3D12 log scan: zero Shader/RDG/D3D12/RHI fatal, GPU crash, assertion, or ensure matches.

Exact automation paths, timings, memory bytes, warning classification, BuildPlugin/clean-host locations, all commits, and Shipping scans are in `Docs/SIGHTWEAVE_M3P5_FINAL_VALIDATION.md`. Canonical performance tables are in `Docs/SIGHTWEAVE_M3P5_PERFORMANCE.md`.

## Preserved issues

- User-operated interactive PIE remains pending.
- The complete D3D12 run's M2P2 4096-source sample failed at 1,015.197 us median; isolated confirmation passed at 985.600 us.
- One passed D3D12 test carried the engine 258 GB/256 GB virtual-resource reservation warning.
- D3D12 startup retained 13 pre-test `Condition failed` lines plus navigation/scalability warnings; focused reports and the M3.5 24-test report have zero test warnings/errors.
- BuildPlugin/clean-host compilation retained UE engine-header C4996 warnings and the VS 14.51 non-preferred-toolchain warning.
- The first clean-host Lab attempt expected a host-dependent Recast warning exactly once and failed when it occurred zero times; that brittle expectation was repaired and the retry passed 1/1. The original report remains preserved.

## Scope and working tree

`/Game/Maps/L_Prototype` was not modified. `Darkwell.uproject` was not staged, restored, or committed; its only local difference remains the approved EngineAssociation GUID. Generated Binaries, Intermediate, Saved, DDC, AutomationReports, temporary packages, and the BuildPlugin-generated default FilterPlugin template are not committed.

## Resume at home

```powershell
git fetch origin
git switch codex/m3p5-sightweave-static-environment-memory
git pull --ff-only
```

After opening the project with Unreal Engine 5.8.1, perform the one remaining interactive M3.5 Lab PIE check: verify live white/current Scene Color, neutral remembered static structure, strict-black unknown/clear/block/suppress, and no dynamic-object or current-lighting memory leakage. If that manual check passes, update the milestone status from `PARTIAL` to `COMPLETED` with the user's evidence.

Keep the computer on; do not shut down, sleep, or restart it as part of this handoff.

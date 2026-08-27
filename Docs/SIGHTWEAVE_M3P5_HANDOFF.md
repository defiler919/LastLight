# SightWeave M3.5 static-environment memory handoff

Status: **COMPLETED**

Branch: `codex/m3p5-sightweave-static-environment-memory`

Frozen M3.4 baseline: `22f55b1e744cea37ad5d3c7beb618be0509fbf99`

Final implementation checkpoint before documentation closure: `b6102fa`

Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`

Validation machine: NVIDIA GeForce RTX 4060, driver 610.88, 8188 MiB

## Camera 1 repair follow-up (2026-08-27)

The Camera 1 all-black blocker is repaired in automated D3D12 PIE, and the user has completed the final manual Camera 0-4 PIE inspection. Status is **COMPLETED**.

The root cause was a fail-closed presentation-scope mismatch, not missing CPU HardMemory or missing GPU resources. Memory used Coarse while the live packet was fixed at Standard; after precision alignment, the sparse live binding still omitted the canonical compatibility profile of bypass vision sources. Both GPU mirrors were already available. The final composite now reports `memoryReady=1 staticEnvironmentReady=1 memoryPresentationAvailable=1 memoryScopeMismatchMask=0x00 memoryPrecisionTier=0 livePrecisionTier=0 memoryProfiles=1 liveProfiles=1`, with 55 memory and 9 static page-table entries.

The render world now rebuilds the default live packet at the valid memory scope's precision for the same owner/floor, while explicit presentation scope remains authoritative. Sparse bypass tiles retain their canonical profiles, restoring exact profile equivalence without relaxing fail-closed behavior. State-change-limited diagnostics now expose readiness, availability, packet/eligibility revisions, page tables, resident tiles, resource/residency generations, scope components, mismatch mask, and precision tiers.

Fresh gates on the repair machine (NVIDIA GeForce RTX 2070 SUPER, D3D12 SM6) are:

- Editor Development: succeeded after the final changes.
- complete M3.5 NullRHI: 16/16.
- complete M3.5 D3D12/SM6: 26/26.
- M3.4 presentation: 3/3; M3.4 D3D12 feather/safety/performance: 29/29.
- Camera 1 focused D3D12 capture: 1/1, no warnings or failures.
- final severe Shader/RDG/RHI/GPU/fatal/assert/ensure scan: zero matches.

Lab automation now captures all five cameras. Direct inspection shows Camera 1's authored neutral gray `(68,68,68)` instead of black, continuous Camera 3 page-boundary output, aligned yaw=45 Camera 4 output, and no non-live dynamic/current-light/VFX content promoted into remembered gray. The earlier Overview-only screenshot claim did not prove Camera 1 and is superseded by these five-camera captures.

The only new preserved failure is one intermediate compile error from an extra parenthesis while adding the five-camera test; it was corrected before the successful final build and gates. Generated reports and screenshots remain untracked.

### Final user-operated PIE evidence

The user reported all Camera 0-4 checks passed:

- Camera 0: live Scene Color, 50 cm inward feather, remembered gray, and strict-black Unknown were correct.
- Camera 1: remembered neutral-gray static structure displayed normally; clear and presentation-suppressed regions remained black.
- Camera 2: no dynamic door, moving mesh, current light, or VFX leaked into non-live remembered regions.
- Camera 3: the page boundary was continuous, with no black or bright seam.
- Camera 4: yaw=45 had no visible misalignment or tile/page seam.

The final log state was `memoryReady=1 staticEnvironmentReady=1 memoryPresentationAvailable=1 memoryScopeMatchesBinding=1 memoryScopeMismatchMask=0x00 memoryProfiles=1 liveProfiles=1`, with `submitted-feather bindingFailure=0`.

The initial PIE-frame `bindingFailure=13` is the expected fail-closed state before resources are published; subsequent recovery to healthy `submitted-feather` means it is not a failure. Epic DataRouter/libcurl/OpenSSL warnings are telemetry-network warnings and are unrelated to the SightWeave composite.

## Outcome

M3.5 now provides world-scoped exploration memory for explicitly eligible immutable static environment. The CPU-packed authority is the only HardMemory truth; the R8 GPU memory/static mirrors are derived presentation resources. Final ordering is exact HardLive Scene Color, remembered eligible neutral static environment, then strict-black Unknown. Clear, write-block, presentation-suppress, scope, generation, teardown, fail-black, and Width=0/50 compatibility behavior are covered in native automation.

All implementation, automated performance/memory work, M3.1-M3.5 regressions, M3.5 NullRHI and D3D12/SM6 gates, DARKWELL 24, automated Lab capture, BuildPlugin, clean-host Editor/Game Development/Game Shipping, Shipping isolation, repository closure work, and the user-operated interactive PIE gate are complete. Automated PIE, agent screenshot inspection, and the user's manual evidence remain identified separately.

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

- No M3.5 completion gate remains pending.
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

The user has completed the final interactive M3.5 Lab PIE check and the milestone is `COMPLETED`. No additional M3.5 validation action remains after pulling this documentation closure.

Keep the computer on; do not shut down, sleep, or restart it as part of this handoff.

# Surface Knowledge closure evidence — 2026-09-12

**WIP, not formal acceptance.** See [the Chinese engineering report](../../SIGHTWEAVE_SURFACE_KNOWLEDGE_CLOSURE_20260912_ZH.md) for scope, unresolved contracts and exact interpretation. Raw failures are retained; do not total all historical attempts as a passing suite.

## Final evidence

| Run / file | Interpretation |
|---|---|
| SurfaceClosureFinalBuild.txt | Full DarkwellEditor Win64 Development target succeeded, 15.24s; not Live Coding or a clean rebuild |
| surface_closure_runtime_final | D3D12: 77 passed, including one warning; Surface/P4 and full Apartment reference bit comparison |
| surface_closure_null_verified | Actual NullRHI complement: 1 clean passed |
| surface_closure_visual_verified | Legacy visual contracts: 63 passed, one warning test; inspect RHI reserved virtual allocation warning |
| surface_closure_rules_verified | 64 legacy rules: 62 passed, two failed; occupancy fixture subsequently corrected, fast-sweep contract remains blocked |
| surface_closure_fixture_verified | Final corrected occupancy fixture and Apartment measurement: 2 clean passed |
| surface_closure_manual_isolated | 7 ManualSwitch passed; eighth test was an earlier occupancy fixture failure |
| surface_closure_game_parity_final | Real D3D12 game: 613 updates, 6,512,828,308 cell comparisons, zero Known/Live/Hidden differences; diagnostic timings invalid |
| surface_closure_pair_reference_final / surface_closure_pair_final | Same binary, 240 matched poses, 237 dirty frames; 106.398ms → 17.405ms Surface dirty P95. See summary.json / frames.jsonl |
| surface_closure_native_final | Native-input route: 1,379 measured frames, Surface P95 13.821ms; a different physical route from earlier native reference runs |

## Material failed attempts and baseline evidence

- `surface_closure_8047_legacy_repro` and `SurfaceClosureLegacyBaselineBuild.txt`: three legacy failures reproduced on pristine 8047a5d. `git diff --binary` emitted nothing, so the runner did not create source.patch for this run.
- `surface_closure_game_parity_detail`: conservative margin error in an earlier candidate. `surface_closure_game_parity_margin` established the fix before worker changes; final game parity is the delivery evidence.
- `surface_closure_runtime_candidate`: shared output-normal race in a parallel candidate. Fixed by per-task state; `runtime_isolated` and `runtime_final` passed afterwards.
- `surface_closure_contracts01`, `surface_closure_black_final01`, `surface_closure_legacy_visual_final`: earlier map teardown / RHI readback harness failures; corrected final batch above supersedes them.
- `surface_closure_legacy_motion_final`: monolithic workload exhausted commit memory before later tests ran; no successful complete report. Unexecuted moving/soak tests are not accepted by inference.
- `surface_closure_legacy_rules_final`, `surface_closure_fixture_repair`, `surface_closure_joined_final`: intermediate fixture diagnosis; do not substitute these for final corrected occupancy evidence.
- `surface_closure_profile`: initial profiled Apartment fixture, P95 41.661ms. Profile overhead prevents treating its ratio to final fixture as strict A/B; use final paired runs for the controlled comparison.

Every automation folder contains its selector, logs and report JSON where produced. Game folders contain command/source provenance, raw frames, storage snapshots and viewport capture. `source.json` records the pre-commit HEAD, while `source.patch` captures changes present during that run (including newly added runtime files in final runs). The commit containing this directory is the recoverable WIP. Final paired/native runs share DLL SHA256 `08B263404D023F11E18CAC4CAF9570EF98095ABD0998CEC2EC2997246488280D`.

`archive-index.json` is a convenience inventory of archived automation summaries, not a product acceptance verdict. Raw log/patch whitespace is retained. Unreal assets are unchanged; PNG evidence is stored through Git LFS.

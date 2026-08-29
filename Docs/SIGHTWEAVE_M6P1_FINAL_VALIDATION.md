# SightWeave M6P1 final validation

## Superseding user visual verdict — 2026-08-29

**PARTIAL — USER_PIE VISUAL AND SEMANTIC ACCEPTANCE FAILED. M6P1 is not COMPLETED.**

The automated, build, Cook, formal-View, pixel-readback, screenshot, and repository results below remain preserved as engineering evidence. They do not establish product visual acceptance. The user's subsequent real, dynamic PIE inspection overturned the earlier visual-completion implication: the current DARKWELL presentation is not project-usable.

This failure is not ordinary art tuning. It exposes architectural defects in edge reconstruction, temporal stability during player/camera motion, shared Unknown/Remembered/Live coordinate and sampling alignment, static-scene memory representation and dynamic-information filtering, and 2.5D/3D surface classification at walls and other occluders. Confirmed symptoms include large stair-step edges, crawling/flicker while moving or turning, Width=50 feather merely blurring the steps, a flat-gray Remembered layer instead of recognizable static-scene memory, incorrect black coverage of player-facing wall surfaces, and extra black strips caused by state-layer misalignment.

The highest-priority contract for all later work is `Docs/SIGHTWEAVE_DARKWELL_VISUAL_REQUIREMENTS.md`. No old screenshot, static pixel sample, readback, or green automation result may be cited as proof that the visual result passes. The status may return to `COMPLETED` only after the user operates a real dynamic PIE session and explicitly accepts the visual and semantic result under that contract. The rescue deadline is 2026-09-05, with the first real dynamic visual prototype due within 48 hours; failure of any core requirement at the deadline unconditionally ends SightWeave work as specified by the new contract.

## Status

**PARTIAL — USER_PIE VISUAL AND SEMANTIC ACCEPTANCE FAILED.** All previously recorded agent-owned gates remain historical engineering evidence, but the required user visual gate was performed and failed.

## Validation matrix

| Requirement | Final result |
| --- | --- |
| Explicit product authority | pass: `Legacy` default, `SightWeave` dedicated-map request only |
| Transactional mutual exclusion | pass: Legacy writers/presentation disabled before registration mutation; rollback restores Legacy |
| Duplicate/partial activation | pass: fail-closed, reverse unregister, no dual authority |
| World lifecycle and isolation | pass: multi-world, teardown, restart, and diagnostic reset automation |
| Body circle | pass: radial, player-following, 120 cm, independent of torch |
| Aim cone | pass: 2200 cm, 52°→35°, follows actual character facing |
| Torch authority | pass: existing loadout/on/charge state, 1250 cm, semantic capability, no rendered-light inference |
| Wall occlusion | pass in CPU automation and formal View |
| Static black/live/gray | pass in runtime packet automation and formal View |
| Real Stalker | pass: base product class, stable ID, `NeverRemember`, no Last-Seen/gray residue |
| HUD agreement | pass: same stored adapter snapshot and authority revision as Stalker |
| Integration map | pass: native, non-partitioned, reopenable, bootable, cookable |
| M6P1 tests | NullRHI 4/4; D3D12/SM6 4/4 |
| DARKWELL regressions | original 24/24; total including M6P1 28/28 |
| Full SightWeave | NullRHI 195/195; D3D12/SM6 287/287 |
| Formal View evidence | pass: six `1274×729` D3D12/SM6 images opened and inspected |
| Editor / GameDev / Shipping | pass / pass / pass |
| Isolated integration-map Cook | pass: 530 packages, 0 errors |
| `L_Prototype` | pass: unchanged asset, independent boot remains `authority=Legacy` |
| Shipping leakage | no project path, SightWeave Editor/Tests, or M6P1/Lab strings; no forbidden DARKWELL module dependency |
| Severe logs | zero fatal/assert/ensure/low-level-fatal/access-violation findings in authoritative logs |
| Plugin/project/frozen map changes | none |
| User PIE | **failed:** dynamic visual and semantic acceptance rejected by the user |

## Authoritative evidence

- execution details: `Docs/SIGHTWEAVE_M6P1_EXECUTION_REPORT.md`;
- contract: `Docs/SIGHTWEAVE_M6P1_ADAPTER_INTEGRATION_CONTRACT.md`;
- plan: `Docs/SIGHTWEAVE_M6P1_EXECUTION_PLAN.md`;
- handoff: `Docs/SIGHTWEAVE_M6P1_HANDOFF.md`;
- automation: `Saved/AutomationReports/M6P1_*`;
- logs: `Saved/Logs/M6P1_*`;
- screenshots: `Saved/M6P1_GameViewEvidence/*.png`.

Generated evidence is ignored and untracked.

## Retained warnings and non-authoritative failures

The final full SightWeave reports have two success-with-warning cases: an external connectivity probe timeout and the intentional malformed-ZLIB rollback test. Editor/Game builds retain stock-engine deprecation warnings and the installed compiler/preferred-version warning. Cook retained two environment/toolchain warnings. None is a M6P1 correctness, asset, module, shader, RHI, or Shipping failure.

The initial offscreen D3D12 full-suite result (284/287) is retained as diagnostic history. All three failures were reproduced as offscreen timestamp/viewport constraints, passed in the correct visible-window conditions, and were followed by one authoritative 287/287 full visible-window run. No assertion or threshold was weakened.

## Completion boundary

The earlier automated evidence remains available for engineering regression, but the user has rejected the current dedicated-map result. M6P1 cannot become `COMPLETED` through the old checklist, screenshots, pixel readback, or automation. Completion can be reconsidered only after the rescue contract is satisfied and the user explicitly accepts a new real dynamic PIE result.

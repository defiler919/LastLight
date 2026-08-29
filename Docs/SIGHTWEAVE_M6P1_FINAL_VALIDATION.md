# SightWeave M6P1 final validation

## Status

**PARTIAL — READY_FOR_USER_PIE.** All agent-owned product, automation, formal View, build, Cook, Shipping-isolation, severe-log, and Git/LFS gates are green. User PIE acceptance is deliberately not fabricated.

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
| User PIE | pending |

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

The code and automated evidence are ready for acceptance. M6P1 becomes COMPLETED only after the user performs the dedicated-map PIE checklist in `Docs/SIGHTWEAVE_M6P1_HANDOFF.md` and explicitly reports success.

# SightWeave M4P3 final validation

## Status

**PARTIAL.** Product implementation and all M4P3-specific gates pass. The overall milestone cannot be called completed while one required frozen full-regression performance test fails and the complete requested resource/timing matrix remains unmeasured.

## Validation matrix

| Requirement | Result |
| --- | --- |
| Deterministic V1 raw/Zlib/BLAKE3 format | pass |
| Checked 64 MiB bounds and malformed-input matrix | pass |
| Persistent modifiers and five subject policies | pass |
| Provider registry, exact versioning, localized fail-black fallback | pass |
| Multi-scope validate-before-commit rollback | pass |
| Teardown/revision drift rejection | pass |
| 100 successful + 100 failed restore loops | pass for authority tiles/modifiers/revisions |
| Two-world clear/reacquire/suppress/rebuild | pass |
| Actual D3D12/SM6 full-rebuild GPU readback | pass |
| M3P5 NullRHI / D3D12 | 16/16 and 26/26 |
| M4P1 NullRHI / D3D12 | 9/9 and 12/12 |
| M4P3 NullRHI / D3D12 | 15/15 and 16/16, counting expected-warning success |
| DARKWELL NullRHI | 24/24 |
| Full SightWeave NullRHI | 190/191 non-failing; old M2P2 performance gate failed |
| Full SightWeave D3D12/SM6 | 282/283 non-failing; old M2P2 performance gate failed |
| BuildEditor after final C++ checkpoint | pass |
| BuildPlugin | pass |
| Clean-host Editor/Development/Shipping | pass |
| Runtime/Shipping Editor/Test leakage | zero found |
| Asset/project/descriptor/shader/config changes | none |
| Fatal/assert/ensure/shader/RDG/RHI/GPU/device-removal errors | zero |

## Open gates

1. `SightWeave.M2P2.Performance.Batch512Gate` requires medians at or below 150 us. This host reported worst medians of 155.799 us (NullRHI) and 158.399 us (D3D12); the isolated retry also failed. The test is outside the M4P3 path, but full regression is a required closure gate.
2. The M4P3 test reports maximum-fixture aggregate restore percentiles and one D3D12 per-phase sample. It does not yet report small/typical/maximum per-phase percentiles or direct 100-loop before/after UObject, Render-resource, and GPU-resource counts.

Until both are closed, the correct milestone state is `PARTIAL`.

## Evidence locations

- Contract: `Docs/SIGHTWEAVE_M4P3_PERSISTENCE_RESTORE_CONTRACT.md`
- Execution report: `Docs/SIGHTWEAVE_M4P3_EXECUTION_REPORT.md`
- Handoff: `Docs/SIGHTWEAVE_M4P3_HANDOFF.md`
- Automation reports: `Saved/AutomationReports/M4P3_Closure_*`
- Logs: `Saved/Logs/SIGHTWEAVE_M4P3_CLOSURE_*`
- Screenshots: `Saved/Screenshots/M3P5_PIE_*` and `Saved/Screenshots/M4P1/*`

Generated evidence is intentionally ignored and untracked.

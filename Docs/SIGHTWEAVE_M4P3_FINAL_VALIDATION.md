# SightWeave M4P3 final validation

## Status

**COMPLETED.** Every required correctness, deterministic-format, rollback, performance, timing, resource, regression, packaging, Shipping-isolation, severe-log, and Git/LFS gate passed. There are no open M4P3 gates.

## Validation matrix

| Requirement | Final result |
| --- | --- |
| Deterministic V1 raw/Zlib/BLAKE3 format and 64 MiB bounds | pass |
| Persistent modifiers, five subject policies, exact provider versions | pass |
| Local missing-provider fail-black and validate-before-commit rollback | pass |
| Multi-scope, target lifetime/revision drift, teardown and reacquire | pass |
| Batch512 five-process NullRHI + five-process D3D12 | 10/10 pass; worst p50/p95/p99 138.499/144.001/157.200 us |
| Batch512 exact evidence | 100 distributions and 100,100 raw final samples; 300,300 including A/B |
| Three-fixture, 13-phase timing matrix | 39 distributions x 100 raw samples; pass |
| Six 100-loop resource matrices | pass |
| D3D12 full rebuild/readback 100 | pass; 4,194,320 persistent GPU bytes stable |
| M4P3 NullRHI / D3D12 | 15/15 and 16/16 |
| M3P5 NullRHI / D3D12 | 16/16 and 26/26 |
| M4P1 NullRHI / D3D12 | 9/9 and 12/12 |
| Full SightWeave NullRHI / D3D12 | 191/191 and 283/283 |
| DARKWELL NullRHI | 24/24 |
| Final Editor build | pass |
| BuildPlugin | pass: 106/106, 33/33, 33/33 |
| Fresh clean-host Editor/GameDev/Shipping | pass: 112/112, 45/45, 45/45 |
| Shipping Runtime/Render object count | 20/13 |
| Shipping Editor/Tests/DARKWELL/repository leakage | zero |
| Fatal/assert/ensure/shader/RDG/RHI/GPU/device-removal scan | zero in authoritative logs |
| Asset/project/descriptor/shader changes | none |

## Frozen-gate disposition

The previous Batch512 failure was not accepted as host noise. Exact A/B proved both the frozen baseline and pre-optimization head missed the same gate. The product optimization removes duplicate identical-geometry containment only inside the batch path and retains all authority checks. Ten fresh independent final processes pass all frozen percentile limits with no capacity growth, trimming, or outlier removal.

The previous evidence gap is also closed: small, typical, and maximum fixtures emit p50/p95/p99/max and raw 100-sample arrays for all 13 required phases, plus successful restore, invalid rollback, missing-provider fallback, world lifetime, and actual D3D12 resource stability matrices. Exact values and retained attempt classifications are in the execution report.

## Evidence locations

- Contract: `Docs/SIGHTWEAVE_M4P3_PERSISTENCE_RESTORE_CONTRACT.md`
- Execution report: `Docs/SIGHTWEAVE_M4P3_EXECUTION_REPORT.md`
- Handoff: `Docs/SIGHTWEAVE_M4P3_HANDOFF.md`
- Raw logs: `Saved/Logs/SIGHTWEAVE_M4P3_*.log`
- Automation reports: `Saved/AutomationReports/M4P3_Closure_*`
- Trace: `Saved/Profiling/SIGHTWEAVE_M4P3_BATCH512_OPTIMIZED_NULLRHI.utrace`
- BuildPlugin: `C:\Users\defiler\AppData\Local\Temp\SightWeaveM4P3_Final_BuildPlugin_ad10f3a_20260829_0050`
- Clean host: `C:\Users\defiler\AppData\Local\Temp\SightWeaveM4P3_Final_CleanHost_ad10f3a_20260829_0100`

Generated evidence remains ignored and untracked.

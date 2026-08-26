# SightWeave M2P.5 Vision Tail Classification

Status: **PHASE 1 COMPLETE — PRODUCTION CHANGE AUTHORIZED BY EVIDENCE**

Date: 2026-08-26 (Asia/Shanghai)

## Authority

- Branch: `codex/m2p5-sightweave-vision-solve-tail-closure`
- Instrumentation SHA: `d2691aeffb3332c6caddfe5dd63825a2fb6b1abe`
- Elevated evidence root: `D:\UE_projects\LastLight\Saved\SightWeaveM2P5\VisionTailAttribution\preproduction-detailed-formal-20260826`
- Trace profile: `GeneralProfile.Verbose.File`
- Independent capture processes: 10
- Broad dynamic-door total samples: 1,010
- Source/substage detail rows: 65,650
- ETW event loss: 0
- ETW buffer loss: 0
- PID/TID ownership conflicts: 0
- Unclosed marker timelines: 0
- Unknown classifications: 0
- Affinity/priority changes: none
- Defender/security-service changes: none

The elevated token recorded Administrator role `true`, `High Mandatory Level`, and `fltmc` exit `0`.

## Aggregate intrinsic result

| Metric | p50 | p95 | p99 | max |
|---|---:|---:|---:|---:|
| wall (us) | 154.3 | 276.7 | 349.8 | 547.1 |
| authoritative on-CPU (us) | 154.3 | 272.5 | 305.3 | 388.8 |
| ready (us) | 0.0 | 0.0 | 54.3 | 115.9 |
| blocked (us) | 0.0 | 0.0 | 0.0 | 0.0 |

- Context switches / preemptions: 45 / 45
- Migrations: 34
- Classification: 872 Within budget, 129 Plugin CPU, 9 Scheduler/Preemption, 0 GPU/Driver, 0 Unknown
- Formal broad-door intrinsic p99 contract `< 250 us`: **FAIL**

The result is consistent with the M2P.4 authority: broad-door intrinsic p99 remains around 300 us and the slow work remains dominated by `vision_solve`.

## Marker calibration

Five independent control/detailed process pairs produced 505 samples per mode.

| Workload wall statistic | control (us) | detailed (us) | detailed - control (us) |
|---|---:|---:|---:|
| p50 | 168.2 | 157.7 | -10.5 |
| p95 | recorded in `marker-overhead-summary.json` | recorded | -13.7 |
| p99 | recorded in `marker-overhead-summary.json` | recorded | -2.5 |

No positive diagnostic perturbation was detected at p50/p95/p99. No calibration value is subtracted from ETW authority. Formal capture used lightweight QPC/platform-cycle macro markers and did not enable per-ray micro timers.

## Source-exact structure

The attributed detail join excludes the one illumination geometry solve that the generic geometry probe also observed. The table below contains only the four vision sources.

| Source | Samples | candidates | dirty segments / sectors | total rays | rebuilt | reused | reuse validations | fallback |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| 1 | 1,010 | 65 | 0 / 0 | 518 | 0 | 0 | 0 | `prepared_index_replaced` in 1,010/1,010 |
| 2 | 1,010 | 65 | 1 / 1 | 523 | 49 | 474 | 523 | none |
| 3 | 1,010 | 65 | 1 / 1 | 518 | 38 | 480 | 518 | none |
| 4 | 1,010 | 65 | 1 / 1 | 523 | 52 | 471 | 523 | none |

Source 1 shares its Prepared Event Index entry with compatible illumination geometry. As the door alternates between two exact states, acquisition correctly selects the already-prepared target entry, but `RebuildVisionSnapshotEntry` rejects incremental reuse solely because `PreviousPreparedCache != PreparedCache`. It then performs a full ray solve even though the previous cache can validate the old state and the target cache exactly represents the new state.

Sources 2–4 prove a second redundant path. They rebuild only 38–52 dirty rays, but the current ordered ray loop still inserts and removes active angular intervals for every one of 518–523 rays before it decides whether that ray can be reused. Active-set state is event-based; advancing it only at rebuilt rays preserves the exact active set at every intersection while avoiding updates for the 471–480 rays whose authoritative distance and boundary point are copied unchanged.

## Slow-sample comparison

Comparing total on-CPU samples `<= 250 us` with samples `> 250 us`:

| Contributor (summed per update) | within-budget p50 (us) | slow p50 (us) | slow p95 (us) |
|---|---:|---:|---:|
| `vision_solve` | 113.5 | 196.3 | 228.9 |
| `vision_geometry_solve` | 102.6 | 175.3 | 204.6 |
| illumination solve | 29.7 | 54.3 | 68.3 |
| four vision ray sweeps | 41.3 combined typical | 72.8 combined typical | source-exact data retained |
| vision event sort/local merge | 25.8 | 39.4 | 50.3 |
| vision active initialization | 7.7 | 14.0 | 25.0 |
| vision topology validation | 8.2 | 15.1 | 17.7 |

The tail is broad CPU work amplification rather than one consistently pathological micro-stage. One isolated source-3 reused-ray seam validation spans 202.3 us on-CPU plus 115.9 us ready time after a preemption/migration; its stage p99 is only 0.6 us, so it is retained as a slow sample but is not used to justify an algorithm rewrite.

## Authorized production changes

Phase 1 evidence authorizes only these focused changes:

1. Permit exact dynamic-sector reuse across a Prepared Event Index target-cache replacement by validating the old state with the previous cache and solving into the already-exact target cache. All existing revision, old/new segment, dirty-sector, seam, topology, and synchronous fallback checks remain fail-closed.
2. Advance the angular active set only when a ray must be rebuilt. Reused rays continue to require exact angle, finite previous distance/point, and exclusion from the dirty sector. At each rebuilt ray, interval start/end comparisons advance directly to that ray's angle, producing the same active set as visiting intervening reused rays.

No evidence authorizes changes to gameplay semantics, candidate precision, stable-ID ties, inclusive boundaries, topology rules, snapshot immutability, illumination policy, or Shipping oracle behavior.

## Required next evidence

After production changes:

- focused exact/differential tests, including the shared prepared-binding replacement case;
- broad 4-vision/2-illumination exact comparison against fresh full Optimized solves;
- allocation proof and complete regression/build/package/Shipping audits;
- 36,000-frame NullRHI and D3D12 soaks;
- two independent elevated ten-process final matrices, each retaining Batch512 10,100 and broad door 1,010 samples.

M2P.5 remains **IN PROGRESS**.

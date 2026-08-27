# SightWeave M3.5 performance and memory evidence

Status: **MEASURED / SELECTED TIER GATED**

## Machine and measurement rules

- Date: 2026-08-27
- GPU: NVIDIA GeForce RTX 4060
- Driver: 610.88
- RHI: D3D12 SM6, offscreen
- CPU: Intel Core i5-13500
- Memory: 32 GiB
- Engine: Unreal Engine 5.8.1

These results belong only to this machine. Intrinsic CPU, render-thread, and absolute RHI GPU timestamp data are reported separately. Cold creation is excluded from warmed percentiles. No slow sample was deleted, and no threshold was changed.

## Precision comparison and selection

Common workload: identical 24.8 m x 24.8 m region and exploration path; 64 dirty updates, with cold index zero excluded from the 63 CPU samples; 8 GPU/RT warmups; 56 warmed GPU/RT samples; 16 warmed no-change updates.

| Tier | CPU dirty p50/p95/max (us) | RT total p50/p95/max (us) | GPU dirty p50/p95/max (us) | CPU bytes / tiles | Upload bytes | Result |
| --- | --- | --- | --- | ---: | ---: | --- |
| Ultra 2.5 cm | 748.299 / 1,391.299 / 1,914.799 | 576.500 / 742.000 / 971.302 | 78 / 246 / 719 | 123,008 / 16 | 29,360,128 | rejected: CPU and RT |
| Fine 5 cm | 250.198 / 458.401 / 463.001 | 223.000 / 351.101 / 412.300 | 46 / 249 / 435 | 30,752 / 4 | 12,582,912 | rejected: CPU and RT |
| Standard 10 cm | 65.301 / 82.098 / 88.401 | 66.701 / 71.898 / 80.898 | 29 / 335 / 1,545 | 7,688 / 1 | 4,194,304 | rejected: GPU |
| **Coarse 25 cm** | **48.503 / 73.601 / 141.900** | **48.202 / 97.401 / 221.297** | **30 / 33 / 703** | **7,688 / 1** | **4,194,304** | **selected** |

The maximum column is retained but is not substituted for the frozen p95 gate. Coarse is the highest world precision that passed CPU dirty p95 < 250 us, GT packet p95 < 250 us, RT dirty/setup p95 < 200 us, GPU dirty p95 < 250 us, and memory budgets in the combined run. Standard failed GPU p95 in repeated combined runs, so its one isolated passing run was not used to claim stability.

The selected run's cold values were CPU 46.600 us, RT 728.399 us, and GPU 2,756 us. Its warmed no-change p95 was 0.101 us with zero mirror work. ClearMemory was 26.602 us, BlockMemoryWrites registration 0.801 us, and SuppressMemoryPresentation registration 0.603 us. The selected production config is asserted as `ESightWeaveRenderPrecisionTier::Coarse` by automation.

Evidence:

- `Saved/AutomationReports/M3P5_MemoryPrecision_FinalGate_D3D12`: 4/4, zero warnings.
- `Saved/Logs/M3P5_MemoryPrecision_FinalGate_D3D12.log`: exact per-tier records and gate bits.
- `Saved/AutomationReports/M3P5_SelectedPrecision_D3D12`: selected Coarse 1/1, zero warnings.

## Resident and dirty scale

Each case uses 32 clear/re-explore cycles, discards 8 declared warmups, and retains 24 warmed CPU/RT/GPU samples. The test combines the required resident 1/8/128 and dirty 1/8/32 points.

| Residents / dirty | CPU authority bytes | CPU write p50/p95/max (us) | Clear p50/p95/max (us) | GT publish p95 (us) | RT total p95 (us) | GPU mirror p95 (us) | Upload bytes / max expanded | GPU persistent |
| --- | ---: | --- | --- | ---: | ---: | ---: | --- | ---: |
| 1 / 1 | 7,688 | 63.501 / 67.201 / 67.499 | 48.999 / 50.500 / 50.899 | 1.803 | 103.097 | 51 | 1,572,864 / 1 | 4,194,320 B |
| 8 / 8 | 61,504 | 527.799 / 866.398 / 881.199 | 394.102 / 645.902 / 652.999 | 23.000 | 660.498 | 68 | 12,582,912 / 8 | 4,194,432 B |
| 128 / 32 | 984,064 | 2,157.599 / 3,392.700 / 3,680.602 | 1,860.801 / 3,159.501 / 3,352.098 | 359.498 | 5,151.901 | 518 | 75,497,472 / 48 | 8,390,656 B |

The 1-dirty reference passes the frozen update budgets. The 8- and 32-dirty rows are explicit pressure data, not relabeled reference cases; their super-budget CPU/RT values and the 32-to-48 gutter-neighbor expansion remain visible. Exact requested dirty/removed counts, final residency, and the 64 MiB persistent-memory ceiling passed for all three.

Evidence: `Saved/AutomationReports/M3P5_MemoryScale_D3D12` is 3/3 with zero warnings; exact samples are in `Saved/Logs/M3P5_MemoryScale_D3D12.log`.

## Frozen live presentation baseline

M3.5 leaves the M3.4 HardLive point gate and 50 cm inward feather route intact. The same machine reran the Width=50 matrix for 1080p/1440p, 1/8/128 live tiles, and 2/8/32 sources.

| Resolution | Sources / tiles | Warm total GPU p95 | Budget | Persistent live GPU |
| --- | ---: | ---: | ---: | ---: |
| 1080p | 2 / 1 | 85 us | 1,000 us | 10,306,576 B |
| 1080p | 8 / 8 | 347 us | 1,000 us | 10,306,688 B |
| 1080p | 32 / 128 | 489 us | 2,000 us pressure | 18,697,216 B |
| 1440p | 2 / 1 | 353 us | 1,500 us | 10,306,576 B |
| 1440p | 8 / 8 | 424 us | 1,500 us | 10,306,688 B |
| 1440p | 32 / 128 | 667 us | 3,000 us pressure | 18,697,216 B |

The additional 1080p dirty cases passed: dirty1 total GPU p95 371 us, dirty8 718 us, and continuous dynamic dirty work 36 us. The full Width=50 report is `Saved/AutomationReports/M3P5_FrozenLivePresentation_D3D12` (9/9, zero warnings), with intrinsic stage values in `Saved/Logs/M3P5_FrozenLivePresentation_D3D12.log`.

## Memory accounting

- Frozen maximum M3 live persistent GPU: 18,697,216 B (< 32 MiB).
- Coarse reference Memory mirror persistent GPU: 4,194,320 B.
- 128-resident Memory mirror persistent GPU: 8,390,656 B.
- Coarse packed CPU authority: 7,688 B per allocated tile; 984,064 B at 128 tiles.
- Frozen worst-case SightWeave plugin runtime estimate, including two live pages, two Memory/static pages, 128 packed CPU tiles, and page tables: 36,462,592 B (< 64 MiB).
- Warmed no-change: zero Memory mirror work, uploads, raster, allocations, or capacity growth.

Transient live output was 8,294,400 B at 1080p and 14,745,600 B at 1440p. These per-frame render targets are reported separately from persistent memory.

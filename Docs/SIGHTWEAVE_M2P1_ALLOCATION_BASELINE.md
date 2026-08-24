# SightWeave M2P.1 allocation-call baseline

## Result

This checkpoint replaces the earlier capacity-growth proxy with allocator-call evidence. The baseline was captured on 2026-08-24 from `codex/m2p1-sightweave-final-performance-gate` before solver/runtime optimization.

| Warmed operation | Allocation calls | Reallocation calls | Free calls | Allocated bytes | Peak temporary bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Optimized solver, 2×64 | 12 | 10 | 12 | 74,192 | 34,816 |
| Optimized solver, 8×64 | 48 | 40 | 48 | 296,768 | 34,816 |
| Optimized solver, 8×512 (4,096 total) | 48 | 40 | 48 | 1,845,056 | 228,352 |
| Optimized solver, 8×4,096 (4,096/source) | 48 | 40 | 48 | 14,231,360 | 1,776,640 |
| Authority point query | 1 | 0 | 1 | 32 | 32 |
| Authority batch query, 512 | 0 | 0 | 0 | 0 | 0 |
| Vision-source transform update | 169 | 17–18 | 166 | 189,720–190,344 | 155,884–156,508 |
| Dynamic door update | 416 | 67 | 404 | 376,468–376,804 | 243,664–244,000 |
| Clean publication | 0 | 0 | 0 | 0 | 0 |
| No-change source update | 0 | 0 | 0 | 0 | 0 |

Every row represents three independently marked, warm steady-state samples. Exact values were identical across the three samples except for the ranges shown. All 30 scopes were emitted on trace thread 2. The raw ignored evidence is `Saved/SightWeaveM2P1/BaselineAllocation/baseline.csv`; both capture and analysis Automation reports recorded one success, zero warnings, and zero errors.

## Measurement method

- The capture process starts with `-trace=memory,sightweaveallocation`. UE 5.8 installs its own `FMallocWrapper` during allocator startup, before ordinary engine allocations. The test never replaces `GMalloc` at runtime.
- The Editor-only `SightWeaveTests` module emits a compact `SightWeave.AllocationScope` begin/end event around only the target operation. Fixture construction, world/Actor registration, warmup, Automation assertions, and logging remain outside the scope.
- A second Editor process reads the completed `.utrace` through `TraceAnalysis`. Its analyzer subscribes directly to the raw `Memory.Alloc`, `AllocSystem`, `ReallocAlloc`, `ReallocAllocSystem`, `Free`, `FreeSystem`, `ReallocFree`, and `ReallocFreeSystem` events.
- Scope state is keyed by the trace-specific thread ID. Events from background threads cannot enter a current-thread sample.
- Allocation calls count `Alloc*`; reallocation calls count `ReallocAlloc*`; free calls count ordinary `Free*`. The paired `ReallocFree*` event updates temporary-live accounting without being double-counted as a user free call.
- Allocated bytes are the exact decoded requested sizes from allocation and reallocation events. Peak temporary bytes are the maximum bytes simultaneously live among addresses allocated/reallocated inside the marked scope. `end_temporary_bytes` is retained in the CSV to show output allocations that remain live past scope end.

This is allocator-call proof, not a sampling profiler: every traced allocation event on the measured thread is counted. Its deliberate limitation is scope attribution rather than callstack attribution; allocations intentionally dispatched to a worker must be measured under a separate worker scope instead of being attributed to the dispatching thread.

## Reproduction

```powershell
& .\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot 'D:\UE_5.8'
& .\Scripts\RunSightWeaveAllocationProof.ps1 -Label 'baseline' -EngineRoot 'D:\UE_5.8'
```

The script refuses to overwrite an existing label. Its output root defaults to `Saved/SightWeaveM2P1/AllocationProof/<label>` and includes the trace, CSV, logs, and both Automation JSON/HTML reports.

## Baseline interpretation

- The optimized solver has six ordinary allocations and five reallocations per solve in every measured workload. The counts, rather than only allocated capacity, establish the reusable-scratch target.
- The existing 512 batch, clean-publication, and normalized no-change paths already meet strict zero-allocation/reallocation requirements.
- The warmed point query still allocates a 32-byte returned-result buffer.
- Dynamic door and source updates are dominated by complete rebuild/publication materialization, not merely by the spatial-index mutation.

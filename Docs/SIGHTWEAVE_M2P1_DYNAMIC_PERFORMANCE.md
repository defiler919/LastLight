# SightWeave M2P.1 dynamic authority performance

## Scope and retained design

The production policy remains strict synchronous authority. `UpdateOccluder` prepares the current door geometry, updates the spatial index, discovers affected sources, solves every affected vision and illumination polygon, and publishes the resulting revision before returning. No worker task, stale snapshot window, lower update rate, skipped frame, or conservative pending policy was introduced.

The retained implementation adds:

- source-local raw segment caches patched by stable ID after a dynamic occluder update;
- source-local prepared-segment caches that recompute only changed segment metadata while still sorting and casting the complete exact event set;
- cached angular padding, whose inputs are covered by the prepared-cache invariant key;
- a double-buffered immutable published snapshot; a standby buffer is reused only when its thread-safe shared-reference count proves no reader still owns it;
- retained publication ID scratch arrays and capacity-preserving snapshot materialization;
- stable-ID/same-floor/same-count spatial-index updates in place, with retained cell and attribution arrays and the original remove/insert implementation as the general fallback;
- an exact unnormalized-ray authority query whose comparisons are algebraically equivalent to the former normalized-ray distance calculation and avoid square root/division;
- a one-segment dynamic-door normalization fast path that preserves the one-edge subset of the established normalizer and falls back for general multi-edge updates;
- an exact binary wrap-start calculation for the three radial endpoint/±epsilon sequences; unusual epsilon values outside the single-wrap range retain the original scan.

Source mutation, source removal, occluder registration/removal, and world reset invalidate the applicable caches. Caches contain plain geometry only and own no `UObject` or world references. Non-Shipping Reference and Verify modes retain their established behavior; Shipping still forces Optimized.

## Synchronous performance evidence

The final fixture uses 10 warmups and 101 measured samples. The timer surrounds only `UpdateOccluder`; stage samples are copied after the timed call. Values are median/p95/p99/max in microseconds.

| retained run | dynamic door solve+publish | publication | vision rebuild | illumination rebuild | materialization |
|---|---:|---:|---:|---:|---:|
| `DynamicFinal1` | 222.400 / 226.900 / 230.700 / 231.899 | 220.001 / 224.300 / 228.200 / 229.198 | 145.201 / 148.799 / 153.299 / 154.801 | 68.497 / 70.799 / 72.699 / 73.299 | 5.800 / 6.098 / 6.400 / 6.698 |
| `DynamicFinal2` | 221.301 / 230.897 / 233.699 / 252.098 | 218.801 / 228.401 / 231.098 / 249.602 | 144.199 / 150.200 / 154.402 / 155.602 | 68.299 / 73.798 / 79.300 / 93.598 | 5.901 / 6.203 / 6.501 / 8.602 |
| `DynamicFinal3` | 223.100 / 229.903 / 234.298 / 236.101 | 220.500 / 227.202 / 230.897 / 233.501 | 145.398 / 148.799 / 151.999 / 153.299 | 68.702 / 73.500 / 76.402 / 79.300 | 5.800 / 6.001 / 6.799 / 7.201 |

All three final dynamic distributions pass the strict synchronous median and p99 below 250 us gates. Because the call returns only after publication, public dispatch, worker, and publication latency are respectively the same synchronous total, not applicable, and zero additional delay. There are no stale tasks to drop.

The retained result progresses from the M2P 653/694 us median/p99 baseline through 329/340 us after the checkpoint-4 geometry work, about 250/265 us after raw-segment and snapshot reuse, and finally about 221–223/231–234 us after prepared metadata, angular-padding, normalization, and spatial-index reuse.

## Query and non-regression distributions

The exact unnormalized-ray query reduced the normal 512-anchor batch. Final values are:

| retained run | batch 512 median / p95 / p99 / max (us) | source transform median / p99 (us) | no-change median / p99 (us) |
|---|---:|---:|---:|
| `DynamicFinal1` | 233.799 / 239.700 / 243.198 / 243.600 | 52.802 / 58.800 | 0.201 / 0.302 |
| `DynamicFinal2` | 238.001 / 250.001 / 288.300 / 291.802 | 52.102 / 56.002 | 0.298 / 0.302 |
| `DynamicFinal3` | 235.099 / 239.801 / 245.102 / 245.702 | 52.601 / 57.597 | 0.298 / 0.302 |

Two final batch runs pass all three percentile gates. `DynamicFinal2` contains a sustained host scheduling/frequency band and is retained as a failed distribution rather than hidden. The immediately preceding three independent retained runs (`Dynamic101AngularPadding1..3`) reported batch median 235.800/235.900/235.800 us and p99 243.802/242.900/245.102 us, all passing. This leaves one of the latest six distributions above the formal batch tail gate despite stable passing medians and five passing p99 distributions; final status must not describe the batch evidence as uniformly passing.

No-change calls preserve revision. Source-transform latency remains comfortably below 250 us, but source transform is not zero-allocation and is reported separately below.

## Allocator-call evidence

The retained proof is:

```powershell
& .\Scripts\RunSightWeaveAllocationProof.ps1 -Label 'after-dynamic-cache-final' -EngineRoot 'D:\UE_5.8'
```

The proof uses startup `-trace=memory,sightweaveallocation`, current-thread begin/end scope events, and a second-process raw trace analyzer. Dynamic door input storage is built before the marked region and the production path is warmed twice, matching the stated warm steady-state gate. All three samples for 2x64, 8x64, 4096-total, 4096/source, point query, batch 512, dynamic door update, clean publication, and no-change update record exactly 0 allocation calls, 0 reallocations, 0 frees, 0 allocated bytes, and 0 peak/end temporary bytes.

Dynamic door progression was:

- baseline: 416 allocations, 67 reallocations, 376,468–376,804 allocated bytes;
- after solver/output scratch: 389 allocations, 45 reallocations, 218,812–219,148 bytes;
- before dynamic normalization/spatial reuse: 9 allocations, 0 reallocations, 640 bytes warmed;
- after retained dynamic cache/spatial work: exactly 0/0/0 in every warmed sample.

Source transform improves to 75 allocations, 11–13 reallocations, and 36,516–48,156 bytes, but remains nonzero. The task's allocation hard gates cover warmed solve, point/batch, no-change, and dynamic public dispatch; source transform retains its separate sub-250-us latency gate.

The analyzer additionally records allocation size and callstack ID details. This is analysis-only test instrumentation and is not compiled into Shipping Runtime behavior.

## Correctness evidence and limitations

- `SightWeave.M2P.Differential.Geometry`: both tests passed, covering all nine manual adversarial cases and all 96 fixed-seed randomized cases.
- `SightWeave.M2P.Differential.Runtime.AuthorityAndUpdates`: passed after prepared caching, one-edge normalization, nested-array reuse, and final spatial in-place updates.
- Full Editor builds passed after every retained C++ stage. The only build warning remains local MSVC 14.51 versus Epic-preferred 14.50.

The strict synchronous dynamic gate is closed. Async lifecycle, stale-task rejection, pending policy, worker time, and maximum frame publication delay are not applicable because no async policy was introduced. Expanded high-count differential and snapshot-reader lifetime coverage remain checkpoint-6 work.

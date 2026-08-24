# SightWeave M2P.1 extreme-authority performance evidence

## Scope and measurement

This checkpoint profiles and optimizes the exact production Optimized solver after the reusable-scratch checkpoint. The benchmark uses warmed caller-owned result arrays and `SolvePolygonInto`; it records every individual solve rather than estimating one solve by dividing an eight-source aggregate. `total_us` is the sum of the eight measured solver CPU durations, while `sequential_wall_us` is the enclosing sequential wall duration. Raw ignored evidence is under `Saved/SightWeaveM2P1`.

The severe `dense_8x4096_each` fixture has 4,096 relevant segments for each of eight sources. Each solve retains the Reference event contract and emits approximately 24,704 exact radial-boundary and endpoint/±epsilon rays. It is intentionally distinct from the documented 4,096-total fixture.

## Baseline and current result

| Metric | Post-scratch baseline | Current exact candidate | Requirement |
|---|---:|---:|---:|
| 4,096/source single solve median | about 5,619 us | 1,706.500 us | below 1,000 us |
| 4,096/source single solve p95 | not separately retained | 1,729.101 us | reported |
| 4,096/source single solve p99/max | about 5,619 us | 1,747.902 us | below 2,000 us |
| Eight-source cumulative CPU median | 44,954.803 us | 13,656.195 us | reported |
| Eight-source sequential wall median | approximately cumulative CPU | 13,657.503 us | reported |
| 4,096-total single solve median/p99 | not the failing interpretation | 213.500/222.202 us | below 1,000/2,000 us |
| Typical 8x64 radial all-source median/p99 | about 762 us before this checkpoint | 318.699/327.297 us | p99 no regression above 766 us |

The p99 extreme gate now passes and the median is 69.6% lower than the post-scratch baseline, but the strict 1 ms median gate remains open. This checkpoint therefore does not claim final completion.

## Current 4,096/source stage distribution

The final retained run is `ProfileTopologyOffset2`; values below are eight-source aggregate median/p95/p99/max in microseconds. Divide-by-eight is not used for the reported single-solve distribution.

| Stage | median | p95 | p99 | max |
|---|---:|---:|---:|---:|
| boundary events | 2.101 | 2.205 | 2.205 | 2.205 |
| candidate/range/height/endpoint preparation | 2,222.903 | 2,239.302 | 2,239.302 | 2,239.302 |
| exact event sort/merge and direction preparation | 3,662.098 | 3,685.895 | 3,685.895 | 3,685.895 |
| angular acceleration build | 1,127.899 | 1,152.400 | 1,152.400 | 1,152.400 |
| active sweep, nearest intersection, and output | 5,496.405 | 5,545.188 | 5,545.188 | 5,545.188 |
| polygon postprocess | 0.197 | 0.402 | 0.402 | 0.402 |
| local Reference-parity topology guard | 1,142.796 | 1,151.003 | 1,151.003 | 1,151.003 |
| total CPU | 13,656.195 | 13,761.804 | 13,761.804 | 13,761.804 |

The last sample contains 32,768 candidates, 197,632 rays, 156,641 retained vertices, and 197,765 tested segments across eight sources. The distance-ordered active sweep therefore performs almost exactly one exact segment test per ray.

## Retained iterations

1. Reused output storage in the performance fixture and reported the actual per-solve distribution, aggregate CPU, and enclosing sequential wall independently.
2. Replaced comparison sorting of exact double candidate angles with an order-preserving 64-bit radix key and retained a reusable sort buffer.
3. Cached source-relative segment vectors, cross-product numerators, exact endpoint angles, lower-bound distances, fraction epsilon, origin contact, and stable IDs.
4. Reused cached endpoint geometry while building angular intervals and added exact-start radix sorting with deterministic equal-start tie ordering.
5. Added an AABB reject before inclusive topology intersection and removed modulo operations from the local topology loop.
6. Stored the active set in distance/stable-ID order, pruned it with the exact origin-distance lower bound, and reduced the 4,096/source intersection count to almost one segment per ray.
7. Added a full-circle radial event path: sort only the 2N exact base endpoint angles, construct the -epsilon/zero/+epsilon cyclic sequences, and perform a deterministic four-way merge with radial boundary events.
8. Pre-sized result arrays and wrote distances, boundary points, and canonical vertices sequentially.
9. Replaced swap-based active insertion with binary insertion and skipped expiry scans until the earliest active interval actually expires.
10. Avoided the segment-fraction division when numerator and denominator prove the fraction lies on the closed segment; boundary-adjacent cases retain the exact established quotient.
11. Evaluated endpoint direction reuse in isolation. The retained version computes the exact base direction for every sorted base endpoint, uses that exact value for the zero-offset event, and rotates only the ±epsilon variants. It passed the full manual/fixed-seed differential set.
12. Restricted the constant-neighborhood topology parity guard to edge `i` versus `i+2`. A polar-ordered visibility boundary cannot cross a remote angular wedge; all previously reproduced epsilon parity mismatches were immediately separated edge pairs. All nine manual adversarial and 96 fixed-seed cases passed after the change.

## Rejected iterations

- Multiplying by a precomputed reciprocal in exact ray intersection changed nearest-hit/boundary/canonical results and was removed.
- Normalizing original endpoint vectors instead of computing the established exact trigonometric base direction caused thousands of fixed-seed differential errors and was removed. The failure remains in `Saved/SightWeaveM2P1/EndpointAoSDifferential` as negative evidence.
- Scalar `FMath::SinCos` is implemented as separate double sine/cosine calls in UE 5.8 and measured slower in this workload; the retained path does not use it as a claimed optimization.
- Storing a wider exact event record increased sort traffic enough to offset its benefit and was removed.

No failed iteration was accepted by changing Reference, expanding tolerances, deleting a seed, changing endpoint/±epsilon events, or reporting only a favorable sample.

## Correctness and remaining limit

The retained candidate passed `SightWeave.M2P.Differential.Geometry`, comprising all nine manual adversarial cases and all 96 fixed-seed randomized cases, with zero Automation test warnings/errors. Earlier retained sub-iterations were also checked after each numerical or ordering change.

At 4,096/source the remaining sequential median is dominated by the mandatory exact event contract: two exact endpoint angles per segment, exact ordering of 24,704 candidate rays, exact base directions, exact nearest intersections, and materialization of every parallel candidate result used by runtime authority queries. A further 41.4% reduction is still required. High-count Reference differential coverage and any larger prepared-scene/cache architecture remain work for the coverage/final phase; this checkpoint deliberately preserves the current public result contract.

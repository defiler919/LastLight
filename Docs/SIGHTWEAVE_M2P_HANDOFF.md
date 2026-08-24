# SightWeave M2P — CPU Authority Performance Hardening handoff

## Status

- State: **IN_PROGRESS**
- Baseline branch: `codex/m2-sightweave-2p5d-authority`
- Baseline SHA: `517a486779b53a890377f7cd0bc12b6f1cc62640`
- Working branch: `codex/m2p-sightweave-authority-performance`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current phase: checkpoint 5 — optimized/reference differential coverage complete; final validation next
- Last safe commit: `4a1924d6575a3092de55a493a3bd5630f77acc80`
- Next recovery command: `git switch codex/m2p-sightweave-authority-performance; git pull --ff-only origin codex/m2p-sightweave-authority-performance`

## Objective

Harden the existing M2 CPU-authoritative 2.5D visibility implementation before M3. Establish repeatable stage-level performance evidence, retain the endpoint-ray Reference Solver as the correctness oracle, add a production-optimized deterministic solver and differential coverage, reduce runtime update/snapshot/query allocations and copying, and validate every existing correctness, build, isolation, packaging, Git, and LFS gate.

GPU masks, fog/post process, memory textures or tiles, DARKWELL gameplay integration, `/Game/Maps/L_Prototype`, persistence, and M3 work are explicitly out of scope.

## Known performance problem and current measurement

The M2 test times only `SightWeave::Geometry::SolveReferencePolygon()` and records approximately:

- 2 sources / 64 relevant segments: 6.2 ms total solve time.
- 8 sources / 64 relevant segments: 24.2 ms total solve time.
- The existing 1,000,000 / 4,000,000 microsecond assertions are hang ceilings only and are not acceptance evidence.

The current reference path builds endpoint and fixed 128-step boundary events, sorts/deduplicates them, tests every emitted ray against every candidate segment, constructs the polygon, then executes `IsSimplePolygon`, whose pairwise edge validation is quadratic in emitted vertex count. The subsystem also publishes a fully copied snapshot after every registry mutation and query helpers perform repeated linear searches and temporary-array growth. These are hypotheses until measured by the new stage-level benchmark.

## Performance budgets and interpretation

The design targets are unchanged:

- game/main-thread registration or dispatch: `< 0.25 ms`;
- visibility solves: median `< 1.0 ms`, p99 `< 2.0 ms`;
- up to 8 active sources and 4,096 relevant segments;
- 512-subject batch query: median `< 0.25 ms`;
- steady-state visibility solve/query hot paths: `0` heap allocations;
- deterministic output and no correctness regression.

`VISION_SYSTEM_REQUIREMENTS.md` says 4,096 relevant segments **total**, and `VISION_SYSTEM_ARCHITECTURE.md` says 4,096 relevant segments **across dirty sources**. The M2P task additionally requires the more severe 4,096-per-source interpretation. Both will be measured and reported separately; neither result will be relabeled as the other.

The documents also state that these budgets are provisional until minimum hardware and reference workload are user-approved. This task nevertheless treats the numeric CPU thresholds as hard M2P gates on the current machine, as explicitly requested.

## Planned checkpoints

1. Add reproducible stage-level Development Editor performance benchmarks and baseline data.
2. Profile the Reference Solver and document hotspot shares.
3. Add a production-default optimized solver with Reference and non-Shipping Verify modes.
4. Harden dirty updates, snapshot publication, compatibility lookup, query batching, and scratch allocation.
5. Add manual-edge and large fixed-seed randomized Optimized-vs-Reference differential coverage.
6. Run complete Editor/test/Lab/BuildPlugin/clean-host/dependency/Git/LFS validation and record final performance evidence.

## Checkpoint 2 baseline result

`Docs/SIGHTWEAVE_M2P_PERFORMANCE.md` records the full machine/configuration, methodology, median/p95/p99/max, failure history, and stage profile. The corrected extended run passed 2/2. Key baseline facts:

- 8×64 radial warm p99: 22.872 ms; raycast 58.2%, topology validation 41.0%.
- 8×512 = 4,096 relevant segments total warm p99: 920.468 ms.
- 8×4,096 relevant segments per source warm p99: 46,987.216 ms.
- 512 batch median/p99: 11.631/11.974 ms.
- source transform update + synchronous solve/publish median/p99: 2.925/2.960 ms.
- spatial candidate query and clean snapshot copy are not current primary hotspots.
- Existing `SightWeave.M2.Geometry` after instrumentation: 21/21 passed, zero warning/error/not-run, duration 0.1688 seconds.

## Checkpoint 3 optimized solver result

The production-default solver keeps the exact Reference boundary and endpoint/±epsilon event set, but builds conservative angular coverage intervals for every relevant segment. A deterministic sweep over the already sorted rays maintains only intervals that can intersect the current ray; those candidates still use the same analytic ray/segment math, inclusive tolerance, and stable-ID tie-break. This replaced the measured `rays × all segments` scan without reducing sampling. Production construction omits quadratic `IsSimplePolygon`; Reference retains it. `Optimized`, `Reference`, and non-Shipping `Verify` modes are explicit, and Shipping always selects Optimized.

Final candidate measurements on the same machine:

- 8×64 radial warm median/p99: 0.485/0.495 ms for all 8 solves, versus 22.773/22.872 ms Reference (46.9×/46.2× faster).
- 8×512 = 4,096 total median/p99: 2.828/2.870 ms for all 8 solves, or approximately 0.354/0.359 ms per solve, versus 917.852/920.468 ms Reference.
- 8×4,096 per-source stress median/p99: 31.628/31.660 ms for all 8 solves, or approximately 3.953/3.957 ms per solve. This deliberately severe interpretation remains over the worker budget and will be reported separately from the documented 4,096-total scale.
- Candidate/ray/vertex counts match the Reference baseline in every benchmark row.
- Full Editor builds passed after each solver iteration. `SightWeave.M2` with the optimized runtime path passed 59/59, zero failed/warning/not-run; the known 13 startup self-test error lines predate and are outside Automation test results.

These measurements predate the linear local-topology parity guard found by checkpoint 5. The post-differential final measurements are recorded below and in `Docs/SIGHTWEAVE_M2P_PERFORMANCE.md`.

## Checkpoint 4 runtime result

Immutable snapshot entries now carry exact polar boundary points, a deterministic angle lookup table, and direct compatible-illumination indices. Authority containment is O(1)-seeded/O(log n)-fallback and returns to the original polygon test in the conservative boundary band. Batch queries reuse result-array capacity, share one immutable snapshot/floor context, and cache repeated illumination containment. Clean `PublishSnapshot` calls are no-ops. Identical normalized source, illumination, and occluder updates preserve revision and skip solve/publication.

Final runtime candidate on the 4-vision/2-light/64-segment fixture:

- authority point query median/p95/p99/max: 0.697/0.902/3.502/3.900 µs;
- 512 batch: 244.800/252.001/268.500/268.500 µs; warmed result capacity growth was 0 bytes;
- source transform update + solve + publish: 83.100/89.601/90.502/90.502 µs;
- dynamic door local update + affected solve + publish: 503.898/510.599/511.102/511.102 µs;
- identical source update: 0.298/0.302/0.302/0.302 µs and revision stayed 54;
- clean snapshot publish median 0.000 µs; public by-value snapshot copy median 11.001 µs.

The 512-batch median is under 0.25 ms, while p95/p99 remain slightly above; both are retained. Zero capacity growth is not an allocator-hook measurement, so the strict zero-heap-allocation gate remains unproven. The hardened `SightWeave.M2` run passed 59/59 with zero Automation warnings/failures/not-run.

## Checkpoint 5 differential result

`SightWeave.M2P.Differential` now contains three independent tests:

- 9 hand-authored adversarial solve cases covering radial/cone near awareness, long walls, L/T corners, collinear/shared/duplicate endpoints, narrow opening, source on/near edge, and floor/height filtering;
- 96 fixed-seed randomized cases with non-crossing polar-sector authored geometry and varied source transform, radial/directional/camera shape, range, cone, near awareness, tessellation, segment radius, and segment length;
- a Reference-vs-Optimized runtime trace comparing point classification, pure vision, legal-illumination gating, hard-live, bypass, suppression, source-specific queries, floor/height/owner/capability isolation, boundary epsilon, attribution/rejection flags, samples, batch queries, dynamic-door open/close, no-change updates, revision sequences, determinism, and current-snapshot use.

Comparison checks success/candidate/ray state, exact candidate-angle ordering, nearest-hit distances and critical boundary points, canonical vertices, bounds, area, dense deterministic classification, boundary-offset classification, repeated optimized output, and every public visibility-result field.

The first valid randomized pass exposed four CameraCone near-awareness cases where the Reference Oracle's inclusive topology epsilon rejected two near-collinear edges separated by one boundary edge. Optimized emitted the same boundary but initially skipped that failure classification. The retained minimum reproductions were fixed with a constant-neighborhood, linear `HasLocalVisibilityTopologyDegeneracy` check. It matches the Reference local event-cluster policy while keeping the quadratic general `IsSimplePolygon` exclusively in Reference/diagnostic paths.

Final differential report: `Saved/AutomationReports/SightWeaveM2P_Differential_Pass_20260824/index.json`; **3/3 passed**, zero error/warning/not-run, duration 0.1638 seconds. Full `SightWeave.M2` afterward passed **62/62**, zero failed/warning/not-run, duration 1.1759 seconds.

Post-parity final solver evidence remains inside the documented scale:

- 8×64 radial all-source median/p99: 0.750/0.766 ms;
- 8×512 = 4,096 total median/p99: 4.490/4.606 ms for all eight solves, approximately 0.561/0.576 ms per solve;
- 8×4,096-per-source stress median/p99: 44.405/44.786 ms for all eight solves, approximately 5.551/5.598 ms per solve, still failing the deliberately severe interpretation.

The latest runtime distribution reported a 512-query batch at 206.102/210.300/211.101/211.101 µs median/p95/p99/max, now below 0.25 ms throughout that run; source update p99 was 122.700 µs and dynamic-door p99 was 694.402 µs. Strict allocator-call zero remains unproven despite zero warmed capacity growth.

## Commands executed

```powershell
git -c safe.directory=D:/UE_pro/Darkwell status --short --branch
git -c safe.directory=D:/UE_pro/Darkwell rev-parse HEAD
git -c safe.directory=D:/UE_pro/Darkwell fetch origin --prune
git -c safe.directory=D:/UE_pro/Darkwell pull --ff-only origin codex/m2-sightweave-2p5d-authority
git lfs pull
git lfs status
git ls-remote --heads origin codex/m2p-sightweave-authority-performance
git -c safe.directory=D:/UE_pro/Darkwell switch -c codex/m2p-sightweave-authority-performance
```

The initial `fetch origin --prune` reported a transient peer receive failure. The immediately following explicit `pull --ff-only` fetched the M2 branch and confirmed it was already current at the exact baseline. The target M2P branch did not exist locally or remotely.

## Context read before source changes

Read in full: root `AGENTS.md`; requirements, architecture, migration plan, M1/M2 handoffs; plugin README and descriptor; all Runtime public/private source; all M1/M2 test source; the Lab generator; and the repository Editor build script.

## Unverified items

- Actual allocator-call instrumentation and reusable solver scratch are not complete. Batch capacity is stable after warm-up, but that is narrower evidence than zero heap calls.
- M1/all SightWeave/DARKWELL, Lab smoke, BuildPlugin/clean hosts, dependency scans, Git/LFS, and final warnings/fatals audit remain.
- Final state must remain `PARTIAL` unless every hard correctness, isolation, build, and performance gate passes. The 4,096-per-source stress interpretation currently exceeds the worker solve budget even though the documented 4,096-total scale passes per solve.

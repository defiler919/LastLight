# SightWeave M2P — CPU Authority Performance Hardening handoff

## Status

- State: **IN_PROGRESS**
- Baseline branch: `codex/m2-sightweave-2p5d-authority`
- Baseline SHA: `517a486779b53a890377f7cd0bc12b6f1cc62640`
- Working branch: `codex/m2p-sightweave-authority-performance`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- Current phase: checkpoint 1 — scope recovery and starting handoff
- Last safe commit: `517a486779b53a890377f7cd0bc12b6f1cc62640`
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

- No M2P benchmark, profile, optimized implementation, differential suite, build, automation, Lab smoke, or packaging validation has run yet.
- Current workstation hardware/compiler details have not yet been captured.
- The production algorithm and exact scratch/allocation strategy remain pending profile evidence.
- Final state must remain `PARTIAL` unless every hard correctness, isolation, build, and performance gate passes.

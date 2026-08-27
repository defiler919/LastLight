# SightWeave M3.5 exploration-memory contract

Status: **IN_PROGRESS — contract freeze pending precision experiment design**

Branch: `codex/m3p5-sightweave-static-environment-memory`

Frozen baseline: `22f55b1e744cea37ad5d3c7beb618be0509fbf99`

## Contract-freeze rule

Production memory and neutral-gray presentation code must not be written until this document freezes the CPU HardMemory authority, static-environment eligibility, immutable packet identity, modifier ordering, GPU-mirror failure behavior, precision experiment, cost model, and objective selection rule.

## Fixed milestone invariants

- `HardLive` is the existing binary CPU EffectiveLive authority. Its GPU image is derived and point-sampled. M3.4 inward feather can only reduce live presentation brightness.
- `HardMemory` is binary CPU authority recording prior effective exploration. GPU memory pages are derived mirrors only.
- `StaticEnvironmentEligibility` is a separate explicit content classification. It never follows from actor name, color, velocity, current frame appearance, or memory bits.
- Presentation priority is `HardLive`, then eligible and unsuppressed `HardMemory`, then strict-black `Unknown`.
- Camera position/rotation/projection, viewport/resolution, current lighting/shadow/post-process, Scene Color, and visual feather do not write, clear, or revise HardMemory.
- Unsupported or uncertain remembered content fails black.

## Pending freeze sections

The next documentation checkpoint will define:

1. full world/scope/generation/revision identity without hash-only equality;
2. packed tile mapping for negative and large coordinates;
3. dirty/revision/no-change semantics and immutable Memory Packet contents;
4. ClearMemory, BlockMemoryWrites, SuppressMemoryPresentation, and SuppressLiveVision ordering;
5. sparse GPU mirror residency, gutters, invalidation, and fail-closed behavior;
6. supported static-environment material route and rejected presentation routes;
7. identical 2.5/5/10/25 cm experiment scale, metrics, estimated bytes, thresholds, and selection rule.

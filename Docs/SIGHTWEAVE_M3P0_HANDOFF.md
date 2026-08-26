# SightWeave M3.0 architecture-freeze handoff

Status: **COMPLETED — DESIGN/CONTRACT ONLY**

Branch: `codex/m3p0-sightweave-gpu-mask-contract`

Starting authority baseline: `d98440f656a13a8ab396ba2bd93637c6d8e2b15c` (`codex/m2p5-sightweave-vision-solve-tail-closure`)

## Outcome

M3.0 freezes a buildable, testable GPU live-mask route without implementing it. SightWeave keeps M2P.5 CPU geometry and query authority, sends immutable self-contained revision packets across Game Thread/Render Thread ownership, and uses CPU-triangulated polygon geometry for GPU dirty-tile rasterization. The selected persistent representation is a stable floor-local sparse R8 atlas partitioned by World instance, Knowledge Owner, and floor. Complete compatibility profiles use bounded transient tile scratch; bypass is independent and suppression is applied last.

GPU/shader/RHI/capacity/revision failures are black and diagnosed. Nothing falls back to white, rendered-light inference, screen-space authority, a different owner/floor, or a stale world.

No plugin source, shader, Build.cs, descriptor, material, `.uasset`, `.umap`, DARKWELL gameplay, GPU mask implementation, post-process asset, memory layer, or subject proxy changed in M3.0.

## Documents delivered

- `SIGHTWEAVE_M3_GPU_MASK_CONTRACT.md`: authority boundary, immutable packet schema, profile equality, coordinates, inclusive boundary tolerance, revisions, dirtying, teardown, failure, and test-only readback.
- `SIGHTWEAVE_M3_GPU_MASK_ARCHITECTURE.md`: alternatives, selected module/SVE/RDG route, shader/package layout, resources, four precision tiers, exact byte arithmetic, dirty flow, Shipping/Fab/NullRHI behavior, and M3.1 order.
- `SIGHTWEAVE_M3_GPU_MASK_VALIDATION_PLAN.md`: CPU/GPU oracle, edge/tile/profile/owner/floor/lifecycle matrices, Lab regions, GPU/CPU metrics, regression/build/package/clean-host requirements, and verdict rules.
- `VISION_SYSTEM_ARCHITECTURE.md`, `VISION_SYSTEM_MIGRATION_PLAN.md`, and `DECISIONS.md`: old atlas-vs-array ambiguity replaced with the M3.0 selection and stable pointers to the detailed contract.

No new requirement ID was introduced or redefined.

## Frozen decisions

1. CPU visibility/illumination polygons, compatibility, bypass, suppression, queries, and memory-write eligibility remain authoritative.
2. `SightWeaveRuntime` remains renderer-independent; a new `SightWeaveRender` Runtime module will privately depend on UE rendering modules and load at `PostConfigInit`.
3. Runtime publishes renderer-neutral immutable self-contained packets; Render Thread holds no raw reference to mutable Game Thread containers.
4. `FWorldSceneViewExtension` scopes rendering to one world. Atlas updates occur through RDG before SightWeave sampling; strict black/live composition hooks after tonemap and honors `OverrideOutput`.
5. The persistent live result is a sparse floor-local R8 atlas. One 2048-square page is 4 MiB and contains 64 physical 256-square slots; each slot has a 248-square interior and 4-texel gutter.
6. Proposed comparison tiers are 25/10/5/2.5 cm per texel, with 64/128/192/256 active tiles and 4/8/12/16 MiB persistent bytes per owner/floor scope. No shipping tier is selected.
7. Compatibility identity is the full Runtime-canonical capability sequence. Hashes accelerate but cannot establish equality.
8. Profile masks are transient per-dirty-tile scratch. Persistent memory does not multiply by source/profile count.
9. Bypass never enters a profile; suppression applies only after compatible intersections and bypass union.
10. Every packet is self-contained, so newer pending revisions may coalesce older ones. Stale, contradictory, partial, or wrong-world packets are rejected.
11. A scope is not sampled while `DesiredRevision > AppliedRevision`. Multi-frame work shows black until complete; partial/stale pixels are never published.
12. Camera movement/resolution changes do no mask raster work. Old/new source/polygon bounds drive local dirty tile redraw; uncertain impact becomes full scope rebuild.
13. Baseline mask generation uses global graphics shaders and SRV+render-target-capable `PF_G8`; R8 UAV/compute is an optional measured optimization, not a hidden dependency.
14. Test readback is bounded and asynchronous in Development/Editor; Shipping, gameplay, memory, and save never read GPU masks back.
15. Subject Reveal Overrides, neutral-gray memory, current GBuffer reconstruction, and last-seen proxies do not enter the M3.1 live-mask packet.

## UE 5.8.1 source audit

The architecture was checked against local UE 5.8.1 source (`Build.version` changelist `56057345`), including:

- shader directory mapping and plugin global-shader patterns;
- `PostConfigInit` shader module loading and BuildPlugin shader inclusion;
- `FWorldSceneViewExtension`, RDG-aware pre-render callbacks, after-tonemap subscription, Scene Color and `OverrideOutput` conventions;
- external pooled target registration, transient RDG resources, and explicit extraction/allocation APIs;
- `PF_G8` SRV/render-target engine use;
- Render Thread enqueue/resource release and world-extension lifetime patterns;
- `FRHIGPUTextureReadback` plus RDG asynchronous copy;
- UE 5.8 `RDG_EVENT_SCOPE_STAT`; legacy `RDG_GPU_STAT_SCOPE` is deprecated/no-op.

This was source/API validation only. No shader was compiled and no runtime GPU behavior was inferred from it.

## Precision, capacity, and budget status

Approved facts, frozen design, proposed defaults, arithmetic estimates, and future measurements are kept separate in the three M3 documents.

- **Closed formal CPU contracts:** M2 Batch512 on-CPU p50/p95/p99 `<=150/180/200 us`; broad 4V/2L door p99 `<250 us`; Prepared 4096/source p50 `<1 ms` and p99 `<2 ms`; zero-allocation, deterministic differential, and all associated M2P.5 evidence remain regression gates.
- **Provisional GPU requirement targets:** raster plus final composite p95 `<1.0 ms` at 1080p and `<1.5 ms` at 1440p is the proposed interpretation until hardware/workload approval.
- **Proposed limits:** one composited owner, one resident owner/floor scope, eight ordinary active profiles, a 32 MiB total live-mask GPU budget, and two-frame ordinary presentation lag.
- **Arithmetic only:** 1440p has `1.7778x` the pixels of 1080p; R8 pages/tiers are 4/8/12/16 MiB before driver metadata. These are not timings or measured allocations.
- **Unmeasured:** all GT packet, RT, GPU pass, VRAM high-water, visual stability, correctness/readback, and clean-host results.

No pretty-number substitution is allowed. The future 2/8/32 source and 1/4/8 profile matrices retain full scale; 32-source stress has a separately labeled proposed ceiling, not a reduced workload.

## Self-adversarial review

The required 14-question review was performed after the initial drafts. It found missing explicit coverage for damage reveal, L/T and angular seam/height fixtures, 2/8/32-source stress, plugin restart, Lab partitioning, per-pass/Render Thread budgets, and the future proxy seam. Those omissions were corrected in the authoritative documents before this handoff.

| Question | Result after correction |
| --- | --- |
| 1. Can straight walls still wave? | Hard storage is camera-independent world-space; camera/resolution produces composite-only work. Four-tier CPU/GPU diffs, seam gutters, and deterministic motion captures detect movement. Presentation feather remains bounded. |
| 2. Can current door/decor/environment state leak? | The live packet contains only CPU-approved polygons/IDs. It contains no Scene Color/GBuffer/material/door snapshot. Gray memory is not implemented or reconstructed. |
| 3. Can CPU/GPU rules fork? | Same immutable revision, canonical profiles, CPU polygon/triangles, texel-center CPU oracle, stale rejection, and differential readback bind both paths. GPU remains presentation-only. |
| 4. Can profiles merge incorrectly? | Equality is the full normalized capability sequence; fingerprint collisions are explicitly tested and cannot merge profiles. Overflow fails black instead of merging. |
| 5. Are bypass and suppression ordered consistently? | Formula and passes freeze profile intersections -> bypass union -> suppression. Bypass has no profile index. |
| 6. Can another game reuse it? | New Render module and public packet vocabulary are SightWeave-only; no DARKWELL types, paths, tags, or rules enter Runtime/Render. |
| 7. Can it run independently in a Fab user project? | Plugin-relative shader mapping, Runtime module packaging, no project/Engine-private path, BuildPlugin/Shipping/clean-host matrix, and NullRHI fallback are required. Still unverified until implementation. |
| 8. Can VRAM explode by source/profile? | One persistent effective atlas per resident owner/floor; sources/profiles use bounded transient tile scratch. Explicit byte budget/caps reject without truncation. |
| 9. Does camera/resolution destabilize the world mask? | Logical tile/texel keys use floor origin and precision only. Resize, FOV, motion, and 1080p/1440p assert zero raster work and stable hard world samples. |
| 10. Is there a GT/RT dangling-lifetime risk? | Enqueued commands own const packet data; world serial/resource generation reject late work; world subsystem/SVE ownership and Render Thread release are explicit. |
| 11. Does shader/RHI failure reveal the whole map? | Every unavailable/invalid/partial state maps to black with a reason. White/stale/cross-scope fallbacks are forbidden and lifecycle-tested. |
| 12. Is future memory coupled into live masks? | Memory mask, remembered environment, and subjects are explicitly separate future compositor inputs and absent from M3.1 packets/resources. |
| 13. Can tests find a one-pixel/boundary semantic difference? | Selected-tile hard readback compares every texel center, isolates the one-texel edge band, records coordinates/attribution, and fails any non-boundary or occluder-bridging mismatch. |
| 14. Is there a clear future last-seen seam without early implementation? | The compositor reserves separately named memory/subject inputs while M3.1 implements only effective live mask; Lab and scope rules prohibit creating proxy regions/assets now. |

No review item remains as an undocumented architecture ambiguity. Items that require implementation evidence are listed below rather than represented as solved tests.

## User decisions still required

These do not block M3.1 correctness scaffolding, but they block a final product performance/support verdict:

1. minimum supported Windows CPU/GPU and approved driver class;
2. maximum map/floor extents and representative occupied-tile distribution;
3. required simultaneous Knowledge Owners and whether a second resident floor is needed for transitions;
4. final active source/profile workload and whether proposed safety caps are acceptable;
5. supported RHI/feature-level matrix beyond the required first DX12/SM6 evidence;
6. screen percentage/upscaler/build configuration for formal 1080p/1440p GPU budgets;
7. selected shipping mask/memory precision after reviewed 25/10/5/2.5 cm evidence;
8. approved presentation-lag limit and whole-plugin/renderer memory budgets.

The office RTX 4060 is only the primary development performance machine. The home Turing 8 GB GPU name remains truncated as `RTX 2070 ...`; it is not assumed to be an RTX 2060 Super and is not a formal minimum specification.

## Not verified in M3.0

- no `SightWeaveRender` module, packet API, shader, SVE, atlas, composite, or readback exists;
- no C++ build, shader compile, automation, D3D12/SM6, NullRHI M3 path, GPU capture, timing, soak, package, BuildPlugin, clean-host, or visual inspection was run;
- no M3 runtime allocation, VRAM, boundary, profile, owner/floor, lifecycle, Shipping, or Fab claim has evidence;
- no Lab asset or test region was created;
- neutral-gray remembered environment, last-seen proxy, memory GPU mirror, and DARKWELL integration remain later work.

A docs-only task does not require `DarkwellEditor` rebuilding. Existing M2P.5 binaries/evidence were not presented as post-M3 GPU evidence.

## Exact M3.1 scope

M3.1 should implement only the minimum vertical slice that proves the frozen live-mask contract:

1. add production const snapshot acquisition and renderer-neutral immutable packet types/builder without altering CPU query semantics;
2. add `SightWeaveRender`, plugin-relative shader mapping, global shader compile smoke, and black/Unavailable status;
3. add world-scoped Render subsystem/SVE lifecycle, world serial, revision coalescing/stale rejection, resource generation, and Render Thread-owned R8 external pooled target;
4. rasterize one CPU-triangulated polygon for one owner/floor/profile into one dirty tile, combine to the hard effective atlas, and prove it with asynchronous test-only readback;
5. add the smallest focused CPU/GPU differential, NullRHI, teardown/restart, D3D12/SM6, build, BuildPlugin, and clean-host evidence required for that slice.

M3.1 MUST NOT implement memory masks, neutral-gray environment, last-seen proxies, Subject Reveal Override rendering, DARKWELL adapters/gameplay, broad authoring assets, screen-space/Scene Capture fallback, GPU visibility solving, or full 1/4/8-profile optimization before the one-profile vertical slice is correct.

After M3.1 proves the foundation, a separately reviewed step can add complete 1/4/8 compatibility groups, bypass, suppression, full dirty-update matrix, four-tier comparison, composite/feather, and final GPU performance acceptance.

## Git handoff

The intended four documentation commits are:

1. `docs: start SightWeave M3 GPU mask contract`
2. `docs: define SightWeave GPU mask data flow`
3. `docs: define SightWeave GPU validation and budgets`
4. `docs: record SightWeave M3P0 architecture freeze`

The final full commit SHA, local/upstream/remote equality, Git LFS status, and exact worktree residue are reported after the fourth commit. `Darkwell.uproject` remains the user's local unstaged EngineAssociation difference and must remain neither staged nor committed.

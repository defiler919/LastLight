# SightWeave M2P.5 Handoff

Status: **IN PROGRESS — SAFE VALIDATION COMPLETE; ELEVATED FINAL MATRICES PENDING**

Last updated: 2026-08-26 (Asia/Shanghai)

## Objective

Close the intrinsic CPU tail of the authoritative broad dynamic-door workload without changing its inputs, sampling contract, correctness semantics, or production scope.

The formal hard gate remains broad dynamic-door aggregate administrator ETW on-CPU p99 `< 250 us`. The engineering target is `< 225 us`, and the ideal target is `< 200 us`.

## Branch and immutable baseline

- Branch: `codex/m2p5-sightweave-vision-solve-tail-closure`
- Baseline branch: `codex/m2p4-sightweave-etw-dynamic-sector`
- Baseline SHA: `c3a3323edadb648058fd33c4c1e57806eeac8536`
- M2P.4 runtime optimization SHA: `1462afa65dbb3c338d6d21166bcb26144e5f26c6`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`

At task start, local baseline branch HEAD, its upstream, and `git ls-remote` all resolved to the required baseline SHA. Git LFS status was clean and `git lfs fsck` passed. The new M2P.5 branch was created directly from that SHA.

`Darkwell.uproject` contains a local-only `EngineAssociation` difference. It is intentionally retained, unstaged, and excluded from every M2P.5 commit.

## Scope locks

- Do not enter M3 or merge, rebase, or force-push `main`.
- Do not modify DARKWELL gameplay, GPU masks, post-processing, memory textures, or `/Game/Maps/L_Prototype`.
- Do not change production Runtime before the first fail-closed, ten-independent-process, fine-grained broad-door administrator ETW classification is complete.
- Do not represent wall time or raw cycle deltas as intrinsic CPU time.
- Do not change workload sizes, warmups, nearest-rank statistics, thresholds, affinity, priority, Defender, or other security services.

## Starting authority

M2P.4 final administrator ContextSwitch/CSwitch ETW is preserved at:

`D:\UE_projects\LastLight\Saved\SightWeaveM2P4\EtwAttribution\post-ray-reuse-formal-20260826`

| Workload | Samples | on-CPU p50 / p95 / p99 / max (us) | ready p99 (us) | blocked p99 (us) | Classification | Verdict |
|---|---:|---:|---:|---:|---|---|
| Batch512 | 10,100 | 92.1 / 135.8 / 160.9 / 437.3 | 18.6 | 0 | 11 Plugin, 53 Scheduler, 0 GPU, 0 Unknown | PASS |
| broad dynamic door | 1,010 | 153.7 / 235.7 / 300.2 / 427.2 | 22.1 | 0 | 43 Plugin, 4 Scheduler, 0 GPU, 0 Unknown | FAIL |

The authority has ten independent PIDs, 105,120 markers, zero event loss, zero buffer loss, zero ownership conflicts, zero unclosed timelines, and zero Unknown samples. Of 43 broad-door Plugin CPU slow samples, 42 were attributed to `vision_solve`.

Supporting preserved evidence:

- calibration: `D:\UE_projects\LastLight\Saved\SightWeaveM2P4\EtwCalibration\post-ray-reuse-final-20260826`
- elevated orchestration: `D:\UE_projects\LastLight\Saved\SightWeaveM2P4\Final\post-ray-reuse-final-elevated-20260826`
- allocation: `D:\UE_projects\LastLight\Saved\SightWeaveM2P1\AllocationProof\M2P4PostRayReuseElevatedFinal_20260826`

## Phase 1 diagnostic contract

M2P.5 will first add non-Shipping/test-only diagnostics with stable machine-readable names and no new Shipping dependency. The diagnostics must associate every sample with PID, TID, sample ID, source ID, source counts, candidate/dirty/event/ray/active-set/polygon counts, revision, reuse/fallback metadata, wall/cycle measurements, and fail-closed ETW on-CPU attribution.

The existing solve has two measurement shapes:

1. Contiguous macro stages, suitable for begin/end ETW markers: source dirty discovery, candidate collection and event preparation, dirty-sector determination, event sorting or local merge, topology validation, fallback detection, and publication preparation.
2. Repeated micro stages interleaved inside the ordered ray loop: active-set update, reuse lookup, reused-ray validation, changed-ray intersection, stable-ID tie-break, and vertex emission.

The diagnostic design must retain exact ordering and semantics while avoiding enough per-ray markers to manufacture the tail being measured. It will therefore preserve the existing authoritative total/stage marker schema, add sample/source detail keyed by stable identifiers, accumulate micro-stage wall/cycles in test-only state, and expose only calibrated ETW marker granularity whose overhead remains below the recorded calibration bound. Any micro-stage lacking defensible ETW on-CPU attribution will remain explicitly unproven rather than inferred from wall/cycles.

Before any production algorithm change, the diagnostic path must be built and tested, its marker overhead calibrated, and at least ten independent elevated broad-door processes captured and analyzed with zero loss, ownership conflict, unclosed timeline, or Unknown classification.

## Phase 2 and final acceptance

Only a hotspot proven by Phase 1 authority may justify a production change. Any such change triggers the full correctness, differential, allocation, 36,000-frame NullRHI and D3D12 soak, regression, BuildPlugin, clean-host, dependency, Shipping, NVIDIA stability, and two-independent-formal-matrix requirements in the M2P.5 task contract.

M2P.5 can be marked **COMPLETED** only when both independent final ten-process matrices preserve all fail-closed requirements, keep Batch512 within its existing gates, and independently report broad dynamic-door aggregate authoritative on-CPU p99 `< 250 us`.

## Phase 2 result and safe validation checkpoint

The two Phase 1-authorized production changes are implemented and pushed:

1. old-state incremental validation uses the previous Prepared cache while the
   exact target solve uses the selected target Prepared cache;
2. reused rays bypass angular active-set advancement, while rebuilt rays
   advance directly to their ordered angle.

All stale binding, revision, dirty-sector, seam, topology, and capacity cases
remain synchronous and fail-closed. The final code checkpoint before this
handoff update is `c8971a4ac1781b4a945e7a7bb3a4c415837027e9`.

Post-include focused M2P.5 is 4/4, full SightWeave is 116/116, and Darkwell is
24/24. The final strict allocation proof has 20 warmed workloads with three
samples each and all allocation/reallocation/byte counts at zero. Fresh
36,000-frame NullRHI and D3D12/SM6 soaks have zero correctness failures,
capacity growth, and Unknown. Final BuildPlugin plus clean-host Editor
Development, Game Development, and Game Shipping all pass. Runtime/Shipping
dependency isolation and both Lab smokes pass.

Two narrower serial scopes retain Batch512 wall p99 failures; the later full
scope and three independent extended performance processes pass. These wall
failures remain recorded and are not relabeled as intrinsic CPU. Exact paths,
metrics, hashes, warning audit, and package evidence are recorded in
`Docs/SIGHTWEAVE_M2P5_FINAL_VALIDATION.md`.

`Scripts/RunSightWeaveM2P5FinalEtwMatrices.ps1` is a parse-checked thin wrapper
over the existing M2P.4 calibration/attribution workflow. It will run one
calibration and two independent ten-process formal matrices inside one
high-integrity PowerShell child without changing samples, thresholds, or
analysis.

## Phase 1 result

The production-change boundary is now open based on the fail-closed ten-process authority recorded in:

`D:\UE_projects\LastLight\Saved\SightWeaveM2P5\VisionTailAttribution\preproduction-detailed-formal-20260826`

Aggregate broad-door authoritative on-CPU was `154.3 / 272.5 / 305.3 / 388.8 us` at p50/p95/p99/max. Event loss, buffer loss, ownership conflicts, unclosed timelines, and Unknown were all zero. Marker calibration used five independent control/detailed process pairs and detected no positive p50/p95/p99 perturbation; no subtraction was applied.

Source-exact attribution found two focused redundant paths:

1. The vision source sharing Prepared geometry with compatible illumination reported `prepared_index_replaced` in 1,010/1,010 samples and executed a full ray solve while alternating between two already-exact Prepared cache states.
2. The other three sources rebuilt only 38/49/52 dirty rays but advanced the angular active set for all 518/523 candidate rays before testing reuse.

The complete classification and authorized Phase 2 changes are in `Docs/SIGHTWEAVE_M2P5_VISION_TAIL_CLASSIFICATION.md`.

## Exact resume point

The user is away. No UAC or elevated ETW was attempted after the explicit pause
instruction, and the absence of a run is not a performance failure. Wait until
the user replies `我回来了`. Then verify the branch and worktree, explicitly say
`现在请点击UAC的是`, and launch one visible elevated child for the prepared final
matrix wrapper:

```powershell
Set-Location -LiteralPath 'D:\UE_projects\LastLight'
git switch codex/m2p5-sightweave-vision-solve-tail-closure
git status --short --branch
```

Do not stage `Darkwell.uproject`. Do not modify Runtime unless the two
authoritative post-change matrices prove a remaining fixable Plugin CPU tail.

# SightWeave M3.2 final validation

Status: **IN_PROGRESS**

Branch: `codex/m3p2-sightweave-sparse-atlas-residency`

Frozen M3.1 baseline: `a96e85f9c0ac32400abf81c87f75e2db15cdb36f`

Current validated checkpoint: pending

Engine: Unreal Engine 5.8.1, changelist `56057345`, at `D:\UE_5.8`

Primary home evidence path: Windows D3D12 SM6 on NVIDIA GeForce RTX 2070 SUPER, Turing, 8192 MiB, Studio Driver 610.88

## Admission record

M3.1 home admission passed before M3.2 began:

- full Editor Development build passed;
- NullRHI 10/10 passed;
- cold D3D12/SM6 29/29 passed;
- warmed-process D3D12/SM6 29/29 passed;
- all completed readbacks were binary `PF_G8` and had zero CPU/GPU texel mismatch;
- lifecycle, stale/duplicate revision, explicit clear, teardown/restart, and stale asynchronous readback passed;
- the cold process compiled every SightWeave global shader type;
- Git, upstream, remote, and LFS closed at the M3.1 baseline.

This is independent Turing validation and does not replace or merge with the office RTX 4060 evidence.

## Completion gates

The final verdict remains `IN_PROGRESS` until the following evidence is recorded without an unexplained correctness, lifecycle, performance, packaging, or Git/LFS failure:

- deterministic logical-to-physical mapping including negative and large coordinates;
- multi-tile polygon parity and horizontal, vertical, diagonal, corner, angular, and gutter seams;
- deterministic allocation, reuse, eviction, slot-black clear, pin/in-flight/readback protection, and capacity+1 behavior;
- no-change zero work and coalesced add/move/delete/profile/illumination/bypass/suppression dirty scheduling;
- one dirty tile does not redraw the entire atlas;
- owner, floor, full canonical profile, world, and revision isolation;
- persistent-resource teardown and delayed-command isolation;
- NullRHI fail-black and D3D12/SM6 selected-tile asynchronous readback parity;
- cold creation separated from warmed no-change, one-tile, eight-tile, multi-tile, and eviction/reuse samples;
- Editor, M3.1 regression, M3.2 focused, full SightWeave, DARKWELL 24, Lab, BuildPlugin, clean-host Editor/Game Development/Game Shipping, and Shipping dependency scans;
- severe-log, diff, Git, and Git LFS closure.

## Results ledger

| Area | Status | Evidence |
| --- | --- | --- |
| M3.1 home admission | Passed | Handoff admission record and ignored reports/logs |
| Persistent atlas pages | Pending | — |
| Residency/allocation/eviction | Pending | — |
| Dirty-tile scheduling | Pending | — |
| Seam/gutter parity | Pending | — |
| Scope/revision/world isolation | Pending | — |
| Cold/warmed performance | Pending | — |
| Allocation/capacity high-water | Pending | — |
| NullRHI/D3D12 matrix | Pending | — |
| Packaging/clean host/Shipping | Pending | — |
| Git/LFS closure | Pending | — |

## Retained M3.1 performance risk

The home one-shot M3.1 harness reported RT RDG setup p95 of 493.500 us in the cold process and 433.501 us in the second process, above the proposed 200 us target. Because every case creates a fresh render state and resources, neither distribution is warmed persistent-update evidence. M3.2 must replace this ambiguity with explicit persistent-resource measurements and must not lower the target or delete slow samples.

## Final verdict

**IN_PROGRESS**

The final document will use only `COMPLETED`, `PARTIAL`, or `BLOCKED` after the full validation matrix is evaluated.

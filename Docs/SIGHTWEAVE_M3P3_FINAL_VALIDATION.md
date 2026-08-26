# SightWeave M3.3 final validation

Status: **IN_PROGRESS**

Branch: `codex/m3p3-sightweave-hard-mask-composite`

Frozen M3.2 baseline: `ccb4c02c7a0bcbd9295847f16da17985bd8fd39c`

Engine: Unreal Engine 5.8.1, changelist `56057345`, at `D:\UE_5.8`

Primary home evidence: Windows D3D12/SM6 on NVIDIA GeForce RTX 2070 SUPER (Turing), 8192 MiB, Studio Driver 610.88. Turing samples remain independent from RTX 4060 evidence.

## Validation ledger

| Area | Status | Evidence |
| --- | --- | --- |
| M3.2 admission | Passed | local/upstream/remote `ccb4c02`; clean worktree/LFS |
| Presentation binding contract | Pending | — |
| UE 5.8 post-tonemap injection | Pending | — |
| Screen/depth/world/tile/page mapping | Pending | — |
| Hard Scene Color/black composite | Pending | — |
| Fail-closed behavior | Pending | — |
| Lifecycle and teardown | Pending | — |
| Seam/gutter/page/readback matrix | Pending | — |
| Lab visual inspection | Pending | — |
| Cold/warmed performance and memory | Pending | — |
| M3.1/M3.2/full/DARKWELL regressions | Pending | — |
| BuildPlugin/clean host/Shipping | Pending | — |
| Git/LFS remote closure | Pending | — |

## Required final record

The closing revision records:

1. final status;
2. branch, baseline, and final SHA;
3. complete commit sequence;
4. RTX 2070 SUPER and driver identity;
5. UE 5.8 post-process injection evidence;
6. immutable presentation-scope contract;
7. screen/world/logical-tile/physical-page mapping;
8. hard composite rule;
9. fail-closed behavior;
10. lifecycle and teardown results;
11. seam, gutter, slot, and page results;
12. asynchronous GPU final-output readback results;
13. Lab visual inspection status;
14. separated cold/warmed performance distributions;
15. persistent/transient GPU memory and allocation high-water;
16. exact automation counts;
17. BuildPlugin, source-only clean-host, and Shipping isolation;
18. retained failures and warnings;
19. unvalidated items;
20. remaining risks;
21. Git/LFS/local/upstream/remote closure;
22. document paths;
23. next recovery command.

The two known M2P2 wall-time failures remain retained without threshold changes or sample deletion.

## Current verdict

**IN_PROGRESS**

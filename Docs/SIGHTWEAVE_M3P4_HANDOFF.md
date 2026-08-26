# SightWeave M3.4 inward-feather handoff

Status: **IN_PROGRESS**

Branch: `codex/m3p4-sightweave-inward-feather`

Frozen M3.3 baseline: `35411db9b193ec6a669278af6e43e38fa72f9d9a`

Engine: Unreal Engine 5.8.1, changelist `56057345`, at `D:\UE_5.8`

Validation GPU: NVIDIA GeForce RTX 2070 SUPER, Turing, 8192 MiB, Studio Driver 610.88

## Objective

M3.4 adds world-space inward-only visual feathering above the immutable M3.3 hard presentation gate:

```text
FinalPresentationWeight = HardLivePointSample * VisualFeatherWeight
OutputColor = SceneColor * FinalPresentationWeight
```

HardLive remains the sole presentation-eligibility authority. VisualFeather is presentation-only, may only reduce already-live Scene Color, and cannot feed gameplay, AI, HUD, interaction, lock-on, memory, Last-Seen, or save state.

## Starting disposition

Local HEAD, upstream, and the remote M3.3 branch all matched the frozen baseline. The worktree was clean, `git diff --check` passed, Git LFS had no pending objects, `git lfs fsck` passed, and `Darkwell.uproject` was unchanged.

Algorithm selection, rejected routes, resource policy, failure behavior, implementation evidence, retained failures, and recovery instructions will be recorded as reliable checkpoints complete.

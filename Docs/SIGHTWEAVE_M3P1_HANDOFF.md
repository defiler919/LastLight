# SightWeave M3.1 Handoff

Status: IN PROGRESS

Branch: `codex/m3p1-sightweave-render-single-tile`

Baseline: `2cb3f82ab44e810a09f18bed036fa1e4d36db4aa`

## Objective

Implement and validate the minimum executable M3 GPU presentation loop without changing the frozen M2 CPU Authority:

`authoritative polygons -> deterministic triangles -> immutable revisioned packet -> world-scoped render path -> one R8 tile -> asynchronous test readback -> CPU/GPU texel differential`

## Non-negotiable boundaries

- `SightWeaveRuntime` cannot depend on `SightWeaveRender`; the render module consumes a compact read-only Runtime contract.
- GPU data is presentation-only and cannot decide gameplay, AI, HUD, interaction, Last-Seen, or memory.
- M3.1 owns no full atlas, final post process, feathering, memory/proxy/reveal visuals, DARKWELL integration, SceneCapture, or GPU visibility solver.
- D3D12 SM6 is the supported GPU path. NullRHI is safe and unavailable/black. D3D11 and Vulkan are deferred.
- A missing current revision, invalid packet, unsupported resource, shader/RHI failure, stale world, or teardown produces black rather than old or white data.
- `Darkwell.uproject` contains an intentional local EngineAssociation difference and must never be restored, staged, or committed.
- No merge, rebase, force-push, or automatic shutdown.

## Current checkpoint

- Recovery, branch creation, remote publication, context reading, and local UE 5.8.1 API audit are complete.
- Implementation and validation evidence are in progress.

## Resume

```powershell
Set-Location 'D:\UE_projects\LastLight'
git status --short --branch
git rev-parse HEAD
git rev-parse '@{upstream}'
```

Continue from the first incomplete section in `Docs/SIGHTWEAVE_M3P1_FINAL_VALIDATION.md`. Preserve the unstaged `Darkwell.uproject` difference.

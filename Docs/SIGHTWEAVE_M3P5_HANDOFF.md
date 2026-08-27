# SightWeave M3.5 static-environment memory handoff

Status: **IN_PROGRESS**

Branch: `codex/m3p5-sightweave-static-environment-memory`

Frozen M3.4 baseline: `22f55b1e744cea37ad5d3c7beb618be0509fbf99`

Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`

Validation machine: NVIDIA GeForce RTX 4060, driver 610.88, 8188 MiB

## Objective

M3.5 adds world-scoped exploration memory for immutable static environment above the M3.4 hard-live and inward-feather presentation path. The final ordering is `HardLive -> HardMemory plus StaticEnvironmentEligibility -> strict-black Unknown`.

CPU packed memory is the only HardMemory authority. GPU resources are derived presentation mirrors. Camera state, viewport state, Scene Color, GBuffer appearance, lighting, shadows, VFX, dynamic subjects, and visual feathering are never memory-write authority.

## Starting audit

The milestone starts from the exact M3.4 repair baseline. Local HEAD, upstream, and remote branch all resolved to `22f55b1e744cea37ad5d3c7beb618be0509fbf99`. Git LFS status was empty and `git lfs fsck` passed. `Config/DefaultGame.ini` had no local override. M3.4 retained the 50 cm default `VisualFeatherWidthCentimeters` and the shader continued to point-sample HardLive before presentation feathering.

The only working-tree difference was the approved local `Darkwell.uproject` EngineAssociation change from `"5.8"` to `{1C0F19FD-493D-6BCD-A0CC-9FAB451BA183}`. It is excluded from every M3.5 commit.

## Scope guardrails

- Do not modify `/Game/Maps/L_Prototype` or integrate DARKWELL gameplay.
- Do not implement Last-Seen, subject memory, save persistence, networking, SceneCapture, GPU authority, or camera-derived writes.
- Do not let uncertain or unsupported environment content appear in memory; it fails black.
- Preserve the M3.4 HardLive point gate and 32 MiB live-presentation budget.
- Keep total SightWeave runtime memory at or below 64 MiB.
- Modify the Lab map only through Unreal Editor asset APIs and keep it under Git LFS.

## Resume

```powershell
cd D:\UE_projects\LastLight
git fetch origin
git switch codex/m3p5-sightweave-static-environment-memory
git status --short --branch
git pull --ff-only
```

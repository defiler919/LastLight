# DARKWELL Project Fog Visual Rebuild Handoff

Date: 2026-08-30

Branch: `codex/darkwell-project-fog-visual-rebuild`

Starting SHA: `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb`

Status: **PARTIAL — P1 CONTINUOUS LIVE COVERAGE PROVEN / P2 AUTHORIZED**

Deadline: 2026-09-05

## Current disposition

The user rejected the final SightWeave SurfaceMaterial visual candidate. The rejected result showed discrete world/tile stepping, periodic jaggies/flicker and orientation-dependent wall classification even though its texture format was floating point. The old candidate is neither accepted nor ready for another PIE.

This branch starts a DARKWELL-owned presentation rebuild. SightWeave remains the authority backend; its rejected visual paths stay as historical source but may not register or run in the formal candidate. The first implemented policy is `RememberedFromStart`, with gray static scene everywhere outside continuous Live coverage and no black Unknown.

## Proven checkpoint

P1 has passed its first reliable implementation attempt. The DARKWELL project presentation consumes continuous authoritative body/cone geometry, evaluates analytic fractional coverage in a project-owned 1400x1000 linear `R16F` target at 2.5 cm/texel, and binds it to the real tile BaseColor in `L_VisionIntegration`. The rejected SightWeave render/composite path is explicitly suppressed while the candidate is active.

The ignored P1 index is:

```text
Saved/DarkwellProjectFogVisualRebuild/P1/p1_evidence_index.json
```

It records 15/15 validated D3D12/SM6/normal-TSR cases, 0 severe hits, 3/3 continuous-coverage automation results and 1/1 vertical-slice result. Raw X/Y/diagonal centroid motion matches 0.25/0.5/1.0 texel input and all samples contain fractional edge values. The 20-second fixed and four 30-second motion captures were opened as contact sheets and adjacent-frame strips; no periodic whole-grid step or large jagged recurrence was observed.

## Immediate next gate

Implement P2 continuous free-space occlusion only. Restore the proof fixture walls through the existing Unreal-owned fixture path, cache stable 2D occluder geometry, produce continuous nearest-hit visibility clipped by the legal circle/cone, and prove that behind-wall free space remains Remembered.

Do not begin P3 wall/object surface coverage until raw P2 occlusion passes. Dynamic subjects, Torch/Lantern/Torch and HUD restoration remain P4 work.

## Protected results and prohibitions

Retain CPU authority, legal source/light decisions, subject policy, `NeverRemember`, snapshots, persistence/restore, world lifecycle and gameplay tests. Do not revive old composite/Stencil/SurfaceMaterial visual paths, rederive the final edge from tiles, disable TSR, reduce resolution, add blur, hide walls, modify `Darkwell.uproject` or touch `L_Prototype`.

Build and tests remain serial and bounded. Do not run BuildPlugin, Fab, clean-host, full historical SightWeave regression, Cook, Package or a full performance matrix. Keep the computer on.

## Maximum future handoff state

Even after every phase passes, the highest possible agent state is:

```text
PARTIAL — READY_FOR_USER_GRAY_LIVE_PIE
```

Only the user's dynamic PIE can accept gray/live usability. Unknown/exploration history is a later independent phase and must not begin automatically.

## Recovery command

```powershell
cd D:\UE_pro\Darkwell
git fetch origin
git switch codex/darkwell-project-fog-visual-rebuild
git pull --ff-only
```

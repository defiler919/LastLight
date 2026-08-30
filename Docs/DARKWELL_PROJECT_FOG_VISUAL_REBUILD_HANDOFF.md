# DARKWELL Project Fog Visual Rebuild Handoff

Date: 2026-08-30

Branch: `codex/darkwell-project-fog-visual-rebuild`

Starting SHA: `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb`

Status: **PARTIAL — P0 CONTRACT FROZEN**

Deadline: 2026-09-05

## Current disposition

The user rejected the final SightWeave SurfaceMaterial visual candidate. The rejected result showed discrete world/tile stepping, periodic jaggies/flicker and orientation-dependent wall classification even though its texture format was floating point. The old candidate is neither accepted nor ready for another PIE.

This branch starts a DARKWELL-owned presentation rebuild. SightWeave remains the authority backend; its rejected visual paths stay as historical source but may not register or run in the formal candidate. The first implemented policy is `RememberedFromStart`, with gray static scene everywhere outside continuous Live coverage and no black Unknown.

## Immediate next gate

Audit the exact backend seam and old visual activation points, then implement only P1: a no-wall high-contrast floor, body circle and one cone using genuinely continuous world-space coverage under 1080p D3D12/SM6 and normal TSR.

Required P1 evidence is 20 seconds fixed; 30 seconds each horizontal, vertical, diagonal and rotation; plus raw 0.25/0.5/1.0 texel translations. P1 must show intermediate coverage values and smooth edge displacement without discrete plateaus, periodic teeth or blur dependence.

Do not begin occlusion, walls or dynamic objects until P1 passes. If P1 fails twice, record and push `BLOCKED — CONTINUOUS LIVE COVERAGE PROOF FAILED` and stop.

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

# DARKWELL Project Fog Visual Rebuild Handoff

Date: 2026-08-30

Branch: `codex/darkwell-project-fog-visual-rebuild`

Starting SHA: `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb`

Status: **PARTIAL — READY_FOR_USER_GRAY_LIVE_PIE**

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

## P2 checkpoint

P2 now caches and evaluates 11 continuous world-space segments for the doorway, rotated wall, cube, wall ends, concave obstacle and T-junction. The normal D3D12/SM6/TSR raw probe measured `front=1`, `behind=0`, `doorway=1`, with 5,662 fractional pixels. Five formal evidence cases passed with zero severe hits and were opened as full contacts and adjacent-frame strips.

The validated index is:

```text
Saved/DarkwellProjectFogVisualRebuild/P2/p2_evidence_index.json
```

The first shader-input failure remains under `P2/FailedAttempts`; it is not part of the formal index.

## P3 checkpoint

P3 leaves ground on `FreeSpaceLiveCoverage` and classifies only the owning static surface. Walls use the maximum of two stable object-local exterior samples; the cube uses four. The sample frames are bound from native component transforms, so opposing rendered faces and camera directions cannot select contradictory states.

The validated index is:

```text
Saved/DarkwellProjectFogVisualRebuild/P3/p3_evidence_index.json
```

It records 7/7 D3D12/SM6/normal-TSR cases, 0 severe hits, 1/1 P3 automation and 1/1 vertical-slice validation. The four opposing views plus 30-second along-wall and doorway strips were opened and checked frame by frame: wall top/sides, cube sides and doorway surfaces did not reverse or flicker, while behind-wall ground remained gray. The P2 raw probe and all 5,662 fractional edge pixels remained intact.

## P4 checkpoint

P4 removed the leftover P1 forced-hidden/controller-paused override from the normal candidate. The Stalker Actor and threat HUD now consume the same authoritative `NeverRemember` snapshot and exact revision. The real loadout path was captured through Torch/Lantern/Torch: Lantern hid both subject and threat label; restoring Torch restored both. A separate 30-second sample ran with the normal Stalker controller enabled.

The validated index is:

```text
Saved/DarkwellProjectFogVisualRebuild/P4/p4_evidence_index.json
```

It records 1/1 updated vertical-slice automation, 2/2 D3D12/SM6/normal-TSR captures and 0 severe hits. Three complete tool cycles produced matching `Live actorHidden=0`, `Hidden actorHidden=1`, and restored-Live transitions; every HUD revision equaled the Actor authority revision.

## P5 final agent checkpoint

The bounded P5 matrix passed 19/19 normal D3D12/SM6/TSR captures and 6/6 targeted automation tests with zero severe log hits. It includes the complete 1080p movement/wall/tool/gameplay set, 1440p static/horizontal/rotation/wall/tool evidence and a 600-second normal gameplay soak.

The validated index is:

```text
Saved/DarkwellProjectFogVisualRebuild/P5/p5_evidence_index.json
```

Gray-layer metrics passed at both resolutions:

- 1080p: correlation `0.97934`, standard-deviation retention `86.97%`, RMS contrast retention `97.17%`;
- 1440p: correlation `0.97732`, standard-deviation retention `87.95%`, RMS contrast retention `97.42%`.

The agent opened the key contacts, 20-frame strips, opposing wall views, tool-cycle transitions, ROI pairs and soak contacts. No periodic tile stepping, large jagged recurrence, direction-dependent wall reversal, doorway closure, black Unknown layer, behind-wall leak or NeverRemember subject leak was observed.

## User acceptance next

Run a real dynamic PIE on this exact branch and verify:

1. static, horizontal, vertical and diagonal movement remain smooth;
2. slow rotation does not show periodic grid teeth or crawling;
3. walls are consistent from north, south, east and west while space behind stays gray;
4. gray floor, wall and object textures remain readable at normal play scale;
5. enemies and the threat HUD appear only in Live and never remain in gray;
6. Torch/Lantern/Torch restores the cone without sticking or leaking;
7. the gray/live mode is usable as a DARKWELL gameplay presentation.

Do not accept or reject this phase from screenshots alone. The user dynamic PIE verdict is the remaining gate. Do not start black Unknown/exploration history automatically.

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

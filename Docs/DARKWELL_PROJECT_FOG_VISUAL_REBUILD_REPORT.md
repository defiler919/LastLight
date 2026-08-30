# DARKWELL Project Fog Visual Rebuild Report

Date: 2026-08-30

Branch: `codex/darkwell-project-fog-visual-rebuild`

Starting SHA: `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb`

Status: **PARTIAL — P0 CONTRACT FROZEN**

24-hour P1 gate: 2026-08-31

48-hour vertical-slice gate: 2026-09-01

Final stop-loss: 2026-09-05

## 1. Controlling user failure

The user's final dynamic PIE rejected the SightWeave SurfaceMaterial candidate at `e76f9893ca544e861b1b0c4e7e30df55e1bea0fb`.

Confirmed failures:

- the Live edge follows the world/tile grid and shows periodic stepping, jaggies and flicker;
- `RGBA16F` storage did not provide continuous coverage because the input was still derived from discrete tile states;
- outward per-face wall sampling gave opposite states to faces of one wall and produced north/south asymmetry;
- the required wall rule is local cross-section visibility from either legal side without leaking Live into space behind;
- Remembered static texture improved, but the full visual architecture still failed user acceptance.

The prior automatic runs and image metrics remain engineering history. They did not predict or override the real PIE verdict.

## 2. Frozen retained backend

The new project presentation continues to consume SightWeave's reliable CPU authority, legal light/source decisions, occlusion inputs, subject policy, `NeverRemember`, static-memory ownership, snapshots, persistence/restore, world lifecycle and gameplay tests. These are not reimplemented.

The old post-/pre-TSR composite, CustomDepth/Stencil composite, discrete SurfaceMaterial coverage, per-face wall sampling and tile-mirror final edge are rejected presentation paths. They remain in history but will not register or run in the formal project candidate.

## 3. P0 result

- verified prior branch clean at the expected SHA locally and at upstream after fetch/pull;
- verified no unpushed Git or LFS work;
- confirmed the new branch did not exist remotely;
- created and pushed `codex/darkwell-project-fog-visual-rebuild` from the exact baseline;
- superseded the old readiness claim in the rescue report/handoff;
- froze the project-owned continuous coverage contract and gated execution plan;
- made no source, shader, asset, map, config or project-file change in P0.

## 4. Architecture and phase findings

Not started at this checkpoint. The next work is the read-only activation/interface audit, followed by P1 no-wall continuous coverage only. P2 wall/occlusion work is forbidden until P1 passes.

## 5. Evidence ledger

The ignored evidence root will be `Saved/DarkwellProjectFogVisualRebuild`. It must contain per-phase logs, raw coverage, extracted frames, adjacent strips, contacts and metrics. Nothing below `Saved` is committed.

## 6. Current limitations

No new visual implementation or visual pass is claimed at P0. `RememberedFromStart`, continuous coverage, occlusion, wall surface coverage, dynamic subjects and final 1080p/1440p proof remain pending. The current state is not ready for user PIE.

# SightWeave M6P1 execution report

## Superseding user visual verdict — 2026-08-29

**PARTIAL — USER_PIE VISUAL AND SEMANTIC ACCEPTANCE FAILED. M6P1 is not COMPLETED.**

The implementation, automation, build, Cook, formal-View, readback, screenshot, log, and repository evidence recorded below is retained without deletion or concealment. The user's later real dynamic PIE inspection nevertheless overturned the prior visual-completion implication and established that the current picture is not usable in DARKWELL.

The failure is architectural, not ordinary art tuning: edge reconstruction remains visibly stepped; player/camera motion produces crawling, jitter, and flicker; Width=50 feather blurs rather than removes the stair steps; Unknown/Remembered/Live layers have coordinate or sampling misalignment; Remembered is a flat-gray obstruction instead of recognizable filtered static-scene memory; and wall/surface classification incorrectly blacks out player-facing wall sides instead of beginning behind the occluder.

The new highest-priority contract is `Docs/SIGHTWEAVE_DARKWELL_VISUAL_REQUIREMENTS.md`. Old screenshots, static pixel samples, readbacks, and green automation must not be cited as proof of visual acceptance. Only explicit user approval after real dynamic PIE under the new contract can restore `COMPLETED`. The first dynamic visual prototype is due within 48 hours, the hard deadline is 2026-09-05, and failure of any core requirement at that deadline unconditionally abandons SightWeave.

## Status

**PARTIAL — USER_PIE VISUAL AND SEMANTIC ACCEPTANCE FAILED.** The DARKWELL-owned adapter vertical slice and its recorded engineering gates remain implemented and evidenced, but the required user-operated dynamic visual and semantic acceptance failed.

Implementation branch: `codex/m6p1-sightweave-darkwell-adapter-vertical-slice`

Audit baseline: `25eb813a87cacd7dadb34f1b234c582df1467964`

Validated implementation/map head before closure documents: `868c89299f04012386dd1722a0c68c484c6bdb6d`

## Reliable checkpoints

| Commit | Result |
| --- | --- |
| `b4d2ddc` | froze the M6P1 adapter integration contract and execution plan |
| `0b96bd1` | normalized contract formatting only |
| `d8f4bc5` | added the DARKWELL authority types, adapter lifecycle, rollback, diagnostics, and foundation tests |
| `136c563` | connected player body/cone/torch sources, static environment, Stalker `NeverRemember`, HUD snapshot agreement, and vertical-slice tests |
| `868c892` | added the sole authorized binary asset, `/Game/Maps/L_VisionIntegration.umap` |

Every checkpoint was pushed normally. No merge, rebase, reset, clean, force-push, or plugin modification was used.

## Product implementation

### Authority and lifecycle

- `EDarkwellVisibilityAuthorityMode` in `Source/Darkwell/Public/Visibility/DarkwellVisibilityAuthority.h` has explicit `Legacy` and `SightWeave` states; default diagnostics and subsystem construction are Legacy.
- `UDarkwellSightWeaveWorldSubsystem` in `Source/Darkwell/Private/Visibility/SightWeave` is a `UTickableWorldSubsystem` and the sole product-to-SightWeave translation seam.
- `ADarkwellVisionIntegrationGameMode` asks the subsystem to activate only when the dedicated fixture exists. Normal maps never request SightWeave.
- Activation validates one player, one base Stalker with a stable ID, one fixture, empty Runtime registrations, and Runtime/Render services before publishing authority.
- Legacy visibility ticking/writes, Stalker legacy callbacks, threat filtering, and the legacy HUD fog blendable are disabled before the first SightWeave registration mutation. Failed or duplicate activation unregisters in reverse, clears View selection, restores Legacy consumers, and reports World plus authority mode.
- World teardown unregisters subject, static environment, occluder, light, vision sources, floor, and presentation selection, then resets diagnostics. Multi-world automation proves isolation.

### Frozen source semantics

| Source | Final semantics |
| --- | --- |
| Body | player-centered radial source, `120 cm`, `BypassLegalIllumination`, always registered while the valid player exists |
| Cone | player-facing directional source, `2200 cm`, half-angle `52°` to `35°` from real shotgun aim progress, `RequiresLegalIllumination` |
| Torch | radial legal light, `1250 cm`, capability `Darkwell.Visible.Torch`, active only for the existing equipped/on torch with positive charge and a living player |

The adapter updates transforms and changed descriptions in place. It does not create registrations per tick and does not sample rendered light intensity. The existing lantern remains a visual/gameplay item but emits no M6P1 SightWeave legal-light capability; pressing and releasing `E` to equip it is the verified torch-off path.

### Static environment, Stalker, and HUD

- `ADarkwellVisionIntegrationFixture` supplies a floor, two wall segments forming a doorway, a landmark, four static-environment surfaces, and a rendered directional key light that has no gameplay authority.
- Exploration uses the existing Coarse memory contract. The formal View shows black unexplored cells, live visible cells, and neutral-gray remembered static cells.
- The real base `ADarkwellStalkerCharacter` uses stable ID `Enemy.Stalker.VisionIntegration` and `ESightWeaveSubjectMemoryPolicy::NeverRemember`.
- The adapter performs one subject query/evaluation per update, stores one neutral `FDarkwellVisibilitySubjectSnapshot`, assigns one authority revision, applies it to the Stalker, and exposes that same stored snapshot to `ADarkwellHUD`.
- The HUD requires its snapshot revision to equal the Stalker's applied revision. When not hard-live, both the Stalker presentation and its threat entry disappear; no Last-Seen or gray enemy residue is allowed.

No DARKWELL consumer accesses SightWeave private implementation classes. The two product consumers depend only on the DARKWELL adapter and neutral snapshot types.

## Integration map

Asset: `/Game/Maps/L_VisionIntegration.umap`

Repository path: `Content/Maps/L_VisionIntegration.umap`

The map was created as a new non-partitioned level with `LevelEditorSubsystem.new_level`, not copied from `L_Prototype`. It was saved, reopened, and revalidated through Unreal Editor Python APIs.

Map-authored actors and starting transforms:

- one native fixture at `(0, 0, 0)`;
- one `PlayerStart` at `(-650, 0, 100)`, yaw `0°`;
- one real base Stalker at `(550, 0, 92)`, yaw `180°`;
- one `NavMeshBoundsVolume` centered at `(0, 0, 100)`.

The map uses native `ADarkwellVisionIntegrationGameMode`, contains no World Partition or external actors, and has no Blueprint, Material, Texture, Data Asset, Niagara, redirector, autosave, or second-map companion. It loaded under NullRHI and D3D12/SM6, and its adapter logged exactly two vision sources, one legal light, one occluder registration containing the two segments, four static surfaces, and one subject.

The only new binary asset in M6P1 is `Content/Maps/L_VisionIntegration.umap`; Git LFS tracks it.

## Automation results

All Unreal runs were serial.

| Gate | RHI | Result | Retained report |
| --- | --- | --- | --- |
| M6P1 vertical slice | NullRHI | 4/4 success, 0 warning, 0 failure | `Saved/AutomationReports/M6P1_VerticalSlice_NullRHI_Rerun` |
| M6P1 vertical slice | D3D12/SM6 | 4/4 success, 0 warning, 0 failure | `Saved/AutomationReports/M6P1_VerticalSlice_D3D12_SM6` |
| Full DARKWELL | NullRHI | 28/28 success: original 24/24 plus M6P1 4/4 | `Saved/AutomationReports/M6P1_FullDarkwell_NullRHI_Final` |
| Full SightWeave | NullRHI | 195/195, comprising 193 success plus 2 success-with-warning | `Saved/AutomationReports/M6P1_FullSightWeave_NullRHI` |
| Full SightWeave | D3D12/SM6 visible final | 287/287, comprising 285 success plus 2 success-with-warning | `Saved/AutomationReports/M6P1_FullSightWeave_D3D12_SM6_VisibleFinal` |

The four M6P1 tests are:

- default Legacy and teardown/restart state;
- multi-world isolation;
- vertical-slice body, cone, torch, lantern exclusion, wall occlusion, static packet, NeverRemember, and shared HUD/Stalker revision;
- duplicate-fixture transactional rollback to Legacy.

The two retained warnings in each full SightWeave run are pre-existing expected/environmental cases: the MotionTrace connectivity probe timed out on `generate_204`, and the malformed rollback matrix intentionally sent corrupted/incomplete ZLIB data. Neither is a failed assertion or product error.

### Preserved D3D12 attempt classification

The first full `-RenderOffscreen` D3D12 run produced 284/287 with three failures and is retained rather than hidden:

1. the warmed sparse-readback timing case could not obtain an absolute GPU timestamp in the offscreen mode;
2. `Camera34Observability` used a `1009×315` embedded PIE viewport, below its frozen ROI geometry;
3. `LastSeenLab` used the same short viewport and its yaw proxy count fell below the frozen lower bound.

The warmed timing and Camera34 cases passed isolated visible-window reruns. LastSeenLab passed 1/1 at its frozen `1009×340` viewport with 264 deterministic neutral pixels in the identity-reuse state. The entire suite was then rerun in one visible D3D12/SM6 process at that frozen viewport and passed 287/287. No plugin source, threshold, assertion, TAA setting, or compositor setting was changed.

## Formal D3D12/SM6 View evidence

Evidence directory: `Saved/M6P1_GameViewEvidence` (ignored and intentionally untracked).

Environment for every image:

- real game View from `UnrealEditor.exe -game`, not SceneCapture;
- client resolution `1274×729`;
- NVIDIA GeForce RTX 2070 SUPER, D3D12, Feature Level SM6, shader platform `PCD3D_SM6`, hardware shader model 6.7;
- `r.TemporalAA.Quality=2`; no TAA, feather, or formal-compositor bypass was used;
- authority log: `World=L_VisionIntegration authority=SightWeave active` with `vision=2 light=1 occluder=1 static=4 subject=Enemy.Stalker.VisionIntegration`;
- map-authored starts are player `(-650,0,100)` and Stalker `(550,0,92)`. The real AI remained active, so later capture frames intentionally contain runtime movement; the input trajectory, not a fake screenshot actor, produced the occlusion/reacquisition states.

| File | Input/state | Expected and observed result | SHA-256 |
| --- | --- | --- | --- |
| `01_torch_on_cone_stalker_visible.png` | Torch equipped/on, cursor aimed through doorway from the authored start lane | live cone plus close body circle; real Stalker visible; red `THREAT STALKER HUNTING` HUD visible | `229760D7FD2D0D478B38BD1138ABBFD95AA736A7819429728871E40CFA7D2BBE` |
| `02_torch_off_body_only.png` | normal `E` press/release equips lantern, disabling the only legal torch source | only the live body circle remains; outer cone is neutral memory; Stalker and threat HUD are both absent | `E8FAAD304ABF63800F0BFC2602B56F7F3E608ACF57526BD9F44AD37C5044A9F7` |
| `03_static_remembered_stalker_hidden.png` | torch restored with `E`, cursor aimed away from Stalker | previously explored floor remains gray; new cone is live; Stalker and threat HUD remain absent; no gray enemy silhouette | `8E30CBD914F33CA6BD322B83078AF83E905110CDC6D39654E063C90E813DCCC1` |
| `04_stalker_reacquired_hud_visible.png` | cursor returned through doorway | Stalker and threat HUD reappear together; static memory remains gray outside live coverage | `CF0A60A5A6EA1F8027C4DBC1DA956AEE16554AD0931CBB6CF59E912E249E1F15` |
| `05_wall_occlusion_stalker_hidden.png` | `D` strafe for 1.6 s puts the player north of the doorway; cursor aims toward the Stalker across the wall | wall truncates live coverage; Stalker and threat HUD are absent; occluded/unexplored region remains black | `FE11DCD1426B7ECCD0772E62A0E9893D35DE4F41DB30687059D956AA0A68CAE7` |
| `06_doorway_reacquired_no_dual_fog_edge.png` | `A` strafe for 1.6 s restores doorway lane and cursor aim | Stalker and threat HUD return together; a single SightWeave live/gray/black boundary is visible with no Legacy double edge or flicker | `853F718FA44F12B5EDECA78722DD764836904FDBACCCB4F7CCBA24E2D7DB95F6` |

The agent opened all six images at original detail and visually inspected them. Invalid early desktop-obscured/menu captures were overwritten and are not evidence.

## Builds, map boot, and Cook

| Gate | Result |
| --- | --- |
| `Scripts/BuildEditor.ps1` / DarkwellEditor Win64 Development | success |
| Darkwell Win64 Development | success |
| Darkwell Win64 Shipping | success |
| Integration map NullRHI boot | success; map loaded and SightWeave activated |
| Integration map D3D12/SM6 boot | success; RTX 2070 SUPER / `PCD3D_SM6` |
| Integration map isolated Cook | `BuildCookRun` success; 530 packages, 0 errors |
| `L_Prototype` legacy boot | success; `DarkwellGameMode`, `authority=Legacy`, no SightWeave activation |

The isolated Cook command used `-cook -map=/Game/Maps/L_VisionIntegration -skipstage -nocompileeditor`. Two retained Cook warnings were environment/toolchain warnings; no class, asset, module, or map reference error occurred.

Build warnings are limited to UE 5.8 deprecations and MSVC 14.51 being newer than Epic's preferred 14.50 family. They do not originate from a failed DARKWELL or SightWeave compile.

## Shipping and severe-log scans

- `Darkwell.Build.cs` depends on `SightWeaveRuntime` and, for non-server targets, `SightWeaveRender`; it does not depend on SightWeaveEditor, SightWeaveTests, UnrealEd, or an automation module.
- The Shipping receipt has no SightWeaveEditor, SightWeaveTests, or UnrealEd build product/module dependency. The engine distribution includes an engine-owned `AutomationTestToolset.uplugin` descriptor as a UFS runtime dependency, but DARKWELL does not link that module and no DARKWELL M6P1 test name survives in the Shipping executable.
- Shipping binary scans found no `D:\UE_pro\Darkwell`, `SightWeaveEditor`, `SightWeaveTests`, `LastSeenLab`, `Camera34Observability`, `M4P1_`, or `M6P1_` string.
- Generic `UnrealEd`, `AutomationTest`, and screenshot strings exist inside the monolithic stock Engine image; module receipts and the absence of project test strings distinguish them from a DARKWELL Runtime dependency or leaked M6P1 command.
- Final authoritative map, View, full SightWeave, full DARKWELL, and legacy-boot logs contain zero fatal, assertion-failed, ensure-failed, low-level-fatal, or access-violation records.

## Scope preservation

- `Darkwell.uproject` was not modified.
- `Content/Maps/L_Prototype.umap` was not modified.
- no SightWeave source, shader, Build.cs, or descriptor was modified.
- no SaveGame persistence, damage-source reveal, lantern authority, Warden, multi-floor, dynamic-door, or broad game integration was started.
- generated builds, Cook output, logs, automation reports, screenshots, and temporary scripts remain ignored and untracked.

## User gate result

The user performed dynamic PIE and rejected the current visual and semantic result. The next phase may address only project-usability rescue under `Docs/SIGHTWEAVE_DARKWELL_VISUAL_REQUIREMENTS.md`; it must not treat the earlier green automation or screenshots as visual completion.

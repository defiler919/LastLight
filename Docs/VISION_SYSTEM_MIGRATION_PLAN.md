# Independent vision system migration plan

Status: proposed plan. Milestones after M0 require human architecture approval.

## Migration invariants

1. The existing DARKWELL fog remains the production prototype system until the independent plugin passes acceptance.
2. New algorithms are developed and tested in plugin-owned lab/example maps, not directly in `/Game/Maps/L_Prototype`.
3. No existing fog code, material, save field, or asset is deleted, renamed, or replaced during plugin prototyping.
4. The generic plugin contains no DARKWELL class names, gameplay tags, missions, items, enemy logic, Niagara, or audio.
5. DARKWELL integration is through project-owned adapters and an explicit single-authority switch.
6. Old and new systems never both control enemy/subject hiding, HUD visibility, interactions, memory writes, save restoration, or final post-process composition in one run.
7. Dynamic door, straight wall, room/opening, height/floor, multi-source, modifier, subject-memory, and save tests pass before DARKWELL integration.
8. Legacy removal is a separate, explicitly approved milestone and separate commit after new-system acceptance.

## Branch and commit strategy

Current design branch: `design/independent-vision-plugin`.

This branch contains validation, audit, requirements, architecture, and migration documentation only. The current requested local commit is:

```text
docs: define independent vision plugin foundation
```

After architecture approval, implementation should use a new `codex/`-prefixed feature branch unless the user requests another branch name. Suggested commits remain small and reviewable:

1. plugin/module skeleton and empty lab map;
2. explicit geometry/reference solver and unit tests;
3. optimized solver/spatial index and debug drawing;
4. world-space masks and gray environment composition;
5. memory modifiers and persistence;
6. subject policies/proxies;
7. editor authoring/validation and example content;
8. DARKWELL adapters and single-authority switch;
9. migration/acceptance fixes;
10. separately approved legacy deletion.

Do not combine legacy deletion with adapter integration or save migration.

## M0 — Architecture and baseline gate

### Work

- review `VISION_BASELINE_VALIDATION.md`, `VISION_EXISTING_SYSTEM_AUDIT.md`, `VISION_SYSTEM_REQUIREMENTS.md`, and `VISION_SYSTEM_ARCHITECTURE.md`;
- answer/approve the architecture questions;
- decide product/API name before public C++ types are created;
- repair the local Windows toolchain: install a compatible Windows SDK and current VC++ Redistributable;
- rerun the full `DarkwellEditor Win64 Development` build;
- rerun all `Darkwell` automation tests and confirm whether 24/24 are discovered and pass;
- execute the manual baseline PIE checklist on the legacy system;
- record an honest current-machine legacy performance sample if desired.

### Exit criteria

- human approval of authority algorithm, module boundary, memory strategy, floor scope, and v1 exclusions;
- successful Editor target build;
- executable automation count reconciled with 24 source definitions;
- legacy PIE baseline recorded, or an explicitly accepted decision to proceed with those regressions still pending.

No plugin implementation begins before the architecture approval portion of this gate.

## M1 — Plugin skeleton and isolated lab

### Work after approval

- create `Plugins/<ApprovedName>/` with Runtime, Editor, and Tests modules;
- add neutral settings, handle/result enums/structs, and a world subsystem shell;
- create a plugin-owned lab/example map through Unreal Editor APIs;
- add simple authored lanes for straight wall, diagonal wall, corner, room, opening, rotating door, height bands, floors, and two observers;
- establish automation categories and test-map loading without adding an algorithm shortcut;
- add an example knowledge owner/source/subject using plugin-generic actors only.

### Isolation rule

The plugin is disabled or unused by `/Game/Maps/L_Prototype`. No DARKWELL character, HUD, enemy, save, or material references the plugin yet. The old fog runs exactly as before.

### Exit criteria

- Editor build passes;
- plugin modules load/unload cleanly;
- lab map opens and runs with no DARKWELL dependency;
- packaging/dependency inspection confirms Runtime has no Editor dependency;
- initial empty registration/lifecycle tests pass.

## M2 — Explicit 2.5D authority

### Work

- implement floor definitions and explicit source/occluder components;
- implement authoring normalization and floor-local segment spatial index;
- implement the correctness/reference visibility-polygon solver;
- implement exact point/multi-sample queries and source attribution;
- implement immutable revisions and event-driven dirty updates;
- add debug drawing for source shape, segments, endpoint rays/events, polygon, floor, and query reason;
- add dynamic door segment updates;
- profile the reference solver before optimizing;
- add angular-sweep optimization only while preserving reference equivalence.

### Required tests before raster presentation

- long straight wall under lateral source movement and rotation;
- L/T corners, collinear/duplicate edges, narrow doorway, source on/near edge;
- closed, opening, rotating, and fully open door;
- static and dynamic geometry revision invalidation;
- floor separation and occluder height below/through/above eye band;
- two and eight sources, including a remote off-screen source;
- query point/bounds behavior at the exact hard boundary;
- deterministic results across repeated runs.

### Exit criteria

- all geometry tests pass;
- straight-wall polygon vertices remain on the authored wall line within the documented world-space epsilon;
- door transitions create the expected polygon opening without stale segments;
- gameplay queries consume only the immutable hard result;
- debug tools make every rejection/contribution explainable.

## M3 — World-space masks, detailed gray memory, and modifiers

### Work

- choose atlas versus texture-array path using a focused UE 5.8.1 spike;
- rasterize per-source hard polygons to stable world-space live masks;
- add CPU packed memory tiles and dirty GPU mirrors;
- implement hard memory writes independent of viewport;
- implement circle, box, room volume, polygon, floor, and height regions;
- implement `ClearMemory`, `BlockMemoryWrites`, `SuppressMemoryPresentation`, and `SuppressLiveVision` with deterministic overlap;
- add presentation-only feather/AA after hard rasterization;
- implement gray immutable-environment composition using detailed Base Color/stable material cues without live illumination;
- document supported/unsupported material domains;
- add mask/tile/modifier debug views and stats.

### Required tests

- hard CPU reference mask versus GPU polygon raster within an approved edge tolerance;
- straight/diagonal wall capture under 1080p, 1440p, window resize, movement, and turn paths;
- memory written by an off-screen remote source;
- no memory written across an occluder or by visual feather;
- clear then re-explore;
- blocker with live vision normal and no new memory;
- `BlockMemoryWrites` produces normal live vision and immediate black after live vision leaves, without writing new memory;
- memory suppression hide/restore;
- live suppression hiding hard subject queries as well as presentation;
- overlapping modifiers and tile-border seams;
- static floor/wall/decor material detail visible in gray, with no live light/shadow leak.

### Exit criteria

- hard masks and queries agree;
- unchanged straight walls meet the approved stability threshold;
- gray memory is materially richer than depth/normal outlines and contains no current lighting information;
- all four modifier operations and shapes pass automated/runtime inspection;
- no full GPU readback is needed for memory or save.

## M4 — Subject policies and persistence

### Work

- implement `NeverRemember`, `VisibleOnly`, `StaticEnvironment`, `LastSeenSnapshot`, and `Custom` policies;
- implement anchor/bounds/custom sample rules;
- implement live primitive/proxy transitions and a basic opaque static-mesh snapshot descriptor;
- implement host snapshot-provider interface for complex objects;
- ensure proxies are render-only and emit no collision/audio/VFX/live light;
- implement versioned deterministic plugin snapshot, tile compression, provider records, validation, and atomic restore;
- add clear/suppress/floor-stream behavior for snapshots/proxies.

### Required tests

- enemy/active monster never appears in memory and never exposes a HUD/interaction result outside hard live vision;
- visible-only pickup disappears outside live vision;
- placed stationary item uses last-seen only when explicitly configured;
- door/container/machine proxy retains last-seen state while authoritative state changes off-screen;
- reacquisition swaps proxy to live state immediately;
- clear deletes proxy record; suppression hides then restores it;
- save round-trip is deterministic; corrupt/future/oversized/duplicate/missing-provider cases fail explicitly;
- restore recomputes live state and uploads derived GPU tiles without restoring stale live polygons.

### Exit criteria

- subject policy matrix passes;
- no current enemy/dynamic state leaks through gray memory;
- save size/restore time are measured against the approved reference workload;
- unsupported remembered components are reported by validation instead of silently leaking.

## M5 — Fab-quality authoring, compatibility, and performance gate

### Work

- complete Editor visualizers, polygon/floor tools, conversion preview, undo, and validation;
- create neutral example actors/maps and user documentation;
- verify clean-project installation and removal;
- verify UE 5.8.1 DX12/SM6, Editor, and packaged Development;
- test fallback mask path if the chosen texture-array/RDG path is not universal;
- collect performance on the approved minimum hardware and declared workloads;
- enforce visible warnings/counters for caps, solve latency, memory growth, and unsupported content;
- audit public headers/names for game leakage and future binary/API stability.

### Exit criteria

- a new project can configure floors, occluders, sources, modifiers, subjects, and save restore from documented steps;
- no DARKWELL reference exists in plugin Runtime/Editor/Tests/content;
- all functional, render, persistence, lifecycle, and performance suites pass;
- performance budgets are met or revised with user approval and evidence;
- example maps demonstrate every advertised feature and limitation.

## M6 — DARKWELL adapter integration without authority overlap

### Integration map

Create a dedicated DARKWELL integration map through Unreal Editor asset APIs, for example `/Game/Maps/L_VisionIntegration`. It may be based on approved prototype geometry but must not replace or mutate `/Game/Maps/L_Prototype` while algorithms are still being tuned.

### Authority modes

Use one project-owned setting with mutually exclusive modes:

| Mode | Legacy computes/presents/hides/writes | WorldVision computes/presents/hides/writes | Purpose |
| --- | --- | --- | --- |
| `Legacy` | Yes | No | production rollback/baseline |
| `WorldVisionObserveOnly` | Yes | Debug computation only; no masks, subject callbacks, memory, HUD, save | compare queries safely without dual control |
| `WorldVision` | No | Yes | integration and acceptance |

Observe-only may compute polygons/stats but cannot change actor render state, UI, interaction, memory, or save data. `Legacy` and `WorldVision` modes must restore all component/proxy/blendable state when switching during development; shipping should select at world start.

### DARKWELL adapters

- player facing/aim and approved light rule -> player source/filter;
- security camera -> camera source;
- remote observation/lighting item -> explicit remote source;
- black fog/energy field -> modifier volumes;
- Stalker/Warden -> `NeverRemember` subject;
- loose pickups -> approved `VisibleOnly` or `LastSeenSnapshot` policy;
- exit, door, container, machine -> snapshot provider/proxy;
- HUD threat rows and interaction focus -> one WorldVision query/result path;
- legacy v6 explored cells -> optional one-way plugin tile migration;
- plugin snapshot -> field embedded in the next approved DARKWELL save schema.

Niagara/audio reactions remain in DARKWELL event adapters and are not plugin dependencies.

### Integration order

1. Observe-only source/occluder/floor mapping and query comparison.
2. Switch final post-process ownership on the integration map; legacy blendable off.
3. Route HUD/interactions to WorldVision while subjects remain visibly instrumented.
4. Route enemy/pickup/facility subject authority; legacy callbacks/tick disabled.
5. Add DARKWELL snapshot providers.
6. Add save embedding and optional old-grid migration only after runtime state is stable.
7. Run full automation and PIE regression.

### Exit criteria

- all lab gates remain green;
- integration map passes straight walls, rooms/openings, dynamic door, height/floor, multiple sources, modifiers, subjects, and save/load;
- exactly one camera fog blendable and one subject visibility authority are active;
- enemy world/HUD/interaction results agree with hard current vision;
- legacy mode still behaves as its baseline and remains a one-setting rollback.

## M7 — DARKWELL acceptance on production content

Only after M6 approval should WorldVision be enabled for an acceptance build using `/Game/Maps/L_Prototype` or its approved successor. This is integration/tuning, not algorithm development.

### Required automation/build

- full `DarkwellEditor Win64 Development` build;
- all existing DARKWELL tests plus plugin tests;
- save compatibility/migration tests;
- packaged Development smoke if save/render behavior differs from Editor;
- recorded performance on current workstation and approved minimum target.

### Required PIE/manual regression

- movement/facing/sprint/aim while live edge remains stable;
- shooting/reload and torch/lantern profiles;
- enemy world and threat-HUD visibility under player, camera, and remote source;
- exploration, gray detail, leaving/re-entering sight;
- every memory modifier behavior;
- dynamic door and fixed facility last-seen state;
- save/exit/load including plugin memory and snapshots;
- fuse/exit mission, death, escape, restart;
- menu/pause/viewport resize and floor transition;
- no error/ensure/assert/fatal logs.

### Acceptance evidence

Capture build/test logs, deterministic motion-path images/video for wall stability, query/mask debug captures, source/segment/subject counts, CPU/GPU timings, machine/RHI/resolution/build configuration, save size, and unsupported-content warnings.

### Exit criteria

- user accepts visual behavior and gameplay knowledge rules;
- all automated/manual gates pass or every exception is explicitly accepted;
- performance meets approved budgets;
- multiple save/load sessions prove no data loss;
- rollback to legacy is still available until the deletion milestone begins.

## M8 — Separately approved legacy deletion

This milestone is not authorized by the current task and must receive explicit approval.

### Work

- remove legacy HUD CPU mask generation and blendable wiring;
- remove old visibility component/grid/radial occlusion code and old-only tests;
- remove or migrate the game-specific fog-subject callback implementations;
- remove legacy material/Python generation assets only through Unreal Editor/official asset APIs;
- retain necessary backward save readers/migration for the approved compatibility period;
- remove authority switch/legacy mode only after rollback is no longer required;
- update README, progress, decisions, visibility contract, architecture, and save docs.

### Required separate commit

Suggested commit, subject to user approval:

```text
refactor: remove accepted legacy Darkwell fog system
```

Do not mix this commit with new plugin features, gameplay tuning, unrelated cleanup, or asset moves.

### Exit criteria

- full Editor build and complete automation pass;
- production PIE/manual regression pass;
- old saves migrate according to approved policy;
- Git diff confirms only approved legacy removal/migration;
- no orphaned asset references or duplicate visibility authority remains.

## Coexistence safety checklist

Before any integration session:

- assert exactly one subject authority mode;
- assert exactly one final fog composite has nonzero weight;
- assert only the selected system writes memory;
- assert HUD and interactions query the selected authority;
- assert the non-selected system does not restore/save memory;
- reset previously hidden live primitives and destroy/hide stale memory proxies on mode change;
- log authority mode, snapshot revision, and save schema at world start;
- keep legacy and plugin save fields separately versioned;
- never use ordinary filesystem commands to copy/rename/delete `.uasset` or `.umap` files.

## Rollback plan

Until M8 begins, rollback is setting authority mode to `Legacy` and loading a save whose compatibility is documented. Plugin components may remain present but must unregister/disable all effects. A failed WorldVision restore must not partially replace valid legacy memory. Adapter save migration should write a new host save version or preserve a backup/transactional old payload according to the approved save policy.

If plugin performance or material support fails acceptance, keep the old system, retain lab evidence, and narrow/revise the plugin architecture without deleting production fog.

## Recommended next milestone

The immediate next milestone is **M0: human architecture decision plus local toolchain repair and reproducible legacy baseline**. After approval and a successful 24-test discovery/run, proceed only to M1 (neutral plugin skeleton and independent lab map). Do not begin with DARKWELL integration, post-process replacement, or legacy deletion.

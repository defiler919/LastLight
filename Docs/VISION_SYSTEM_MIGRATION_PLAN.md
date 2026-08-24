# Independent vision system migration plan

Status: migration plan revised with the latest human product decisions. This documentation-only revision does not authorize plugin source, shaders, or Unreal assets; milestones after M0 still require separate implementation approval.

## Migration invariants

1. The existing DARKWELL fog remains the production prototype system until the independent plugin passes acceptance.
2. New algorithms are developed and tested in plugin-owned lab/example maps, not directly in `/Game/Maps/L_Prototype`.
3. No existing fog code, material, save field, or asset is deleted, renamed, or replaced during plugin prototyping.
4. The generic plugin contains no DARKWELL class names, gameplay tags, missions, items, enemy logic, Niagara, or audio.
5. DARKWELL integration is through project-owned adapters and an explicit single-authority switch.
6. Old and new systems never both control enemy/subject hiding, HUD visibility, interactions, memory writes, save restoration, or final post-process composition in one run.
7. Vision and legal illumination are separate explicit authorities. Old/new systems never infer legal illumination from Scene Color, rendered light brightness, or an ordinary `ULightComponent`.
8. Hard CPU queries and GPU presentation use the same vision and illumination revisions; effective live coverage is the vision-mask/illumination-mask intersection unioned with explicitly bypassing vision.
9. Subject Reveal Overrides are presentation-only and remain separate from `Visible`/`Remembered`/`Unknown`; they never write memory or last-seen snapshots.
10. V1 is single-player and has exactly one active queried/presented floor.
11. Dynamic door, straight wall, room/opening, height/floor, vision/illumination multi-source, modifier, subject-memory/reveal, and save tests pass before DARKWELL integration.
12. Legacy removal is a separate, explicitly approved milestone and separate commit after new-system acceptance.

## Branch and commit strategy

Current design branch: `design/independent-vision-plugin`.

This branch contains validation, audit, requirements, architecture, and migration documentation only. This decision revision must not create a plugin directory, C++/shader source, or Unreal asset. The current documentation commit is:

```text
docs: revise WorldVision design decisions
```

After architecture approval, implementation should use a new `codex/`-prefixed feature branch unless the user requests another branch name. Suggested commits remain small and reviewable:

1. plugin/module skeleton and empty lab map;
2. explicit vision/illumination geometry, reference solver, and unit tests;
3. optimized solver/spatial index and vision/illumination debug drawing;
4. world-space vision/illumination/intersection masks and neutral-gray environment composition;
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
- review the recorded human decisions and close or explicitly defer the remaining implementation gates;
- replace the `WorldVision` code name with an approved product/API name before public C++ types or plugin paths are created;
- repair the local Windows toolchain: install a compatible Windows SDK and current VC++ Redistributable;
- rerun the full `DarkwellEditor Win64 Development` build;
- rerun all `Darkwell` automation tests and confirm whether 24/24 are discovered and pass;
- execute the manual baseline PIE checklist on the legacy system;
- record an honest current-machine legacy performance sample if desired.

### Exit criteria

- separate human implementation approval of the revised vision/illumination authority, module boundary, memory/reveal strategy, single-active-floor scope, and v1 exclusions;
- successful Editor target build;
- executable automation count reconciled with 24 source definitions;
- legacy PIE baseline recorded, or an explicitly accepted decision to proceed with those regressions still pending.

No plugin implementation begins before the architecture approval portion of this gate.

## M1 — Plugin skeleton and isolated lab

### Work after approval

- create `Plugins/<ApprovedName>/` with Runtime, Editor, and Tests modules only after implementation and public-name approval;
- add neutral settings, distinct vision-source/illumination-source/reveal-override handle and result types, and a world subsystem shell;
- create a plugin-owned lab/example map through Unreal Editor APIs;
- add simple authored lanes for straight wall, diagonal wall, corner, room, opening, rotating door, height bands, stacked-floor rejection, vision/illumination intersection, an always-active attached circular bypass source, and two explicitly activated remote sources;
- establish automation categories and test-map loading without adding an algorithm shortcut;
- add an example single local knowledge owner, explicit vision/illumination sources, subject, and Subject Reveal Override using generic actors only.

### Isolation rule

The plugin is disabled or unused by `/Game/Maps/L_Prototype`. No DARKWELL character, HUD, enemy, save, or material references the plugin yet. The old fog runs exactly as before.

### Exit criteria

- Editor build passes;
- plugin modules load/unload cleanly;
- lab map opens and runs with no DARKWELL dependency;
- packaging/dependency inspection confirms Runtime has no Editor dependency;
- initial empty registration/lifecycle tests pass.

## M2 — Explicit 2.5D vision and legal-illumination authority

### Work

- implement floor definitions and separate explicit vision-source, legal-illumination-source, and occluder components;
- implement authoring normalization and floor-local segment spatial index;
- implement the correctness/reference polygon solver for both vision and legal illumination while preserving distinct source handles/revisions;
- implement exact illumination, vision, combined-live, point/multi-sample, and batched queries with vision/illumination/bypass attribution;
- implement the hard formula “illumination-gated vision intersect compatible legal illumination, union illumination-bypass vision, subtract `SuppressLiveVision`”;
- implement a generic always-active attached circular test source as an illumination-bypass profile; the DARKWELL player mapping waits for M6;
- implement immutable revisions and event-driven dirty updates;
- add debug drawing for vision/illumination source shape, segments, endpoint rays/events, separate polygons, intersection/bypass result, floor, and query reason;
- add dynamic door segment updates;
- profile the reference solver before optimizing;
- add angular-sweep optimization only while preserving reference equivalence.

### Required tests before raster presentation

- long straight wall under lateral source movement and rotation;
- L/T corners, collinear/duplicate edges, narrow doorway, source on/near edge;
- closed, opening, rotating, and fully open door;
- static and dynamic geometry revision invalidation;
- floor separation and occluder height below/through/above eye band;
- two and eight vision sources plus matching/nonmatching illumination sources, including Adapter-simulated remote off-screen activation;
- legal illumination alone reveals and writes nothing;
- gated vision is rejected outside illumination and accepted inside the hard intersection;
- permanent body-circle coverage remains live without illumination but still stops at occlusion and `SuppressLiveVision`;
- illumination, live, and bounds/multi-sample queries at the exact hard boundary;
- deterministic results across repeated runs.

### Exit criteria

- all geometry tests pass;
- straight-wall polygon vertices remain on the authored wall line within the documented world-space epsilon;
- door transitions create the expected polygon opening without stale segments;
- gameplay and memory queries consume only the immutable hard combined result and expose both source attributions or the bypass reason;
- debug tools make every rejection/contribution explainable.

## M3 — World-space vision/illumination masks, neutral-gray memory, and modifiers

### Work

- choose atlas versus texture-array path using a focused UE 5.8.1 spike;
- rasterize illumination-gated vision polygons, legal-illumination polygons, and illumination-bypass polygons into separate stable world-space masks;
- derive the GPU effective-live mask as `(VisionMask INTERSECT IlluminationMask) UNION BypassVisionMask`, then apply live suppression, using the same immutable CPU revisions;
- add CPU packed memory tiles and dirty GPU mirrors;
- treat memory precision as a spike parameter and run identical 2.5 cm, 5 cm, 10 cm, and 25 cm configurations before selecting any default;
- implement hard memory writes independent of viewport;
- implement circle, box, room volume, polygon, floor, and height regions;
- implement `ClearMemory`, `BlockMemoryWrites`, `SuppressMemoryPresentation`, and `SuppressLiveVision` with deterministic overlap;
- add presentation-only feather/AA after hard rasterization;
- implement the decided neutral-gray immutable-environment composition using desaturated Base Color/stable material cues and stable neutral shading without current or frozen last-lit illumination;
- document supported/unsupported material domains;
- add mask/tile/modifier debug views and stats.

### Required tests

- hard CPU reference versus separate GPU vision, illumination, intersection, bypass, suppression, and effective-live masks within an approved edge tolerance;
- straight/diagonal wall capture under 1080p, 1440p, window resize, movement, and turn paths;
- memory written only by effective live coverage from an Adapter-activated off-screen remote source;
- illumination-only and vision-outside-illumination paths write no memory;
- body-circle bypass writes legal memory without illumination inside its own occluded polygon;
- no memory written across an occluder or by visual feather;
- clear then re-explore;
- blocker with live vision normal and no new memory;
- `BlockMemoryWrites` produces normal live vision and immediate black after live vision leaves, without writing new memory;
- memory suppression hide/restore;
- live suppression hiding hard subject queries as well as presentation;
- the strict-blackout composition later used by the monster Adapter clears existing memory/snapshots and blocks all new writes by combining `ClearMemory` with `BlockMemoryWrites`;
- overlapping modifiers and tile-border seams;
- static floor/wall/decor material detail visible in neutral gray, with no current or last-lit light/shadow leak;
- 2.5/5/10/25 cm comparison records fidelity/error, update time, upload bytes/time, runtime bytes, and compressed snapshot size.

### Exit criteria

- vision/illumination/intersection/bypass masks and hard queries agree at the same revision;
- unchanged straight walls meet the approved stability threshold;
- neutral-gray memory is materially richer than depth/normal outlines and contains no current or captured last-lit information;
- a memory precision is selected only from reviewed 2.5/5/10/25 cm evidence; until then it remains unset;
- all four modifier operations and shapes pass automated/runtime inspection;
- no full GPU readback is needed for memory or save.

## M4 — Subject policies and persistence

### Work

- implement `NeverRemember`, `VisibleOnly`, `StaticEnvironment`, `LastSeenSnapshot`, and `Custom` policies;
- implement DARKWELL's fixed-uncollected-item mapping to `LastSeenSnapshot` in the later Adapter, with generic lab coverage now;
- implement anchor/bounds/custom sample rules;
- implement live primitive/proxy transitions and a basic opaque static-mesh snapshot descriptor;
- implement host snapshot-provider interface for complex objects;
- ensure proxies are render-only and emit no collision/audio/VFX/live light;
- implement versioned deterministic plugin snapshot, tile compression, provider records, validation, and atomic restore;
- add clear/suppress/floor-stream behavior for snapshots/proxies;
- implement time-bounded Subject Reveal Override handles/results, reveal primitive policy, expiry/revocation, and callbacks as a lane separate from the knowledge-state machine and save schema.

### Required tests

- enemy/active monster never appears in memory and never exposes a HUD/interaction result outside hard live vision;
- generic visible-only transient subjects disappear outside live vision;
- fixed, uncollected item uses `LastSeenSnapshot` and restores the exact recorded proxy state outside live vision;
- door/container/machine proxy retains last-seen state while authoritative state changes off-screen;
- reacquisition swaps proxy to live state immediately;
- clear deletes proxy record; suppression hides then restores it;
- save round-trip is deterministic; corrupt/future/oversized/duplicate/missing-provider cases fail explicitly;
- restore recomputes live state and uploads derived GPU tiles without restoring stale live polygons;
- attacker-hit reveal may render only through an active Subject Reveal Override while the underlying query remains unchanged; it never enables normal HUD/interaction/targeting, never sets memory, never refreshes a last-seen record, leaves no remembered afterimage, and is not restored from save.

### Exit criteria

- subject policy and independent reveal-override matrices pass;
- no current enemy/dynamic state leaks through gray memory;
- save size/restore time are measured against the approved reference workload;
- unsupported remembered components are reported by validation instead of silently leaking.

## M5 — Fab-quality authoring, compatibility, and performance gate

### Work

- complete distinct vision-source/illumination-source, polygon/intersection/bypass, reveal-override, and single-active-floor Editor visualizers plus conversion preview, undo, and validation;
- create neutral example actors/maps and user documentation;
- verify clean-project installation and removal;
- verify UE 5.8.1 DX12/SM6, Editor, and packaged Development;
- test fallback mask path if the chosen texture-array/RDG path is not universal;
- collect performance on the approved minimum hardware and declared workloads;
- enforce visible warnings/counters for caps, solve latency, memory growth, and unsupported content;
- audit public headers/names for game leakage and future binary/API stability.

### Exit criteria

- a new project can configure the single active floor, occluders, vision sources, legal-illumination sources, illumination bypass, modifiers, subjects, reveal overrides, and save restore from documented steps;
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

Observe-only may compute separate vision/illumination polygons, hard intersections, bypass results, and stats but cannot change actor render state, UI, interaction, memory, reveal presentation, or save data. `Legacy` and `WorldVision` modes must restore all component/proxy/reveal/blendable state when switching during development; shipping should select at world start.

### DARKWELL adapters

- player facing/aim -> illumination-gated player vision source;
- permanent player-attached circle -> always-registered illumination-bypass vision source;
- approved DARKWELL legal lights -> explicit legal-illumination-source components/profiles, never an opaque generic filter or rendered-light sample;
- security camera -> camera vision source activated/deactivated by the Adapter;
- remote observation/lighting item -> explicit remote vision and/or illumination source activated/deactivated only by the Adapter;
- monster permanent black fog -> immediate `ClearMemory` plus active `BlockMemoryWrites`; other energy fields use only their explicitly selected generic modifiers;
- Stalker/Warden -> `NeverRemember` subject;
- fixed, uncollected items -> `LastSeenSnapshot`;
- truly transient/moving subjects -> explicit `VisibleOnly`/`NeverRemember` policy rather than inheriting the fixed-item rule;
- exit, door, container, machine -> snapshot provider/proxy;
- designated attacker subject receives the relevant hit/damage event -> time-bounded Subject Reveal Override, not a `Visible` state transition;
- HUD threat rows and interaction focus -> one WorldVision query/result path;
- legacy v6 explored cells -> no WorldVision conversion; start fresh unless the save already contains a valid WorldVision snapshot;
- plugin snapshot -> field embedded in the next approved DARKWELL save schema.

Niagara/audio reactions remain in DARKWELL event adapters and are not plugin dependencies.

### Integration order

1. Observe-only vision-source/illumination-source/occluder/single-floor mapping and hard query/intersection comparison.
2. Switch final post-process ownership on the integration map; legacy blendable off and separate vision/illumination/bypass masks active.
3. Route HUD/interactions to WorldVision's hard effective-live result while subjects remain visibly instrumented.
4. Route enemy/fixed-item/facility subject authority and attacker-hit reveal overrides; legacy callbacks/tick disabled.
5. Add DARKWELL snapshot providers.
6. Add save embedding with explicit no-v6-fog-migration behavior only after runtime state is stable.
7. Run full automation and PIE regression.

### Exit criteria

- all lab gates remain green;
- integration map passes straight walls, rooms/openings, dynamic door, height/single-active-floor, multiple vision/illumination sources, hard intersection, body-circle bypass, modifiers, subjects/reveal overrides, and save/load;
- exactly one camera fog blendable and one subject visibility authority are active;
- enemy world/HUD/interaction results agree with hard current vision; attacker-hit reveal remains a separately inspectable presentation exception and does not change those query results;
- legacy mode still behaves as its baseline and remains a one-setting rollback.

## M7 — DARKWELL acceptance on production content

Only after M6 approval should WorldVision be enabled for an acceptance build using `/Game/Maps/L_Prototype` or its approved successor. This is integration/tuning, not algorithm development.

### Required automation/build

- full `DarkwellEditor Win64 Development` build;
- all existing DARKWELL tests plus plugin tests;
- plugin save compatibility/schema tests plus explicit no-v6-fog-import coverage;
- packaged Development smoke if save/render behavior differs from Editor;
- recorded performance on current workstation and approved minimum target.

### Required PIE/manual regression

- movement/facing/sprint/aim while live edge remains stable;
- shooting/reload and torch/lantern profiles;
- enemy world and threat-HUD visibility under illumination-gated player vision, permanent body-circle bypass, camera, and Adapter-activated remote sources;
- legal-illumination polygons/queries and the CPU/GPU “vision mask intersect illumination mask” path;
- exploration, neutral-gray detail, leaving/re-entering sight;
- fixed uncollected-item `LastSeenSnapshot` behavior and attacker-hit Subject Reveal Override with no memory/HUD/interaction authority;
- every memory modifier behavior;
- dynamic door and fixed facility last-seen state;
- save/exit/load including plugin memory and snapshots;
- fuse/exit mission, death, escape, restart;
- menu/pause/viewport resize and floor transition;
- exactly one queried/presented floor before and after every floor transition;
- no error/ensure/assert/fatal logs.

### Acceptance evidence

Capture build/test logs, deterministic motion-path images/video for wall stability, separate vision/illumination/intersection/bypass query and mask captures, source/segment/subject/reveal counts, CPU/GPU timings, machine/RHI/resolution/build configuration, 2.5/5/10/25 cm comparison data, save size, and unsupported-content warnings.

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
- remove superseded game-specific fog-subject callback implementations while retaining the accepted WorldVision Adapter paths;
- remove legacy material/Python generation assets only through Unreal Editor/official asset APIs;
- do not add or retain a v6-fog-to-WorldVision converter; old v6 fog memory is intentionally not migrated, although unrelated host save fields may have their own compatibility policy;
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
- old saves follow the approved no-v6-fog-migration policy and begin with empty WorldVision memory unless they already contain a valid WorldVision snapshot;
- Git diff confirms only approved legacy removal and no v6 fog-memory converter;
- no orphaned asset references or duplicate visibility authority remains.

## Coexistence safety checklist

Before any integration session:

- assert exactly one subject authority mode;
- assert exactly one final fog composite has nonzero weight;
- assert only the selected system writes memory;
- assert effective WorldVision live coverage is the hard gated-vision/illumination intersection unioned with explicit bypass vision at one shared revision;
- assert HUD and interactions query the selected authority;
- assert Subject Reveal Overrides cannot set `Visible`, qualify HUD/interaction, or write/refresh memory;
- assert the non-selected system does not restore/save memory;
- reset previously hidden live primitives and destroy/hide stale memory proxies on mode change;
- log authority mode, snapshot revision, and save schema at world start;
- keep legacy and WorldVision save fields separately versioned and never convert v6 fog cells into WorldVision tiles;
- never use ordinary filesystem commands to copy/rename/delete `.uasset` or `.umap` files.

## Rollback plan

Until M8 begins, rollback is setting authority mode to `Legacy` and loading a save whose compatibility is documented. Plugin components may remain present but must unregister/disable all effects. A failed WorldVision restore must not partially replace valid legacy memory. There is no legacy-fog migration into WorldVision: selecting WorldVision with no valid WorldVision snapshot starts fresh, while selecting `Legacy` may continue to use its own separately versioned v6 fog payload.

If plugin performance or material support fails acceptance, keep the old system, retain lab evidence, and narrow/revise the plugin architecture without deleting production fog.

## Recommended next milestone

The immediate next milestone remains **M0: close or explicitly defer the remaining implementation gates, repair the local toolchain, and capture a reproducible legacy baseline**. The product decisions in this revision are recorded, but they do not authorize source or assets. After separate implementation approval and a successful 24-test discovery/run, proceed only to M1 (approved-name skeleton and independent lab map). Do not begin with DARKWELL integration, post-process replacement, v6 fog migration, or legacy deletion.

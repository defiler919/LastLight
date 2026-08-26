# Independent vision system migration plan

Status: migration plan revised with the latest human product decisions. SightWeave M1 is separately authorized; all later milestones still require separate implementation approval.

## Migration invariants

1. The existing DARKWELL fog remains the production prototype system until the independent plugin passes acceptance.
2. New algorithms are developed and tested in plugin-owned lab/example maps, not directly in `/Game/Maps/L_Prototype`.
3. No existing fog code, material, save field, or asset is deleted, renamed, or replaced during plugin prototyping.
4. The generic plugin contains no DARKWELL class names, gameplay tags, missions, items, enemy logic, Niagara, or audio.
5. DARKWELL integration is through project-owned adapters and an explicit single-authority switch.
6. Old and new systems never both control enemy/subject hiding, HUD visibility, interactions, memory writes, save restoration, or final post-process composition in one run.
7. Vision and legal illumination are separate explicit authorities. Old/new systems never infer legal illumination from Scene Color, rendered light brightness, or an ordinary `ULightComponent`.
8. Hard CPU queries and GPU presentation use the same normalized compatibility configurations, registered source data, and revision. Effective live coverage is the union of per-source compatible gated coverage, represented on GPU by matching complete-compatibility mask groups, plus explicitly bypassing vision; one global vision/illumination intersection is forbidden.
9. Subject Reveal Overrides are presentation-only and remain separate from `Visible`/`Remembered`/`Unknown`; they never write memory or last-seen snapshots.
10. V1 is single-player and has exactly one active queried/presented floor.
11. Dynamic door, straight wall, room/opening, height/floor, vision/illumination multi-source, modifier, subject-memory/reveal, and save tests pass before DARKWELL integration.
12. Legacy removal is a separate, explicitly approved milestone and separate commit after new-system acceptance.

## Branch and commit strategy

Current design branch: `design/independent-vision-plugin`.

This branch contains validation, audit, requirements, architecture, and migration documentation only. This decision revision must not create a plugin directory, C++/shader source, or Unreal asset. The current documentation commit is:

```text
docs: correct reveal trigger and illumination compatibility
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
- use the approved `SightWeave` product/API name for public C++ types, modules, plugin paths, and plugin assets;
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

- create `Plugins/SightWeave/` with Runtime, Editor, and Tests modules under the approved name;
- add neutral settings, distinct vision-source/illumination-source/reveal-override handle and result types, normalized complete illumination-compatibility configuration/key types, and a world subsystem shell;
- create a plugin-owned lab/example map through Unreal Editor APIs;
- add simple authored lanes for straight wall, diagonal wall, corner, room, opening, rotating door, height bands, stacked-floor rejection, visible-only and infrared-only compatibility, a source accepting both types, an always-active attached circular bypass source, and two explicitly activated remote sources;
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
- implement exact illumination, vision, combined-live, point/multi-sample, and batched queries with vision/illumination/compatibility/bypass attribution;
- implement the hard formula `GatedCoverage(s) = VisionPolygon(s) intersect Union(compatible IlluminationPolygon(l) for s)`, then union all `GatedCoverage(s)` and every bypass polygon before subtracting `SuppressLiveVision`;
- normalize each complete accepted-illumination set into a compatibility key and make any source/profile compatibility change publish one new revision for affected CPU mappings and GPU groups;
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
- visible/infrared isolation: overlapping Source A (visible only) and Source B (infrared only) under infrared-only coverage rejects A, accepts B, and preserves CPU attribution;
- multi-channel compatibility: Source C accepts visible plus infrared, either accepted type satisfies it, and an unrelated channel does not;
- legal illumination alone reveals and writes nothing;
- each gated source is rejected outside its own compatible illumination union and accepted inside its source-compatible gated coverage;
- permanent body-circle coverage remains live without illumination, is never assigned a compatibility key, and still stops at occlusion, floor/height rejection, and `SuppressLiveVision`;
- illumination, live, and bounds/multi-sample queries at the exact hard boundary;
- deterministic results across repeated runs.

### Exit criteria

- all geometry tests pass;
- straight-wall polygon vertices remain on the authored wall line within the documented world-space epsilon;
- door transitions create the expected polygon opening without stale segments;
- gameplay and memory queries consume only the immutable source-compatible hard result and expose both source attributions or the bypass reason;
- debug tools make every rejection/contribution explainable.

## M3 — World-space vision/illumination masks, neutral-gray memory, and modifiers

### Work

- implement the M3.0-selected sparse/tiled floor-local R8 atlas in a separate `SightWeaveRender` Runtime module, using transient compatibility-profile scratch and the immutable revisioned contract in `SIGHTWEAVE_M3_GPU_MASK_CONTRACT.md`;
- within each Knowledge Owner/floor scope, group only sources with semantically identical complete compatibility configurations `p`, rasterizing `VisionMask[p]` and `CompatibleIlluminationMask[p]` into paired stable world-space resources while keeping illumination-bypass polygons separate;
- derive `EffectiveLiveMask = Union over p (VisionMask[p] INTERSECT CompatibleIlluminationMask[p]) UNION BypassVisionMask`, then apply live suppression, using the same registration data and immutable revision as CPU authority;
- permit grouped textures, array layers, bitmasks, or multiple passes only when they preserve the complete compatibility relation; never merge incompatible channels into a single global pair;
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

- hard CPU per-source compatibility reference versus GPU profile-keyed vision/compatible-illumination intersections, bypass, suppression, and effective-live masks within an approved edge tolerance;
- visible/infrared isolation agrees on CPU and GPU: infrared-only coverage satisfies Source B but never visible-only Source A even where their vision polygons overlap;
- Source C's visible-plus-infrared configuration is satisfied by either accepted type and never by an unrelated channel, with CPU/GPU agreement;
- straight/diagonal wall capture under 1080p, 1440p, window resize, movement, and turn paths;
- memory written only by effective live coverage from an Adapter-activated off-screen remote source;
- illumination-only and vision-outside-illumination paths write no memory;
- body-circle bypass writes legal memory without illumination inside its own occluded polygon and never enters a compatibility mask group;
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

- every CPU per-source compatible result agrees with its GPU complete-compatibility group and the final bypass union at the same revision;
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
- implement the neutral `ApplySubjectRevealOverride(KnowledgeOwner, Subject, RevealSpecification)` semantic, time-bounded handles/results, moving-subject reveal primitives, expiry/revocation, and callbacks as a lane separate from damage systems, the knowledge-state machine, legal illumination, live masks, and the save schema.

### Required tests

- enemy/active monster never appears in memory and never exposes a HUD/interaction result outside hard live vision;
- generic visible-only transient subjects disappear outside live vision;
- fixed, uncollected item uses `LastSeenSnapshot` and restores the exact recorded proxy state outside live vision;
- door/container/machine proxy retains last-seen state while authoritative state changes off-screen;
- reacquisition swaps proxy to live state immediately;
- clear deletes proxy record; suppression hides then restores it;
- save round-trip is deterministic; corrupt/future/oversized/duplicate/missing-provider cases fail explicitly;
- restore recomputes live state and uploads derived GPU tiles without restoring stale live polygons;
- damage-source direction: with Enemy A outside ordinary live vision, Enemy A causes a qualifying attack/damage event on the player Knowledge Owner; the Adapter resolves A from the event and applies the override to A, whose approved reveal follows A temporarily;
- reverse-direction and isolation: the player shooting Enemy A does not trigger this rule by itself; nearby Enemy B and surrounding environment remain unrevealed and unilluminated; ordinary queries/HUD/interaction/targeting remain unchanged;
- persistence isolation: damage-source reveal sets no memory, refreshes no last-seen record, leaves no remembered afterimage, and is not restored from save;
- policy ownership: Adapter tests decide whether armor-absorbed, blocked, zero-final-damage, or similar attack events qualify; the generic plugin has no DARKWELL damage dependency.

### Exit criteria

- subject policy and independent reveal-override matrices pass;
- no current enemy/dynamic state leaks through gray memory;
- save size/restore time are measured against the approved reference workload;
- unsupported remembered components are reported by validation instead of silently leaking.

## M5 — Fab-quality authoring, compatibility, and performance gate

### Work

- complete distinct vision-source/illumination-source, per-compatibility-group intersection, bypass, reveal-override, and single-active-floor Editor visualizers plus conversion preview, undo, and validation;
- create neutral example actors/maps and user documentation;
- verify clean-project installation and removal;
- verify UE 5.8.1 DX12/SM6, Editor, and packaged Development;
- test the selected RDG external-pooled atlas path on declared RHIs and fail closed where required capabilities are unavailable; texture arrays/clipmaps remain separately approved fallbacks, never silent authority changes;
- collect performance on the approved minimum hardware and declared workloads;
- enforce visible warnings/counters for caps, solve latency, memory growth, and unsupported content;
- audit public headers/names for game leakage and future binary/API stability.

### Exit criteria

- a new project can configure the single active floor, occluders, vision sources, legal-illumination sources, complete accepted-illumination profiles, illumination bypass, modifiers, subjects, reveal overrides, and save restore from documented steps;
- no DARKWELL reference exists in plugin Runtime/Editor/Tests/content;
- all functional, render, persistence, lifecycle, and performance suites pass;
- performance budgets are met or revised with user approval and evidence;
- example maps demonstrate every advertised feature and limitation.

## M6 — DARKWELL adapter integration without authority overlap

### Integration map

Create a dedicated DARKWELL integration map through Unreal Editor asset APIs, for example `/Game/Maps/L_VisionIntegration`. It may be based on approved prototype geometry but must not replace or mutate `/Game/Maps/L_Prototype` while algorithms are still being tuned.

### Authority modes

Use one project-owned setting with mutually exclusive modes:

| Mode | Legacy computes/presents/hides/writes | SightWeave computes/presents/hides/writes | Purpose |
| --- | --- | --- | --- |
| `Legacy` | Yes | No | production rollback/baseline |
| `SightWeaveObserveOnly` | Yes | Debug computation only; no masks, subject callbacks, memory, HUD, save | compare queries safely without dual control |
| `SightWeave` | No | Yes | integration and acceptance |

Observe-only may compute separate vision/illumination polygons, per-source compatibility coverage, profile-keyed GPU-equivalent intersections, bypass results, and stats but cannot change actor render state, UI, interaction, memory, reveal presentation, or save data. `Legacy` and `SightWeave` modes must restore all component/proxy/reveal/blendable state when switching during development; shipping should select at world start.

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
- Knowledge Owner receives a qualifying attack/damage event -> Adapter resolves the attack source/Instigator -> apply a time-bounded Subject Reveal Override to that attacker Subject, not a `Visible` state transition;
- HUD threat rows and interaction focus -> one SightWeave query/result path;
- legacy v6 explored cells -> no SightWeave conversion; start fresh unless the save already contains a valid SightWeave snapshot;
- plugin snapshot -> field embedded in the next approved DARKWELL save schema.

Niagara/audio reactions remain in DARKWELL event adapters and are not plugin dependencies.

### Integration order

1. Observe-only vision-source/illumination-source/complete-compatibility/occluder/single-floor mapping and source-compatible hard-query comparison.
2. Switch final post-process ownership on the integration map; legacy blendable off and profile-keyed vision/compatible-illumination mask pairs plus bypass mask active.
3. Route HUD/interactions to SightWeave's hard effective-live result while subjects remain visibly instrumented.
4. Route enemy/fixed-item/facility subject authority and attacker-on-owner-hit damage-source reveal overrides; legacy callbacks/tick disabled.
5. Add DARKWELL snapshot providers.
6. Add save embedding with explicit no-v6-fog-migration behavior only after runtime state is stable.
7. Run full automation and PIE regression.

### Exit criteria

- all lab gates remain green;
- integration map passes straight walls, rooms/openings, dynamic door, height/single-active-floor, multiple vision/illumination sources, visible/infrared isolation, multi-channel compatibility, body-circle bypass, modifiers, subjects/reveal overrides, and save/load;
- exactly one camera fog blendable and one subject visibility authority are active;
- enemy world/HUD/interaction results agree with source-compatible hard current vision; damage-source reveal remains a separately inspectable presentation exception applied only to the attacker after the Knowledge Owner is attacked and does not change those query results;
- legacy mode still behaves as its baseline and remains a one-setting rollback.

## M7 — DARKWELL acceptance on production content

Only after M6 approval should SightWeave be enabled for an acceptance build using `/Game/Maps/L_Prototype` or its approved successor. This is integration/tuning, not algorithm development.

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
- legal-illumination polygons/queries and CPU/GPU agreement for per-source compatibility represented by matching complete-compatibility mask groups;
- exploration, neutral-gray detail, leaving/re-entering sight;
- fixed uncollected-item `LastSeenSnapshot` behavior and attacker-on-owner-hit damage-source Subject Reveal Override with correct direction, no nearby-subject/environment reveal, and no memory/HUD/interaction authority;
- every memory modifier behavior;
- dynamic door and fixed facility last-seen state;
- save/exit/load including plugin memory and snapshots;
- fuse/exit mission, death, escape, restart;
- menu/pause/viewport resize and floor transition;
- exactly one queried/presented floor before and after every floor transition;
- no error/ensure/assert/fatal logs.

### Acceptance evidence

Capture build/test logs, deterministic motion-path images/video for wall stability, per-source CPU compatibility and profile-keyed GPU intersection/bypass captures, visible/infrared isolation evidence, source/segment/subject/reveal counts, CPU/GPU timings, machine/RHI/resolution/build configuration, 2.5/5/10/25 cm comparison data, save size, and unsupported-content warnings.

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
- remove superseded game-specific fog-subject callback implementations while retaining the accepted SightWeave Adapter paths;
- remove legacy material/Python generation assets only through Unreal Editor/official asset APIs;
- do not add or retain a v6-fog-to-SightWeave converter; old v6 fog memory is intentionally not migrated, although unrelated host save fields may have their own compatibility policy;
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
- old saves follow the approved no-v6-fog-migration policy and begin with empty SightWeave memory unless they already contain a valid SightWeave snapshot;
- Git diff confirms only approved legacy removal and no v6 fog-memory converter;
- no orphaned asset references or duplicate visibility authority remains.

## Coexistence safety checklist

Before any integration session:

- assert exactly one subject authority mode;
- assert exactly one final fog composite has nonzero weight;
- assert only the selected system writes memory;
- assert effective SightWeave live coverage is the union of per-source compatible gated coverage plus explicit bypass vision, and that CPU mappings and GPU complete-compatibility groups share one revision;
- assert HUD and interactions query the selected authority;
- assert Subject Reveal Overrides cannot contribute Legal Illumination or Live Vision, set `Visible`, qualify HUD/interaction, reveal neighboring subjects/environment, or write/refresh memory;
- assert the non-selected system does not restore/save memory;
- reset previously hidden live primitives and destroy/hide stale memory proxies on mode change;
- log authority mode, snapshot revision, and save schema at world start;
- keep legacy and SightWeave save fields separately versioned and never convert v6 fog cells into SightWeave tiles;
- never use ordinary filesystem commands to copy/rename/delete `.uasset` or `.umap` files.

## Rollback plan

Until M8 begins, rollback is setting authority mode to `Legacy` and loading a save whose compatibility is documented. Plugin components may remain present but must unregister/disable all effects. A failed SightWeave restore must not partially replace valid legacy memory. There is no legacy-fog migration into SightWeave: selecting SightWeave with no valid SightWeave snapshot starts fresh, while selecting `Legacy` may continue to use its own separately versioned v6 fog payload.

If plugin performance or material support fails acceptance, keep the old system, retain lab evidence, and narrow/revise the plugin architecture without deleting production fog.

## Recommended next milestone

The immediate next milestone remains **M0: close or explicitly defer the remaining implementation gates, repair the local toolchain, and capture a reproducible legacy baseline**. The product decisions in this revision are recorded, but they do not authorize source or assets. After separate implementation approval and a successful 24-test discovery/run, proceed only to M1 (approved-name skeleton and independent lab map). Do not begin with DARKWELL integration, post-process replacement, v6 fog migration, or legacy deletion.

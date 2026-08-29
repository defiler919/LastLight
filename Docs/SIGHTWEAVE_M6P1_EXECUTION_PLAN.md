# SightWeave M6P1 execution plan

Status: **ACTIVE**

Branch: `codex/m6p1-sightweave-darkwell-adapter-vertical-slice`

Starting audit SHA: `25eb813a87cacd7dadb34f1b234c582df1467964`

## 1. Scope controls

All source work stays under `Source/Darkwell`, `Source/Darkwell/Darkwell.Build.cs`, and the five M6P1 documents. The only binary change is the Unreal-API-created `Content/Maps/L_VisionIntegration.umap`. Before every reliable checkpoint, inspect `git status`, `git diff --check`, `Darkwell.uproject`, `Plugins/SightWeave`, `Content/Maps/L_Prototype.umap`, generated directories, and Git LFS state.

Builds, automation, Editor, UBT, and UAT runs are serialized. After an abnormal exit, inspect the process list and relevant log before retrying. Generated logs, reports, screenshots, Cook output, Binaries, Intermediate, Saved, and DDC remain ignored and uncommitted.

## 2. Checkpoint A — contract

- freeze authority, Adapter, lifecycle, source/light, subject/HUD, map, verification, rollback, and non-goal contracts;
- verify exactly the two required new documents;
- run `git diff --check` and prohibited-path checks;
- commit `docs: define SightWeave M6P1 integration contract` and push the new branch.

## 3. Checkpoint B — module and authority foundation

- add private Runtime/non-Server Render module dependencies;
- add neutral DARKWELL authority/snapshot/diagnostic value types;
- add the project World Subsystem with Legacy default, stable IDs, transactional activation, teardown, and duplicate-authority diagnostics;
- add the native integration GameMode/fixture declarations needed to select the map-local mode without changing global defaults;
- add focused authority and lifecycle automation, including two worlds, teardown/restart, failure rollback, no duplicate handles, and Legacy isolation;
- run the standard Editor Development build, isolated tests, diff checks, commit `feat: add DARKWELL SightWeave adapter authority`, and push.

## 4. Checkpoint C — player, body circle, cone, torch, and static fixture

- register the 120 cm bypass body circle and 2200 cm 52-to-35-degree gated cone;
- update transform/facing without rebuilding unchanged descriptions;
- register semantic torch legal illumination at 1250 cm from existing Loadout state, never rendered light state;
- register explicit fixture wall segments and immutable ground/landmark footprints;
- configure Coarse exploration memory and Width=50 presentation scope;
- cover movement, facing, no-change, unregistration, respawn, torch active/off/depleted/restored, body independence, cone/light intersection, wall occlusion, black/live/gray/clear/HardMemory, and scope isolation;
- build, run focused NullRHI tests, diff-check, commit `feat: connect DARKWELL player and torch vision`, and push.

## 5. Checkpoint D — Stalker and HUD

- register the real base Stalker with stable ID and `NeverRemember`;
- query once per Adapter update, store one neutral revisioned snapshot, and apply it to approved Stalker rendering;
- route the existing threat row through that same stored snapshot;
- disable legacy Stalker callbacks and legacy HUD fog/threat authority only in SightWeave mode;
- cover body/cone/light/occlusion visibility, hidden after loss, no Last-Seen/gray proxy, no attack reveal, same actor/HUD revision, no one-frame leak, and unchanged Legacy behavior;
- build, run focused tests, diff-check, commit `feat: synchronize Stalker visibility and threat HUD`, and push.

## 6. Checkpoint E — integration map

- build the native classes first;
- use Unreal Editor Python/API to create a fresh non-World-Partition map, set the native integration GameMode, and add exactly one PlayerStart, one native integration fixture, and one real base Stalker;
- save only `/Game/Maps/L_VisionIntegration`;
- close/reopen through Editor/API and inspect class/content counts, missing references, World Settings, external actors, and asset registry state;
- run LFS attribute/status checks and NullRHI map load/PIE lifecycle automation;
- commit the map as its own reliable checkpoint with `test: add DARKWELL SightWeave integration map` and push.

## 7. Automated validation order

Run serially and record exact discovered/performed/succeeded/warning/failed counts:

1. Editor Development initial/final standard build;
2. isolated Authority tests;
3. isolated Adapter lifecycle tests;
4. player/body/cone tests;
5. torch/occlusion tests;
6. Stalker/HUD consistency tests;
7. map create/save/reopen structural validation;
8. Integration Map NullRHI load and repeated teardown;
9. M6P1 real D3D12/SM6 View automation;
10. M6P1 full NullRHI;
11. M6P1 full D3D12/SM6;
12. affected M3.4, M3.5, M4P1, and M4P3 regressions;
13. full SightWeave NullRHI and D3D12/SM6 because project integration changes the loaded consumer configuration, even though the plugin is frozen;
14. existing DARKWELL 24 and all new DARKWELL integration tests;
15. `L_Prototype` Legacy map regression;
16. Game Development build;
17. Game Shipping build;
18. independent `L_VisionIntegration` Cook;
19. Runtime/Shipping dependency, forbidden-string/import, absolute-path, SceneCapture/test command, and direct-plugin-internal-consumer scans;
20. authoritative severe-log scan.

BuildPlugin and clean-host plugin packaging are omitted only if the final diff proves the SightWeave plugin source/config/descriptor/shaders remain unchanged.

## 8. D3D12/SM6 formal View evidence

Use the real GameViewport/player camera, TAA, normal composition, Coarse memory, and Width=50. SceneCapture is not evidence. Capture stable states for:

1. torch unavailable: body circle only;
2. torch active: gated facing cone;
3. wall occlusion and strict-black unobserved space;
4. explored immutable fixture after turning away: neutral gray;
5. Stalker live with threat row;
6. Stalker hidden with threat row absent;
7. gray static memory with no Stalker silhouette;
8. one clean SightWeave edge with no legacy double composite.

For each capture record image size, adapter/RHI/SM6, TAA/feather state, authority mode, Runtime/DARKWELL revision, player/Stalker position, torch state, ROI pixel/nonfinite counts, expected result, and actual result. The agent opens every retained screenshot and records direct inspection separately from pixel assertions. Screenshots remain untracked.

## 9. Cook and Shipping closure

- build `Darkwell Win64 Development` and `Darkwell Win64 Shipping`;
- Cook only `/Game/Maps/L_VisionIntegration` through UAT/official Unreal tooling;
- prove Runtime has no `SightWeaveEditor`, `SightWeaveTests`, `UnrealEd`, test/readback/benchmark symbols, repository absolute path, Lab command, screenshot command, or Adapter bypass into SightWeave private/render internals;
- prove the Cook resolves the native GameMode, player, fixture, Stalker, engine meshes, and map package without missing class/resource errors.

## 10. Documentation and handoff

Add and push:

- `Docs/SIGHTWEAVE_M6P1_EXECUTION_REPORT.md`;
- `Docs/SIGHTWEAVE_M6P1_FINAL_VALIDATION.md`;
- `Docs/SIGHTWEAVE_M6P1_HANDOFF.md`.

Record actual symbols, values, revisions, exact counts, every retained warning/failure, logs/reports/screenshots, direct image inspection, builds, Cook, scans, prohibited-path proof, commits, and Git/LFS closure. If every automated/Agent gate passes but the user has not run PIE, finish as `PARTIAL — READY_FOR_USER_PIE` with an accurate 3–5 minute control-based checklist. Only a later explicit user pass can change the milestone to `COMPLETED`.

## 11. Stop conditions

Stop and preserve reliable pushed work as `PARTIAL` or `BLOCKED` if single authority cannot be maintained, a required capability is absent from the current public SightWeave API, the only fix requires plugin/L_Prototype/Darkwell.uproject/another binary asset changes, map save/reopen is corrupt, or required D3D12/Cook/Shipping gates cannot be made reliable without weakening a frozen contract. Do not begin SaveGame, damage reveal, lantern, Warden, production-map acceptance, broader consumers/providers, or legacy deletion.


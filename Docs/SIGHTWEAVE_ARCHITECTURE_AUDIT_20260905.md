# Gray observation / memory architecture audit — 2026-09-05

Status: investigation in progress; no new stable or user acceptance.

## Provenance and authority

Home checkout `D:\UE_pro\Darkwell`, branch
`codex/darkwell-prop-memory-gameplay-lab`, baseline `6772074a0081a96a09dbf2ffd961f5a9cab1488d`.
Fetch found no newer commits; clean worktree, empty local stash list, LFS fsck OK.
Remote protected refs: gray core `7534163b9c5718700b610e7677f47fbaa79cf977`,
moving v2 `404a5820739638f1097eaae0aa7fba19733298c3`.
Installed engine is 5.8.2 CL 56702186 at the prescribed `D:\UE_5.8`;
no engine upgrade. Baseline formal Editor build succeeded (up to date).
Old company Saved artifacts are not new local evidence.

## Actual call chain and ownership

1. `UDarkwellSightWeaveWorldSubsystem::Tick` updates SightWeave sources,
   occluders and CPU authority, then publishes a matching source snapshot to
   `UDarkwellFogVisualSubsystem`. World exploration is separate from object reveal.
2. The latter's revision-matched `QueryLiveCoverageAtWorldPoint` and conservative
   footprint tests provide cone/range/wall coverage. `QueryObjectOcclusionAtWorldPoint`
   is the separate physical-wall-only presentation gate used by confirmed Whole.
3. V2 Director Tick calls `ADarkwellMovingPropLabRoom::UpdateRoom` (legacy Lab
   supplies the other entry). It snapshots actual occupancy, selects historical
   candidates, and calls `UpdateTracked` for every registered fixture.
4. Policy resolves per registration in the plugin. Native local span confirmation
   consumes legal samples; Whole permission does not write world coverage.
5. `CurrentLiveGrid` keeps per-primitive local discovery and appearance, derives
   world rasters and submits source textures. `SpatialObservationHistory` owns an
   unsealed current record. This is a duplicated current representation.
6. On valid view loss, StationaryOnly freezes a new record: coarse SpatialMemory,
   binary capture, fine HistoryGrid, frozen AA envelope, revision stamps, plus
   proxy / cap / texture / exclusion caches. Whole freezes full geometry.
7. Historical fine samples evaluate real legal empty coverage and occupancy,
   then ownership by newer observed geometry. Coarse memory ALSO advances its own
   empty/dwell state. Fine and coarse empty rules are different.
8. Surface output combines frozen RGB and a hard ownership alpha. Cap generation
   uses per-record fine state and clips against newer geometry. Spatial sleeping
   limits candidates, but active epochs repeat geometry/ownership/presentation work.
9. Presentation retirement releases UObjects; facts remain until all initially
   remembered samples are VerifiedEmpty. Superseded is terminal and never becomes
   empty. Therefore obsolete operation records can remain forever.

## Confirmed structural findings (source evidence)

- Every StationaryOnly reacquisition after valid loss allocates another epoch,
  even when pose, mesh, appearance and knowledge are unchanged. Epoch is both an
  observation session and a historical state version; those are different facts.
- `FineHistory::Initialize` freezes each session's inward AA envelope. Independent
  envelope boundaries are subsequently hard-gated by newer ownership; no path
  recomputes a common known/unknown boundary across compatible sessions.
  Whether this produces the reported Room 02 lines remains a rendered hypothesis
  until the replay and layer isolation finish.
- Current resets Partial local discovery when epoch changes. Previous discovery
  is not seeded into the next session's appearance. A known surface may thus
  participate in a second independent reveal transition.
- Geometry/appearance snapshots are incomplete. `SpawnMemoryProxy` reads the
  *current* actor's meshes and relative transforms; `BindProxyMaterial` reads the
  tracked current tint. Records contain no complete immutable appearance identity.
  The unrelated older component's appearance hash mixes world pose with material
  pointer identity; MID parameter changes are not a durable appearance revision.
- The mature path is owned by a 6,500-line Lab actor, takes LabFurniture, and
  depends on project Lab materials. Ordinary RememberableProp components go
  through the older maximum-coverage snapshot subsystem, not this path. SightWeave
  provides generic authority/policy, not a portable gray renderer.
- Ownership retirement is presentation, not empty knowledge. The old requirement
  to keep every Superseded record is an implementation constraint, not the user's
  product requirement. Safe compression may retain knowledge without its old
  record or rendering resources; it must not invent VerifiedEmpty.
- Existing V2 room tests mostly check map metadata, labels and ID lists. The GPU
  tour observes Room 02 once. Ownership tests check no double contributor; zero
  contributors / dim internal seams are a different failure. Force-full parity
  checks optimization equivalence, not an independent product oracle.

## Product constraints vs changeable implementation

Preserve per-object Whole/Partial and Always/StationaryOnly/Never, world-cm span,
wall occlusion, observation-only knowledge, hidden-change uncertainty and valid
counterevidence. Current must remain independent of historical residency. Keep
normal quality and legal external caps. No black-layer implementation this task.

64 records, epoch allocation per view loss, frozen AA, the fine/coarse duplication,
Lab ownership and frozen ownership-frontier records are changeable implementation
choices. The former handoff's claim that performance needs a *product retention*
decision is superseded: the user permits lossless representation changes.

## Candidate direction and experimental gate

Prefer one effective knowledge boundary per compatible observed state (object
incarnation + geometry/appearance + rigid pose), with visibility session and
appearance animation kept separate. New observation may extend knowledge;
repeated operations must not append equivalent state. Empty evidence and replacement
facts must survive rebuilding; never union raw old capture masks after erasure.
Surface AA and caps must derive from this effective boundary. Keep differing
poses/appearance and unseen old positions independently verifiable.

Reject cosmetic cap offsets/blur, identity eviction, unlimited epoch storage plus
more caches, and an unconditional rewrite of SightWeave's functioning authority.
Before selecting a migration, reproduce Room 02, inspect cap-hidden frames and
numeric surface gaps, and test stationary repetition, Whole transitions, hidden
motion and genuine growing history independently. Ordinary-host integration and
plugin compilation are separate acceptance requirements.

## Evidence / next entry

`Saved/ArchitectureAudit_BaselineBuild.log`: formal baseline build success.
`Content/Python/audit_gray_memory_episodes.py`: deterministic real Room 02 replay;
`GetMemorySeamAuditForTesting`: on-demand raw center-row surface and vertical caps.
Neither injects visibility or historical knowledge. Diagnostic builds/runs below
must be completed before claiming the rendered root cause or a fix.

### A: reproduced surface seams, 02:24–02:32 UTC

`Saved/ArchitectureAudit/Room02_Baseline` completed 51 frames, but Editor exit
`0xC0000005` after shutdown: failed teardown, not a pass. A second run
`Room02_Baseline2` adds explicit post-PIE GC and exits **0**. Its complete 51-frame
D3D12/SM6 replay is preserved. Original images opened for review show **three
internal lines** after a full observation followed by 52°, 40°, 28° subset views.
Hiding every cap leaves all three lines visible; restoring caps leaves the same
lines. The surface path independently creates the reported artifact.

The center-row submitted shader values have 9 deficient samples around world
X=-210, -27.5 and 152.5 cm: each boundary has two zero samples and one 0.5 sample
inside an already fully observed solid object. Older ownership is suppressed;
the new session's frozen inward-AA boundary still fades to zero. Thus cap removal,
TSR changes or the previous Whole uniform-blend fix cannot repair this Partial
history defect. Caps can additionally exist at these false per-session boundaries.
Reentry continuous frames also show known surfaces restarting the reveal dot pattern.
Eight view cycles retain eight records without geometry/appearance changes.

New independent analytic cuboid oracle `StaticPartialEpisodes` fails on the old
runtime: **36 deficient interior probes, 1,566 internal cap fragments, 1→7 records**
after repeated subset views. `Audit_StaticPartial_Before2`: 0/1, process 0,
test 4.896 s / wall 24.419 s, severe 0. This fails *product* continuity and growth,
not a comparison against the old implementation. The earlier Before run had no
matching test (failed build) and is not a baseline pass. OracleBuild initially
failed because a concurrently edited header was stale in that compiler action;
OracleBuild2 succeeded on the completed source (11.99 s). Future builds run only
after source edits finish.

The first small implementation tests reentry into the same uncontradicted observed
state, keeping accumulated local discovery and resetting only the live lighting
blend. It explicitly rejects changed pose/content, motion and old counterevidence.
This is an experiment, not yet the general state compaction or module migration.

### B: narrow reuse experiment

Formal build `ArchitectureAudit_ReuseTrialBuild2.log` succeeded, 15.99 s (the first
trial build failed on `auto*` deduction from TObjectPtr; corrected explicitly).
`Audit_StaticPartial_Trial` passes **1/1**, 1.647 s test / 21.092 s wall, process
0, severe 0. The same analytic interior/cap/growth assertions now all pass.
GPU replay and wider semantics still pending at this checkpoint.

`Room02_ReuseTrial` now completed the identical 51-frame rendered sequence. Original
final/history and transition images opened for review: three internal seams gone;
known portions stay solid on reentry while newly discovered portions reveal.
Independent recorded-output oracle: all 896 interior probes are solid in each of
six post-full-observation samples, retained state count stays **1** through eight
episodes. This validates the narrow direction. Its Editor shutdown again returned
`0xC0000005` after log close despite PIE stop and explicit GC; teardown remains
an independent blocker, not disguised by the successful surface oracle.

Representative regression `Audit_Reuse_Representative`: 68 total, 65 clean,
1 warning, 2 failed; 41.420 s tests / 64.722 s wall, process 0, severe 0.
All policy, moving-continuity and Whole behavior checks pass. Both failures demand
four or more *duplicate same-pose epochs* (and consequently cross-record cache
hits). Their full-scan parity assertions already pass. Those structural expectations
are migrated to bounded retained knowledge; the full evidence comparisons remain.
`StaticPartialEpisodes` supplies the new independent product oracle. No failed
evidence is removed. No runtime path has yet been removed by this trial.

After migrating those structural assertions, `Audit_Reuse_Representative2` passes
**69/69** (67 clean, 2 warnings), 43.138 s tests / 62.989 s wall, process 0,
severe 0. Standard Editor build succeeded (7.30 s). This is checkpoint B: a
reviewable seam/duplicate-session fix with a passing representative semantic
suite, not completion of the broader architecture/performance/teardown work.
Next entry: immutable observed content, reusable presentation, generic scene
ownership, then corrected performance and long-run validation.

### Performance baseline, separate from the seam oracle

`HomeAudit_Before_20260905` completed all eleven legacy D3D12 short cases and
produced a 214 MB trace. Editor subsequently exited `0xC0000005`: metrics are
completed evidence, teardown is failed. `binary_provenance.txt` clarifies that
the running binary precedes the unbuilt trial changes captured by source.patch.

| Legacy case | frame p50/p95/p99/max ms | system p50/p95/p99/max ms |
| --- | --- | --- |
| Empty turning | 38.30/40.54/42.12/42.73 | .167/.203/.227/.262 |
| One Whole | 38.26/40.62/41.14/42.03 | .477/4.957/5.142/5.543 |
| Eight Whole | 39.11/41.74/43.13/43.54 | 5.519/22.558/24.615/25.350 |
| 32 Whole | 44.88/111.63/124.14/161.55 | 28.729/93.402/105.432/123.248 |
| Room 02 Partial | 40.74/95.80/101.20/105.15 | 14.427/82.823/87.154/90.692 |
| Overlap64 | 38.80/41.67/43.74/46.51 | 4.470/25.181/30.118/31.893 |
| Same-ID64 | 38.84/40.94/41.73/42.95 | 6.092/12.485/13.379/13.458 |
| Distributed184 | 38.38/41.04/42.17/42.60 | 5.762/12.244/13.937/14.447 |
| Distributed repeated 1000 | 38.57/40.82/41.82/48.12 | 6.041/12.168/13.280/14.009 |

These retain the OLD measurement scope: game-clock frame deltas, 90 frames
excluded before sampling. The two one-shot sweep cases only measure settled idle
because the sweep finishes during that warmup; their ~.2 ms system p95 is **not**
fast-turn evidence. Batch publication spikes are likewise omitted by this table.
Corrected measurement must include setup/first update and wall-clock intervals,
while retaining this original comparison. Empty-room full-frame time already
fails 16.6 ms although system cost is .2 ms: whole-frame and gray-system costs
cannot be conflated. No performance threshold is changed.

## Checkpoint C — observed content is independent of the source

- Historical records now capture mesh references, actor-relative primitive transforms and bounds, gray tint/UV scale, and a content revision only at legal observation. Proxy reconstruction reads that capture, including after source destruction. World pose is separate from authored content; presenter texture updates do not advance the content revision.
- A newly observed content change seals eligible old content instead of rewriting it. Hidden content changes cannot update the capture. `NotifyMemoryAppearanceChanged` marks authored parameter changes; the current gray material contract stores tint/UV, not an arbitrary material graph or every MID parameter.
- Inspection caught an accidental recursive call in the first uncommitted draft (compilation alone could not detect the behavioral failure). It was removed before running this checkpoint's tests. Relative transforms now compose the attachment chain to the actor root, so root-mesh and nested-component hosts do not mix actor motion into content.
- Formal build: `Scripts/BuildEditor.ps1`, `Saved/ArchitectureAudit_ContentBuild2.log`, succeeded, 28.31 s.
- `RunGrayObjectPolicyTests.ps1 -RunName Audit_Content_Contract -Tests 'Darkwell.PropLab.ArchitectureAudit+Darkwell.PropLab.GrayObjectPolicy+Darkwell.PropLab.MovingLiveContinuity+SightWeave.ObjectPolicy+SightWeave.RevealPolicy'`: 70/70, 69 clean + 1 warning, zero severe lines, process 0, 43.356 s test / 66.866 s wall.
- New independent assertions exercise both Whole and Partial captures, hidden mesh/scale/tint changes, pose/content separation, source destruction, and exact old mesh/transform/tint/UV reconstruction. They do not use current source content as the expected answer.
- Remaining: Live resource ownership still follows epochs; Lab/runtime extraction, terminal-state residency, ordinary host, corrected performance measurements and graphical teardown remain open.

## Checkpoint D — current resources belong to the source

- Live texture/pixel/signature storage moved from `FRecordVisual` into per-source `FCurrentPresentation`. Entering/leaving an unchanged view reuses the same concrete UTexture2D objects. Geometry/size changes and explicit unregister/reset release incompatible allocations.
- Removed the redundant current world-atlas texture and upload. Original meshes already bind per-part rasters; historical records still own their immutable world raster and cap representation.
- Exact uniform Whole fields use one texel. Partial uploads contain the active raster plus a clamp border, instead of uploading unused square-atlas padding. The actual spatial sample density, bilinear edge, render resolution and TSR are unchanged.
- `Audit_CurrentResources` retained as failed evidence: three moving suites locked the previous four-texture implementation, and thousands of repeated failure log events also coincided with a working-set gate failure. Migrated count assertions to compare with the actual pre-motion allocation count; the 64 MiB memory gate remains unchanged.
- Full build `ArchitectureAudit_CurrentResourcesBuild2.log` succeeded, 19.49 s. `Audit_CurrentResources2`: 70/70, 69 clean + 1 warning, process 0, severe 0, 35.460 s tests / 56.025 s wall; the original working-set gate now passes. This suite timing is not a formal performance comparison.
- D3D12/SM6 `Saved/ArchitectureAudit/Room02_CurrentResources`: same continuous 51-frame protocol; independent surface oracle passes all six post-full snapshots (896 interior probes each). Inspected final original screenshot and first reentry frame: seamless final surface; only previously unknown strip dots. One state and persistent live allocations after repeated views.
- Graphical process again returned 0xC0000005 after `Log file closed`, despite completed PIE stop and protocol; normal-exit acceptance remains FAILED. Do not count screenshot collection as teardown success.
- Next: remove Lab ownership of production runtime, add ordinary-host integration, then compare corrected transition-inclusive timings and existing strict gates.

## Checkpoint E — ordinary runtime host, Lab as fixture

- Extracted the gray lifecycle, captured content, geometry/occlusion queries, current/history presentation, caps and diagnostics into `ADarkwellObjectMemoryScene`. Sources are ordinary Actors with rememberable primitives and per-object SightWeave policy components. The Lab derives from this scene; only fixture construction, authored motion, interaction and evidence control remain there. The source implementation contains no map/StableID allowlist, Lab Actor dependency or global policy switch.
- Added `Fog.ActivateForWorld` using the same analytic source/segment coverage and render target without an integration fixture. Invalid publications now invalidate query evidence instead of silently leaving the previous valid source available as evidence. Host ordering is explicit: physical transforms, coverage publication, memory update.
- `bUseSpatialMemory` opts a component out of the legacy remembered-prop presenter. Unrelated legacy gameplay keeps its path. Original source bindings use GC-visible ownership and are restored on explicit scene reset; source actors are not destroyed by ordinary reset.
- `Darkwell.ObjectMemory.OrdinaryHost` creates six plain mesh Actors in a transient non-Lab world, covering policy coexistence, root mesh pose/content separation, hidden movement/stop, source destruction and subsequent memory updates, legal empty evidence, invalid publication, duplicate identity rejection and reset.
- Failed extraction evidence retained: `ExtractBuild` had three leftover furniture-field references in fixture motion; `OrdinaryHost` double-initialized its test world; `OrdinaryHost2` exposed the old fixture's intentional NAME_None memory component sentinel plus an over-specific Always sealed-record assertion; `OrdinaryHost3` exposed a real GC root cycle caused by native strong references in the first source-binding draft. None was counted as passing. Those issues were fixed; the Always stationary gray-current compatibility remains intact.
- Final checkpoint formal build: `Saved/ArchitectureAudit_ExtractBuild5.log`, succeeded 32.87 s. `Audit_OrdinaryHost4`: 71/71 (69 clean + 2 warnings), process 0, severe 0, tests 36.453 s / wall 58.424 s, including the existing Play/Stop GC contract.
- Graphical `Saved/ArchitectureAudit/Room02_OrdinaryRuntime`: D3D12/SM6, unchanged normal quality and 51-frame replay, exit 0 / protocol complete / PIE stop complete / zero severe, wall 51.995 s. Independent surface oracle passes all six post-full snapshots. Inspected final and reentry originals; no internal seam and only newly unknown geometry enters through dots. A single clean exit does not establish the earlier intermittent shutdown fault's cause or permanently resolve it.
- Integration contract: `Docs/OBJECT_MEMORY_INTEGRATION.md`. No asset migration, engine change or plugin API change. `SightWeaveRuntime` is a public project-module dependency because its types occur in project public interfaces. Plugin still supplies reusable authority/policy/confirmation; this project-material-dependent gray backend is explicitly not advertised as a standalone plugin renderer.
- Next: eliminate redundant reconstruction on unchanged captures, compact terminal replaced state without fabricated empty evidence, extend order/rate/counterevidence tests, then corrected performance matrix, strict gates and broader real visual/lifecycle checks.

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

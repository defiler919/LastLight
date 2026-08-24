# SightWeave M1 handoff

## Status

- State: **IN PROGRESS**
- Baseline branch: `design/independent-vision-plugin`
- Baseline SHA: `d3060604db18f406f8eecef6fff7d3602c82ee85`
- Working branch: `codex/m1-sightweave-skeleton-lab`
- Current phase: Checkpoint 0 — approved identity and initial handoff
- Last completed checkpoint: remote working branch created from the validated baseline
- Next command: `git -c safe.directory=D:/UE_projects/LastLight add Docs/DECISIONS.md Docs/VISION_SYSTEM_REQUIREMENTS.md Docs/VISION_SYSTEM_ARCHITECTURE.md Docs/VISION_SYSTEM_MIGRATION_PLAN.md Docs/SIGHTWEAVE_M1_HANDOFF.md`

## Approved identity

- Plugin: `SightWeave`
- Runtime module: `SightWeaveRuntime`
- Editor module: `SightWeaveEditor`
- Tests module: `SightWeaveTests`
- Public/internal C++ prefix: `SightWeave`
- Plugin content root: `/SightWeave/`
- Lab map: `/SightWeave/Maps/L_SightWeave_Lab`

## M1 minimum public API inventory

- Strong handles: `FSightWeaveVisionSourceHandle`, `FSightWeaveIlluminationSourceHandle`, and `FSightWeaveSubjectRevealHandle`; each has an invalid default and generation-safe identity.
- Revision: `FSightWeaveRevision`, monotonically changed by authoritative registration, update, and removal.
- Floor/height: `FSightWeaveFloorId` and `FSightWeaveHeightRange` remain independent of source descriptions.
- Vision source: `FSightWeaveVisionSourceDescription` with shape, active state, floor/height, illumination policy, and full compatibility profile.
- Legal illumination source: `FSightWeaveIlluminationSourceDescription` with shape, active state, floor/height, and emitted compatibility identifiers.
- Compatibility: `FSightWeaveIlluminationCompatibilityProfile` retains a normalized complete accepted set; it is not a global boolean or prematurely fixed GPU bit layout.
- Subject reveal: `FSightWeaveSubjectRevealSpecification` and a distinct reveal handle, kept outside the normal vision registry.
- Queries: `FSightWeaveVisibilityQueryResult` with explicit authoritative, not-ready/unsupported, invalid-handle, and invalid-input/floor status; M1 returns not-ready rather than visible.
- Settings: `USightWeaveSettings` containing only neutral M1-safe defaults.
- World lifecycle: `USightWeaveWorldSubsystem` for register/update/unregister, validation, ownership, revision, cleanup, and explicit placeholder queries without per-frame ticking.

## Completed checks

- Read repository guidance, all five vision design/baseline/audit documents, decisions, project descriptor, and build script.
- Confirmed `HEAD` and `origin/design/independent-vision-plugin` both equal the baseline SHA.
- Confirmed the only pre-existing worktree change is the unstaged `Darkwell.uproject` EngineAssociation GUID.
- Confirmed the index is empty and Git LFS has no pending object.
- Fetched `origin` successfully.
- Created and pushed `codex/m1-sightweave-skeleton-lab`; local HEAD, upstream, and remote branch initially matched the baseline SHA.
- Approved the formal plugin/module/API identity as SightWeave.

## Modified files

- `Docs/DECISIONS.md`
- `Docs/VISION_SYSTEM_REQUIREMENTS.md`
- `Docs/VISION_SYSTEM_ARCHITECTURE.md`
- `Docs/VISION_SYSTEM_MIGRATION_PLAN.md`
- `Docs/SIGHTWEAVE_M1_HANDOFF.md`

## Commands executed

```powershell
git -c safe.directory=D:/UE_projects/LastLight status --short --branch
git -c safe.directory=D:/UE_projects/LastLight log -5 --oneline
git -c safe.directory=D:/UE_projects/LastLight diff -- Darkwell.uproject
git -c safe.directory=D:/UE_projects/LastLight diff --cached
git -c safe.directory=D:/UE_projects/LastLight lfs status
git -c safe.directory=D:/UE_projects/LastLight fetch origin
git -c safe.directory=D:/UE_projects/LastLight switch -c codex/m1-sightweave-skeleton-lab
git -c safe.directory=D:/UE_projects/LastLight push -u origin codex/m1-sightweave-skeleton-lab
```

## Build and test results

- Editor build: not yet run for this branch.
- SightWeave M1 automation: not yet implemented or run.
- DARKWELL regression: not yet run for this branch.
- Lab map load/PIE: not yet created or run.
- Independent BuildPlugin: not yet run.

## Known warnings and blockers

- Git requires the command-local `-c safe.directory=D:/UE_projects/LastLight` option because repository ownership differs from the process user. No global Git configuration was changed.
- `Darkwell.uproject` has the expected machine-local EngineAssociation GUID change. It must remain unstaged and uncommitted.
- No current blocker.

## Commits

- Pending Checkpoint 0: `docs: approve SightWeave plugin identity`

## Remote synchronization

- Remote: `origin`
- Remote branch: `origin/codex/m1-sightweave-skeleton-lab`
- State: branch created and pushed at baseline; documentation checkpoint pending push.

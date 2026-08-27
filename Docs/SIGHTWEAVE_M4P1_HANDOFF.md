# SightWeave M4P1 subject policy and Last-Seen proxy foundation handoff

Status: **PARTIAL**

Branch: `codex/m4p1-sightweave-subject-policy-last-seen`

Frozen M3.5 baseline: `941dcb75fe9fcda05bcb972a4d2d9651c6d521be`

Last reliable implementation checkpoint: `8ecfbbd4399901632cfd12862c08ac277f6324ba`

Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`

Primary RHI: D3D12 / SM6

## Outcome

The M4P1 foundation is implemented, built, exercised under NullRHI and real D3D12/SM6, and pushed through the transition-fixture checkpoint. It provides the five-policy CPU contract, immutable revisioned Last-Seen descriptors, exact-scope falling-edge capture, versioned Custom provider routing, a transient opaque static-mesh proxy, and a fail-closed presentation bridge that selects live, proxy, or black without granting gameplay authority.

Status remains **PARTIAL**, not `COMPLETED`, because no user-operated M4P1 PIE inspection or authored visual M4P1 Lab camera matrix was performed. The automated Lab evidence is a generated transient component fixture; it does not prove page-boundary/yaw visual alignment of an actual M4P1 proxy. Full SightWeave, BuildPlugin, clean-host Editor/Game/Shipping, and expanded packaging matrices were intentionally skipped under the stated quota priority.

## Reliable remote checkpoints

All listed commits were pushed to `origin/codex/m4p1-sightweave-subject-policy-last-seen`:

1. `eb7f70e` — `docs: start SightWeave M4P1 subject policy foundation`
2. `b344453` — `docs: define SightWeave subject memory contract`
3. `96b05a1` — `feat: add SightWeave subject memory policies`
4. `04cb024` — `feat: capture SightWeave last-seen snapshots`
5. `ee4d594` — `feat: render SightWeave last-seen proxies`
6. `e0d21c9` — `test: validate SightWeave subject memory transitions`
7. `8ecfbbd` — `test: complete SightWeave M4P1 isolation coverage`

The first push attempt for `e0d21c9` received an `unexpected EOF` from the unsupported Git LFS locking endpoint. An unchanged ordinary `git push` retry succeeded; no force push or Git configuration change was used.

## Implemented contract and behavior

- `NeverRemember`: live is permitted only for legal HardLive; falling edges never create a snapshot or proxy.
- `VisibleOnly`: live-only presentation; non-live is hidden with no remembered state.
- `StaticEnvironment`: delegates to the completed M3.5 path; M4P1 creates no duplicate static-memory store or proxy.
- `LastSeenSnapshot`: a legal HardLive-to-non-live edge captures one immutable descriptor. Remaining non-live does not churn revisions; reacquire immediately selects live; a later edge creates the next revision.
- `Custom`: exact provider name and nonzero schema version are required. Missing, mismatched, rejected, or invalid provider output fails closed. No DARKWELL gameplay provider was added.

The descriptor carries stable ID and instance generation, complete non-hash memory scope, policy, snapshot/eligibility/source-live revisions, transform, bounds, stable mesh/material paths, visual variant, capture reason/transition, and validity flags. Exact scope includes world identity/generation, owner, floor, floor origin/plane, precision, and the complete canonical profile sequence.

Clear deletes intersecting exact-scope descriptors. Suppression and block hide the proxy while retaining the descriptor. Unknown, stale revision, scope mismatch, world mismatch, or generation reuse selects black with no stale fallback. Authority reset removes subjects and snapshots for teardown/world restart.

The proxy is a transient `UStaticMeshComponent` with no collision, overlap, physics, navigation relevance, tick, decals, dynamic shadow, or dynamic indirect/distance-field lighting. It loads only stable mesh/material asset paths, requires opaque material overrides, clears stale resources before every decision, and presents only when the CPU result and descriptor revision match. The bridge mutates presentation visibility only; it does not alter gameplay, AI, damage, interaction, save, audio, VFX, or light state.

## Validation summary

- Final `DarkwellEditor Win64 Development`: succeeded, 4 actions. The only retained build warning is MSVC 14.51 being newer than UE 5.8's preferred 14.50.
- M4P1 NullRHI: 8 succeeded, 0 warning, 0 failed.
- M4P1 D3D12/SM6: 8 succeeded, 0 warning, 0 failed. The log explicitly selected D3D12 SM6.
- M3.4 presentation regression D3D12/SM6: 3 succeeded, 0 warning, 0 failed.
- M3.5 complete regression D3D12/SM6: 26 succeeded, 0 warning, 0 failed.
- Severe Shader compiler/RDG/D3D12/RHI/GPU/fatal/assert/ensure scans: zero matches in the final four gate logs.
- M3.5 startup again showed expected pre-publication `bindingFailure=13`, then recovered to `memoryReady=1`, `staticEnvironmentReady=1`, `memoryPresentationAvailable=1`, `memoryScopeMatchesBinding=1`, `memoryScopeMismatchMask=0x00`, `memoryProfiles=1`, `liveProfiles=1`, and `submitted-feather bindingFailure=0`.

Evidence is under `Saved/AutomationReports/M4P1_Final_NullRHI`, `M4P1_Final_D3D12_Retry`, `M4P1_M3P4Presentation_D3D12`, and `M4P1_M3P5Regression_D3D12`, with corresponding logs under `Saved/Logs`. These generated files and screenshots are intentionally not committed.

## Preserved limitations and warnings

- M4P1 has no user-operated PIE result and no dedicated visual screenshot proof for proxy page/tile seams, rotation, or actual viewport composition.
- No authored `.umap` was changed. The M3.5 regression used `/SightWeave/Maps/L_SightWeave_Lab`; Unreal startup logged cleanup of the project default `L_Prototype` world before loading the allowed Lab, but no PIE, save, or modification occurred in `L_Prototype`.
- No WorldSubsystem/gameplay adapter, production subject, persistence, skeletal/VFX/light/audio snapshot, SceneCapture, or reveal override was added.
- Full SightWeave, DARKWELL, BuildPlugin, clean-host, Game/Shipping, and expanded performance/package matrices were not run.
- The first proxy build failed on two local type issues and the first transition build exposed unity-translation-unit namespace leakage. Both failures are retained in terminal/build logs; both were repaired, followed by successful full Editor builds and green tests.
- No rescue branch was required because every retained implementation checkpoint compiled, tested, committed, and reached the normal remote branch.

## Resume

```powershell
git fetch origin
git switch codex/m4p1-sightweave-subject-policy-last-seen
git pull --ff-only
& 'D:\UE_pro\Darkwell\Scripts\BuildEditor.ps1' -EngineRoot 'D:\UE_5.8'
```

The next bounded task should add an authored or runtime-generated real-world M4P1 visual fixture in `/SightWeave/Maps/L_SightWeave_Lab`, then perform automated D3D12 capture and clearly separated user PIE inspection for live/proxy/reacquire/clear/suppression/unknown, identity reuse, page boundary, and yaw 45. Do not modify `/Game/Maps/L_Prototype`.

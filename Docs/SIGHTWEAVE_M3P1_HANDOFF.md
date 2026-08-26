# SightWeave M3.1 Handoff

Status: **COMPLETED**

Branch: `codex/m3p1-sightweave-render-single-tile`

Frozen baseline: `2cb3f82ab44e810a09f18bed036fa1e4d36db4aa`

Validated implementation and test checkpoint: `c426283bd2e1cadec6778f0fba40426a5ea766ce`

## Closed result

M3.1 implements and validates the minimum executable M3 GPU presentation loop without changing the frozen M2 CPU Authority:

`authoritative polygons -> deterministic triangles -> immutable revisioned packet -> world-scoped render path -> one PF_G8 tile -> asynchronous test readback -> CPU/GPU texel differential`

- `SightWeaveRuntime` owns the compact immutable packet/builder and snapshot publication event. It has no dependency on `SightWeaveRender`.
- `SightWeaveRender` is a Runtime module loaded at `PostConfigInit`, maps `/Plugin/SightWeave`, owns the world-scoped SVE/RDG path, and reads Runtime data only.
- Each world lifetime has a monotonic identity. Owner, floor, profile, packet/registry/snapshot revision, and tile scope travel together across GT/RT and readback.
- Deterministic polygon normalization and bounded ear clipping support convex and concave inputs. Invalid geometry, scope, revision, or resources fail black.
- The single 256 x 256 `PF_G8` tile executes `(Vision intersection compatible Illumination) union Bypass`, then applies Suppression last.
- Duplicate, stale, conflict, world teardown/restart, source deletion, explicit clear, NullRHI, and asynchronous stale-readback behavior are covered.
- The final D3D12 runs produced only 0/255 texels and zero CPU/GPU boundary or non-boundary mismatches across all 65,536 texels per completed case.
- No-change consumes produce no mask work. GPU readback exists only behind `WITH_DEV_AUTOMATION_TESTS` and is absent from Shipping symbols.

## Validation summary

- `DarkwellEditor Win64 Development`: passed.
- M3.1 NullRHI: 10/10 passed.
- M3.1 D3D12/SM6: 29/29 passed on RTX 4060 / Studio Driver 610.88.
- Independent packaged-host M3.1 D3D12/SM6: 29/29 passed.
- Full SightWeave NullRHI: 125 passed, 2 failed, 0 not run. The failures are retained M2P2 wall-clock noise already superseded by the M2 administrator ContextSwitch ETW closure; thresholds and Runtime were unchanged.
- DARKWELL: 24/24 passed.
- Lab NullRHI and D3D12/SM6: loaded `/SightWeave/Maps/L_SightWeave_Lab`, returned authoritative live query status, and exited cleanly.
- BuildPlugin: Editor Development, Game Development, and Game Shipping source builds passed.
- Independent packaged host: Editor Development rebuilt all four modules; Game Development and Shipping consumed the packaged precompiled products; all passed.
- Shipping directories contain only `SightWeaveRuntime` and `SightWeaveRender`. Forbidden dependency/import/string/COFF-symbol scans returned zero matches.
- Final severe-log scans are clean. D3D logs contain only UE's informational `TDR settings OK` summary, not a TDR event.

Authoritative retained evidence:

- Repository D3D report: `D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM3P1_D3D12_PackagingFinal_Retry1`
- Repository full-suite report: `D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM3P1_FullSightWeave_PackagingFinal`
- Repository DARKWELL report: `D:\UE_projects\LastLight\Saved\AutomationReports\SightWeaveM3P1_Darkwell24_PackagingFinal`
- BuildPlugin package: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM3P1_BuildPlugin_Final3`
- Independent packaged host/report: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM3P1_PackagedHost_Final3\Reports\M3P1D3D12`
- Detailed validation and performance evidence: `Docs/SIGHTWEAVE_M3P1_FINAL_VALIDATION.md`

## Preliminary performance risk

Packet builder p95 is 2.300 / 3.300 / 19.100 us for 2 / 8 / 32 sources and passes the suggested 250 us GT target with zero post-warmup capacity growth. In the final successful 21-sample D3D run, GT submit p95 is 9.701 us, RT consume p95 is 1.099 us, GPU mask p95 is 375 us, and async readback p95 is 10,365.102 us.

RT RDG setup p95 is 542.302 us and misses the suggested 200 us target. This test creates a fresh one-shot render state/resources per case rather than benchmarking warmed persistent updates, but the miss is retained as a real M3.2 investigation item. M3.1 does not claim final GPU performance acceptance.

## Retained non-authoritative failures

- Early shader, RDG, readback, lifecycle, pooled-resource, and timestamp-query development failures remain under `Saved/AutomationReports/SightWeaveM3P1_*`.
- The first BuildPlugin Game failure caused by missing public-header includes remains at `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM3P1_BuildPlugin_Final`.
- `SightWeaveM3P1_D3D12_PackagingFinal` retained one cold-start timestamp-unavailable failure; its independent retry and packaged-host run both passed 29/29.
- Slow M2P2 samples remain in every full-suite report. Do not delete them, lower their thresholds, or treat wall time as intrinsic CPU attribution.

## Commit sequence

1. `40d6a64` - docs: start SightWeave M3P1 render implementation
2. `fe02c0b` - feat: add SightWeave immutable GPU packets
3. `3f7ad67` - feat: add SightWeave render module and shader mapping
4. `4e4bd16` - feat: publish SightWeave immutable render snapshots
5. `1c6b29d` - feat: rasterize SightWeave single tile masks
6. `3d22638` - test: add SightWeave GPU readback differential coverage
7. `cbab909` - test: measure SightWeave render setup and capacity
8. `e99cc97` - fix: make SightWeave game headers self-contained
9. `c426283` - test: validate SightWeave packaging boundaries
10. final documentation closure commit - see branch HEAD

Every reliable checkpoint was pushed without merge, rebase, or force-push. `Darkwell.uproject` was never staged or committed.

## Frozen boundaries for the next task

Do not reinterpret this handoff as approval for final post process, memory/Last-Seen, damage-reveal presentation, DARKWELL gameplay integration, SceneCapture, GPU visibility solving, or D3D11/Vulkan work.

Recommended M3.2 scope:

- sparse multi-tile allocation/residency;
- dirty-tile scheduling and eviction;
- continued world/owner/floor/profile/revision isolation;
- warmed persistent-resource RT/GPU measurements that separate creation cost from update cost;
- preservation of M3.1's immutable packet, CPU Authority boundary, hard-mask formula, and fail-black behavior.

## Resume

```powershell
Set-Location 'D:\UE_projects\LastLight'
git status --short --branch
git rev-parse HEAD
git rev-parse '@{upstream}'
git ls-remote origin refs/heads/codex/m3p1-sightweave-render-single-tile
```

Expected worktree after closure: only the intentional unstaged `Darkwell.uproject` EngineAssociation difference. The computer must remain on for user inspection.

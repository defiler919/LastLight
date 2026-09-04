# Confirmed Whole view-edge handoff

Status: `PARTIAL — READY_FOR_USER_CONFIRMED_WHOLE_VIEW_EDGE_RETEST`  
Performance: `PERFORMANCE BLOCKER RETAINED`

Branch: `codex/darkwell-prop-memory-gameplay-lab`. Start SHA:
`a3f2a53dc7e39938145b813d251ac93b48b668af`. Checkpoints:

- A `28c31601fd2ef63b44984cae644de67746daf39c` — deterministic pre-fix
  reproduction and divider classification.
- B `f4cb7889976c7ea23cb79c2970b6749c46912357` — runtime Current/Freeze fix.
- C `8eeca5afdf16ea9d5dc8e22b252c01e16c2e8f12` — positive controls,
  rate/angle/hitch/multi-primitive/ownership regression and diagnostic masks.
- D is the documentation commit containing this file; use branch HEAD as the
  authoritative final SHA.

Stable refs remain frozen at `7534163b9c5718700b610e7677f47fbaa79cf977`
(`stable/sightweave-gray-core-20260903`) and
`404a5820739638f1097eaae0aa7fba19733298c3`
(`stable/moving-history-grid-v2-20260902`). No new stable was created.

## Root cause and corrected contract

The illegal divider came from the confirmed-Whole **Current** path. Raw live
coverage already combined cone/range and wall occlusion. It first advanced
per-cell appearance, then `ApplyWholeObjectPresentation` retained the maximum of
those cells and the object scalar. Cone-edge cells therefore remained at 1.0
beside object-level cells at 0.083333. The divider was not authored by frozen
history, a cap, a material channel, bilinear filtering or TSR.

`FullGeometryMask` is now built from every registered memory primitive's local
upright projected box with cell/polygon intersection. It includes cabinet body,
door and handle while excluding true primitive gaps and actor-AABB empty corners.
Rigid pose/view changes do not redefine local sample identity.

For a confirmed Whole, raw coverage is used only to decide the object-level
`ObjectHasLegalContact`. The Current formula is:

`FullGeometryMask × ObjectLevelLiveBlend × PhysicalOcclusionGate`

`PhysicalOcclusionGate` uses the existing project-side occlusion-only query and
checks only real occluder segments; it does not apply view-cone angle/range.
Whole presentation assigns one exact `.20/.18` object scalar to all live samples.
No plugin API changed.

Confirmed-Whole freeze has a dedicated geometry-mask entry. Its formula is:

`FrozenHistoryMask = FullGeometryMask at LastLegalPose`

The freeze requires matching authority, coverage, pose, policy and geometry
revisions. An inconsistent snapshot logs `WHOLE_FREEZE_REJECTED` and waits for a
coherent publication; it never freezes a half-updated result. Current wall gates,
raw cone edges, dither output and presentation cells are not capture inputs.
Confirmed Whole creates no object cap. SpatialPartial retains its observed-mask
Current, observed-mask freeze and legal deep-gray cap path unchanged.

## Diagnostics and verification

Development/test diagnostics classify `VIEW_EDGE`, `WALL_OCCLUSION`,
`WHOLE_CURRENT_MASK`, `PARTIAL_CURRENT_MASK`, `HISTORY_SURFACE`, `HISTORY_CAP`,
`MIXED_CURRENT_HISTORY` or `UNKNOWN`. On demand they expose counts/hashes for
FullGeometry, RawLiveCoverage, object contact, physical gate, Whole presentation,
current legal observation, last capture, frozen history, cap, final Current and
final History masks. Normal runtime does not create per-frame long strings or GPU
readbacks. Runtime telemetry also separates `occlusion_only_queries` and resource
creation/upload counters.

Pre-fix D3D12/SM6 evidence (`Saved/AutomationReports/ConfirmedWholeViewEdge_PreFix_A2`)
failed all six 90/160/180 single/multiframe cases: raw coverage was split while
the physical gate was 800/800, appearance min/max was 0.083333/1.0, and source
was `VIEW_EDGE`. Post-fix D3D12 C2 passed 8/8. Final minimal C run
`Saved/GrayObjectPolicy/confirmed_whole_view_edge_c_final3` passed 8/8 with no
warning/failure/severe line. The six angle cases report raw 380-390/800,
occlusion 800/800, Whole 800/800 and uniform min=max 0.083333.

The deterministic matrix covers 90/160/180 degrees, single and multiframe turns;
1000 alternating sweeps at 30/60/120/144 Hz all produced hash
`1413828885326587171` and complete capture 12800/12800. 100/200/333 ms hitch
cases produced either a complete 12800/12800 freeze or no freeze until the next
coherent publication. Body/door/handle and real-gap/AABB-corner assertions pass.
Current/stale/cap 3D contributor exclusivity passes. Wall occlusion moves with
the observer, never enters the frozen mask and produces zero Whole caps.
SpatialPartial retains a strict local capture and a positive cap.

Broader isolated evidence: functional selector 139 tests, 0 failures, 0 severe;
GrayPolicyLabV2 16/16 clean; isolated PlayStopResourceLifetime 1/1 clean. A
combined-process run had one WorldPartition teardown ensure, then both involved
suites passed in isolation, establishing test-process pollution rather than a
gameplay failure.

Visual C5 (`Saved/ConfirmedWholeViewEdge/confirmed_whole_view_edge_visual_c5`)
completed D3D12/SM6 with Screen Percentage 100, AA method 4/TSR, normal project
materials/UI, 19 original files, PIE stopped, callback unregistered and severe 0.
Every original—not only a contact sheet—was opened. Review covered unconfirmed,
confirmation, corner contact, adjacent 90/160 frames, 180, Whole history/re-entry,
both wall sides, SpatialPartial/current/history/cap, 333 ms, 1000 sweeps and
ownership negative control. No Whole cone-edge color/alpha divider was visible;
real wall cuts and the SpatialPartial cut remained.

The standard `DarkwellEditor Win64 Development` build passed after the final C++
change. SightWeave plugin source/public API was untouched, so BuildPlugin is not
applicable. The requested five-case short performance matrix was not completed
before the unattended quota-conservation stop. An accidentally selected long
history case was interrupted rather than misreported as a pass; its late samples
still show the known blocker (about 15.1 ms mean room work, p95 25.7 ms, p99
36.6 ms, peak 62.3 ms near frame 21,432). This task does not claim a performance
close. The independent long-history/overlap/publication blocker remains.

## User retest and next entry

Open `/Game/Maps/L_SightWeaveGrayPolicyLab` with D3D12/SM6. Confirm the Room 01
Whole cabinet, then repeatedly flick the cone edge across one corner at 90, 160
and 180 degrees; it must stay one uniform object when no wall intervenes, leave a
complete gray history, re-enter cleanly and never create a cap. In Room 05,
confirm from the open side, move across the wall and verify only the current wall
boundary changes; after leaving/re-entering no wall outline may persist. In Room
02 verify local history and a legal deep-gray cap still exist.

Next Codex window should start from branch HEAD, rerun the dedicated D3D12 visual
tour if desired, complete the five-case short performance table, and wait for the
user's visual verdict. Do not enter the black layer or create/move a stable ref.

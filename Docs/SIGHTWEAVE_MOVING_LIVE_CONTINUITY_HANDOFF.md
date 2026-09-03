# Moving Live Continuity — implementation and evidence handoff

Status: **PARTIAL — READY_FOR_USER_MOVING_LIVE_CONTINUITY_RETEST**.
User manual acceptance is pending. The separately reproduced teardown access
violation remains open; this handoff does not declare that issue fixed.
This does not change the user-accepted Mode 2 baseline or choose new presentation rules.

## Scope and refs

Actual start: `2dda647164e99224e9624f7a9e98af0c7370d5b9` after fetch and fast-forward checks.
Development branch: `codex/darkwell-prop-memory-gameplay-lab`.
Protected refs, neither moved:
- `stable/sightweave-gray-core-20260903` → `7534163b9c5718700b610e7677f47fbaa79cf977`.
- `stable/moving-history-grid-v2-20260902` → `404a5820739638f1097eaae0aa7fba19733298c3`.

No new branch, map, asset, plugin interface, identity registry, WholeObject,
confirmation threshold, region/black-layer feature or parameter productization.
`.uproject` originally lacks its final newline; it is not a GUID change on this
machine. Preserve it byte-for-byte and exclude it from all commits.

## User failure and proved cause

Short entry dither is intentional. Continuous, already observed motion repeatedly
becoming a sparse screen-door surface is not intentional or user accepted.
Checkpoint A leaves runtime behavior unchanged and records the destructive path:
`Transform change → RebaseCurrentObservedLocation → Initialize → BeginPresent`.
The epoch stayed 1, but its internal evidence restarted on every transformed frame.
The changing world AABB also repeatedly reallocated current textures.

`Saved/MovingLiveContinuity/A.log`, A_Report: 240/240 collapsed frames in a four-
second rotation; Initialize and BeginPresent each 241 including initial setup;
152 texture creations; every changed frame Appearance/Live was 0.083333.
This is an input/state error feeding the existing DitherTemporalAA material.
No material, shader, TSR or entry-time change was used to hide it.

## Pose/evidence separation

`FDarkwellCurrentLiveGrid` is project-adapter state, not a second subject registry.
Each original Primitive has its own descriptor (component key, mesh key, local
bounds and relative transform), fixed local XY sample grid, local D/Appearance/
Live, and a reusable world-space raster. Ordinary rigid motion transforms sample
positions; it never initializes the local evidence again. Physical sample footprints
are at most 2.5 cm along each primitive axis, including thin doors and handles.

Coverage is queried afresh from the existing SightWeave-backed legal world field,
with validity and authority/draw revision checks. Local appearance does not confer
knowledge at a new position. Per-pose observation bits plus the current legal gate
prevent previously seen local samples from revealing their new hidden world space.
Fresh local samples still enter over .20; original Live exit math remains .18.
4×4 conservative presentation and bilinear filtering are unchanged.

The original three source components keep mesh, vertices, bounds, transform,
collision and same-source shadow behavior. Each original MID receives its own
current atlas, so a handle cannot borrow body evidence. Fixed capacity follows the
maximum yaw envelope, not changing AABB dimensions. The active texture rectangle
is remapped every pose. A one-texel copy of the active rectangle's outer edge
preserves its original clamp addressing; all remaining padding is cleared so
filtering cannot revive an old pose. No per-frame texture or MID creation occurs.

World snapshots are derived from inverse primitive-local coordinates, not world
nearest-neighbor resampling. The old geometry-clipped envelope is retained once
all real primitive samples have been observed: its empty AABB padding must not
invent observation cuts in a fully known body. This auxiliary padding never
controls source rendering or occupancy. Fine current ownership queries the exact
primitive-local evidence directly, not a coarse world cell's center. They never feed back into local evidence. The old
fixed-footprint Rebase compatibility API now preserves evidence and rejects a
size change; ordinary Lab motion uses UpdateCurrentObservedPosePreservingEvidence.
Topology/mesh/registered-scale changes require explicit geometry reset. This is
the current upright XY/yaw Lab adapter, not a claim of arbitrary pitch/roll or
skinned/deforming-mesh support. Unsupported descriptors fail closed.

## History policy and lifetime

Always keeps the last legitimately observed pose; hidden motion freezes that
pose, not the origin or the next unseen transform. Current local data never moves
an existing historical epoch. Each frozen record retains its world-space
HistoryGridV2, caps and SpatialEvidenceOnly validation.

StationaryOnly retains continuous current Live while moving, abandons unsealed
moving current on view loss, and requires a new legal stationary observation after
stopping before history becomes eligible. Never keeps transient Live while legal,
then hides without history/proxy/cap/historical texture/MID creation.

After already finishing and settling at 180 degrees, the Always adapter can retain
gray evidence in its unmoved current record (current=1, stale=0), without allocating
an additional historical proxy. That is distinct from hiding during ongoing motion,
which freezes the last legal pose. The GPU moving-loss run records one Always
stale record; StationaryOnly/Never record zero moving-loss historical resources.

A fully discovered per-primitive current pose has no internal observation boundary;
an empty world-AABB corner outside rotated geometry does not invent a cut. This
proof skips empty current-cap rebuilds. Actual partial cuts still use the existing
cap path. Historical cap generation/clipping, #343A40 and ownership are unchanged.

No-history/no-cap diagnostics use the structural single-current-record proof
instead of scanning a nonexistent overlap pair. History-present forensic checks
remain active. Dirty/active timers still stop current work after settling.
Freeze/abandon release source texture bindings and their owned resources.

UE's UpdateTextureRegions skips its cleanup callback when GetResource() is null
(e.g. NullRHI). Both project-owned upload sites now avoid allocating in that case.
CPU submitted-presentation counters remain diagnostic plans in NullRHI; only real
D3D12 runs constitute GPU upload/visual evidence.

## Checkpoints

- A `cff8fdd4567f2ba4bfc3521cda829e5a965597e0` — test: reproduce moving live reveal reset.
- B `78b6e9cacae7096cb5653d567aafa26fd3526b76` — fix: preserve live reveal through rigid motion.
- C `bc978ff9be498ffbbf4f2c6437a50b1dae04f6a8` — test: close moving live continuity regression.
- C follow-up `b977fea9e2ef01e384af7ac5287aecd0a693d030` — fix: preserve current atlas clamp at physical boundaries.
- D — the containing `docs: hand off moving live continuity retest` commit; final
  SHA is reported after its push (avoids a self-referential commit hash).

Each completed checkpoint was immediately pushed and local/upstream/remote checked.

## Evidence ledger

Build/native logs, reports and the new-route PNGs are ignored under
`Saved/MovingLiveContinuity`; reused GPU drivers retain their existing Saved paths
listed below.
A: 1 clean / 1 total, 0 failed, 1.857593894 s, exit 0.
The original 91-test suite is retained; this task adds ten requested moving-live
contracts plus one atlas-border regression caught during original-image review.
B4: 11 total, 5 clean + 6 warnings, 0 failed/not-run, 78.615684509 s, exit 0.
C2: 10 total, 7 clean + 3 warnings, 0 failed/not-run, 36.034400940 s, exit 0.
Full3: **101/101**, 65 clean + 36 warnings, 0 failed/not-run,
821.762817383 s, process exit 0. All old rule assertions remain intact.
C7 supplements the real-Lab 30/60/120/144 Hz route and F-control texture mapping;
its only delta after Full3 is additional test coverage, not runtime code.
C7: 11 total, 7 clean + 4 warnings, 0 failed/not-run, 53.139617920 s, exit 0.
Real-room rates have 120/240/480/576 motion updates respectively, all with one
current epoch, zero stale, one initialization, four textures, three original
visible primitives and ordered local-state hash **3882854774440386219**.
Final submitted hash is **1236205723991600003** at all four rates.

Atlas follow-up D1_Focused: 11 total, 9 clean + 2 warnings, 0 failed/not-run,
40.339813232 s, exit 0. The additional NoConsole selector did not match a test
in this run; real interaction remains covered by C7 and the full suite.

Final native rotation: 240/240 preserved frames, current epoch 1, stale epoch 0,
Initialize/BeginPresent each 1, 4 current textures (aggregate plus three originals),
0 new MID/UObject, 1,922 local samples touched per moving frame. Fully observed
motion rebuilt no cap and scanned no history. Mean GT 7.477294 ms; p50 7.6398,
p95 10.2475, p99 11.5181, peak 13.2702 ms; 32,917.179 coverage queries/frame and
2.433 logical texture submissions/frame. Working-set delta 5,517,312 bytes.
Settled 600-frame current idle mean 0.025743 ms.
Eight-epoch idle: 600 frames / 0.030684 ms; fine storage 13,302,784 bytes.
Five-minute soak: 18,000 frames / 0.031280 ms, +393,216 bytes, unchanged UObjects
and proxy/cap/texture/MID counts; no idle scans/queries/uploads/cap rebuilds.
These are lab diagnostics, not a production performance acceptance matrix.

Source Appearance and Live input remain 1 throughout the fully legal sequence.
These CPU alpha values are not mislabeled as measured GPU pixel percentages;
actual rendered continuity is checked separately in the images.

**Final Full4 after the atlas correction: 102/102**, 66 clean + 36 warnings,
0 failed, 0 not-run, **819.961303711 seconds, process exit 0**.
Report: `Saved/MovingLiveContinuity/Full4_Report/index.json`;
log: `Saved/MovingLiveContinuity/Full4.log`; exit: `Full4_exit.txt`.
The 37 warning entries are 34 HTTP timeout entries and three deliberate duplicate-
identity/capacity-rejection fixtures. Severe scan: 0; 13 known startup Condition
failed Error lines, 0 other Error lines. All original tests remain present.

Final Full4 rotation GT: mean **7.033842 ms**, p50 7.2212, p95 9.2909,
p99 10.4910, peak 10.8995; 32,917.179 queries/frame, 2.433 submissions/frame,
1,922 local samples/frame, 0 historical scans/cap rebuilds/UObject growth,
working-set delta 5,369,856 bytes. Ordinary motion has one geometry initialization
and fixed texture/MID ownership; no per-frame grid allocation or texture creation.
Idle: 600 current frames 0.027038 ms; 600 eight-epoch frames 0.030089 ms.
Final 18,000-frame soak: 0.030099 ms/frame, +393,216 bytes, unchanged 93,923
UObjects and proxy:8/cap:10/texture:16/MID:24. These final measurements supplement
the earlier Full3 values above, without rewriting that evidence.

B2 GPU preliminary: `GPU_20260903_131242/checks.json`, 312/312 landed PNGs,
80 full-rotation keyframes per mode plus partial coverage/view-loss sequences.
D3D12/SM6, TSR, SP100; actual game viewport 1526×549 inside 1920×1032 Editor shots.
Exit 0. Agent opened original full/partial/never images. This preceded the final
performance/resource corrections; final close-camera evidence is still required.
The evidence driver invokes the existing F-control entry; it does not pretend to
send a physical F key. Original native tests cover trace/prompt/input interaction.
Screenshots use deferred Shot: request-time state and the next rendered frame are
explicitly distinguished. Camera-only forensic framing never moves legal sight.

### Atlas-border defect found in the GPU review

`GPU_Close2.log` / `GPU_20260903_141859`: 312/312 images, route assertions pass,
PIE stopped and callback unregistered, exit 0. Nevertheless the original 180-degree
image has a stippled door. It is **not accepted as a visual pass**. The actor's
three local states are already one; its fixed-capacity atlas incorrectly samples
zero padding at the active rectangle's maximum border. Bilinear returns 0.5 on
that side and 0.25 at its XY corner, feeding a real dither despite correct state.

`CopyAtlasWithClampBorder` preserves the logical texture's TA_Clamp behavior
inside a larger allocation. One copied row/column prevents that artificial drop;
coordinates, active interior samples, .20/.18 and inward 4x4 AA are unchanged.
An illegal edge is copied as zero. This creates no visible geometry or legal
coverage outside the original primitive. Every other padded texel is cleared.
The new test checks full corners, zero illegal edges, unchanged fractional AA
values and stale-padding removal across 24x8 / 8x24 / 4x4 logical rectangles.
This is a project current-atlas upload fix, not a material or historical cap change.

### GPU route clocks and retained teardown issue

`GPU_Final.log` / `GPU_20260903_140519`: 312 images and PASS/PIE_STOPPED/
CALLBACK_UNREGISTERED, but process **-1073741819 (0xC0000005)** after log closure.
The initial forensic camera was also positioned before teleporting its owning
player and consequently looked at the wrong nearby green furniture. Those images
are retained but not counted as target-cabinet near-view proof. The new driver
sets the camera after player pose; it does not change legal sight authority.

The abnormal exit is an **OPEN TEARDOWN BLOCKER**. A subsequent normal exit does
not erase it. `WindowsApplicationErrors.json` records the Application log search
for IDs 1000/1001: no matching UnrealEditor entry was available at query time.
No faulting module can be responsibly inferred from that absence. No engine,
quit protocol or crash handling change was made.

`GPU_FastSweep1.log`: 85/85, equal ordered states, exit 0.
`GPU_FastSweep2.log`: 85/85, exit 0 but equal-state assertion failed. Both routes
already had zero residual proxies/caps/contact/leak; their **pre-sweep** partial
observations differed: fast 104.249951996 degrees vs slow 105.189205312 degrees.
One Slate callback had consumed a frame without advancing game time. The evidence
driver now advances frame waits only for a fresh game timestamp. No trajectory,
state assertion, threshold or screenshot was removed. `GPU_FastSweep3.log` and
`GPU_FastSweep4.log` then consecutively passed, 85/85 each, exit 0, stopped PIE,
unregistered callbacks; both partial poses 105.189205312 degrees and final hashes
`[4141937835291346275, 4170176150748022068]` for both fast and slow.

`GPU_Caps.log` was launched incorrectly with fixed game-step flags despite that
unchanged driver's wall-clock waits. Hidden rotation had not completed when its
180-degree assertion ran. Exit 0 is not a test pass; failed.json and images remain.
The corrected invocation uses that driver's original ordinary clock (no
UseFixedTimeStep/FPS flags), keeping all original assertions intact.

### Final GPU evidence, runtime b977fea

All four processes below used D3D12, SM6, normal TSR and SP100. Game viewport
was **1526x549**; screenshots include Editor UI at **1920x1032**. This is functional
evidence at the recorded viewport, not strict 1080p/1440p performance evidence.
Paths are relative to project Saved; no PNG, JSON report or log is committed.

| Run | Evidence directory | Landed PNGs | Result / process exit |
| --- | --- | ---: | --- |
| Close3 | MovingLiveContinuity/GPU_20260903_142748 | 312/312 | All assertions; PIE stopped; callback unregistered; 0 |
| FastSweep5 | PropGameplayLab/MovingMulti/FastSweep/GPU_20260903_142905 | 85/85 | Equal ordered states, zero old resources/contact/leak; 0 |
| FastSweep6 | PropGameplayLab/MovingMulti/FastSweep/GPU_20260903_143032 | 85/85 | Same, consecutive second pass; 0 |
| Caps2 | PropGameplayLab/MovingMulti/CapResidual/GPU_Mode2_20260903_143200 | 125/125 | Positive cap present, final old resources zero; 0 |

Total final originals: **607**. Close3 contains 80 full-rotation keyframes and
20 partial-rotation keyframes per policy, plus initial/final and view-loss frames.
Full rotation checks all 240 fixed 60Hz motion states per policy, with retained
Appearance/Live=1, epoch=1, stale=0, initialize=1 and current textures=4.
The fast passes separately log stopped PIE and callback unregistration before
normal exit; complete ledger: `MovingLiveContinuity/GPU_exit_codes_final.json`.
Their same partial pose and ordered final hashes agree with FastSweep3/4 above.

Agent opened original-size Always 90/180, Never 90, StationaryOnly partial and
the positive historical cap from northwest; also opened the three-policy full
rotation sheet, partial-rotation sheet, 18 adjacent 90-degree keyframes, and 18
fast-exit/settled-gray adjacent frames. The original body, door and handle remain
solid during continuous full sight; no repeated entry collapse is visible.
New partial areas retain normal short dither. Fast one-frame reacquisition still
has its legitimate first-entry transition before settling to gray, not recurring
motion-driven resets. No old cap sliver or overlapping representation appeared
in the inspected final/adjacent images. Dark-gray legal empty-space cap remains
visible. Screenshot contact sheets are review aids, not replacement GPU renders.

The original shader probe checked 131,072 pixels: 3,608 blocked-positive samples,
46,864 allowed-positive samples, **0 failures**. Caps2 all sampled actual surface
contact/cap contact/hard-filter leakage counters are zero. A visual review cannot
prove every unsampled frame free of TSR artifacts; final manual PIE is pending.
No exact GPU dither-mask pixel-percentage pass was rendered, so CPU Alpha=1 is
not reported as a measured 100% pixel coverage statistic.

Final GPU logs and D1_Focused severe scan: **0** fatal/assert/ensure/device-loss/
GPU-crash/access-violation lines, **13 known startup Condition failed Error lines
per process**, 0 other Error lines. This does not override the separately retained
GPU_Final abnormal exit. Caps2 used the unchanged older callback shutdown path;
its log has no new PIE_STOPPED/CALLBACK_UNREGISTERED markers, only normal teardown
and exit 0. The two mandatory FastSweep runs have both explicit markers.

Render-setting provenance: Close3 explicitly sets `r.ScreenPercentage 100` and
`r.AntiAliasingMethod 4`. Reused FastSweep/Caps drivers use unchanged project/
Editor defaults, without a per-frame screen-percentage readback in their JSON.
Engine defaults are TSR and automatic resolution (100% at this small viewport);
do not treat those ledgers as precise internal-render-resolution measurements.

### Retained failures and limits

- BuildA and BuildB initial compile failures from const auto pointer deduction on
  TObjectPtr; fixed with explicit pointer types. Successful builds remain separate.
- C.log: development assertion, process exit 3: PrepareCurrentRaster was called
  before BeginPresent during initial descriptor setup. Corrected ordering; C2
  verifies all 10 tests. The assertion was not weakened.
- GPU_B.log: initial script tried ResetCurrent before any scenario was selected.
  The control correctly rejected it. Zero images; failed.json retained. Driver now
  begins without that unnecessary reset. GPU_B2 completed separately.
- Full.log: interrupted by agent after observed working-set growth (18+ GiB),
  not reported as a completed suite. Local Engine source inspection proved the
  missing NullRHI upload cleanup case. Full2 is the corrected rerun.
- Full2 completed with 101 total: 71 clean + 24 warnings, 6 failed, 0 not-run,
  805.153503418 s, exit 255. Five failure cases exposed the remaining coarse
  ownership query (4 unresolved fine samples at the current edge); one diagnostic
  still equated texture allocation extent with world AABB extent. Ownership now
  reads the exact primitive-local evidence, with pose/revision guards. The texture
  diagnostic checks each actual source MID, fixed atlas capacity, active logical
  size and UV domain. No original test assertion was removed or relaxed.
- C5 targeted rerun: 11 total, 8 clean + 3 warnings, 0 failed/not-run,
  107.060676575 s, exit 0. Slow/30/60/120/144 fast routes all have zero old proxies,
  zero seen-empty survivors and ordered state hash 13323734992896764684, matching
  the old reliable baseline. Fully observed frozen poses have no artificial cap.
- Previous task's FastSweep 0xC0000005 remains part of history; no engine teardown
  workaround, callback suppression or evidence deletion is authorized here.
- Ordinary startup condition errors, HTTP timeout and toolchain/deprecation
  warnings must be enumerated separately from test failures and severe errors.

## Build and final checks

Standard build: `Scripts/BuildEditor.ps1 -Configuration Development -EngineRoot D:\UE_5.8`.
Actual engine reports 5.8.2 CL 56702186; repository AGENTS states 5.8.1.
BuildC7 succeeded, 6.93 s (four actions). Atlas follow-up BuildD1 succeeded,
20.34 s (nine actions); both standard DarkwellEditor Win64 Development, not
Live Coding. Nonpreferred MSVC 14.51.36256 and engine C4996 warnings retained.
Plugin source is unchanged; BuildPlugin is not applicable, not a newly passed run.

Final suite: Darkwell.PropLab + Darkwell.FogVisual + Darkwell.SightWeave.M6P1 +
SightWeave.ObjectPolicy. It includes old mixed-cell/late-angle/fast-sweep/frame-rate/
positive-cap/Mode1&2/multi-object/reset/idle/soak tests and eleven moving-live contracts.
Final GPU: close-camera three modes and partial coverage, two consecutive existing
FastSweep runs and existing cap regression are recorded above, including exact
landed images and process exits. No screenshots were used to replace native state
checks or user acceptance.

## Changed file groups

- New `Source/Darkwell/{Public,Private}/VisionPresentation/DarkwellCurrentLiveGrid.{h,cpp}`:
  stable per-primitive evidence and reusable current atlas packing.
- `DarkwellMovingPropLabRoom.{h,cpp}` in those same directories: consume that state,
  source MID/texture lifetime, fine ownership query and developer telemetry.
- `DarkwellSpatialObservationHistory.{h,cpp}`: non-destructive current pose update.
- `DarkwellSpatialPropMemory.{h,cpp}`: reusable derived raster and diagnostic counters.
- `Source/Darkwell/Private/Tests/DarkwellMovingLiveContinuityTests.cpp`: eleven new
  contracts; old suites and assertions remain.
- `Content/Python/verify_moving_live_continuity.py`: same-cabinet three-policy GPU driver.
- `Content/Python/verify_moving_fast_sweep.py`: game-tick gate and explicit shutdown markers.
- This handoff and a dated append-only reference in `SIGHTWEAVE_HISTORY_POLICY_HANDOFF.md`.

No plugin, shader, material, map, config, binary asset or build-script delta.

### Reproducing the evidence

Run heavy jobs serially, with no other Editor/build worker active. Standard build
is shown above. Automation selector is
`Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1+SightWeave.ObjectPolicy`;
use `-nullrhi -nosound -unattended` and `-TestExit="Automation Test Queue Empty"`
with an explicit report directory. NullRHI is state/resource evidence only.

For visual drivers, launch UnrealEditor into the Lab with
`-d3d12 -sm6 -PropLabMovingControls -PropLabAsyncCapture -nosound -unattended`
and `-ExecutePythonScript=<absolute script path>` plus a unique `-abslog`.
MovingLive and FastSweep require `-UseFixedTimeStep -FPS=60`; FastSweep also uses
`-CapResidualMode=2 -FastSweepCloseView`. The unchanged cap-residual driver uses
`-CapResidualMode=2` **without** fixed-step/FPS flags because its yields are wall
seconds. Always record process exit separately from a script PASS marker.

## Manual PIE after validation

Launch ordinary Editor into `/Game/Maps/L_ProjectFogPropGameplayLab` with
`-d3d12 -sm6 -PropLabMovingControls -PropLabHistoryPolicies`; leave Play stopped.
Click Play, walk to the visible-rotate F control, keep the asymmetrical orange
cabinet in legal sight until solid, press F, and keep looking through its four-
second rotation. Watch body/door/handle at 90° and 180°, then try partial sight and
look away mid-motion. The launch fixture assigns VISIBLE TRANSLATE=Always, VISIBLE ROTATE=StationaryOnly,
COVERAGE EDGE=Never; Multi high=Always, low=StationaryOnly, box=Never. Use those
existing F controls and explicit RESET CURRENT EXPERIMENT, not a nonexistent
policy-toggle control. The automated same-cabinet comparison uses explicit
per-object test re-registration. No console is required for the manual route.

User manual acceptance is still pending. The next planned feature remains
WholeObject + Confirmation Threshold, only after this continuity retest passes.
Stop all automated UE/Python/PIE activity at handoff. Keep the computer on.

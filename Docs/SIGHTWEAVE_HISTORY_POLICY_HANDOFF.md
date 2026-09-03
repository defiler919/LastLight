# SightWeave object history capture policy

## Scope and frozen starting point

Work remains on `codex/darkwell-prop-memory-gameplay-lab`.
Actual fetched/fast-forward-checked start: `7534163b9c5718700b610e7677f47fbaa79cf977`.
Before any source change, `stable/sightweave-gray-core-20260903` was created and
pushed at exactly that commit. It contains the previous gray-core candidate,
not these new object policies and not a claim of final gray-layer acceptance.
The older `stable/moving-history-grid-v2-20260902` stays at
`404a5820739638f1097eaae0aa7fba19733298c3`.

Only history capture policy is exposed. WholeObject, confirmation thresholds,
profiles, automatic motion detection, world/region/black policy, save migration,
and moving HistoryGridV2 into the plugin remain outside this change.
No map, material, shader, original prop geometry, `.uproject`, or third-party
asset was modified. Installed engine: **5.8.2, CL 56702186**, under the requested
`D:\UE_5.8` root (the repository guidance still says 5.8.1).

## Plugin boundary and public API

`SightWeaveRuntime` owns `SightWeaveObjectPolicy.h`:

- `ESightWeaveHistoryMode`: Always / StationaryOnly / Never.
- `ESightWeaveObjectPolicySource`: UseProjectDefault / Override.
- `FResolvedSightWeaveObjectPolicy::Resolve` and its resolved HistoryMode.
- `FSightWeaveObjectHistoryCapture`: ordinary-data, per-object explicit motion
  and capture-qualification state; no world, identity or geometry dependencies.
- `USightWeaveObjectPolicyComponent`: optional Blueprint-spawnable component;
  it does not register a second subject or assign a StableID.

The existing subject-registration/last-seen system serves a different provider
contract. The Moving Lab owns multi-record spatial histories outside that
system. The optional component is shared authoring/motion metadata, not a
parallel identity registry or a replacement for subject registration.

Project Settings > SightWeave > Object History > Default History Mode defaults
to **Always**. The component exposes only Policy Source and History Mode.
OnRegister resolves the project default plus object override once. Hot paths
read the resolved value; they do not read config, reflection or string names.
Edits to authoring properties do not migrate a registered object's history.
Reconfiguration requires explicit host reset and re-registration.

Blueprint and C++ methods:

```cpp
Policy->SetSightWeaveMoving(true);  // before actual motion
Policy->SetSightWeaveMoving(false); // after actual motion
Policy->IsSightWeaveMoving();
Policy->GetResolvedHistoryMode();
Policy->GetMovingRevision();
Policy->IsHistoryEligible();
Policy->RequiresFreshStationaryObservation();
```

`SetSightWeaveMoving` is the single state-changing motion API. No separate
Begin/End depth exists: repeated true/false and an initial false are harmless.
Revision increments only when the Boolean state changes. C++ host adapters call
`NotifyLegalObservation()` only after obtaining valid legal coverage. There is
no Blueprint shortcut for asserting that evidence. The component never ticks.

Motion must be explicitly reported by the owning gameplay code. There is no
automatic Transform Delta, velocity threshold, jitter filter, or stationary
delay. Existing Lab transform comparisons still update geometry/coverage
revisions; they do not infer the plugin's Moving state.

## History modes and observation lifecycle

**Always** follows the prior capture path. Static observations and last-seen
intermediate moving poses can become history. No override in an ordinary Lab
launch means Always, preserving the baseline. Mode 1/2 continue sharing the
Moving Lab history model.

**StationaryOnly** retains normal legal Live presentation while moving, but
cannot freeze intermediate moving observations. Motion start invalidates
capture qualification without sealing a pose or clearing an earlier epoch.
Coverage loss abandons the current unsealed observation instead of freezing it.
Stopping requires a fresh valid legal stationary observation before capture
can rearm. Stopping offscreen therefore creates no final memory. Continuous
visible motion keeps Live; after the final stationary observation, looking away
seals that final pose once. Existing previously sealed records remain eligible
for ordinary spatial evidence updates throughout motion.

**Never** permits transient current Live state, but zero historical epochs,
HistoryGridV2 samples, proxies, stale caps, historical textures or historical
MIDs. Coverage loss immediately hides the current source and abandons its
unsealed record. It never converts that record to an invisible historical one.
No runtime HistoryMode migration is supported.

Project adapter details:

- `AbandonCurrentObservationWithoutHistory` removes only the current record;
  NextEpoch stays monotonic and sealed records/evidence are untouched.
- The Lab counterpart releases that record's visual resources and hides the
  current source. It does not write VerifiedEmpty or use identity invalidation.
- Ineligible current observations use a transient Live-only input to the
  unchanged conservative 4x4 presentation builder. This zeros uncovered source
  opacity before the same inward AA/filter path; D/V/R cells are not mutated.
- Such a transient observation creates no cap. Historical caps from earlier
  qualified observations continue through the unchanged V2 cap path.
- Fresh stationary qualification comes from the next valid legal observation,
  not a StableID lookup, timer, motion-end callback, or stale cached pose seal.

HistoryMode gates **new capture**, not world knowledge. Old history can still
be resolved only by legal empty-space evidence or newer legally observed
ownership. Looking at B does not identity-clear A. HistoryGridV2 four-state
rules, sweep proof, hard ownership, OBB tolerances, cap construction/color,
`.20/.18`, TSR and the 4x4 AA math have not been changed.

## Manual entry without console

Launch the ordinary editor with:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'D:\UE_projects\LastLight\Darkwell.uproject' `
  '/Game/Maps/L_ProjectFogPropGameplayLab' -d3d12 -sm6 `
  -PropLabMovingControls -PropLabHistoryPolicies
```

Click Play. There is no new map and no required in-game console command.

1. **VISIBLE TRANSLATE — Always**: F, watch the four-second move; look away and
   back during motion to check normal last-seen behavior.
2. **VISIBLE ROTATE — StationaryOnly**: observe initially, F, keep looking during
   the one-second delay, then look away when HUD shows MOVING 1. Briefly look at the moving
   cabinet several times. There must be no intermediate gray pose. Let it stop
   while looking away; it must remain hidden. Look at the final static cabinet,
   then away: only that final stationary pose may be remembered.
3. **COVERAGE EDGE — Never**: F, watch the eight-second crossing and repeatedly
   turn away/back. The object has Live but no gray history.
4. **MULTI PROP**: high cabinet Always, low cabinet StationaryOnly, small box
   Never, fixed table inherited Always. Policies and motion remain per object.
5. **RESET CURRENT EXPERIMENT** is the sole F reset; it recreates only that zone
   with the launch fixture's policy. Ordinary start/finish/focus never resets.

To check preservation of an already sealed stationary history, first observe
the stationary rotation cabinet and turn away before pressing F. That earlier
epoch may remain during motion; it is not a prohibited new moving history.

HUD shows HISTORY MODE, MOVING, HISTORY ELIGIBLE, FRESH REQUIRED, CURRENT
RECORDS, STALE RECORDS, PROXIES and CAPS. Counts distinguish transient current
records from historical records. ENEMY remains 0. The normal launch without
`-PropLabHistoryPolicies` continues to inherit project defaults on all objects.

## Checkpoints and validation ledger

- A: `f147663fe814d487109deea753638a2fa9508c86` — plugin policy contract,
  settings, optional component, generic tests; immediately pushed.
- B: `89ef8d59dc237c3f50ab6894e19271f0dc654149` — project capture adapter,
  abandon lifecycle, optional manual fixture and native tests; immediately pushed.
- C: final regression/documentation commit is reported in the handoff reply.

Initial A build: Success, 17 actions, 31.31 seconds. First test run passed two
tests but failed in the third fixture because it initialized an already
initialized UWorld twice (WorldSettings name collision). `A.log` preserves that
failure. The fixture now follows existing NewObject/InitializeNewWorld practice;
no runtime gray algorithm was changed to hide the test failure.

Corrected A test: 3 total, 3 clean, 0 warnings/failures/not-run, 0.0297535 seconds.
B integration build: Success, 16 actions, 27.20 seconds; subsequent B2 test-only
build precedes B tests. Focused B: 13 total, 13 clean, 0 warnings/failures/not-run,
32.254505 seconds. Logs and reports are under ignored `Saved/HistoryPolicy`.

### Editor builds and automation

All builds used `Scripts/BuildEditor.ps1 -Configuration Development -EngineRoot
"D:\UE_5.8"`, with UE/UBT/dotnet/ShaderCompileWorker process checks first. No
Live Coding evidence is used. Builds and UAT were serialized.

| Run | Result | Actions | Seconds |
| --- | --- | ---: | ---: |
| A | Success | 17 | 31.31 |
| A2 fixture correction | Success | 4 | 4.64 |
| B | Success | 16 | 27.20 |
| B2 test correction | Success | 4 | 5.11 |
| C additional plugin test | Success | 9 | 18.03 |
| C2 serialization fixture correction | Success | 4 | 4.92 |
| C3 explicit include dependencies | Success | 5 | 11.81 |

Full command-line automation filter:
`Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1+SightWeave.ObjectPolicy`.
`Saved/HistoryPolicy/Full_Report/index.json`: **90 total, 61 clean, 29 successful
with warnings, 0 failed, 0 not-run, 676.950012 seconds**. These are the existing
77 tests plus 10 new DARKWELL cases and the first 3 generic plugin cases.
Warnings comprise 30 entries: 27 HTTP connectivity-probe timeouts, two expected
duplicate-ID rejections and one expected capacity rejection. They were not
deleted or converted to clean passes.

The additional generic config/public-surface test initially failed because
normal delta export omits the zero/default Always enum, producing an empty
string. `C_Plugin.log` / `C_Plugin_Report` preserve that fixture failure. The
corrected fixture explicitly exports a value before import; no runtime/config
policy changed. `C2_Plugin_Report/index.json`: **4 total, 4 clean, 0 warnings,
0 failed, 0 not-run, 0.033147603 seconds**. Thus 91 distinct tests have passing
evidence across these runs; this is not a claim of a single 91-test full run.

Coverage includes resolution/default/override, explicit motion idempotence and
revision, registration caching, config enum roundtrip and Blueprint API flags;
current abandon, stationary fresh rearm, preserving sealed history, Never
resource zero (10 cycles), simultaneous independent object policy, native
interaction/continuous intermediate transforms, and authority/AA input parity.
The unchanged full regression includes fast sweep at 30/60/120/144 simulated
Hz, late angles 120/128/145/157, positive and forbidden caps, mixed-cell
ownership, Mode 1/2, A→B / A→B→C, multi-prop, visible motion, FogVisual and M6P1.

### Performance and resource evidence

The existing 8-epoch idle test (600 frames) reported **65.628 us/frame** update
cost and 7.251 us report cost; 415,712 resident fine samples / 13,302,784 bytes.
Idle coverage, occupancy, fine scans, texture uploads and cap builds were all
zero. Proxy/cap/texture/MID counts stayed 8/10/10/24.

The existing 18,000-tick five-minute-equivalent idle soak reported **64.831
us/frame**. Working set changed from 12,487,290,880 to 12,487,684,096 bytes
(+393,216 bytes); UObjects remained 87,514 and resources remained stable.
The 50-reset test passed its existing bounds: UObjects 88,817→89,097, final
proxy/cap/texture/MID counts 4/6/6/12. This is bounded evidence, not zero growth.

At matched moving angles approximately 94.8–105.2 degrees, the 12-frame GPU
diagnostic samples had tracked-room game-thread costs of 26,150–41,266 us for
Always, 11,643.2–14,999.1 us for StationaryOnly, and 11,992.2–17,002.8 us for Never.
These short instrumented changed-frame samples demonstrate capture/resource
differences; they are not benchmark percentiles or 60fps performance approval.
Changed-view work can still be expensive. No new per-tick config lookup, second
identity registry, full historical scan, or motion detector was introduced.

### D3D12 / SM6 evidence and actual visual review

Normal TSR (`r.AntiAliasingMethod 4`), ScreenPercentage 100. Actual PIE viewport:
**1526×549**. PNGs are **1920×1032 full editor-window captures**, not strict 1080p
or 1440p render/performance evidence. Simulation used fixed 1/60 seconds; this
does not assert a measured real-time 60fps rate.

| Evidence directory | Rows / landed PNGs | Outcome |
| --- | ---: | --- |
| `Saved/HistoryPolicy/GPU_20260903_113732` | 69 / 69 | Policy/resource assertions passed; process exit 0 |
| `Saved/PropGameplayLab/MovingMulti/FastSweep/GPU_20260903_113919` | 85 / 85 | Ordered-state, contact and resolved-resource assertions passed; abnormal process exit below |
| `Saved/PropGameplayLab/MovingMulti/HistoryGridV2/GPU_Mode2_20260903_114144` | 105 / 105 | Positive cap, held reacquire, final resources and shader assertions passed; process exit 0 |

Total: **259 landed PNGs** in the successful evidence datasets. The last run's
two real shader textures tested 131,072 pixels: 9,120 blocked-positive and
53,646 allowed-positive samples, 0 failures. Fast and slow ordered final states
match within their respective unchanged fixtures; old resolved resources are
zero. Positive empty-space caps remain present, including after offscreen A→B.

The agent actually opened the three-policy comparison, all three 12-adjacent-
frame sheets, individual current-source frames, fast-sweep adjacent/final gray
frames, reacquire adjacent frames, and full west/northwest positive-cap views.
Review sheets are under `Saved/HistoryPolicy/GPU_20260903_113732/review` and
`Saved/HistoryPolicy/Review`. StationaryOnly ends hidden with zero new moving
history, then makes one final stationary proxy after a fresh observation and
view loss; Never stays at zero history over repeated entry/exit. The mixed
fixture simultaneously moves Always/StationaryOnly objects and removes Never.

The new Python driver uses the existing F-control evidence entry. It does not
pretend to synthesize physical keyboard input. Native tests separately cover
focus/trace/prompt/interaction. Camera and motion driving are deterministic
test controls; user evaluation remains free manual PIE. `Shot` is deferred:
telemetry is request-time, the PNG is rendered on the following frame. The last
frame before a camera change can therefore already show the next view. This is
documented in every new-driver row, not counted as a visibility flicker.

**Retained visual limitation — moving Live readability is not accepted.**
Actual adjacent images show sparse/dithered current geometry during continuous
rotation in all three capture modes. Always's older gray pose partly hides it.
Inspection of the unchanged baseline `RebaseCurrentObservedLocation` path shows
that each changed transform initializes the current SpatialMemory again,
restarting local entry accumulation. Source visibility / no-stale assertions
therefore do not prove a solid, readable continuously moving Live surface.
This was not corrected by changing the frozen `.20/.18`, AA, D/V/R or cap path.
The policy lifecycle is testable, but this visual limit remains explicit; do
not report the whole moving visual behavior as passed or production-ready.

**Retained first GPU fixture failure.** `GPU_Policy.log` and
`GPU_20260903_112704/failed.json` retain the first attempted run (19 landed PNGs,
not included in the 259). Looking away in the F control's one-second stationary
delay legitimately sealed an earlier static history. The driver's zero-moving-
history assertion was therefore invalid. It now waits for MOVING 1 before
looking away, identically for all modes. No old record was deleted to pass.

**Retained abnormal FastSweep exit.** `GPU_FastSweep.log` reaches
`FAST_SWEEP_GPU_PASS`, `LogExit: Exiting` and log closure, but the launching
process returned **-1073741819 / 0xC0000005**. This is a post-evidence/teardown
failure with no corresponding fatal line in that log. Cause is unconfirmed.
Do not turn successful JSON assertions into a claim of successful process
completion. The later cap/shader GPU run exited normally; it does not erase
this failure. No broad shutdown/engine fix was attempted.

Severe text scan of Full, C2_Plugin and the three final GPU logs found **0**
fatal/assert/ensure/device-removed/device-hung/GPU-crash/access-violation lines.
Each retains the same **13 startup `LogAutomationTest: Error: Condition failed`
lines**, with 0 other Error lines. This text-scan result does not override the
abnormal FastSweep exit code. Initial failed runs remain saved separately.

### Standalone plugin smoke

The first standard Win64 `RunUAT.bat BuildPlugin` attempt passed its independent
UnrealEditor build (108 actions, 150.97 seconds), then failed UnrealGame
Development (53.89 seconds), UAT exit 6. Log: `Saved/HistoryPolicy/BuildPlugin.log`.
It exposed pre-existing dependencies hidden by the Editor PCH:
`FThreadSafeBool`, `FTextureRenderTargetResource`, and `FSceneViewFamily` were
used without including their defining headers. Only three direct includes were
added to the private SceneViewExtension .h/.cpp (`HAL/ThreadSafeBool.h`,
`TextureResource.h`, `SceneView.h`). No rendering function, branch, type layout,
public API, shader or visual semantics changed. This small build dependency
correction is part of checkpoint C, not a gray-core algorithm refactor.

Corrected standard `BuildPlugin -TargetPlatforms=Win64 -Rocket`: **Success,
exit 0, 3m39s**, `Saved/HistoryPolicy/BuildPlugin2.log`. Independent clean host
stages: UnrealEditor Development 108 actions / 120.49 seconds; UnrealGame
Development 34 actions / 54.21 seconds; UnrealGame Shipping 34 actions / 42.35
seconds. This is plugin smoke, not DARKWELL Cook/Package or a Shipping product
acceptance test. Package:
`Saved/HistoryPolicy/BuildPlugin2_20260903_115410`. No host Darkwell dependency
was needed. Compiler errors: 0. Retained warnings: 116 C4996 deprecation lines
and MSVC 14.51.36256 not-preferred-toolchain warnings in all three stages.

After the include-only correction and C3 standard Editor build, the final
focused rerun `Darkwell.PropLab.HistoryPolicy+SightWeave.ObjectPolicy` passed
**14 total, 14 clean, 0 warnings/failed/not-run, 32.411903381 seconds**, process
exit 0. Report: `Saved/HistoryPolicy/C3_Focused_Report/index.json`. C3 log has
the same 13 startup condition errors, 0 other Error lines and 0 severe lines.
No behavior change followed the full regression or GPU runs; the only runtime
file delta since those runs was the three explicit include dependencies.
The first failed package and all previous failed logs remain preserved.
Packages remain under ignored Saved, never Git.

### Handoff bounds

Status ceiling: **PARTIAL — READY_FOR_USER_HISTORY_POLICY_RETEST**. User manual
PIE is pending; moving Live readability and the one abnormal exit above remain
open limitations. No WholeObject / Confirmation Threshold option is exposed.
The planned next feature stage remains WholeObject + Confirmation Threshold,
not black-layer work; do not silently expand that stage to hide these failures.

The actual local `.uproject` difference at task start is a missing final
newline, not an EngineAssociation GUID change. It is preserved byte-for-byte
and excluded from every commit. Evidence, videos, logs, packages and generated
directories stay outside Git. Neither stable branch is moved after creation.
Final development and stable refs are checked after push and reported alongside
the final checkpoint SHA. Stop automated UE/PIE/scripts; ordinary Lab editor
may remain open with Play stopped. **Keep the computer on.**

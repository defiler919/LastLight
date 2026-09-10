# Surface Receiver V1 verification — 2026-09-10

Parent SHA: `a3ac3d955a9f05b3ee5cfdd5659425e7fc2d33e1`. Evidence belongs to the working source committed with this report. Branch `codex/darkwell-prop-memory-gameplay-lab`; stable baseline tag unchanged. No binary Unreal asset changes.

## Build and tests

- `Scripts/BuildEditor.ps1`: full DarkwellEditor Win64 Development target **Succeeded**, not Live Coding. Final invocation 6.71s, incremental full target, not a clean rebuild. `build.txt` preserves output with trailing whitespace removed.
- D3D12/SM6 Runtime/Surface/Adapter/Static/M2 batch: **33 passed** (32 clean + 1 with warning), 0 failed, 0 notRun. `runtime-d3d12.json`.
- D3D12 Whole/Partial/Unknown-region batch: **7 passed** (5 clean + 2 with warnings), 0 failed, 0 notRun. `memory-d3d12.json`.
- Total **40 passed**, including 5 RuntimeV1 tests, 90 independent six-face oracle cases with yaw/pitch rotation, true Adapter eye/direction override, old Hard GPU differential, illumination closure, exact cache invalidation, old memory Clear/Block/Whole/Partial.
- Engine actually reports **5.8.2-56702186**, at `D:\UE_5.8`; AGENTS.md references 5.8.1. No engine/config changes. MSVC 14.51.36256 is non-preferred; build emits existing engine deprecation warnings.

Warnings are not hidden or counted as clean:

1. `M6P1.Lifecycle.DuplicateFixtureRollback`: deliberate duplicate stable-id rejection.
2. `UnknownPartial.TemporalSurface`: engine `r.MotionVectorSimulation` render-thread access warning.
3. `UnknownRegion.Whole`: unrelated startup connectivity HTTP timeout to generate_204.

No SurfaceV1 test warning/error. Earlier NullRHI run was 26/27: lifecycle test incorrectly instantiated abstract UObject, fixed to concrete component owner. Other V1 and legacy tests passed then. Compilation iterations fixed inherited generic `Flags` identifier colliding under unity build and missing UpdateOccluder argument; no warning suppression or weakened product assertions. Initial build also reported the existing unrelated fixture include-order diagnostic. Final full target succeeded.

## Runtime microbenchmark

D3D12 batch, one observer, 257 registered boxes (256 deliberately off-ray), 400 unique front-face samples. All 400 are checked legal; exact first batch visited 6,800 BVH nodes and only 400 primitive tests, rather than 102,800 all-box tests. Warm 100 batches generated 40,000 exact cache hits with no further geometry tests. Timing is diagnostic, not a hardware-dependent test threshold.

| Measurement | Result |
|---|---:|
| First 400-sample batch | 160.698 us |
| Same-frame cache P95 / P99 | 19.699 / 21.301 us |
| Eye-motion query batch P95 / P99 | 223.801 / 253.800 us |
| Source update/publication P95 / P99 | 187.099 / 201.099 us |

Motion loop includes 100 measurements (initial repeated pose then 99 changed eye heights). The scene/BVH pointer is asserted unchanged across all observer publications. Motion queries invalidate frame-local results; they do not recycle stale eye evidence. This measures current-frame Runtime query/publication, not whole-game GT/GPU, multi-light worst case, geometry-update cost or large-scale streaming. Geometry changes currently rebuild the scene; batching/refit remains a known scalability boundary.

## Reproduction

From repository root in PowerShell, use a fresh ReportExportPath per run:

```powershell
./Scripts/BuildEditor.ps1
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE_projects\LastLight\Darkwell.uproject' -unattended -nop4 -nosound -d3d12 -sm6 -RenderOffscreen '-ExecCmds=Automation RunTests Darkwell.SightWeave.SurfaceObservation+Darkwell.SightWeave.StaticKnowledge+SightWeave.M2.Query+Darkwell.SightWeave.Differential.VisionIllumination+Darkwell.SightWeave.Closure.VisionIlluminationBoundary+Darkwell.SightWeave.M6P1' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\SurfaceV1FinalReport' '-abslog=D:\UE_projects\LastLight\Saved\SurfaceV1Final.log'
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE_projects\LastLight\Darkwell.uproject' -unattended -nop4 -nosound -d3d12 -sm6 -RenderOffscreen '-ExecCmds=Automation RunTests Darkwell.UnknownRegion+Darkwell.UnknownPartial+Darkwell.PropLab.GrayObjectPolicy.WholeObjectConfirmedStaticHistory+Darkwell.PropLab.GrayObjectPolicy.SpatialPartialStaticKeepsLegalCap' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:\UE_projects\LastLight\Saved\SurfaceV1MemoryReport' '-abslog=D:\UE_projects\LastLight\Saved\SurfaceV1Memory.log'
```

Inspect report counts/events, not just editor exit status. Old XY/Static gaps are now explicitly named `LegacyConsumers.KnownGaps`. New `RuntimeV1` tests assert successful production receiver behavior; neither set claims the deferred ObjectMemory/Static/P4 surface migration is implemented. GPU readback validates the old published Hard path, not a new surface atlas. No surface memory persistence, whole-face region proof, animated character or cross-floor propagation is claimed.

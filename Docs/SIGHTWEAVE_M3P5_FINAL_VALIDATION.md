# SightWeave M3.5 final validation

## 2026-08-27 Camera 1 remembered-presentation repair

Status is **COMPLETED**. The automated repair gate passed and the user subsequently completed the interactive Camera 0-4 PIE inspection. The earlier Overview-only automated screenshot did not prove that Camera 1 worked and is superseded by the five-camera automated evidence plus the user's final manual validation below.

### Root cause

The CPU HardMemory authority and both GPU mirrors were healthy, but the real PIE composite correctly failed closed because its memory scope was not equivalent to the live presentation binding:

1. exploration memory used the selected Coarse tier while the render-world default presentation and live sparse packet were hard-coded to Standard;
2. after aligning precision, the remaining mismatch was canonical profiles: memory retained the active bypass vision source profile, while the sparse live packet omitted bypass profiles from tile identity and the derived presentation binding.

The pre-fix Camera 1 diagnostic was `memoryReady=0 staticEnvironmentReady=0 memoryPresentationAvailable=0`, with both mirrors `Available`, 55 memory entries, 9 static entries, and finally `memoryScopeMismatchMask=0x20 memoryProfiles=1 liveProfiles=0`. This explains why `submitted-feather` coexisted with an all-black remembered branch.

### Repair

- The render-world subsystem now derives the default live presentation precision from the valid world-scoped memory packet for the same owner/floor and rebuilds the live sparse packet at that tier. Explicit presentation scope still has priority. Clearing explicit scope, changing memory scope, and disabling memory rebuild the packet at the newly selected tier.
- Bypass vision profiles are now validated and retained in sparse tile canonical identity, so the presentation binding and CPU memory authority share the same canonical profile set.
- Scope, revision, readiness, page-table, residency, resource-generation, and precision diagnostics are emitted only when the captured state changes. The mismatch mask bits are world `0x01`, owner `0x02`, floor `0x04`, precision `0x08`, floor origin `0x10`, profile count `0x20`, and profile identity `0x40`.
- Lab automation now captures Camera 0 Overview, Camera 1 Remembered, Camera 2 DynamicLeak, Camera 3 PageBoundary, and Camera 4 Rotated45 in one real D3D12 PIE run.
- New native tests cover memory-derived precision lifecycle and bypass canonical-profile binding. Existing GPU mirror/composite readback continues to cover live color, remembered neutral static color, strict black Unknown/suppressed/unavailable, wrong-world rejection, gutter/page behavior, and fail-closed resource loss.

The final healthy Lab line is:

```text
memoryReady=1 staticEnvironmentReady=1 memoryPresentationAvailable=1
memoryAvailability=1 staticEnvironmentAvailability=1
memoryPageTableEntries=55 staticPageTableEntries=9
memoryResidentTiles=55 staticResidentTiles=9
memoryScopeValid=1 memoryScopeMatchesBinding=1 memoryScopeMismatchMask=0x00
staticScopeMatchesMemory=1 memoryPrecisionTier=0 livePrecisionTier=0
memoryProfiles=1 liveProfiles=1
```

### Exact repair validation

| Gate | RHI | Exact result |
| --- | --- | ---: |
| `DarkwellEditor Win64 Development` after final source changes | build | succeeded, 4 actions |
| M3.5 complete | NullRHI | 16 succeeded, 0 warning, 0 failed |
| M3.5 complete | D3D12/SM6 | 26 succeeded, 0 warning, 0 failed |
| M3.4 presentation contracts | D3D12/SM6 | 3 succeeded, 0 warning, 0 failed |
| M3.4 D3D12 feather/safety/performance | D3D12/SM6 | 29 succeeded, 0 warning, 0 failed |
| Focused Camera 1 Lab capture | D3D12/SM6 | 1 succeeded, 0 warning, 0 failed |
| Memory-derived precision lifecycle | NullRHI and complete D3D12 gate | passed |
| Bypass canonical-profile binding | complete NullRHI and D3D12 gates | passed |

Reports are `Saved/AutomationReports/M3P5_Restore_Final_NullRHI`, `M3P5_Restore_Final_D3D12`, `M3P4_Presentation_RestoreRegression_D3D12`, and `M3P4_D3D12_RestoreRegression`. Their logs have zero Shader compiler, RDG validation, D3D12/RHI error, GPU crash/hang, fatal, assertion, or ensure matches under the final severe scan.

The final five-camera screenshots are `Saved/Screenshots/M3P5_PIE_Camera0_Overview.png` through `M3P5_PIE_Camera4_Rotated45.png`. Direct image inspection found:

- Camera 1 is no longer all black. Its dominant remembered value is the authored neutral `(68,68,68)` (125,561 pixels), with authored wall values 168/176/184 and strict `(0,0,0)` elsewhere.
- Camera 0 and yaw=45 Camera 4 contain the same authored 68 gray, without page shift or a visible tile seam. Camera 3's page-boundary strip is continuous.
- Camera 2 retains current light/Scene Color only inside HardLive. The non-live dynamic-door, moving-mesh, and emissive/VFX sentinel regions do not become remembered gray; outside the live/static eligibility branch remains black.
- Unknown, clear, block, and presentation-suppressed areas are black. The CPU transition remains `remembered:1 live:1 clear:0 block:0 suppressed:1/1 unknown:0`.

One intermediate Editor build failed because the first five-camera test edit had one extra closing parenthesis. It was immediately corrected; all subsequent full Editor builds and automation gates passed. The existing non-preferred Visual Studio 14.51 versus preferred 14.50 warning remains. Generated reports, logs, screenshots, binaries, and intermediates are not committed.

## 1. Final status

**COMPLETED**

All implementation, automated Lab capture, performance matrices, NullRHI and D3D12/SM6 gates, full SightWeave and DARKWELL regression coverage, BuildPlugin, source-only clean-host builds, Shipping isolation scans, repository integrity checks, and the user-operated interactive PIE inspection are complete. Automated capture and direct agent image inspection remain recorded separately from the user's final manual evidence.

One full-suite D3D12 M2P2 wall-time sample exceeded its historical strict median threshold by 1.52%. The exact failure is retained below. Its single permitted isolated confirmation passed without changing the gate, so this is classified as full-suite load sensitivity rather than an M3.5 correctness failure.

## 2. Identity and machine

- Branch: `codex/m3p5-sightweave-static-environment-memory`
- Frozen M3.4 baseline: `22f55b1e744cea37ad5d3c7beb618be0509fbf99`
- Final implementation checkpoint before documentation closure: `b6102fa`
- Engine: Unreal Engine 5.8.1 at `D:\UE_5.8`
- OS/RHI: Windows 11, D3D12 SM6 and NullRHI
- GPU: NVIDIA GeForce RTX 4060, driver 610.88, 8188 MiB
- CPU/RAM: Intel Core i5-13500, 32 GiB

The approved local `Darkwell.uproject` EngineAssociation GUID difference remains uncommitted. `/Game/Maps/L_Prototype` was not modified. No generated build, report, cache, or temporary-package directory is committed.

## 3. Delivered behavior

- CPU-packed HardMemory is the only exploration authority: 7,688 bytes per allocated logical tile, exact non-hash scope identity, immutable revisioned packets, no-change revision suppression, and fail-closed capacity handling.
- ClearMemory, BlockMemoryWrites, and SuppressMemoryPresentation obey the frozen ordering and do not turn the GPU, camera, viewport, Scene Color, lighting, or feathering into memory-write authority.
- A persistent sparse R8 GPU mirror consumes authority packets and fails black on invalidation, scope mismatch, stale generation, or missing binding.
- Explicit static-environment eligibility and its neutral attribute mirror are separate from dynamic subjects and current lighting.
- The final composite ordering is exact HardLive Scene Color, then remembered eligible static environment, then strict black Unknown. M3.4 point-gated live semantics and Width=50 inward feathering remain intact.
- M3.5 Lab fixtures cover remembered, live, clear, block, suppress, unknown, teardown, page-boundary, and scale cases without changing `/Game/Maps/L_Prototype`.

## 4. Automation results

| Gate | RHI | Exported report | Exact result |
| --- | --- | --- | ---: |
| M3.5 final | NullRHI | `Saved/AutomationReports/M3P5_Final_NullRHI` | 14 succeeded, 0 warning, 0 failed |
| M3.5 final | D3D12/SM6 | `Saved/AutomationReports/M3P5_Final_D3D12` | 24 succeeded, 0 warning, 0 failed |
| Full SightWeave | NullRHI | `Saved/AutomationReports/M3P5_FullSightWeave_Final_NullRHI_Retry` | 164 succeeded, 0 warning, 0 failed |
| Full SightWeave | D3D12/SM6 | `Saved/AutomationReports/M3P5_FullSightWeave_Final_D3D12` | 246 succeeded, 1 succeeded-with-warning, 1 failed; 248 performed |
| M2P2 4096 isolated confirmation | D3D12/SM6 | `Saved/AutomationReports/M3P5_M2P2_Prepared4096_Isolated_D3D12` | 1 succeeded, 0 warning, 0 failed |
| DARKWELL | NullRHI | `Saved/AutomationReports/M3P5_Darkwell24_Final_NullRHI` | 24 succeeded, 0 warning, 0 failed |
| Four-tier precision | D3D12/SM6 | `Saved/AutomationReports/M3P5_MemoryPrecision_FinalGate_D3D12` | 4 succeeded, 0 warning, 0 failed |
| Selected Coarse | D3D12/SM6 | `Saved/AutomationReports/M3P5_SelectedPrecision_D3D12` | 1 succeeded, 0 warning, 0 failed |
| Resident/dirty scale | D3D12/SM6 | `Saved/AutomationReports/M3P5_MemoryScale_D3D12` | 3 succeeded, 0 warning, 0 failed |
| Frozen M3.4 Width=50 live matrix | D3D12/SM6 | `Saved/AutomationReports/M3P5_FrozenLivePresentation_D3D12` | 9 succeeded, 0 warning, 0 failed |
| Selected Lab capture | D3D12/SM6 | `Saved/AutomationReports/M3P5_LabVisual_Coarse_D3D12` | 1 succeeded, 0 warning, 0 failed |
| Packaging boundaries | NullRHI | `Saved/AutomationReports/M3P5_PackagingBoundaries_NullRHI` | 1 succeeded, 0 warning, 0 failed |

The full D3D12 suite's retained failure is `SightWeave.M2P2.Performance.PreparedEventIndex4096`: median 1,015.197 us against the frozen `< 1,000 us` gate. The isolated confirmation produced 985.600 / 1,519.501 / 1,563.501 / 1,584.601 us median/p95/p99/max and passed 1/1. No sample was deleted and no threshold was changed.

The full D3D12 suite's one passed-with-warning case was `SightWeave.M2.Query.EffectiveLive.OcclusionHeightAndFloor`, carrying the engine `LogRHICore` warning that 258 GB reserved virtual resource size exceeded a 256 GB budget. It is retained as an engine warning, not silently suppressed.

## 5. Performance and memory

The canonical tables, cold/warm methodology, and exact per-case values are in `Docs/SIGHTWEAVE_M3P5_PERFORMANCE.md`. The frozen four-tier run selected Coarse 25 cm/texel:

| Tier | CPU dirty p95 | RT total p95 | GPU dirty p95 | Decision |
| --- | ---: | ---: | ---: | --- |
| Ultra 2.5 cm | 1,391.299 us | 742.000 us | 246 us | reject CPU/RT |
| Fine 5 cm | 458.401 us | 351.101 us | 249 us | reject CPU/RT |
| Standard 10 cm | 82.098 us | 71.898 us | 335 us | reject GPU |
| Coarse 25 cm | 73.601 us | 97.401 us | 33 us | selected |

The later complete M3.5 D3D12 gate independently sampled selected Coarse at CPU dirty p95 51.800 us, GT publish p95 1.397 us, RT total p95 65.997 us, GPU dirty p95 82 us, no-change p95 0.101 us with zero mirror work, and worst plugin runtime estimate 36,462,592 bytes.

Resident/dirty evidence covers 1/1, 8/8, and 128/32. CPU authority is 7,688, 61,504, and 984,064 bytes respectively. GPU mirror persistent memory is 4,194,320, 4,194,432, and 8,390,656 bytes. The pressure row retains its 32 dirty tiles expanding to 48 gutter-neighbor uploads rather than relabeling it as a reference case.

The frozen M3.4 Width=50 live GPU p95 matrix passed at 1080p with 85/347/489 us for 2-source/1-tile, 8/8, and 32/128; at 1440p it passed at 353/424/667 us. Additional 1080p dirty1, dirty8, and continuous cases were 371/718/36 us. Persistent live GPU memory peaked at 18,697,216 bytes; live transient output is reported separately as 8,294,400 bytes at 1080p and 14,745,600 bytes at 1440p.

## 6. Lab evidence

The authoritative transition record is:

```text
remembered:1 live:1 clear:0 block:0 suppressed:1/1 unknown:0
```

The final complete D3D12 run also passed `SightWeave.M3P5.Visual.LabCapture`. The agent directly inspected the resulting `Saved/Screenshots/M3P5_PIE_Overview.png`: the live region retained white/gray Scene Color, remembered static structure remained neutral, and unknown/clear/block/suppress regions were strict black; cyan content was limited to authored Lab boundaries and markers. No dynamic-subject, current-lighting, or viewport-history leakage was visible.

The user subsequently completed the interactive M3.5 PIE inspection for Camera 0-4 and reported all five cameras passed:

- Camera 0: live Scene Color, the 50 cm inward feather, remembered gray, and strict-black Unknown were correct.
- Camera 1: remembered neutral-gray static structure displayed correctly; clear and presentation-suppressed areas remained black.
- Camera 2: no dynamic door, moving mesh, current light, or VFX leaked into non-live remembered areas.
- Camera 3: the page boundary was continuous, with no black or bright seam.
- Camera 4: yaw=45 showed no visible misalignment or tile/page seam.

The final observed log state was `memoryReady=1 staticEnvironmentReady=1 memoryPresentationAvailable=1 memoryScopeMatchesBinding=1 memoryScopeMismatchMask=0x00 memoryProfiles=1 liveProfiles=1`, followed by `submitted-feather bindingFailure=0`. The PIE startup's initial `bindingFailure=13` occurred before resource publication and was the expected fail-closed transition; recovery to healthy `submitted-feather` means it is not a validation failure. Epic DataRouter/libcurl/OpenSSL warnings were telemetry-network failures and unrelated to the SightWeave composite.

With this user-operated evidence, the milestone is `COMPLETED`.

## 7. BuildPlugin and clean-host

- Final BuildPlugin package: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM3P5_BuildPlugin_Final2_20260827_1758`
- Result: `BUILD SUCCESSFUL`, AutomationTool exit 0, 3m42s.
- Editor Development: 94 actions, succeeded.
- Game Development: 30 actions, succeeded.
- Game Shipping: 30 actions, succeeded.
- Source-only clean host: `C:\Users\defiler919\AppData\Local\Temp\SightWeaveM3P5_CleanHost_Final_20260827_1740`
- Initial source-only clean build: Editor 94, Game Development 30, Game Shipping 30 actions, all succeeded.
- After the final three test-only repairs, clean-host Editor rebuilt 6 actions successfully; Game Development and Shipping each confirmed up-to-date with 0 actions and `Result: Succeeded`.
- Repository, final BuildPlugin package, and clean host have 115/115 matching SHA-256 files across `Source`, `Shaders`, and `Content`: 0 missing, 0 mismatch, 0 extra in either comparison.
- Final repository-standard `Scripts/BuildEditor.ps1` returned `Result: Succeeded`; target was independently up-to-date with 0 actions. The preceding source checkpoint build compiled the final changed tests successfully.

The initial clean-host M3.5 report retained 23 successes and one Lab failure because an engine Recast warning expected exactly once did not occur on the clean host. The expectation was repaired to accept the environment-dependent warning without weakening gameplay or visual assertions. `M3P5_CleanHost_LabRetry_D3D12` then passed 1/1, so the combined clean-host M3.5 gate is 24/24. The original failing report remains preserved.

## 8. Shipping isolation

- Shipping object count: 30.
- Shipping module directories: exactly `SightWeaveRender` and `SightWeaveRuntime`.
- Exact binary-string hits were zero for test readback types, `FRHIGPUTextureReadback`, M3.5 test/performance identifiers, AutomationTest, SightWeaveTests, SightWeaveEditor, Darkwell, UnrealEd, and LastSeen.
- COFF forbidden-symbol hits: 0 files.
- Forbidden linker directive/import hits: 0 files.
- The generic text `SceneCapture` occurs in five render objects only as UE engine type/debug metadata such as `ESceneCaptureSource`, `ESceneCaptureCompositeMode`, `bIsSceneCapture`, and `SceneCaptureCopySceneDepth`. There is no SightWeave SceneCapture symbol, implementation, module dependency, or authority route.

## 9. Preserved warnings and severe-log scan

Final M3.5, full SightWeave, and isolated-confirmation D3D12 logs have zero matches for Shader compiler errors, RDG validation errors/warnings, D3D12/RHI errors, GPU crash/hang/device removal, fatal errors, assertions, or ensures.

Preserved diagnostics:

- 13 pre-test `LogAutomationTest: Error: Condition failed` lines during D3D12 engine startup; they precede individual test execution.
- one navigation serialized-maxTiles warning;
- two console-variable/scalability warnings;
- the 258 GB versus 256 GB `LogRHICore` reserved virtual resource warning noted above;
- UE engine-header C4996 deprecation warnings during clean plugin builds;
- Visual Studio 14.51.36256 is newer than UE's preferred 14.50.35717 toolchain;
- the full-suite M2P2 1,015.197 us median failure, followed by the retained isolated 985.600 us pass.

## 10. Checkpoints

M3.5 commits after the frozen baseline:

```text
d922a6e docs: start SightWeave M3P5 static environment memory
507c759 docs: define SightWeave M3P5 memory contract
0f05a37 feat: add SightWeave exploration memory authority
5714f8a feat: add SightWeave memory modifiers
b1ddc42 feat: mirror SightWeave exploration memory
ebdb269 feat: composite SightWeave static environment memory
8807385 test: validate SightWeave memory safety
9fa833e test: add SightWeave M3P5 lab coverage
a6c9e7d perf: measure SightWeave exploration memory
f2dd2ae test: validate SightWeave M3P5 packaging boundaries
6e3f354 test: tolerate clean-host Lab engine warnings
9565857 test: classify M3P5 Lab capture as non-null RHI
b6102fa test: preserve regressions across M3P5 Lab expansion
```

The documentation closure commit is the branch tip containing this file. The final handoff performs and records `git status`, local/upstream/remote SHA equality, diff checks, Git LFS checks, and Git object integrity after pushing that tip.

## 11. Home recovery

```powershell
git fetch origin
git switch codex/m3p5-sightweave-static-environment-memory
git pull --ff-only
```

The final interactive PIE gate has already been completed by the user; no additional M3.5 validation action remains. No shutdown, sleep, or restart is part of this handoff.

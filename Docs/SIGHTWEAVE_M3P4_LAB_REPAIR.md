# SightWeave M3.4 integrated Lab repair

Status: **COMPLETED — company D3D12/SM6 and NullRHI retest passed**

Branch: `codex/m3p4-sightweave-lab-repair`

Baseline: `4d9070999c6a45227560a99c681e05707dfdd3f9`

## Failure reproduced

The company UE 5.8.1 Editor rendered the M3.4 Lab almost entirely black. The result remained black after launching with:

```text
-ini:Game:[/Script/SightWeaveRuntime.SightWeaveSettings]:VisualFeatherWidthCentimeters=0.0
```

This proves the failure occurs in the shared M3.3 hard presentation closure rather than the M3.4 VisualFeather transform.

The page-boundary source used Y=11000 cm. With Ground minimum Y=-6500 cm and a Standard logical span of 2480 cm, logical rows 6 and 7 meet at Y=10860 cm. At 160000 cm range and a 0.2-degree half angle, the cone endpoint half-width is about 558.5 cm, so the AABB spans both rows. Sixty-five columns times two rows is 130 tiles, above the intentionally frozen capacity of 128. Capacity failure is therefore correct fail-closed behavior.

## Repair contract

- CPU Authority, deterministic triangles, immutable packet revisions, HardLive, atlas formats, and the 128-tile capacity are unchanged.
- The page-boundary fixture is centered at Y=12100 cm and remains entirely within logical row 7.
- The existing checked-in map is repaired transiently in PIE; no binary `.umap` is edited outside Unreal Editor APIs.
- Default Lab mode is M3P4. `-SightWeaveLabMode=M3P3` selects M3.3 and `-SightWeaveLabMode=M2` selects M2 for manual work.
- M3P4 enables only `SW_M3P4_*` authority components plus the shared `SW_M3P3_PageBoundary*` fixture. Historical M2 and broad M3P3 sources are disabled for that PIE world.
- The overview camera is assigned with `SetViewTarget` once the PIE player controller exists. Later manual camera changes remain possible.
- `SightWeave.Lab.Camera <0-4>` switches PIE cameras by stable Actor label instead of UE's transient internal object name: `0` overview, `1` closeup, `2` dynamic door, `3` page boundary, `4` rotated 45 degrees.
- All isolation code is in `SightWeaveEditor`; Game and Shipping builds do not contain it.
- `Project Settings > Plugins > SightWeave` is explicitly registered by the Editor module.

## New diagnostics

The editor log emits:

```text
LogSightWeaveEditor: Lab isolation mode=M3P4 ...
LogSightWeaveEditor: Lab camera bound mode=M3P4 actor=SW_M3P4_OverviewCamera
LogSightWeaveEditor: Lab render packet healthy desiredTiles=<N> capacity=128 ... presentation=enabled
```

Any remaining capacity or packet error instead emits:

```text
LogSightWeaveEditor: Warning: Lab render packet fail-closed failure=<code> desiredTiles=<N> capacity=128 ...
```

## Validation available in this environment

- `git diff --check`: required before commit.
- Static regression: corrected cone endpoints remain in row 7; old placement reproduces 130/128 capacity failure.
- Packet-builder regression: M3P4 overview plus corrected page fixture fits capacity; old placement fails with `CapacityExceeded`.
- Fixture-routing regression covers M2, M3P3, M3P4, and the shared page fixture.
- Settings registration regression requires `Project/Plugins/SightWeave` to exist.

No UE 5.8.1 toolchain or D3D12 RHI is available in the cloud workspace, so an Editor build and visual proof cannot be claimed here.

## Company retest closure

The company UE 5.8.1 machine rebuilt the Editor and exercised the repaired path in user-operated PIE. The Lab reported a healthy packet with 113 desired tiles under the frozen 128-tile capacity and submitted both `submitted-hard` and `submitted-feather` with `bindingFailure=0`, resource generation 3, residency generation 114, and 113 page-table entries/resident tiles.

Interactive screenshots covered 0, 50, and 100 cm from `SW_M3P4_CloseupCamera`, followed by `SW_M3P4_PageBoundaryCamera` and `SW_M3P4_Rotated45Camera`. The hard edge, narrow inward ramp, and wider inward ramp were visibly distinct. Black SceneColor remained black outside HardLive, the page view showed no bright seam, and the rotated view showed no periodic wave or outward expansion. Development cyan/green/thin fixture overlays are drawn after the composite and are not Mask leakage.

Focused automation closed without hidden failures:

- D3D12/SM6: `37/37`, `ExitCode=0`, report `Saved/AutomationReports/M3P4_Repair_D3D12_20260827_133152`, log `Saved/Logs/M3P4_Repair_D3D12_20260827_133152.log`;
- NullRHI: `8/8`, `ExitCode=0`, report `Saved/AutomationReports/M3P4_Repair_NullRHI_20260827_133521`, log `Saved/Logs/M3P4_Repair_NullRHI_20260827_133521.log`.

The D3D12 filter includes `Width0BitExact`, geometry/seam safety, negative/large coordinates, slot 63/64 page crossing, 21 performance cases, settings registration, fixture isolation, bounded transform, binding isolation, safety contract, and Shipping-boundary checks.

## Company retest

Preserve any local `Darkwell.uproject` EngineAssociation edit; do not commit it.

```powershell
cd D:\UE_projects\LastLight
git fetch origin
git switch codex/m3p4-sightweave-lab-repair
git pull --ff-only
git status --short --branch
```

Close Unreal Editor, build `DarkwellEditor Win64 Development`, then launch UE 5.8.1 and open `/SightWeave/Maps/L_SightWeave_Lab`. Press Play without using `viewactor`; the repair binds `SW_M3P4_OverviewCamera` automatically.

Confirm in Output Log:

1. `Lab isolation mode=M3P4`;
2. `Lab camera bound ... SW_M3P4_OverviewCamera`;
3. `Lab render packet healthy` with `desiredTiles <= 128`, `capacity=128`, and `presentation=enabled`;
4. no later `Lab render packet fail-closed` warning.

Then compare 0, 50, and 100 cm from `Project Settings > Plugins > SightWeave` (restart PIE after each config change if the live default object does not refresh):

- 0 cm: visible pixels retain SceneColor and the edge is hard;
- 50/100 cm: only the visible interior fades near the edge;
- HardLive outside remains strictly black;
- camera pan/rotation/zoom does not change world-space width;
- the page boundary has no bright seam.

For a readable close-up comparison while PIE is running, enter `SightWeave.Lab.Camera 1` in the Output Log `Cmd` field. Use `SightWeave.Lab.Camera 0` to return to the overview. Unlike `viewactor`, this selector resolves the checked-in Actor label and does not depend on a transient `CameraActor_N` object name.

M3.4 is `COMPLETED` on the repaired branch. Any future `L_Prototype`, gameplay, memory/Last-Seen, D3D11/Vulkan, or distribution integration remains a separately authorized milestone.

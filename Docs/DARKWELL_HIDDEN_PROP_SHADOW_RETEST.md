# 第一阶段：真实柜子隐藏投影复测

状态：**PARTIAL — READY_FOR_USER_HIDDEN_PROP_SHADOW_RETEST**。用户人工确认仍待执行；没有开始第二阶段，没有选定最终模式。

起点为 `f882f54d797faf9cb09864be583ebac8ffe66a3d`，分支 `codex/darkwell-prop-memory-gameplay-lab`。开始时 HEAD / upstream 一致、工作区和 LFS 干净；Git Tree 与可靠基线 `6322d9a3068f9038ab35c62d7f9135e6b594bcba` 一致。已撤销的 `0032533` 不恢复。

## 唯一运行时改动

`ADarkwellManualStaleRoom::SpawnActualCabinet()` 在原柜子完成生成后，仅对 `Memory->GetMemoryPrimitives()` 的三个既有部件调用 `SetCastShadow(true)` 和 `SetCastHiddenShadow(true)`。这些标记在该实际柜子的整个生命周期内保持不变。

- ABSENT：原 Actor 和部件销毁，当前真实阴影源为 0。
- PRESENT 但未显示：原 `ApplySourceLiveState` / `ApplySourceGeometryVisibility` 继续控制主通道隐藏；三个原部件继续投影。
- 正常显示：同一 Actor、同一组部件恢复原显示，没有第二套投影几何，也不切换阴影源。
- 灰色代理：沿用原 `RebuildProxy` 的 `SetCastShadow(false)`；未赋予隐藏投影。
- 阴影标记不调用快照、Observe、验证或清除接口；StableID、碰撞、材质、Transform、Scale、Bounds、尺寸和网格资产不改。

UE 5.8 的 `UPrimitiveComponent::ShouldComponentAddToScene` 和 `FPrimitiveSceneProxy::IsShadowCast` 明确允许 `CastShadow && bCastHiddenShadow` 在现有 `SetVisibility(false)` 下投影，因此不需要改变项目隐藏路径。

**没有新增资产、柜子几何、代理盒、组件或呈现系统；没有做裁切、封口、渐进显露或抗锯齿。** Mode 0/1/2 的原有主体及记忆显示规则完全保留。只在手动房间的实际柜子生成路径启用，其他家具、正式地图和 SightWeave 公共合同不动。

## 接收面限制

原 `M_PropLabSurface` 在合法覆盖区域使用 Lit BaseColor，在记忆区域使用不受光照影响的 Emissive。全部材质保持原样。真实影子只有落在现有受光接收区域时才能看见，不会强行把整个灰色记忆地面变成受光材质；“允许隐藏投影”不等于无论朝向都必须看到影子。用户复测应观察柜子侧前方、靠近玩家的已覆盖地面。

## 验证

标准构建：`Scripts/BuildEditor.ps1`，`DarkwellEditor Win64 Development`，非 Live Coding。

```powershell
& 'D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UE_pro/Darkwell/Darkwell.uproject' -unattended -nullrhi -nosound '-ExecCmds=Automation RunTests Darkwell.PropLab.ManualSwitch.HiddenShadowSameGeometry+Darkwell.FogVisual.RememberedProp+Darkwell.PropLab.Scope+Darkwell.FogVisual.GrayUnlit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/UE_pro/Darkwell/Saved/AutomationReports/HiddenShadow01' '-abslog=D:/UE_pro/Darkwell/Saved/PropGameplayLab/HiddenShadow/Automation01.log'
```

Build01 成功；Automation01 共 **5/5 通过、0 失败、0 带警告测试、0 未运行**。新增 `HiddenShadowSameGeometry` 在 Mode 0/1/2 各运行两个存在性循环，验证真实部件数 3、原生槽位数 12、未用槽位不投影、代理不投影、隐藏/显示复用相同部件指针、Actor/部件 Transform、Bounds、Dimensions、网格资产及 AppearanceRevision 不变，并检查未观察重生不改快照或空置证据。

GPU03：**D3D12 / SM6，RTX 2070 SUPER，正常 TSR (`r.AntiAliasingMethod=4`)、100% 渲染比例，原生 1920×1080**。两个完整存在性循环，**136 条状态记录、136 张原始 PNG，逐张尺寸核对通过**。两轮隐藏到显示分别捕获 57 / 58 张连续帧，记录中各发生一次原有的主体显示转换，整个序列保持同一 Actor 和三个相同部件路径；再次离开视野后仍为同三个隐藏投影源。

实际打开并查看了两个循环的同站位 ABSENT / 隐藏 PRESENT / 正常显示对照图，以及各 9 张跨越显示转换的相邻帧。ABSENT 时灰色记忆保留，但附近受光地面没有当前柜子阴影；隐藏 PRESENT 时同一块地面出现真实阴影；本体按原规则显示后影子继续存在，未观察到复制或因切换投影源造成的跳变。柜子从灰色变绿色仍是原整件规则，没有将其误报为渐进显露。

同站位 `(4840,330,92)`、朝向 `270°` 的固定地面 ROI `(1008,520)-(1032,546)` 共 624 像素：两轮 ABSENT 的 RGB 各通道均低于 65 的暗像素均为 **0**，隐藏 PRESENT 均为 **600**。这只是保留原像素的对照测量，不是新的可调验收门限；视角旋转时手电位置与受光覆盖会正常变化，不要求整张阴影逐像素不动。

证据仅在本机忽略目录：

```text
Saved/PropGameplayLab/HiddenShadow/PIE_20260831_211726/
  checks.json / geometry.json / review.json
  shadow_state_pairs.png
  cycle0_transition_adjacent.png
  cycle1_transition_adjacent.png
  0001_...png 到 0136_...png（全部原始帧）
```

同目录的接触图是从原生 PNG 裁剪拼接用于审阅，未替换原图。原有异步截图器在下一渲染帧消费请求，标签为请求时状态，可能与图像相差一帧；因此以真实连续图检查阴影延续，以原组件路径和自动化检查几何/主通道规则，不声称像素和遥测同帧锁步。截图会增加 GPU 读回开销，本轮不作帧率验收。

复现：用 `UnrealEditor.exe` 加 `-d3d12 -sm6 -nosound -unattended -NoVSync -PropLabAsyncCapture -ExecutePythonScript="D:/UE_pro/Darkwell/Content/Python/verify_manual_hidden_shadow.py"` 启动；等 `HIDDEN_SHADOW_READY`，通过官方 `EditorAppToolset.StartPIE` 启动 `PlayMode_InEditorFloating`。脚本完成后恢复玩家/控制器 tick、结束 PIE、恢复播放窗口设置并退出测试编辑器。没有运行旧四项 Mode 2 矩阵。

## 失败与警告原样保留

所有本阶段产物仅存于忽略的 `Saved/PropGameplayLab/HiddenShadow/`，不覆盖此前阶段的证据。

- GPU01：初始可见状态断言失败。角色保留了旧鼠标目标，即使 Controller tick 暂停仍会在 Character tick 中旋转；第一张图/状态记录为隐藏柜子，不能当作初始观察成功。仅修改测试驱动的瞄准点输入，不改变运行时逻辑。该轮 PNG 为 1920×1082，后续按真实 viewport 差值调整 PIE 窗口；没有缩放旧截图冒充 1080p。
- GPU02：Python 的 `CurrentAimPoint` 是只读属性，测试驱动不能写入，启动检查失败。没有修改属性权限或运行时代码；驱动改用公开的 `SetActorTickEnabled` 暂停玩家与控制器的瞄准更新，测试结束恢复。世界、权威、房间、灯光和组件继续真实 tick。这是受控 GPU PIE，不替代用户人工输入复测。
- 引擎启动、工具链和第三方工具的原有警告保留；测试通过数不代表引擎日志无警告。

本阶段构建共 **1 次成功**，自动化共 **1 轮 5 项全部通过**，GPU 尝试 **3 次：2 次采集驱动失败、1 次完成两循环并通过实际画面检查**。

| 日志 | Warning 行 | 启动 `Condition failed` 行 | 其他 Error 行 | Fatal / 崩溃 |
| --- | ---: | ---: | ---: | ---: |
| Build01 | 5 | 0 | 0 | 0 |
| Automation01 | 3 | 13 | 0 | 0 |
| GPU01 | 7 | 13 | 2 | 0 |
| GPU02 | 10 | 13 | 2 | 0 |
| GPU03 | 9 | 13 | 1 | 0 |

构建为 MSVC 14.51 非首选版本、3 条 Character `GetMovementBase` 和 1 条 AISystem `CleanupWorld` 引擎头文件弃用警告。UE 每次启动的 13 条 Condition failed 独立于后续 5 项项目测试，未隐藏。GPU03 唯一其他 Error 是编辑器重启后 MCP 旧 session id 失效，随后重连成功；9 条 Warning 涉及原阴影 CVar 优先级、MCP EULA、NavMesh、MotionVector 线程标记、Crowd、Shot 查询性能和退出时 HTTP 请求取消。原柜子表面的局部自阴影噪声也在截图中保留，本阶段不扩修材质或抗锯齿。

## 人工复测

打开 `/Game/Maps/L_ProjectFogPropGameplayLab`，点击 Play。

```text
Darkwell.PropLab stalemanual reset
Darkwell.PropLab stalemanual mode 2
Darkwell.PropLab stalemanual teleport top
```

看完整柜子，让快照有效。然后执行：

```text
Darkwell.PropLab stalemanual teleport bottom
```

走入圆盘使柜子 ABSENT，退出圆盘。沿右侧走廊返回上房，保持瞄准远离柜子，观察其侧前方已覆盖地面：灰色记忆可以保留，但不能有当前柜子的真实影子。建议站在柜子屏幕左下侧、靠外墙的一边，面朝下房；GPU 对照站位为 `(4840,330,92)`、朝向 `270°`，此处柜子覆盖为 0，玩家身边的圆形受光区与影子相交。**后续返回不要用 teleport top，它会自动朝向柜子。** 再回到圆盘退出并重新踩入，使柜子 PRESENT，保持相同朝向与站位回上房；真实本体仍隐藏，此时现有受光地面允许出现影子。最后把视野转到柜子，检查原本体正常显示、阴影没有复制或切换跳变。完整重复两轮。

本阶段故意保留原来的整件显示与残影表现，不以其是否渐进出现、封口或边缘平滑作为本阶段验收。用户确认前不进入第二阶段。

## 修改文件

1. `Source/Darkwell/Private/VisionPresentation/DarkwellManualStaleRoom.cpp`：唯一运行时改动，5 行。
2. `Source/Darkwell/Private/Tests/DarkwellSightWeaveAdapterTests.cpp`：本阶段回归测试。
3. `Content/Python/verify_manual_hidden_shadow.py`：编辑器专用双循环 GPU 驱动。
4. `Docs/DARKWELL_HIDDEN_PROP_SHADOW_RETEST.md`：范围、证据、失败和人工复测说明。

无 `.uasset` / `.umap`、项目文件、构建配置或插件变更。没有提交 Saved、截图、录像、Binaries、Intermediate 或 DDC。

# 手动柜子 Mode 2：实心封口与空间重新显露

状态：**PARTIAL — READY_FOR_USER_MODE2_SOLID_REVEAL_RETEST**。用户真实 PIE 复测仍待执行，未选择最终模式。

基线 `6322d9a3068f9038ab35c62d7f9135e6b594bcba`；原分支 `codex/darkwell-prop-memory-gameplay-lab`。开始时 HEAD / upstream 一致且工作区干净。本轮不新建分支，不重做旧自动路线，不改变正式地图、默认 0/0、SightWeave 插件合同或 Mode 0/1。

## 实现与边界

- **边缘**：手动房间 Mode 2 独占 `UDarkwellMode2SolidComponent`。灰色残影从同一份 DisplayedOpacity 重建封闭几何，以 2.5 cm 三角化和 C2 样条消除硬纹理像素边界，再由正常 TSR 抗锯齿。没有随机抖动透明掩码、扩大视野、降低遮挡精度或追加观察等待。
- **封口**：柜体、门和把手都是已知轴对齐盒体；按原各部件真实边界构造顶面、底面、外侧面与切面。黑色 Opaque/Unlit 材质只绑定切面，灰色表面也写深度。已清空位置不生成黑色平面，不使用地面贴片。新动态几何全部无碰撞、无投影、无导航和间接光贡献。
- **隐藏投影**：原真实柜子的三个部件始终是唯一真实投影源；在 Mode 2 设置 `CastHiddenShadow` 和 `HiddenInGame`，不篡改原组件 `Visibility` 缓存。柜子销毁后投影源同时消失。显露网格不投影，因此完整显露时也不会切换或叠加第二套阴影。Mode 2 地面单独使用 Lit 接收材质；退出 Mode 2 恢复原地面绑定。实际灯光、遮挡和源部件位置决定阴影。
- **重新显露**：对当前真实部件的每个空间位置调用原 `EvaluateLiveCoverageAtWorldPoint`，使用 `.99` 合法边界生成裁切几何。每个位置的独立 0.20 s alpha 从首个合法帧开始增长，失去合法覆盖立即关闭 Live 显示；不是先等 0.20 s，也不是整件 alpha。原真实几何始终隐藏，显示网格只覆盖合法区域。全局 `.50/.25` 对象 Live 阈值不控制空间显露。离开视野后仍按原记忆政策显示快照。

权威仍使用原 10 cm 占用格、四角加中心、合法覆盖 `.99`、无遮挡、实际空置、连续 `.10 s`，单帧贡献上限 `1/30 s`。本轮不改 StableID、Observe、快照捕获、碰撞、验证、清除时机或压力开关。`ApplyErasure` 仍独占原 `.20 s` 完成后的清除调用。适配层最后仅补一次渲染隐藏，防止普通 Live 写入引起一帧整件泄漏。

样条采用向残留侧收缩的 `.8` 视觉等值面，直边约内缩 4 cm；这是局部边缘重建，不修改权威覆盖。一个已完全清空的格子即便被八个完整残留格包围，其内部样条值也不超过约 `.7704`，不会被黑色体积填回。边缘仍来源于有限 10 cm 数据；微小残片和薄结构可能被这层视觉内缩削弱。该方案明确只绑定这个柜子的盒状占用体积，**不是任意旋转、凹形、带洞或复杂生产网格的通用封口器**。

新显示材质沿用实验原有 Unlit 颜色/法线明暗风格；动态阴影由真实部件和独立接收地面提供。初次尝试 Lit 半透明显示出现自阴影噪声，已根据实际截图撤换。地面仅在 Mode 2 接收真实灯光，因此与 Mode 0/1 的原 Unlit 地面亮度不同。

## 用户复测

打开 `/Game/Maps/L_ProjectFogPropGameplayLab` 后点击 Play，在活动 PIE 控制台执行：

```text
Darkwell.PropLab stalemanual reset
Darkwell.PropLab stalemanual mode 2
Darkwell.PropLab stalemanual teleport top
```

1. 瞄准左上柜子，看到完整绿色柜子与 `Snapshot: VALID`。
2. 输入 `Darkwell.PropLab stalemanual teleport bottom`。落点在圆盘之外；向前走进圆盘一次，确认 `actual=ABSENT`。不要连续站着等待第二次切换，必须退出再进入。
3. 保持鼠标指向柜子外侧，沿右侧走廊返回上房，再慢慢扫过旧位置。**此时不要用 teleport top**：它会自动面朝柜子，可能在控制台关闭前已经完成验证。中途停住：残留灰色体积应有黑色剖面；擦掉一侧是正常地面，不应有柜子阴影。
4. 扫完整个足迹，确认 `Snapshot: EMPTY`、Verified 100%。回下房退出圆盘再踩入，让柜子 PRESENT。
5. 保持朝下或背离柜子，沿走廊回到上房（同样不用 teleport top）：柜子本体不可见，但地面允许提前有真实阴影。慢慢从一边扫向柜子，先出现窄片，然后逐步扩展；注意是否整件弹出、闪烁或穿过隔墙显露。
6. 看完整件后转身离开，再返回。完整重复两轮。分别在 1920×1080、2560×1440 下做静止瞄准和 WASD 移动扫描。

`reset` 保留当前 Mode；正式规则仍未选定。Mode 0/1 原行为保留，可切换比较，但本轮验收目标仅 Mode 2。手动房间仍无计时、自动移动、强制瞄准或自动结束。

## 可复现工具和证据

标准构建：`Scripts/BuildEditor.ps1`，目标 `DarkwellEditor Win64 Development`，非 Live Coding。

相关自动化：

```powershell
& D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe D:/UE_pro/Darkwell/Darkwell.uproject -unattended -nullrhi -nosound '-ExecCmds=Automation RunTests Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/UE_pro/Darkwell/Saved/AutomationReports/Mode2Solid03' '-abslog=D:/UE_pro/Darkwell/Saved/PropGameplayLab/Mode2Solid/Automation03.log'
```

最终 Build05 成功。Automation03 共 **26 项通过、0 失败、0 未运行**：25 无警告，1 原有预期重复 Fixture 警告；包括原 23 项及新增 `SpatialRampAndLegalGate`、`ContourDoesNotWriteEvidence`、`TwoCyclesAndModeIsolation`。新增测试包含两个状态全循环、Mode 0/1 隔离、无重复投影、真实部件数、空间部分帧和已清空格禁黑。

资产生成使用 `Content/Python/create_manual_mode2_materials.py`，通过 Unreal Editor Python API 创建四个独立材质，没有覆盖原材质或任何地图。

GPU 运行使用 `Content/Python/verify_manual_mode2_solid.py`，参数 `-d3d12 -sm6 -NoVSync -PropLabAsyncCapture -Mode2Width=1920` 或 `2560`，以 `-ExecutePythonScript` 载入。等日志 `MODE2_SOLID_READY` 后，用官方 `EditorAppToolset.StartPIE` 启动 `PlayMode_InEditorFloating`。脚本临时暂停测试 Controller 的鼠标更新，以真实世界时间连续移动/瞄准；结束恢复输入、结束 PIE 和恢复窗口设置。这是受控真实 GPU PIE，不能代替用户人工游玩。

正常 `r.AntiAliasingMethod=4`、`r.ScreenPercentage=100`；无降级 AA、无 NullRHI 截图、无 HighResShot、无图像缩放冒充标准分辨率。异步 PNG 写入沿用已有 Lab 捕获器；所有生成物仅留在忽略的 Saved。离线工具 `Scripts/ReviewManualMode2Solid.py` 核对每张 PNG 的实际尺寸，输出状态矩阵、扫描序列、连续帧、像素变化和帧时，不删除失败样本。

最终 GPU：RTX 2070 SUPER，D3D12 / `PCD3D_SM6`，正常 TSR、100% 原生渲染。两次运行均完成两个“消失—验证—重新出现—重新发现”循环，合计 **4 循环、1393 条状态记录、694 张原始 PNG、0 尺寸错误**。每个分辨率都包括静止封口、移动擦除、移动重新显露和无截图开销的移动段。已实际打开两个分辨率的状态矩阵、连续显露帧、连续擦除帧，并单独打开 1440p 原始封口截图；没有把状态断言等同于视觉检查。

| 最终运行 | 原生视口 / 每张 PNG | 状态记录 | 原始 PNG | 两循环部分显露帧 | 相邻帧绿色面积最大增长 / 完整体面积 | 静止封口 ROI 最大像素变化 |
| --- | --- | ---: | ---: | --- | --- | --- |
| GPU1080_08 | 1920×1080 | 836 | 374 | 105 / 112 | 4.080% / 4.174% | 0.128% |
| GPU1440_04 | 2560×1440 | 557 | 320 | 80 / 85 | 5.552% / 5.658% | 0.0902% |

这些数值是保留原帧的描述性审计，不是临时降低的验收门限。截图显示灰色残留为封闭实体，黑面只在剖面；验证后原位置露出正常地面。ABSENT 状态真实投影部件数为 0，PRESENT 为 3；未显露时已经有灯光决定的影子，显露过程仍由同一组源部件投影。连续帧显示绿色窄片沿扫描方向扩展，没有观察到整件弹出、双影、穿墙或状态反复切换。封口边缘仍有少量正常 TSR 像素变化，不声称逐像素完全静止。

无截图移动段的帧时中位数 / P95 / 最大值：1080p **22.800 / 24.594 / 26.398 ms**；1440p **24.862 / 27.617 / 32.029 ms**。逐帧截图会额外读回 GPU：1080p 扫描段两循环中位数 46.27 / 47.79 ms、最大 84.53 ms；1440p 中位数 59.74 / 61.04 ms、最大 137.60 ms。慢帧全部保留，不能据此宣称锁定 30/60 FPS。相邻截图标签记录请求时状态，原异步捕获器并非同帧同步遥测；测试切段已给渲染线程消费窗口，避免下一段传送污染最后一张图。

本机证据目录（均在 Git 忽略范围内，不提交）：

```text
Saved/PropGameplayLab/Mode2Solid/PIE_1920_20260831_202911/
Saved/PropGameplayLab/Mode2Solid/PIE_2560_20260831_202636/
```

各目录包含原始 PNG、`checks.json`、`review.json`、`state_matrix.png`、`cycle{0,1}_reveal_adjacent.png` 和 `cycle{0,1}_erase_active_adjacent.png` 等派生审阅图。完整构建、自动化、材质生成和 GPU 历史日志保留在 `Saved/PropGameplayLab/Mode2Solid/`；自动化明细保留在 `Saved/AutomationReports/Mode2Solid01`、`02`、`03`。

日志审计 `FinalLogInventory.json` 精确计数如下；“启动 Condition failed”发生于引擎启动，不属于后续已完成的 26 项测试，单独报告而不隐藏：

| 最终日志 | Warning 行 | 启动 Condition failed 行 | 其他 Error 行 | Fatal / 崩溃 |
| --- | ---: | ---: | ---: | ---: |
| Build05 | 7 | 0 | 0 | 0 |
| Automation03 | 5 | 13 | 0 | 0 |
| GPU1080_08 | 8 | 13 | 1 | 0 |
| GPU1440_04 | 9 | 13 | 1 | 0 |

构建警告是 MSVC 14.51 非首选版本与 6 条 UE GeometryFramework / Chaos 头文件弃用。GPU 的其他 Error 均为编辑器重启后 Unreal MCP 旧 session id 失效，随后自动重连成功。其余警告包括原有阴影 CVar 优先级、Unreal MCP EULA、NavMesh tile 数、缺少 Recast 的 Crowd、MotionVector CVar 线程标记、Shot 控制台查询性能提示；1440p 另有一次 HTTP 连通性超时。没有将这些日志改为全绿。

本轮标准构建累计 **5 次：4 成功、1 失败**；相关自动化累计 **3 轮 × 26 = 78 次：77 通过、1 失败**。最终检查点是 Build05 和 Automation03；失败的 Build01、Automation02 以及全部早期 GPU 失败仍可复查。

## 保留的失败与限制

- Build01：`TObjectPtr` 范围迭代的 `auto*` 类型推导编译失败；显式组件指针修正。Build02/03/04/05 成功，未使用 Live Coding 作为最终证据。
- Automation01：26 通过。Automation02：25 通过、1 失败，边界对称性断言捕获 float 累加误差；以 double 权重/累加修复，断言和容差未放宽。Automation03：26 通过。
- GPU1080_01/02：播放设置的 Python 类型/属性不可见；GPU1080_03：私有 memory_primitives 字段不可见；均是采集脚本失败，未算 GPU 验收通过。
- GPU1080_04：状态脚本完成两循环，但实际打开截图发现 Lit 柜顶噪声、裁切台阶、带窗口边框；保留为视觉失败，不用脚本 PASS 覆盖视觉失败。
- GPU1080_05：已修正表面噪声并完成两循环，但 PNG 实测 1920×1082，离线尺寸断言失败。后续修正的是实际窗口请求，绝不重采样旧图冒充 1080p。
- GPU1080_06：异步 PNG 尚未完成，过早文件检查失败；增加仅测试驱动的写盘等待，玩法时间不变。
- GPU1080_07：371 张精确 1920×1080、两循环状态通过，但扫描末张请求被紧接着的测试传送覆盖，像素面积审计出现约 53%/56% 的虚假跳变。旧图和数值保留；在扫描结束与测试传送之间留出一个渲染消费窗口，随后使用 GPU1080_08 重采。这个等待只在测试脚本，不在玩法代码。
- GPU1440_01/02/03：实际客户端分别为 2560×1362、2560×1422、2560×1422，尺寸断言拒绝。UE 的 WindowsWindow 实现说明需要避免系统窗口尺寸限制；测试脚本仅对本进程的 PIE 窗口使用相应窗口尺寸标志，按实际视口差值调整，不改桌面分辨率。GPU1440_04 已取得真实 2560×1440 PNG。
- 原实验的所有历史失败、警告和证据不删除。当前实验不扩修引擎启动 Condition failed、工具链建议、引擎弃用、NavMesh/CVar 提示等已识别问题。
- 全画面静止像素差仍包含原手电/墙面亮度变化；不能用全画面差分直接声称封口闪烁。封口区域和逐帧空间变化单独检查，原全画面数据保留。
- 盒状代理、有限 2.5D 采样和轻微内缩仍是实验限制。用户真实 PIE 的主观运动稳定性与最终 Mode 选择仍未验收。
- 本次只有一个柜子的 GPU 检查，未声称验证了大量家具同时动态重建的性能。

## 修改文件

共 17 个文件；其中 4 个新 `.uasset` 通过 LFS 管理。没有修改任何 `.umap`、原 Mode 0/1 材质、SightWeave 插件、`Darkwell.uproject` 或正式规则配置。

```text
Source/Darkwell/Darkwell.Build.cs
Source/Darkwell/Public/VisionPresentation/DarkwellManualStaleRoom.h
Source/Darkwell/Private/VisionPresentation/DarkwellManualStaleRoom.cpp
Source/Darkwell/Public/VisionPresentation/DarkwellMode2SolidComponent.h
Source/Darkwell/Private/VisionPresentation/DarkwellMode2SolidComponent.cpp
Source/Darkwell/Private/VisionPresentation/DarkwellRememberedPropSubsystem.cpp
Source/Darkwell/Private/Tests/DarkwellEmptyVerificationTests.cpp
Source/Darkwell/Private/Tests/DarkwellSightWeaveAdapterTests.cpp
Content/Darkwell/Vision/PropLab/M_ManualMode2Cut.uasset
Content/Darkwell/Vision/PropLab/M_ManualMode2Floor.uasset
Content/Darkwell/Vision/PropLab/M_ManualMode2Live.uasset
Content/Darkwell/Vision/PropLab/M_ManualMode2Memory.uasset
Content/Python/create_manual_mode2_materials.py
Content/Python/verify_manual_mode2_solid.py
Scripts/ReviewManualMode2Solid.py
Docs/DARKWELL_MODE2_SOLID_REVEAL_RETEST.md
Docs/DARKWELL_MANUAL_STALE_SWITCH_ROOM_PIE.md
```

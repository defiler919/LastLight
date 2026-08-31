# 累计空间记忆状态机修复

起点：`0ecac64aab6318ca66634bea2865dc8c753c5f01`，分支 `codex/darkwell-prop-memory-gameplay-lab`。

**用户真实 PIE 对上一轮的判定：FAILED — FIXED GEOMETRY PASSED / SPATIAL MEMORY STATE MACHINE FAILED。用户录像结论高于旧自动化和报告。** 旧报告的 READY 只代表当时交给用户复测，不代表本次累计状态语义已经通过。

开始检查：`git status --short --branch` 干净；HEAD / upstream 均为上述基线；`git diff --check` 无问题；`git lfs status` 无待提交或待推送对象。没有 `.uproject` 差异或未知修改。开始时未发现 UnrealEditor / UBT / dotnet 进程。

## 修改前只读审计

1. **Current LiveCoverage 直接控制绿色裁切。** `M_ManualFixedReveal` 的实际表达式是 `Ready > .5 && Raw >= .99 ? saturate(Soft) : 0`。主通道提交原网格以后，OpacityMask 仍由本帧 Raw 硬门控；没有累计已发现纹理。手动 `BindPresentation(..., Mode=0)` 又让原表面的 `LabWholeObject=1`，使通过遮罩的表面始终是绿色 Live，不能在同一个局部表面转成灰色。
2. **覆盖一离开就归零是代码明确规定的，不是 TSR 猜测。** `M_ManualFixedRevealRamp` 为 `Raw >= .99 ? min(1, Previous + Step) : 0`；表面遮罩还会再次检查 Raw。任一帧失去覆盖，软历史和主通道像素同时消失，没有离开时的绿色→灰色过渡。
3. **一点观察触发整件灰色。** `EvaluateMaximumCoverage()` 取三个部件各四角和中心的最大值，不是覆盖面积。任意样本跨过对象级 `.50` 门限即可 `snapshotValid=true`，`CaptureSnapshot/RebuildProxy` 保存三个完整部件。失去对象 Live 时 `bShowProxy=true`；`AttachObservedSnapshot()` 将 `DisplayedOpacity` 全部初始化为 1。未验证为空的灰色代理完全不受“实际观察过哪里”约束。
4. **.20 秒是逐位置的进入渐变，但没有持久记忆。** 同一位置一旦 Raw 掉到门限以下就被清零，重新进入重复从零开始。另有实例/模式切换清 RT。不存在独立的 DiscoveredPresent 或退出渐变状态。
5. **缺少累计发现状态和退出柔化两者。** 原 `FDarkwellEmptyVerification` 的逐位置空位证据及 `DisplayedOpacity=min(...)` 已有单调擦除基础，但它从整件快照的全 1 状态开始，无法表达只发现了 10% 的记忆。不能用更长进入延迟、放宽覆盖门限或整件淡入解决。

## 本轮设计边界

在 DARKWELL 手动房间中新增独立、可直接测试的逐位置状态：CurrentLegalCoverage、DiscoveredPresent、VerifiedEmpty、InitialRemembered、RemainingStale，以及不承担知识判断的 AppearanceBlend / LiveBlend / StaleOpacity。状态由合法覆盖写入；同一 PRESENT 世代 D 只能增加，同一 ABSENT 世代 V 只能增加、R 只能减少。短退出保持及 .18 秒绿色→灰色仅作用于已发现位置，不能发现未观察位置。

PRESENT 新世代清除当前发现和 Live 渐变；尚未验证的旧残影作为独立冻结知识保留，不借用上一世代的发现/验证数组。ABSENT 冻结当时真正已知的区域，后续空位验证只减不增。对象级快照允许保留，但源材质与已有代理各自受局部掩码约束，避免重复表面及首次接触整件灰色。

保留三件原始真实组件、既有代理、世界空间遮罩和同源完整隐藏阴影；不修改 Transform、Scale、Bounds、角点、顶点、StableID 或 SightWeave 公共合同。无新渲染组件、无 WPO、无几何重建。Mode0/1 和正式地图不改。黑色剖面和抗锯齿阶段未开始。

本轮尚在实施；后续检查点、准确验证计数、失败日志、视觉证据与最终 SHA 将在这里追加。用户人工交互 PIE 始终是独立待验项目，不能由脚本统计替代。

## 检查点 1：独立累计模型（尚未接入呈现）

新增 `FDarkwellSpatialPropMemory` 和四项直接逐位置测试：PartialDiscoveryPersists、BoundaryOscillationAndExit、EmptyVerificationNeverResurrects、GenerationIsolation。原三个组件和材质路径此检查点尚未修改，原用户失败不能在此时宣称修复。

`Saved/PropGameplayLab/AccumulatedMemory/Build01.log`：标准 Editor Development 构建成功。`Automation01.log`：13 项中 12 成功、1 失败。失败为退出速率的 float 舍入：理论单步 .092592597，实际状态相减 .092592657，超出旧断言约 5.2e-8（小于 FLT_EPSILON=1.192e-7）。断言改为允许一个 float 精度单位，仅处理运算舍入；没有修改 `.99` 覆盖、.20/.18 秒过渡、单调性、空位确认或任何行为门限。原失败日志保留。

`Build02.log` 标准构建成功；`Automation02.log` **13/13 通过，0 失败、0 带警告测试、0 未运行**。此时先提交推送独立模型和审计报告作为可靠检查点，再继续接入，避免把全部修改堆在最后才保存。这个检查点没有宣称用户失败已经修复。

## 检查点 2：原始材质与已有代理接入

检查点 1 已提交推送为 `d29720e`。本检查点在手动 Room 中以原组件固定 Bounds 只读建立 2.5cm 状态坐标网格；不修改任何 Bounds。每格四角及中心都要达到原合法覆盖 `.99`，不放宽覆盖。RGBA16F 状态纹理：R=已发现表面的持续显露程度，G=局部 Live/灰色转换，B=仍未验证的旧代理区域，A=当前覆盖。真实源和旧代理的 R/B 同格互斥。材质默认 SpatialReady=0；代理在第一次提交到场景前已绑定局部掩码，snapshotValid 不能让未知区域整件变灰。

首次发现立刻启动 .20 秒逐位置显露；D 不因离开覆盖而清零。绿色退出有 1/30 秒视觉保持，然后 .18 秒转灰。已知表面的不透明程度不减，只有颜色改变。ABSENT 时冻结真正已发现范围，空位证据用现有 .10 秒保守确认，V 单调增加，R=Initial*(1-V)，旧代理 .20 秒只减不增。重新 PRESENT 清除本世代 D/V/Live，仅保留未验证旧残影。对象级 Observe、快照与清除逻辑、StableID、碰撞不变。

`M_ManualFixedReveal` 仍在三个原始组件上运行，ShadowPassSwitch 的 Shadow=1 保持不变；世界坐标排除 ShaderOffset，无 WPO。新增 `M_ManualAccumulatedMemory` 仅绑定已有代理，不新建渲染几何。Mode0/1 使用原材质分支及原代理路径；正式地图和插件未修改。旧的当前帧 Soft RT 不再驱动手动 Mode2。

`Materials01.log`：UE Python 资产迁移成功，0 错误/2 条汇总警告（项目阴影 CVar 优先级、MCP EULA 提示），没有保存地图。`Build03.log` 标准构建成功。`Automation03.log` 13 项：11 无警告成功、1 带警告成功、1 失败；新增真实状态断言发现旧几何夹具没有 PlayerController，实验 Tick 找不到玩家，未推进 Room 观察状态。修复夹具为显式传入原测试玩家调用同一运行时 UpdateObservation；不放宽断言。外部 google generate_204 超时警告保留。

`Build04.log` 标准构建成功；`Automation04.log` **13/13 通过（12 无警告、1 带警告），0 失败、0 未运行**。481 个角度逐帧检查三个原始组件 Transform、Local/World Bounds、8 角点、全部 LOD0 世界顶点、12 个原有槽位指针和 3 个同源阴影投射组件。新增同一扫描内逐位置 D 单调、未知区域不可见、已知表面不退回地面、源/代理互斥、部分发现及退出保持灰色断言。已有隐藏投影两循环、Mode0/1/2 和隔离测试保留。

引擎启动日志中的 13 条 `LogAutomationTest: Error: Condition failed` 在未修改呈现的 Automation02 中也存在，发生于测试队列之前；不将它们删除或计作本套件成功。完整原日志保留。C++ 构建保留 UE 头文件 C4996 弃用警告。此检查点仍待 GPU 连续帧核验，不是用户人工通过。

## GPU 发现的失败与修复（保留原样本）

检查点 2 为已推送 `df1b5e0`，其 NullRHI 资产结构测试不等于 SM6 编译通过。`GPU01.log` / `PIE_20260831_231209/failed_checks.json` 首次真实运行失败：两份新接线的材质使用向量参数默认 RGB 输出，再取 BA；SM6 报 `Not enough components ... float3 ... component mask 0011` 并回退默认材质。`Materials02.log` 通过 UE Python 把这两个坐标连线修为明确 RGBA→BA，未改几何、遮罩数值或覆盖门限；资产生成脚本同步修正。新增材质结构断言要求读取 A 的节点确实连入 RGBA，防止该错误再次只在 GPU 暴露。

同次驱动还把场景中其他家具的所有 `Remembered_` 代理混计为手动柜子，触发数量断言。驱动改为按手动柜子的 StableID 名称计数，同时记录其他代理并保留“所有代理不投影”的检查；并非允许额外柜子。首轮未取得有效截图，不记为视觉通过。

`Build05.log` 标准构建成功。`Automation05.log` **13/13 通过（12 无警告、1 带外部 HTTP 超时警告），0 失败、0 未运行**。GPU01 的原失败和警告不删除，修复后的真实 GPU 结果另外记录。

`GPU02.log` 不再有两份材质的 SM6 编译错误。实际打开 `PIE_20260831_231534/0000_initial_full00000.png`：原尺寸绿色柜子和同源阴影正常；这一张只能证明完整状态，不能证明累计转换。驱动完成三方向空位校准，但在第一次重新出生帧停止：旧脚本假定主组件尚未提交时 SpatialReady 必须为 0；新 Room 同帧已绑定 D=0 的纹理，下一权威 Tick 才提交原组件，因此 Ready=1 并非泄露。修正为只允许该唯一新 Actor 首帧隐藏，且直接要求 D=0、本体遮罩=0、Live=0；后续帧仍强制三件原始组件正常提交。原失败 JSON 保留。额外代理核实为原自动房间已有 75 个部件；手动快照恰好 3 个，已清空快照为 0，所有代理均无真实阴影。

## 实际 GPU 与逐帧检查结果

状态：**PARTIAL — READY_FOR_USER_ACCUMULATED_SPATIAL_MEMORY_RETEST**。这是修复后交给用户人工判断的检查点，不是 Mode2 最终完成，也不覆盖用户对基线的失败判定。

| 运行 | 原生分辨率 / 渲染 | 完整循环 / 方向 | 截图 | 固定几何逐帧检查 | 直接逐格知识检查 | 结果 |
|---|---|---|---:|---:|---:|---|
| GPU03 / PIE_20260831_231710 | 1920×1080 / D3D12 SM6 / TSR=4 / ScreenPercentage=100 | 3 / 左→右、右→左、斜向 | 83 | 1,442 | 38,321,536 | 驱动无失败；人工交互待验 |
| GPU04-1440 / PIE_20260831_232053 | 2560×1440 / D3D12 SM6 / TSR=4 / ScreenPercentage=100 | 1 / 左→右补充 | 38 | 511 | 13,332,352 | 驱动无失败；人工交互待验 |

两次成功运行各自的材质编译失败数为 **0**；无新增可渲染组件、无原网格/Transform/Bounds/角点变化。当前 PRESENT 恰好三个原组件投影，ABSENT 为零，旧代理始终不投影。运行期间有 3,210 / 1,118 条逐 Tick 遥测，记录 actual、当前覆盖、D/V/R、Live、本体/代理遮罩、快照、世代、toggle 和原组件路径。每格知识位不是按平均比例推断：同世代 PRESENT 逐格检查 D 不减及新 D 必须有合法覆盖；ABSENT 逐格检查 V 不减、R 不增、已验证格不可残留。C++ 扫描额外直接比较全部 LOD0 世界顶点，而 Python 的实时检查比较原网格引用、Transform、Bounds 和角点。

1080p 三次重现前均 D=0、SourceOpacity=0、ProxyOpacity=0、snapshot=false，隐藏真实阴影仍在；每次 10/25/50/75% 停留后转头，D 和 SourceOpacity 保持已发现范围，Live 最终归零成灰。三次空位擦除到 25/50/75/100%，转头后 V/R 不逆转；结尾 generation=8、toggles=7、ABSENT、V=1、R=0、snapshot=false。1440p 完成同样一轮补充及首次进入的五个相邻请求帧。

下面 contact sheet 和原帧均已实际用图像工具打开，**没有仅统计文件数**。根目录为 `D:/UE_pro/Darkwell/Saved/PropGameplayLab/AccumulatedMemory/`，不提交到 Git。

- `PIE_20260831_231710/left_discovery_contact.png`、`right_discovery_contact.png`、`diagonal_discovery_contact.png`：10% 只有原表面与遮罩交集出现；转头仅该部分留灰；25/50/75% 累积扩展，未看到首次接触就出现整件灰柜子。固定门把手及顶面接缝未滑动；窄条是裁切交集，不是缩小后的整件柜子。
- `left_exit_adjacent_contact.png`：原帧 `0007`–`0011`，t=22.234–22.524，D 恒为 .24983288，Live .1960→.1363→.0217→0→0，原表面保持可见并变灰。
- `right_exit_adjacent_contact.png`：原帧 `0038`–`0042`，t=47.730–48.033；`diagonal_exit_adjacent_contact.png`：`0063`–`0067`，t=71.387–71.669。均看到局部绿色向灰色转换，未返回空地。
- `oscillation_adjacent_contact.png`：`0019`–`0024`，24 次 ±.25° 摆动中的六个相邻请求帧。已知完整柜子中约 25% 保持绿色、其余灰色；没有整件绿/灰交替或 D 重置。
- `left_erasure_contact.png`、`right_diagonal_erasure_contact.png`：25/50/75% 的剩余灰色区域和转头对照；完整验证后为正常地面，无残影复活或 ABSENT 柜子阴影。
- `shadow_contact.png`：两次相同位置 ABSENT/PRESENT 隐藏对照，柜子主通道不可见；身体附近合法地面上的柜子阴影仅在 PRESENT 出现。结合每帧同三源和 ShadowPassSwitch=1 断言，没有生成第二套投影几何。
- `PIE_20260831_232053/entry_adjacent_1440.png`：`0004`–`0008`，t=11.896–12.288，D 从 .00008356 到 .01128008，SourceOpacity 从 .0000128 到 .0085416；看到原表面的局部渐显，随后达到约10%，没有整件弹出。
- 同目录 `exit_adjacent_1440.png`、`discovery_1440.png`、`erasure_1440.png` 也已打开，重现保持灰色与空位单调擦除。

所有原帧保持原生分辨率；contact sheet 只是带遥测标签的截图裁剪。截图请求与 GPU 像素不是严格同步时间戳；连续截图有捕获开销，相邻请求间隔约 40–130ms，不能据此宣称正常实时帧率下所有动态闪烁已消除。已看样本没有重现用户的整件早记忆/退回地面/反复重置，但用户鼠标瞄准、自由行走和主观节奏仍须真实人工 PIE 验证。

## 最终构建、自动化、警告与限制

所有 UBT/dotnet/Editor 构建和 commandlet 串行，启动前检查残留进程。标准命令为 `./Scripts/BuildEditor.ps1`（DarkwellEditor Win64 Development），共 **6 次成功、0 次失败**；最后独立 `Build06-final.log` 成功，4 个构建 action，16.78 秒。Build01/02/03/04/05/06 的 UE 头文件 C4996 警告分别为 5/0/9/3/3/3；没有修改或压制这些警告。

| 自动化运行 | 无警告成功 | 带警告成功 | 失败 | 未运行 |
|---|---:|---:|---:|---:|
| Automation01 | 12 | 0 | 1 | 0 |
| Automation02 | 13 | 0 | 0 | 0 |
| Automation03 | 11 | 1 | 1 | 0 |
| Automation04 | 12 | 1 | 0 | 0 |
| Automation05（最终源码/资产） | 12 | 1 | 0 | 0 |

共执行 65 项次：63 通过、2 失败；失败均已说明并保留。最终是 **13/13**，包含 4 个新增状态模型测试和 9 个相关既有测试；不是 65 个不同测试。最后构建只是重编译同一已验证代码，没有后续运行时代码改动。

可复现的套件参数：`-ExecCmds="Automation RunTests Darkwell.PropLab.SpatialMemory+Darkwell.PropLab.ManualSwitch+Darkwell.FogVisual.RememberedProp+Darkwell.PropLab.Scope+Darkwell.FogVisual.GrayUnlit" -TestExit="Automation Test Queue Empty" -unattended -nullrhi -nosound`。详细结果在 `Saved/AutomationReports/AccumulatedMemory01` 至 `05`。GPU 驱动为 `Content/Python/verify_accumulated_spatial_memory.py`，用 `-d3d12 -sm6 -PropLabAsyncCapture -ExecutePythonScript=...` 启动后通过 Editor 原生 StartPIE；`-Accumulated1440` 只运行一个补充循环。驱动结束恢复输入与窗口设置并停止 PIE。

保留的其他警告/错误：每个成功 GPU 日志各 7 条 Warning（阴影 CVar 优先级、MCP EULA、MotionVectorSimulation 的 RenderThreadSafe 警告、RecastNavMesh/CrowdManager 缺失、Shot 的 FindConsoleObject 性能提示）；重启 Editor 后 MCP 旧会话产生一次 Unknown session id，工具重连后成功启动。每次引擎启动的 13 条 Condition failed 原日志保留；非 Win64 可选 SDK 校验提示也保留。未将这些日志删掉、改为静默或通过重试掩盖。

限制：仍是手动家具实验的固定 XY 占用网格，不是任意复杂网格、薄结构或大量家具性能方案。2.5cm 边界台阶、时间抖动颗粒及开放切面可见；**黑色封口和抗锯齿专项尚未开始**。Mode0/1、正式地图（包括 L_Prototype）、SightWeave 公共合同、StableID 和正式模式默认值未修改。房间原 HUD 的旧空位百分比仍是对象级验证字段；新增逐位置字段以 GetSpatialTelemetry 与逐格数组为准。

## 修改范围与用户复测

相对基线的运行时 C++ diff：**268 行增加 / 14 行删除，净 +254 行**（含注释、空行和声明，按 git numstat；没有把测试和 Python 算作运行时）。运行时文件共 7 个；全部修改文件如下：

```text
Source/Darkwell/Public/VisionPresentation/DarkwellSpatialPropMemory.h
Source/Darkwell/Private/VisionPresentation/DarkwellSpatialPropMemory.cpp
Source/Darkwell/Public/VisionPresentation/DarkwellManualStaleRoom.h
Source/Darkwell/Private/VisionPresentation/DarkwellManualStaleRoom.cpp
Source/Darkwell/Public/VisionPresentation/DarkwellPropGameplayLab.h
Source/Darkwell/Private/VisionPresentation/DarkwellPropGameplayLab.cpp
Source/Darkwell/Private/VisionPresentation/DarkwellRememberedPropSubsystem.cpp
Source/Darkwell/Private/Tests/DarkwellSpatialPropMemoryTests.cpp
Source/Darkwell/Private/Tests/DarkwellSightWeaveAdapterTests.cpp
Content/Darkwell/Vision/PropLab/M_ManualFixedReveal.uasset
Content/Darkwell/Vision/PropLab/M_ManualAccumulatedMemory.uasset
Content/Python/update_accumulated_spatial_materials.py
Content/Python/verify_accumulated_spatial_memory.py
Docs/DARKWELL_ACCUMULATED_SPATIAL_MEMORY_RETEST.md
```

用户人工 PIE **仍待执行**。打开独立 `L_ProjectFogPropGameplayLab`，点 Play 后依次执行：

```text
Darkwell.PropLab stalemanual reset
Darkwell.PropLab stalemanual mode 2
Darkwell.PropLab stalemanual teleport top
Darkwell.PropLab stalemanual teleport bottom
```

第一次踩圆盘使其 ABSENT，手动走回上方完整验证空位。回圆盘前先离开使其重置，再踩入重新 PRESENT；背向柜子走回，先看隐藏阴影。**此时不要 teleport top，它会自动朝向柜子。** 缓慢左右及斜向转头，只扫约10%后转开，应仅该部分留灰；继续25/50/75%，重复转开并小幅摆动边界。再踩圆盘消失，分段验证空位并转开，应只减少、不恢复。至少重复两轮。黑色封口不是本轮验收项；不要把仍有阶梯边缘误认为该阶段已做 AA。

全部可靠检查点在当前分支提交后立即推送：`d29720e3b89793b1876c88e1b2b5820a630a6889`（独立状态与测试）、`df1b5e0f5e660461b2377400a2d3b0808467724f`（原几何材质/代理接入）、`f314cea3ef88663be73607f93ea2b816f6729de6`（GPU 暴露的 RGBA 连线修复与回归保护），以及本报告/已验证 GPU 驱动的交接提交。最后提交的完整 SHA 和最终 Git/LFS 闭合输出见交接消息。无截图、录像、Saved、Binaries、Intermediate、DDC 或自动化输出入库。

## Git 与 Editor 闭合记录

GPU 证据及驱动提交：`f2a1bc6512f760673361d2da80883ffeb3c489e1`。首次立即推送发生 `OpenSSL SSL_connect: SSL_ERROR_SYSCALL`，保留该失败；随后一次重试成功。闭合检查 `GitClosure01.json` 的八项命令全部 exit=0：status 干净，HEAD/upstream/ls-remote 均为 f2a1bc6 完整 SHA，diff --check 无输出，LFS 无待提交或待推送对象，LFS fsck OK。`git fsck --no-reflogs` 无 missing/corrupt，报告 49 个 dangling tree 和 14 个 dangling blob；全部保留，没有 clean、reset、revert、rebase、merge、force-push 或对象清理。本节是最后的纯文档提交，提交推送后再次执行同样八项检查，以交接消息给出的最终 SHA 为准。

独立标准 Editor 已重新打开 `/Game/Maps/L_ProjectFogPropGameplayLab`，没有验证脚本或暂停输入。MCP `IsPIERunning=false`；实际打开 `CaptureEditorImage` 确认房间可见、绿色 Play 可点击。`ReadyEditor.log` 记录该状态。没有改写地图或 Darkwell.uproject；电脑保持开启，没有执行关机、睡眠或重启。用户确认前停止于本阶段。

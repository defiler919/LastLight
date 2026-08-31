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

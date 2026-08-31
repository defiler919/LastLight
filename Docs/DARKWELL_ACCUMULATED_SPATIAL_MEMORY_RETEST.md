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

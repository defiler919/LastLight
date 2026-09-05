# Room05 再观察历史修复交接

进行中：已复现并定位，尚未修复或完成回归。本文件随后续检查点补齐。

起点 `bc96e98affefe354597d577f8e03ede85f86a2e0`，继续 `codex/darkwell-prop-memory-gameplay-lab`。
工作区最初干净，fetch 后本地/上游/远端相同，stash 为空，LFS fsck 正常。
两条 stable 仍为 `7534163b9c5718700b610e7677f47fbaa79cf977` 和 `404a5820739638f1097eaae0aa7fba19733298c3`。
引擎沿用 D:\UE_5.8，实际 5.8.2 CL 56702186，没有升级或修改资产。

## 用户复测事实

用户在已测试路线中：01 无问题；02 无内部接缝；03 无问题；05 左侧黄色长柜无问题；05 右侧蓝色柜存在旧残缺历史回退。
这些是路线级通过，不是全系统验收。运行时身份核对：蓝色 `Lab.V2.OcclusionWhole`，WholeObjectAfterSpan / StationaryOnly；黄色 `Lab.V2.OcclusionPartial`，SpatialPartial / StationaryOnly。

## 未修复证据

`Reobservation_BeforeFix` 普通宿主新测试失败（1 项，5 种步长，30/60/120/144 Hz 和 200 ms）。
独立已知长方体内部 28,288 样本中，完整捕获无缺失，但首次 H1 和完整观察后的 H2 各有 9,752 个 AA/提交像素缺失。
单独从完整观察开始的参考通过；连续路径与参考像素不等。

`Reobservation_Baseline03` D3D12/SM6、SP100、TSR4，原始游戏视口 2233×911，完成四步骤与四轮往返及独立参考。
蓝柜保持 (-450,7000,0)，玩家起点 (70,6000,92)，朝 115°；移开至 -90°；经墙缝走到 (70,6750,92)，朝 155°；再移开至 -90°。四步骤间没有 Reset、重新注册、策略/物体变化。
远处合法覆盖约 0.64，已确认；靠近为 1.0。H1 的捕获/记忆均 36,876，但零 AA 样本 13,073；H2 仍为相同 epoch、代理、纹理和 hash `11790327373081981827`。
独立直接完整参考零 AA 为 0，hash `6537765931154518915`。已打开四阶段、第二次退出 00/01 和参考原图。
前两次脚本采集因把纯文本诊断误当 JSON 而中断，未算通过；03 完成采集和 Stop PIE 后进程退出 `0xC0000005`，协议退出门失败，severe 日志 0。保留为上一轮已知退出问题的新复现。

## 最早失败位置

合法视野、确认、Whole 几何捕获均正确；`FDarkwellHistoryGridV2::Initialize(SealedMemory, CaptureMask)` 先从受墙遮挡的粗 Current 图像计算 FrozenAAEnvelope，再只用完整几何重写 InitialRemembered/Opacity/State，遗留错误零包络。
`BuildPresentation` 用 Opacity × FrozenAAEnvelope 输出，因此首次 H1 已永久残缺，并非完整历史被当前墙临时遮挡。
再观察获得完整 Live 后，复用检查发现几何捕获没有变化，合理保留 FineHistory，因而再次显示错误 H1。
实际可见代理已绑定对应纹理、Ready=1；不是错误代理选择、纹理上传遗漏或 GPU 延迟。没有反证/替代样本、对象运动或外观版本变化。

将针对 Whole 几何捕获建立独立细历史入口，不把 Current 的墙/锥边界或渐显包络作为永久 Whole 图像。Partial 的局部包络与现有同状态复用保持原合同。

# Unknown 局部区域第一生产切片（2026-09-07）

起点为已封板的 `checkpoint/sightweave-gray-performance-closure-20260907`，提交 `eeeec6506d1fecfd2d05bd08095b8230287a4d4f`。本报告、运行时代码与最终验证摘要同提交；最终提交号以 Git 为准。家里实际仓库 `D:\UE_pro\Darkwell`，引擎 `D:\UE_5.8`（实际 UE 5.8.2 CL56702186）。开始前核验远端开发分支与本地一致、工作区干净，读取 AGENTS 与最新灰色交接。

## 已完成的闭环

| 序列 | Whole | SpatialPartial |
|---|---|---|
| A：灰 → Clear → 黑 → 新合法观察 → 灰 | 通过；新 epoch | 通过；新 epoch，保留原 Partial 精度与切口 |
| B：灰 → Block → Live → 离开黑 → 解除 | 旧灰恢复 | 旧灰及 cap 恢复，旧 fine evidence hash 不变 |
| C：灰 → Clear+Block → 只 Live → 离开黑 → 解除 | 旧灰不复活；新观察才能重建 | 旧灰/cap 不复活；新观察才能重建 |

另外验证了 Live 中直接调用 Block/Clear：在同一 GT 调用内重新查询合法覆盖、发布当前显示，保留 Whole 当前 reveal 确认与已有 Live appearance blend。退出后仍无被禁止的捕获。纯 Vision 覆盖值和 authority revision 不受内存操作影响。新捕获只来自操作之后的 CPU 合法查询；如果 Clear 发生时仍合法可见，该查询可以立即建立新的观察，不能把这一点误解为旧 epoch 复活。

## Unknown 的权威来源

入口为 C++ `UDarkwellMemoryRegionSubsystem`：`ConfigureRegion(Min, Max)`、`ClearMemory()`、`SetBlockMemoryWrites(bool)`。Blueprint 可以调用这些入口，规则仍在 C++。必须等当前玩家/楼层 authority 与 Fog 已激活后才能配置；未就绪返回 false，不允许稍后的 Adapter 激活绕过核心 Block。

- 地面/环境沿用封板的 `RememberedFromStart` 初始合同，在所选区域物化 CPU packed bits。`ClearMemory` 置零；之后只有有效、同修订的 canonical legal coverage ≥ 0.99 可以重新置位。`QueryKnowledge` 返回 native Gameplay Tag `Darkwell.Knowledge.Unknown` 或 `Darkwell.Knowledge.Remembered`。这是地面记忆状态查询，Live 仍由原 Vision 系统独立判断。
- `HasStoredMemory` 区分保存的旧知识与当前可用灰色：Block 时旧 bits 可以存在，但 `QueryKnowledge` 返回 Unknown。解除 Block 不重新授予知识，只重新允许使用尚存 bits。
- 物体的权威仍是 `FDarkwellSpatialObservationHistory` / coarse cells / `FineHistory`。Clear 删除匹配的整个 record、capture mask、提交缓存和表现对象，并失效准备票据、local epoch / reusable Whole / 当前捕获缓存。epoch 单调增加，不复用已删除编号。
- 同步调用现有 SightWeave `ClearExplorationMemory` / `RegisterMemoryModifier(BlockMemoryWrites)`，避免插件备用 HardMemory 查询保留已清除区域或在 Block 中偷偷累积。使用当前 owner/floor/world-generation scope；scope 改变时拒绝继续写入或操作，不跨域套用旧区域。
- GPU 只接收 CPU 知识镜像。两个现有地面/环境父材质新增只读 gate，关闭 remembered emissive；不会制造 Live 或写回 Remembered。场景和纹理读回只用于自动测试，从不作为运行时知识输入。

## Clear 边界

首片只有 **同一 world、当前玩家/楼层域中的一个固定 XY AABB**。区域最大 640 × 640 cm，地面网格每轴最多 256 个样本、格距不大于 2.5 cm；查询/材质采用 `[Min, Max)`。插件 HardMemory 继续遵守其既有 texel-center 区域栅格合同。

物体按 **完整记录** 清除，包括该区域中的历史 pose；Whole 与已有 SpatialPartial 使用相同区域准入规则。区域必须完全包含相交物体和相交历史记录的 world bounds；切穿任一记录/实际物体时 `ConfigureRegion` 或 `ClearMemory` 返回 false，事务不部分执行。区域外记录证据、身份及地面初始合同保持不变。测试包含实际已建立灰色的区域外身份作对照。

这不是任意边界穿过物体的样本级擦除，也没有把一个 Whole 记录改造成 Partial。不同区域不能在同一实例上重新配置，重复相同区域幂等；生命周期到 world 结束，不是 SaveGame。

## Block 边界

Block 阻止区域内的 capture/seal、旧记录 resume 和永久 ownership 写入；已保存历史暂停证据推进并关闭灰色 proxy/cap。原有 Live 几何、合法照明、Whole reveal 和 Partial 显示路径继续工作。临时 Current 数据只供 Live，不能被封存为灰色；离开或解除 Block 时清理，避免后续 seal 带出被阻止期间的知识。

Block 激活前已有的合格 Current 先按旧合同封存，再隔离临时观察，因此仅 Block 不删除此前知识。资源逐出/重建仍受 CPU Block 约束；测试中 A0 rebuild 成功创建资源也不能把被 Block 的灰色显示出来。Clear 后旧 A0 票据直接失效。

本轮固定区域验证不覆盖任意动态跨边界玩法。运行时物体进入/穿过活动 Block 时对其整个临时 episode 保守阻写，不偷偷把 Whole 切成 Partial；发生 straddle 的 Clear 会拒绝。更细的空间边界必须另做切片。

## 验证与可移植证据

最终验证结果见 [Git 内摘要](Evidence/SIGHTWEAVE_UNKNOWN_REGION_20260907.json)。完整 `DarkwellEditor Win64 Development` 构建：

```powershell
& Scripts/BuildEditor.ps1 -EngineRoot D:\UE_5.8
```

最终构建日志 `Saved/UnknownRegion/Build10.log`，Result: Succeeded。使用官方 Unreal Editor Python 重建且仅重建 `M_DarkwellFogSurface` / `M_PropLabSurface`，脚本 `Content/Python/update_unknown_region_materials.py`；未用普通文件操作修改 Unreal 资产。

最终定向真实 D3D12/SM6：

```powershell
& Scripts/RunGrayObjectPolicyTests.ps1 -RunName UnknownRegionReadyFinal -Rendering -Tests 'Darkwell.UnknownRegion+Darkwell.SightWeave.Closure.VisionIlluminationBoundary+Darkwell.ObjectMemory.PresentationResidency+Darkwell.PropLab.GrayObjectPolicy.WholeObjectConfirmedStaticHistory+Darkwell.PropLab.GrayObjectPolicy.SpatialPartialStaticKeepsLegalCap'
```

共 6 项：Unknown Whole / SpatialPartial 两项各包含 A/B/C 与 Live 中切换；另有 Vision/照明边界、A0、Whole 灰色、Partial cap 四项回归。最终 **6/6 clean PASS，warning 0、severe 0、exit 0**（具体耗时见摘要）。逐阶段比较 RHI 的 CPU 知识纹理与 CPU bits；同时拍摄真实场景图，检查灰色、黑色、Live、恢复和无复活。

- [Whole 场景图](Evidence/SIGHTWEAVE_UNKNOWN_REGION_WHOLE_20260907.png)
- [SpatialPartial 场景图](Evidence/SIGHTWEAVE_UNKNOWN_REGION_PARTIAL_20260907.png)

每行依次为 A/B/C；每格标签给出阶段。末两格是补充的 Live 中 Clear+Block 与离开后不复活。Partial 使用能看清表面及切口的斜视角，Whole 使用俯视角；都没有改变实际 Vision。截图前等候资产编译完成，避免把默认材质当成证据。最终原图位于 `Saved/GrayObjectPolicy/UnknownRegionReadyFinal/Captures`，Git 内保存图表及逐图 SHA-256，换机器不要求 Saved 存在。

过程中的 NullRHI 两项功能通过；较早两次测试各有一次 UE 自带 Google 连通性探测超时 warning，与断言无关，未隐藏。第一组 Partial 场景截图不足以判断灰色切口，未作为最终视觉证据；后续等待资产编译并调整视角后已重新生成。没有用公司与家里的性能绝对数进行比较。

## 范围与下一步

没有新增 SuppressLiveVision、SaveGame、Monster Adapter、完整 region shapes 或性能专项。A1/P1/B0 仍默认 0；Synthetic cold184 未改未重测，**INITIALIZATION 仍 FAIL**。原 gray closure checkpoint 与 stable/tag 不移动。

下一最小切片建议：仍只用这一个 AABB，为 **边界切穿 SpatialPartial 的样本级 Clear/Block** 定义并验证精确边界；Whole 保持独立原子规则。先补空间边界，再考虑实际玩法触发器，不扩大形状或恢复性能专项。

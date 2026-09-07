# Unknown：固定 AABB 切穿 SpatialPartial（2026-09-07，未完成）

**本轮状态：CPU 样本级 Clear / Block 闭环和定向自动回归已通过；旋转切割的灰层细条纹尚未消除，D3D12 视觉验收未通过。不能宣布本生产切片完成。** 这是当前开发分支上的可恢复阶段成果，不创建生产 checkpoint，不移动已有 stable/tag。下一次应继续修复本片，而非开始下一黑色层功能。

环境为家里 `D:\UE_pro\Darkwell`，分支 `codex/darkwell-prop-memory-gameplay-lab`，起点 `26eb673876875faffaa4a136fb5943b10440bd76`。本次实际引擎 `D:\UE_5.8` 为 5.8.2 CL56702186；没有修改 AGENTS 的历史版本说明。运行时、测试、本文、交接和证据同本提交，最终 SHA 由本文件所属 Git 提交确定。所有新原始证据均在家里生成，未依赖公司 Saved。

## 已实现的权威边界

- `MemoryRegionSamples::Contains` 统一固定世界 XY AABB 的 `[Min, Max)` 样本中心归属。coarse 与既有 4×4 fine 独立判断；局部 primitive 样本先用当前 pose 转到世界坐标。边界落在格内时按原采样格量化，不把 AABB 扩成整 coarse cell，也不宣称几何无限精度。
- SpatialPartial Clear 对区域内 coarse 的 D / V / Initial / Remaining / StaleOpacity / EmptyDwell 清零；fine 重置为 `NeverObserved`，包括 InitialRemembered、Opacity、FrozenAAEnvelope、空置事实及过渡队列。捕获 mask 和同姿态暂存支持中的区域内 bits 一并删除。外部已有样本字段逐项保持。
- Clear 前封存正在 Live 的 episode，删除完成后使本地 epoch、捕获复用缓存失效；同 GT 使用原合法观察重新查询并发布 Live。旧缓存或 A0 重建只能读取清除后的 CPU 状态，不能重授 Remembered。
- Block 的 durable 写门作用于区域内样本。原合法覆盖和 Live blend 保留；旧 fine 状态与 fade 冻结，表现 hard A 关闭。解除只 Block 可以恢复旧知识；Clear+Block 后已删除的知识不会因解除而恢复。
- 跨 coarse/fine 边界时，用**原局部/coarse 合法观察**生成 CPU 暂存快照，再只允许区域外 fine 中心进入捕获，避免“coarse 中心在内”误伤同格外部 fine。没有增加 Vision 采样密度，没有使用 GPU 读回写知识。显示专用 Live/cap 临时值也不回写权威。
- 同姿态、同内容、同网格的合法旧 fine 支持只在未清除、未阻写、未被反证时保留到新 episode；pose 改变即清空暂存。已有区域外 AA envelope 不丢失。新合法观察可建立新 epoch，不能初始化回已清除的旧 epoch。
- Whole 保持原子规则：仍拒绝横切 Whole，完整包含时清整条记录。即便策略后来改为 Partial，已确认的 Whole 历史也不被样本切割。

## cap / cut

cap 由有效 CPU fine 边界生成，Block 内按不可贡献样本处理。同姿态跨 epoch 的剩余知识取有效并集，消除内部 epoch 接缝；封存时同帧刷新 fine ownership 与旧 cap，防止粗 Live ownership 缓存漏掉细边。Live 的暂时遮挡与永久 ownership 区分，Block 期间不永久吞掉旧灰。

旋转边缘占用查询复用现有几何 footprint 相交证明，防止中心在 OBB 外、格子却仍与实体相交的已知 fine 样本被误记 VerifiedEmpty。观察精度与空置/合法覆盖阈值没有降低。cap 仍按原 primitive 几何裁剪和原精度容差提交。

自动诊断通过：cap 顶点越界 0，gray/cap 最大贡献者均不超过 1；空闲连续 10 帧 texture/cap 签名稳定；旋转后清除区域没有旧 epoch InitialRemembered 复活；区域外已知 AA 支持无损失。

**上述结果不等于视觉通过。** 最终 A/B/C 的第 09 阶段，37° 旋转后再次在 Live 中 Clear+Block、离开、解除，灰色前表面仍有细竖条纹。第 10/11 阶段仅用于诊断：隐藏 cap 后条纹仍可见，cap-only 表面连续；两张图都不能替代第 09 阶段验收。现有证据支持继续检查灰层透明度/AA 合成，但尚未建立最终根因及安全修正。没有强制 alpha、删除 cap、降低精度、删除失败场景或允许晚一帧来获得 PASS。

![A/B/C 定向阶段图](Evidence/SIGHTWEAVE_UNKNOWN_PARTIAL_CUT_20260907.png)

![未解决的视觉问题及隔离诊断](Evidence/SIGHTWEAVE_UNKNOWN_PARTIAL_CUT_VISUAL_BLOCKER_20260907.png)

## 场景结果

| 场景 | CPU / GPU 镜像及生命周期结果 | 视觉结果 |
|---|---|---|
| A：已有灰 → 横切 Clear → 新合法观察 | 内部清空、外部字段不变；新观察可重建，旧 epoch 不复活 | 未旋转的内黑外灰、重建正确；后续旋转事务仍有条纹 |
| B：已有灰 → Block → Live → 离开 → 解除 | 内部新知识为 0，外部照常写；Live 同调用正常；解除旧灰恢复 | 未旋转的遮断/恢复正确；后续旋转事务仍有条纹 |
| C：Clear+Block → Live → 离开 → 解除 → 新观察 | Block 期间内部只 Live；解除及空闲不复活；新合法观察才重建 | 未旋转流程正确；后续旋转事务仍有条纹 |
| 旋转 37°、再次扫视及 Live 中事务 | 无 false-empty、无旧 pose 灰复活，区域外已知 AA 不减少 | **未通过最终无 seam/条纹要求** |
| Whole / 既有全包含 Partial | 原第一片 A/B/C、Whole 原子边界、既有灰色/照明/A0 回归通过 | 定向原场景未见新增回归，不代表完整矩阵 |

新观察的同条件 no-Block 对照：区域外 fine knowledge 差异 0、已知样本 FrozenAAEnvelope 差异 0。另有 32 个未观察、零 opacity、几何外样本的无效 AA 暂存值不同，已在证据中明确记录；没有将其当成合法知识或可见灰色。测试网格 coarse 60×34 / fine 240×136，保持原采样精度。

## 构建与无人值守验证

```powershell
& Scripts/BuildEditor.ps1 -EngineRoot D:\UE_5.8
& Scripts/RunUnknownPartialCutTests.ps1 -RunName UnknownPartialForegroundFinal
```

完整 `DarkwellEditor Win64 Development` Build24Final **Succeeded**，15.48 秒。最终运行 DLL SHA-256：`f7c280a69355c57878b841f7036aa426ca3017e168662dd11adaebdaf34ee399`。最终构建后没有继续改运行时 C++。

最终真实 D3D12/SM6 自动测试 **7/7 成功，6 clean + 1 warning，0 failed / not-run / severe**，44.323 秒。唯一 warning 为 UE 的 Google `generate_204` HTTP 探测 3 秒超时，不是规则断言失败，未为了消除 warning 重跑。

`RunUnknownPartialCutTests.ps1` / `run_unknown_partial_cut_tests.py` 复用仓库已有 `GrayBenchmarkSession` 与 `await_foreground`，启动上限 90 秒、最多 8 次激活且间隔 2 秒、连续 12 引擎帧前台后批准、测试上限 180 秒，批准后不再抢焦点，失败保存日志并有界结束本进程。最终运行启动时已在前台，激活尝试 0，退出码 0，防休眠 guard 已恢复。这里核验的是 Editor 应用前台，viewport 为 0×0；视觉证据来自测试独立世界的真实 D3D12 SceneCapture/纹理读回，**没有声称它是 PIE 游戏视口或性能测量**。无需人工点击。

7 项为：SampleCut、新旧 UnknownRegion Whole/SpatialPartial、VisionIlluminationBoundary、PresentationResidency、WholeObjectConfirmedStaticHistory、SpatialPartialStaticKeepsLegalCap。每次截图同时核对 fine hard A 与 CPU 提交一致，以及区域纹理与 CPU QueryKnowledge 一致。142 张原始图（含原合同回归与诊断）及日志在 `Saved/GrayObjectPolicy/UnknownPartialForegroundFinal`，Git 内保存阶段图、视觉阻塞图和 [哈希/测试/历次有界运行摘要](Evidence/SIGHTWEAVE_UNKNOWN_PARTIAL_CUT_20260907.json)。先前失败及诊断未删除。

## 交接与停止点

本轮有可提交的 CPU 样本权威闭环、回归测试和复现基础设施，因此按用户的无人值守阻塞约定保存阶段提交并 push 当前开发分支；**不是完整任务成功，也不是生产封板**。没有改资产或自动启用 region，没有扩新 shape、Monster Adapter、SuppressLiveVision、SaveGame 或性能专项。

下一最小工作仍是**本片旋转切割的灰层 AA/透明度条纹修复**：从保留的第 09/10/11 阶段、逐样本 AA 对比入手，保持原 knowledge、精度、cap 及 0 额外首显帧合同。只有该项获得真实 D3D12 无 seam 证据后，才能宣告本片完成，再讨论下一黑色层切片。

灰色封板 checkpoint 仍 `eeeec6506d1fecfd2d05bd08095b8230287a4d4f`；stable/tag 不动。A1/P1/B0 默认仍 0，Synthetic cold184 未改未重测，**INITIALIZATION 仍 FAIL**。

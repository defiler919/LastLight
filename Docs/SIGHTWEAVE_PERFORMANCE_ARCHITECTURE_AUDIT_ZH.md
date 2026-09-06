# SightWeave 灰色层性能架构审计

2026-09-06。本轮起点 `d986b536fbd21a0fb0c600577f9912369fe221a3`；本地、实时远端一致，工作树干净，LFS fsck PASS。本文先记录修改前模型，后续实验追加，不以优化假设代替结论。外部规则和性能门槛继续采用 `SIGHTWEAVE_GRAY_STABILIZATION_HANDOFF_ZH.md`。

## 1. 基线与可证范围

已有正式六组 before / 六组 after、143 项功能、四个视觉协议、54,000+600 同步长测和真实十分钟长测均保留，不为恢复任务重复运行。实际引擎 5.8.2 CL56702186，3900X / RTX 2070 SUPER，D3D12 SM6。旧原生 runner 的 NullRHI 单参数展开错误已修复；FinalFunctional03 和 FinalSoak01 实际使用 D3D12。

| 现有证据 | 直接说明 | 不能推出 |
| --- | --- | --- |
| Stabilization_FinalLongRun01，29,741 个 LongInteraction 帧 | wall p50/p95/p99/max 21.004/26.430/28.963/112.532 ms；57 帧 >33，2 帧 >100 | 未开启 Trace，不能事后声称已精确分解这 26.43 ms 的临界路径 |
| Optimization_AttributionMatrix01 | 184 setup 553.849 ms，首个 memory update 1949.427 ms，historical 1903.609、ownership 1493.731、cap 253.628 ms；几何查询 32,971,279 次 | 不等于后来每一组正式最大帧的同一调用栈 |
| FinalAttribution_Standalone01 | Empty GPU coverage 平均 .243 ms，TSR 5.345、LumenScreenProbeGather 3.485；SourceUpdate CPU 5.193；GPU SceneRender 19.550 | GPU/CPU 含异步并行、父子作用域、跨帧等待，分位数和 inclusive 时间均不可相加 |
| FinalSoak01 | 同步调用内工作集 4.77→11.20 GB，存活 UObject 基本稳定 | 不足以认定泄漏，也不足以认定所有增长都是缓存 |
| UploadCleanupD3D12Frames01 | 2048×294,912 bytes 上传可排队 576 MiB；回调全部释放后工作集仍高；每 32 次 flush 的峰值仅 9 MiB | 不能将用户缓冲区已释放等同于 RHI/驱动和 allocator 页面已归还 |

完整帧是 GT、任务、RT、RHI、GPU、Present 的流水线临界路径，加上测量驱动开销和调度，不能用 `wall - memory` 当 GPU。旧 Trace 中 RT 明确等待 GPU occlusion queries，GT 又等待渲染线程；这些是下游背压而非独立 CPU 计算成本。PIE 的全局 RHI/RT 计数还可能被 Slate 窗口覆盖。

## 2. 数据、计算与复杂度模型（修改前）

令 I 为身份数，H_i 为某身份未解决的记录数，S_r 为某记录细网格采样数，P_r 为几何 primitive 数，D_r 为该帧 dirty samples，K_r 为与它实际可能接触的较新记录数。当前粗网格 2.5 cm，细网格每轴四分，合法采样和 cap 精度都不调整。

| 层 | 权威 / 缓存与更新入口 | 成本与审计结果 |
| --- | --- | --- |
| Source | Adapter::UpdateDynamicAuthority → plugin UpdateSourceGroupTransform → PublishSnapshot | 2 个 vision + 1 个 illumination 已合并发布；约 5.2 ms exclusive CPU 需要继续展开插件内求解，不能当作对象历史成本 |
| Current | 原始 source 的 primitive-local CurrentLive grid，Whole 达标后统一像素；会话资格独立 | Changed view 按合法局部样本更新；Current presentation 每 part 生成/散列像素，并反复设置 MID 参数。Whole 复用 1×1 与局部 atlas，不从旧历史授予资格 |
| History | SpatialObservationRecord 保存 capture pose/content、coarse memory、FineHistory、immutable capture/geometry mask | Record 保存知识；FRecordVisual 保存派生呈现及 evidence cache。FindRecord 线性查找；场景 tile index 只筛选待更新 record，没有替代 record 内的 ownership 候选遍历 |
| Ownership | UpdateHistoricalContributionExclusion → 每个 dirty footprint 多点查询 → VisitNewerOwnedVerticalIntervals | 对每个 D_r 点扫描同身份较新记录，最坏 O(Σ D_r·H_i·P)。footprint 的边交点会再次调用全候选点查询，复杂几何最坏还会多一层候选乘积。首帧 D_r=S_r，incremental epoch 和逐帧复用缓存尚不能省去工作 |
| Geometry | BuildGeometryDirtyIndices 缓存 physical/newer geometry，变化时构建 dirty regions；QueryVerticalInterval 已有保守 planar AABB | 几何快拒绝发生在点查询内，仍重复支付候选循环、hash lookup、坐标转换；同帧数组复制、逐项比较也随 H 增长。退化缩放必须保留原 slab fallback |
| Cap | UpdateRecordCap 将 fine samples 临时转换成 coarse-like cells；扫描本 record 和所有较新 record 的 fine state / suppression / coarse cells 得到签名，再决定是否重建 | 即使无相交，签名路径仍 O(Σ_r Σ_newer S_newer)，等密度即 O(H²S)。构造边断点也扫描所有较新网格。签名开销发生在“签名命中则跳过”之前；不能将 rebuild 次数小误读为成本小 |
| Texture | CPU FLinearColor presentation → signature → FFloat16Color staging → UpdateTextureRegions → render command → RHI cleanup callback | 变化时 O(S)，签名不变也可能已支付生成/散列/复制。每次上传独立堆分配；同步 automation 可在没有完整 engine frame 的情况下排队大量命令。Current 与 History 两条上传路径都有 resource-null guard |
| Proxy | 真实捕获的 mesh/pose/material snapshot，独立 history proxy；cap 为 DynamicMeshComponent | Prepared resources 可在合格 capture 前分配，但不授予知识；退役释放 OwnedTextures/Materials/Caps，Destroy proxy/component，随后由 GC 和渲染生命周期回收。不能仅按 actor 数判断 CPU/GPU 资源量 |

184 distributed 的实际构造为三个身份的 **64+64+56** 个姿态，每条 yaw+17°、X+7 cm；三个簇在不同房间，簇内仍密集重叠。不是 184 个互不相交的对象。SetStressMode 同步调用 ConfigureHistoricalEpochCount，后者逐条完整 capture/seal/texture/cap，因此 setup 和首个真实 update 必须分别计时。合法反证后热态约 121/122 条；禁止把这个变化隐藏成持续 184 的成绩。

当前没有证据支持把所有数据层推倒重建。首先需要将与 record/primitive 无关的空间候选扫描移出每采样点热循环，并核验 cap 依赖签名是否能按实际相交候选构建。所有原精确谓词保留为最终判据，测试 oracle 强制原全扫描。

## 3. 资源生命周期模型与尚未解释的部分

CPU 知识持续到合法反证/完全被新知识替代；呈现对象可先于知识退役。原始数据、派生缓存、暂存上传及驱动资源生命周期不同：

1. capture → record/FineHistory/masks；只有合法知识增加才允许持续增长。
2. presentation dirty → CPU full-float presentation、half-float staging；UpdateTextureRegions 不拥有永久业务知识。
3. render command → RHI command，cleanup lambda 删除 staging/region。引擎 Texture2D.cpp 的 cleanup 位于 RHI lambda，FlushRenderingCommands 只证明相应队列边界，不是驱动全部内存归还。
4. record retirement / world EndPlay → 移除 GC-visible owners、销毁 proxy/cap → GC / render resource release → allocator / D3D12 pool / driver 再分别处理页面。

实际 `UploadCleanupD3D12Frames01` 日志 `LogInit: Allocator: Mimalloc`。需用 allocator stats 或 Memory Insights 的 live allocation/callstack 与 LLM/RHI 计数对照；不能套用 Binned pool 解释。已知 12 个完整 engine frames 后工作集仍未回落，排除“只要回调结束工作集立即恢复”的说法。真实十分钟 RHI texture bytes 后半程约 1.939 GB、cleanup 后工作集从 3.575 降至 3.386 GB，与同步 fixture 的增长模式不同；尚未形成 6.4 GB 的闭合归因账本。

## 4. 决策和验证顺序

1. 用已有 trace 配合独立 native 规模实验，保留 setup、首次 update、后续 update 与实际记录/样本数，验证 H/S/K 模型；先写模型再改运行时。
2. 优先局部重构 ownership broad phase 和 cap 依赖工作，原 predicate/顺序/精度不变。若规模仍由必要 S 或同步资源构造主导，再决定批处理/分帧设计，不能提前宣布异步一定正确。
3. 定向全扫描 parity、退化/接触/cap 测试；阶段性完整 143+ 新增项及四套视觉。正常 quality 下独立真实 D3D12 帧样本验证收益；Trace 与无 Trace 分开。
4. 展开完整帧 Source 和 GPU 成本，针对 live allocations 做资源归因；阶段成果 commit+push。最终长测只在有意义的运行时阶段完成后运行。

当前状态：ARCHITECTURE AUDIT **IN PROGRESS**；FRAME PERFORMANCE **FAIL（继承基线）**；INITIALIZATION / BATCH HITCHES **FAIL（继承基线）**；LONG-RUN RESOURCES **PARTIAL**；FUNCTIONAL REGRESSION **PASS（已有基线，尚未修改运行时）**。

## 5. 首轮规模测量与 ownership/cap 局部重构

模型文档检查点 `4de9daa`、诊断检查点 `1ff0d3a` 均已推送。独立 `OwnershipScaling` 使用原大柜体（约每姿态 5.4 万细样本），不是 48 cm 压力方块；每个规模新建 world，首步前不预热。`PerformanceAudit_ScalingBefore01` 实际 NullRHI、4/4 通过、11.711 秒测试 / 33.022 秒进程，0 severe。

| H | resident fine samples | setup ms | 首步 memory ms | ownership record visits | cap dependency sample visits |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 8 | 415712 | 89.384 | 274.739 | 2086112 | 3093090 |
| 16 | 862832 | 190.554 | 885.307 | 7815042 | 13669326 |
| 32 | 1729104 | 382.802 | 2539.248 | 30528595 | 55911735 |
| 64 | 3451712 | 767.722 | 6329.328 | 119044038 | 224939583 |

证明的是 H/S 相关的 CPU 重复工作，不是 GPU 等待造成首次更新的主尖峰。此版新增 footprint counter 误放在 Whole occupancy 函数，故该列 0 无效，已在 InstrumentationBuild02 修正；上述 visits、cap samples 与计时不受其影响。

实现第一阶段：

- 每身份、每次必要历史更新临时建立 16 cm broad-phase tiles，存放原顺序的 record/primitive 候选；精确区间、完整 footprint 边采样与所有原容差不变。无 dirty ownership 时不构建。倾斜、奇异缩放、异常范围或超过 scratch 预算时走原全扫描，不删除知识、不限制采样。
- cap 依赖先各自计算 digest，同一次升序历史阶段按 epoch 复用；较新的依赖尚未被修改，当前观察已更新。缓存只活到该 UpdateTracked 结束，capture/阶段外重新计算；全扫描 oracle 使用相同 digest 但不复用。由 O(H²S) 变为 O(HS+H²)，仍保留全部依赖。
- 加入 4140 组完整区间顺序及 footprint 对照，含负坐标、接触容差、负缩放，以及倾斜/零缩放 fallback；批量 parity 增加 cap 三角形计数。目标测试 10/10 clean、0 severe、实际 NullRHI（PerformanceAudit_OwnershipIndexTarget02）。完整阶段回归尚待完成。

第一版仅索引点查询时，64 大柜体 visits 降到 3717 万，但几何测试数不变、时间几乎不变。进一步索引 footprint primitive 后，大柜体仍约 6.26 秒；不得宣称这一极端路径已解决。cap 扫描降至 705 万，cap 首步 435.938→115.004 ms；其余几何工作仍主导。

真实 Lab 的独立 **Trace Batch** 对照（不是正式无 Trace 矩阵）如下；原 Matrix 不变，新 Batch 只取 Empty、SameIdentity64、Distributed184 并保留 cold/setup。两组实际质量/前台逐帧异常均 0、complete、exit 0、severe 0：

| case | before setup / max wall / max memory / max ownership / max cap ms | index+digest 对应 ms |
| --- | --- | --- |
| SameIdentity64 | 179.987 / 836.025 / 645.747 / 512.949 / 88.937 | 184.788 / 687.475 / 492.371 / 428.782 / 20.052 |
| Distributed184 | 515.271 / 2425.605 / 1898.791 / 1445.126 / 257.318 | 544.797 / 2021.136 / 1464.461 / 1224.835 / 65.630 |

目录 `PerformanceAudit_BatchBefore01`、`PerformanceAudit_BatchIndex01`。后者等待前台较久，110.448 秒进程；保留 Empty 的 246.853 ms 尖峰。两组热态压力都依法降到约 120，不能声称持续 184。重构有实际收益但仍有秒级尖峰，继续 FAIL。

本轮修改前 Empty 已变成 p95 18.633 ms（Trace）；旧 FinalAttribution 为 29.498 ms。质量字段相同，新 Trace GPU SceneRender 平均14.759 ms、TSR4.625、Lumen2.972、SourceUpdate5.153、RT occlusion wait10.726。**这是发生在算法修改前的运行环境/渲染时间差，不能计入重构收益。** 仍需新的完整帧归因；不同 GPU 作用域 inclusive 和跨线程等待不能相加。

从已有真实十分钟 JSONL 重算：native memory p50/p95/p99/max=.127/4.341/5.319/31.668 ms；Current p95=4.211 ms，History p95=.064 ms；异步 engine RT p95=24.605、GPU p95=17.259 ms。支持持续基础成本主要位于渲染流水线、部分 Current 工作影响尾部的判断，但未开启 Trace 的旧长测无法重建逐帧依赖图。

资源源码新证据：实际 Mimalloc 默认 `mi.MemoryResetDelay=10000` ms；D3D12 DefaultFastAllocator 使用 **4 MiB** upload pages，RHIEndFrame 延迟 **10 个 frame fence**，页池最低保留 5 页、每帧最多回收 1 页（D3D12Device.cpp / D3D12RHI.cpp / D3D12Allocation.cpp）。因此 12 个 GFrameCounter 帧不是“大量上传页应全部回收”的有效证明；仍须用 allocation trace 和实际 RHI 帧验证，不能仅凭源码宣布 6.4 GB 已解释。

施工中 SpaceCraft 再次出现，真实采集脚本拒绝启动；用户随后确认已退出，重新核对后才继续真实样本。新增脚本仅在本次 benchmark 采样开始前置前其自身窗口，不发送全局键盘输入，采样期间不抢回焦点。

阶段状态：审计继续；FRAME / INITIALIZATION **FAIL**，RESOURCES **PARTIAL**，本次运行时功能 **PARTIAL（10 项定向通过，完整阶段回归待做）**。不移动 stable。

# SightWeave 灰色层性能架构审计

最新occupancy生产切片 **f12c6c1** 已完成：最终4/4定向、Contracts/Episodes必要视觉PASS；同二进制两对真实D3D12中位occupancy **64.916→38.129ms**、最大整帧 **587.833→543.815ms**，初始化仍FAIL。完整边界、失败/修正和最终证据见第20节。

阶段性收尾已完成：三个生产切片、现有证据和后续施工顺序统一见第19节。本次仅更新文档，运行时保持9c14ecc。

当前19f2e76之后的cap运行时切片 **9c14ecc** 已推送，最终完整构建、**148/148功能**及Episodes/Contracts必要视觉通过。两次真实A/B中位最大整帧 **836.191→771.154ms**（-7.8%），setup **449.593→397.038ms**；初始化仍FAIL。完整成本、验证和下一入口见第18节。以下capture阶段数据是此前已完成证据。

最新初始化施工起点8fb4d6e，运行时59030ab已推送：capture准备切片完成，前后必要历史/cap视觉及最终147/147功能通过。三次真实Batch中位数：184 setup **538.769→451.701ms**，最大整帧 **926.468→848.486ms**。初始化仍FAIL；完整证据、剩余cap/resource瓶颈见第17节。第16节保留上一轮ownership与真实A/B证据。

上一轮施工（cf3a3c1之后）：第16节记录同帧join的封存ownership切片，运行时6c66747已推送。用户退出SpaceCraft后已补最小真实D3D12 Batch A/B：184最大整帧 **1363.970→921.871ms**、ownership **566.111→107.680ms**，setup **549.266→536.297ms**。这是一次有效同条件对照，初始化仍FAIL；详见16.3。第1–15节为更早证据，未重新审计或重跑其长测。

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

## 6. 第一阶段完整功能与分配级归因

`18251b8` 已推送。PerformanceAudit_Stage1Functional01 **144/144**（133 clean、11 warnings、0 failed/not-run、severe 0、exit 0），实际 NullRHI；测试 169.431 秒，进程 191.207 秒。此前 10 项定向与本次完整回归都包括新的 ownership oracle。阶段视觉尚待做。

新增独立 UploadResourceAttribution，2048+2048 次原尺寸上传，之后实际 GT/RT 帧推进到 12/120/360/600，最后 GPU idle、GT/RT allocator trim、纹理释放+GC+60 帧。诊断干预不进入玩法。ResourceProbeBuild01 全量 Editor 19.53 秒成功；PerformanceAudit_UploadAllocation01 真 D3D12/SM6，-LLM、启动即 memory trace，1/1 clean、exit 0、severe 0、21.608 秒测试/68.121 秒进程。trace 858,479,748 bytes，原始转储和四份 live CSV/存活差分在其 Saved/GrayObjectPolicy 目录。

- CPU 用户缓冲：无 flush 峰值 603,979,776 bytes；每 32 次 flush 峰值 9,437,184；4096 次 callback 全部完成，最终 pending 0。
- 4 MiB、Textures 标签、D3D12RHI 分配数量：Baseline 10 → BurstFlushed 152 → BatchedFlushed 298 → Released 11；对应上传页模型。通用 D3D12.DumpTrackedAllocations **没有列出这些页**，仅靠该命令会漏掉上传池，原输出保留。
- 工作集 4,872,335,360 → 6,687,464,896（两批排空后）→ 5,648,818,176（600 帧）→ 5,499,977,728（纹理释放后）。GPU idle 和显式 trim 本身没有可见额外下降；继续真实帧和时间后明显下降，不能说永不回收。
- Released 相对 Baseline 的最大保留组是 `Darkwell/UploadAttribution` 73×8 MiB=612,368,384 bytes，调用栈含项目 operator new[] → Probe::Submit，其间引擎函数无 PDB、仅模块名。本机实际 mimalloc.Build.cs 选择 **2.0.0**，其 MI_SEGMENT_SIZE 为 8 MiB；不能误套同目录未选用的 2.1.2。Windows MemoryTrace hook 跟踪 VirtualAlloc reserve 并标记为 heap，MEM_DECOMMIT 不生成 Free，故 live reservation 不等于仍驻留用户 allocation。仍需直接核对这些地址的 committed/reserved 状态。
- CSV 总数混有系统预留、allocator 子分配和 GPU heap，不可将约 21 GB 的总和当进程工作集。Insights 也报告部分 RootHeap=1 的 invalid FREE 并补零大小记录；不隐藏此采集边界，不用其混合总和闭合 6.4 GB 旧长测。

该探针把“用户 buffer 队列 → D3D12 上传页 → allocator/VM 保留”分开，并证实前两层可清理；仍不足以把旧同步 54,000 步的全部增长归给单一原因。LONG-RUN RESOURCES 保持 PARTIAL。

## 7. 完整帧 Source 成本进一步展开

只添加 CPU Trace scopes，未改插件规则；SourceTraceBuild01 全量 Editor 构建成功 13.53 秒。独立 FrameAudit 保留 Empty、快速扫视、Partial 与运动停止，原正式 Matrix 不变。FrameCost01 在运动未结束时切换场景被 GRAY_POLICY_STRESS_REJECT 拒绝，complete false、severe 1、exit 0，失败原目录保留。FrameCost02 调整该诊断顺序并延长最后运动观察，complete、exit 0、severe 0，46.886 秒，窗口激活成功。

FrameCost02 Empty 的 300 次 SourceUpdate 平均 **5.503 ms**：PublishSnapshot 5.496、MemoryWriteEffectiveLive 5.420、其中 polygon raster 共 3.674 ms/帧，其余写入合并 1.746 ms；Vision 两次求解合计约 .034 ms，Illumination 约 .010 ms，MemoryPublishPacket 约 .003 ms。父子 scope 已明确，不能再把约 5 ms 归为光线求解。GPU SceneRender 平均 14.853 ms，LumenScreenProbeGather 2.947 ms；CPU 与 GPU 不相加。

下一项有模型支撑的局部改造：地面记忆每字节仅执行 `Prior OR EffectiveLive`。完整已记住行满足 `0xff OR x = 0xff`，因此写入阶段可跳过其重复 raster/combine；ClearMemory/持久状态替换后必须重新依据实际 packed bits 判断，不把 CurrentLive、合法墙体或对象 Whole 资格改为“曾见即可”。先做完整位图/修订/dirty packet 的 reference parity，再测收益。

## 8. 旧表面提前排除与扫描线复用

完整 footprint 查询首先必须与某个旧表面相交。第一阶段仅索引较新几何，仍反复把较新边接触点送回旧几何判定，造成约一亿次查询。新增旧几何局部 XY 分离轴的保守提前排除：沿用原 inverse transform，包含原 inclusive tolerance 和额外浮点余量；倾斜/奇异缩放回原路径。索引不可用和 full oracle 均保留旧路径。FootprintRejectBuild01 完整 Editor 15.48 秒成功；6 项定向全部 clean，其中实际名称为 BatchOwnershipSamplesEquivalent、OwnershipSpatialIndexMatchesScan 和 4 个规模诊断。第一次 selector 的 ArchitectureAudit.Planar 没有匹配项，后续按实际 GrayObjectPolicy.PlanarProjectionMatchesOriginalSlab 补验，未将不存在的选择器算通过。

64 大柜体 resident samples 仍 3,451,712；首步 memory 6264.089→2715.223 ms，ownership 4376.510→898.434 ms，geometry tests 102,022,900→10,438,162。所有原 footprint 接触样本和精确区间判据仍执行于不能安全排除的区域；不丢历史、不减少合法采样。

真实 BatchFootprint01 complete、exit 0、severe 0、环境异常 0，26.153 秒，Trace 诊断：SameIdentity64 setup181.139 / max wall444.362 / native254.018 / ownership188.386 / cap20.928 ms；Distributed184 对应 **532.000 / 1317.517 / 774.759 / 540.056 / 64.905 ms**。184 的 GeometryDirtyIndices 合计66.892 ms。相对 BatchBefore01 的2425.605 ms有显著收益，仍远超门槛，INITIALIZATION保持FAIL；热态合法反证后120条继续明示。

地面写入第一步跳过实际 packed bits 全1的行，11项目标全clean（0.618秒测试/20.627秒进程）。256次完整位图、修订和dirty packet对照覆盖四档精度、illumination/bypass、clear/block/replacement，确实跳过560,504行。FrameRows01真实Trace complete、exit0、severe0、环境异常0：Empty SourceUpdate均值5.503→4.120 ms，MemoryWrite5.420→4.051 ms；完整帧p95=18.636 ms，仍FAIL。

第二步在同一次WriteEffectiveLive中复用稳定快照polygon在相同tile Y、相同行上的完整有序交点；原SampleY、边求交、排序、每个X tile的舍入和位操作均不变。只缓存快照拥有的vision/illumination数组，不缓存临时modifier/suppression polygon地址；最多128组临时行表，超出走原求交，非知识上限。模型从按X×Y tile重复扫描所有polygon边，变为同polygon/Y/row求交一次，再分别写各X tile。缓存于本次写入返回即销毁，不跨revision猜测。Reference开关同时关闭两项优化；新增凹多边形和clear后重获对照。RasterCacheBuild01完整Editor6.64秒成功，RasterCacheTarget01 **12/12 clean**、severe0、实际NullRHI，.736秒测试/20.430秒进程，含完整原slab oracle。阶段完整功能和视觉随后验证。

## 9. 上传地址的提交/预留状态核对

UploadAllocation02 在原探针上仅增加VirtualQuery，记录每个用户缓冲所属的AllocationBase，释放后只查询地址状态、不解引用已释放指针。84个区域包含这些buffer曾使用的allocator段，也可能含其它分配，不能称为buffer独占内存。它们总共704,643,072 bytes=672MiB；600帧时 committed218,234,880 / reserved-uncommitted486,408,192 bytes。最后Released时265,027,584 /439,615,488，说明部分地址又被正常工作复用。所有4096个callback完成、pending0。工作集4,875,702,272→6,710,784,000→5,257,981,952（600帧）→5,104,390,144（Released）。这直接区分了仍预留地址与已decommit的后备内存，支持Mimalloc缓存而非用户buffer永久泄漏的解释。

该次真D3D12/SM6、启动memory trace/LLM，1/1 clean、exit0、severe0，22.352秒测试/70.690秒进程。新VM变量最初与texture region重名导致RememberedRowsBuild01失败，修名后完整RememberedRowsBuild02成功5.89秒；失败日志保留。不会把这个受控探针的恢复量外推成旧54,000步全部6.4GB的精确账本。

FrameCache01：complete、exit0、severe0、环境异常0，45.463秒Trace诊断。Empty SourceUpdate平均 **2.585 ms**，其中MemoryWrite2.519 ms（polygon raster合计约1.057 ms/帧，余下写入约1.462 ms）；对照FrameCost02分别5.503/5.420 ms。Empty wall p50/p95/p99/max=16.101/18.276/20.038/21.383 ms；Partial p95=21.245、max37.874；FastSweep160 cold max407.764，不能只报其15.885 ms的p95而隐藏初始化。GPU SceneRender均值14.645 ms，TSR4.549、Lumen2.953 ms；这些为父子作用域，不能相加。Source节省并没有等量缩短完整帧，GT等待渲染任务的均值反而增至约11.114 ms，符合GPU背压仍主导的流水线模型。

Exporter核查发现引擎TimingExporter.cpp将GPU队列过滤恒设true，因此`ExportTimerStatistics -threads=GameThread`仍含GPU timers。FrameCache01保留的thread_*.csv不能按文件名当作纯CPU/单GPU队列统计。新的导出器改为ExportTimingEvents，保留实际ThreadId和TimerId再归因；原统计中可由唯一timer名称确认的Source/MemoryWrite数据不受影响。旧Trace中的Preload/Session引擎region警告保留，四个本协议region都有明确起止。

第二阶段运行时代码冻结于 `b03bcfbc5ff3c00f56d4da10c52220b706362714` 并已推送：保留局部几何排除、cap依赖摘要和地面扫描线复用；不添加会让合法Current与旧history在不同帧发布的异步捷径。剩余批量路径仍需同步建立全部纹理/cap/捕获记录和约540ms精确ownership；要消除该停顿，需要独立的不可变几何查询工作集、线程安全计算及原子提交模型，不能只把压力脚本延后或隐藏未完成记录。当前阶段不宣称达到秒级尖峰门槛。完整Editor Stage2FinalBuild01成功6.44秒；最终回归见下节。

## 10. 第二阶段完整功能和视觉回归

`PerformanceAudit_Stage2Functional01` 最终 **145/145**（135 clean、10 warnings、0 failed/not-run、severe0、exit0），实际NullRHI，164.530秒测试/188.408秒进程。报告在 `Saved/GrayObjectPolicy/PerformanceAudit_Stage2Functional01/PerformanceAudit_Stage2Functional01_Report`；baseline-coverage.json确认原142项无遗漏，新增ConservativeDrawSupport、OwnershipSpatialIndexMatchesScan、WorldMemoryRememberedRowsParity。测试启动时HEAD为4c2a765且runtime改动在source.patch内，随后提交的b03bcfb没有改变所测代码。

四个独立D3D12/SM6视觉协议全部complete、teardown complete、exit0、severe0；所有独立分析器返回PASS。下列目录均位于Saved/ArchitectureAudit：

| 最终运行目录 | 协议与独立判据 | 进程秒 |
| --- | --- | ---: |
| PerformanceAudit_Stage2Qualification01 | 906帧、9资格会话，8632检查全部通过 | 140.100 |
| PerformanceAudit_Stage2Contracts01 | 178帧、3次PIE；Whole首次离开24张图像通过 | 57.409 |
| PerformanceAudit_Stage2Episodes01 | 8轮观察；历史/cap隐藏和恢复六组各896内部样本，无缺口 | 40.135 |
| PerformanceAudit_Stage2WholeSessions01 | 118取证帧；Reobservation192、ConfirmedWholeCurrent97、WholeSessions332检查全通过 | 66.163 |

实际PNG2233×911、固定时间步；这些是正确性证据，不混入1080p真实性能。人工查看了Qualification达标前/后、Contracts移动前、Episodes第四轮完整历史、WholeSessions H2等代表图，结合全部独立oracle验证Whole每轮资格、达标不退灰、Partial与合法历史、cap和正常深度。分析命令为 `python Scripts/AnalyzeWholeQualification.py <目录>`、AnalyzeGrayWholeTransitions.py、AnalyzeGrayMemoryEpisodes.py、AnalyzeGrayReobservation.py、AnalyzeConfirmedWholeCurrent.py、AnalyzeWholeSessions.py，输出保留原目录。

最终无Trace批量复核 `PerformanceAudit_FinalBatch01/02/03`：同一b03bcfb运行时、Standalone、1920×1080/SP100、原Epic/TSR/Lumen/VSM/D3D12质量，逐帧环境异常0，complete/exit0/severe0。每次独立进程，26.058/28.310/29.332秒。此为新阶段的三个Batch局部协议，不冒充重新完成原PIE×3+Standalone×3正式全矩阵。

| case | p95 ms，三次 | setup ms，三次 | 最大整帧 ms，三次 |
| --- | --- | --- | --- |
| Empty | 18.156 / 18.436 / 18.465 | .110 / .108 / .147 | 19.952 / 19.404 / 19.958 |
| SameIdentity64 | 17.733 / 17.312 / 17.558 | 179.268 / 198.081 / 182.488 | 436.816 / 462.747 / 439.590 |
| Distributed184 | 17.244 / 17.557 / 17.421 | 561.233 / 512.112 / 516.630 | 1354.999 / 1288.085 / 1293.440 |

全部cold/setup/离群帧保留；每组64和184各一帧>100ms。setup真实创建64/184条；同身份64在合法反证后最终为0（第二次首个采样已是0），184首个采样及热态为120，fine bytes41,157,632。不能把17ms热态称为持续184满负载。秒级停顿缩短但INITIALIZATION仍FAIL，Empty p95仍超过16.6ms。不会重复旧完整矩阵或54,000步同步长测。

## 11. 最终完整帧复核与按实际线程校正的成本模型

`PerformanceAudit_FinalFrame01/02/03` 为同一最终运行时的三次独立无Trace Standalone FrameAudit，45.046/44.786/44.822秒；complete、exit0、severe0、实际D3D12/SM6、环境异常0，质量保持原值。统计包含全部case帧，含初始化。

| case | p95 ms，三次 | p99 ms，三次 | 最大整帧 ms，三次 |
| --- | --- | --- | --- |
| Empty | 18.067 / 18.812 / 18.599 | 19.311 / 19.632 / 20.058 | 19.812 / 40.250 / 21.182 |
| FastSweep160 | 15.940 / 16.035 / 16.324 | 17.227 / 16.736 / 17.803 | 408.535 / 409.242 / 424.341 |
| PartialNewThenRepeat | 22.093 / 21.691 / 21.940 | 32.916 / 31.785 / 32.807 | 38.960 / 38.090 / 40.097 |
| StationaryStop | 16.451 / 16.251 / 16.559 | 17.433 / 17.047 / 17.490 | 24.402 / 24.788 / 25.737 |

Empty第二次40.250ms保留；FastSweep每次一个>100ms冷帧，第三次有连续两个>33ms。Partial每次5帧>33ms，native memory p95约5.42–5.48ms；不能由快速扫视/停止的p95接近目标宣称完整帧PASS。新阶段只复核局部Standalone协议，没有重新验证全PIE矩阵；旧正式PIE FAIL继续有效。

用 `ExportGrayTrace.ps1 -RunName PerformanceAudit_FrameCache01 -EventsOnly` 从**已有**capture.utrace补导492,228个相关事件，保存 `events_critical_all_threads.csv`，原统计、旧受限线程导出和日志不覆盖。`AnalyzeGrayCriticalEvents.py` 按实际ThreadId/TimerId与region时间求交，总量裁切边界、事件均值/分位数只取完整落入region的事件。其分位数使用线性插值，wall分析器采用原有次序统计，两种分位数不混称。第一次补导已成功但外层临时命令用未定义的LASTEXITCODE误报“Export failed”；核查Insights正常退出、文件和492,228条输出后直接分析原文件，未重新采集。

Empty region为[14.304289,19.181216]秒，校正后的代表成本如下。相同名称可能有多个TimerId，保留身份避免把CPU或细小GPU准备段合并进主pass：

| 实际线程 / scope | 完整事件数 | mean ms | p95 ms | 解释 |
| --- | ---: | ---: | ---: | --- |
| GameThread / Frame | 299 | 16.236 | 17.953 | GT帧含等待，不等于纯计算 |
| GameThread / SourceUpdate | 300 | 2.585 | 3.236 | 插件权威更新；MemoryWrite子段2.519ms |
| GameThread / GameThreadWaitForTask | 300 | 11.114 | 12.832 | 下游渲染任务背压，不能再加到GPU帧 |
| 各Foreground/Background Worker / GPUBound_WaitingForGPUForOcclusionQueries | 299合计 | 10.577 | 不合并各线程分位数 | 等待在任务线程执行；早前“RT等待”应理解为渲染流水线依赖，非独占RT上的同名scope |
| GPU0-Graphics0 / Frame，Timer4453 | 299 | 16.103 | 17.675 | 独立GPU graphics队列帧 |
| GPU0-Graphics0 / SceneRender | 300 | 14.678 | 15.923 | 包含以下渲染pass |
| GPU0-Graphics0 / TSR，Timer4445 | 300 | 4.554 | 4.938 | 原Epic/1080p输入与原TSR历史密度 |
| GPU0-Graphics0 / LumenScreenProbeGather，Timer4419 | 300 | 2.953 | 3.304 | 同名Timer4404仅.017ms，未误合并 |

这形成可信的流水线成本模型：Source确有约2.9ms可消除的CPU重复写入；修复后GT更多时间等待，graphics队列约16ms，TSR和Lumen占其中明确部分，完整wall仍约18ms的尾部。所有父子pass和跨线程时间均有重叠；没有使用`wall-memory`或跨线程相加得到虚构的总帧预算。其它渲染pass、RHI提交、调度/Present与测量开销仍在原trace中，本文不把未选择的scope残差命名为单一瓶颈。

旧十分钟26.430ms未录Trace，无法事后重建该p95帧的因果链；旧Source5.193ms、GPU SceneRender19.550ms、TSR5.345ms与本轮修改前SceneRender14.759ms的差异发生在局部重构之前，不能纳入算法收益。对旧数字的准确回答是“基础渲染/背压为主，部分Current写入影响尾部，有未闭合的跨运行渲染时间差”，不是“26.43全部由历史或某个GPU pass造成”。

## 12. 改造后的复杂度、资源边界与后续架构决策

| 路径 | 现在避免的工作 | 仍存在的成本与生命周期 |
| --- | --- | --- |
| Ownership | 每必要历史阶段构建空间候选；大量不接触旧表面的footprint整体拒绝，原谓词保留 | dense overlap时K仍随H增长，候选间边交点查询仍昂贵；首帧D=S。16cm索引只在本次阶段存活，超过scratch预算回全扫描 |
| Cap | 各较新record依赖摘要只扫描一次；O(H²S)降为O(HS+H²) | cap轮廓的必要精确计算、边断点、临时FineCells转换仍存在；不删cap、不改变精度 |
| World memory | 已全1行不再执行等价OR；相同polygon/Y/row交点跨X tile复用 | 仍遍历需要的packed bits/tiles，实际新增合法知识仍写入；临时行表返回即释放，clear/replace直接以真实bits为准 |
| Current/History textures | 本轮未用降低纹理精度或延迟合法上传取巧 | full-float presentation约16B/sample、缓存coverage约4B/sample、细知识和bit masks并存；签名不变之前仍可能生成/散列像素、重复MID参数；可继续优化，但先需dirty输入契约和完整像素等价证据 |
| History geometry / Proxy | 避免每点全身份扫描，不复制或删除权威知识 | CachedNewerGeometry最坏O(H²P)元数据；FindRecord线性查询、批量capture/proxy/texture/cap构造仍同步；GC和RHI页池回收有独立时序 |

优先级判断没有改变：下一阶段若要真正消除184的秒级停顿，应改造**运行时历史工作单元和提交边界**，而不是继续堆点查询微优化。可先在GT建立只读几何/知识输入，对CPU ownership/cap做任务化计算，验证输入epoch后在GT原子发布；同时保持未完成期间Current与已合法历史一致、cap可见和GPU资源有效。该设计还需要处理新观察/反证使任务失效、取消与世界退出、资源上传背压。用户授权允许重构，但本轮证据只支持以上局部实现达到功能等价；尚未实现或验证的异步方案不写成成果。

旧同步54,000步不含正常RHIEndFrame；同调用中可积累576MiB用户缓冲、数百个4MiB上传页和Mimalloc提交页。三个生命周期已分别给出回调、分配跟踪和VirtualQuery证据；这是资源积压与缓存的实证，不能把仍预留的8MiB地址段当活跃用户泄漏。现有对象/纹理资源计数有界也不能反向证明所有分配无泄漏。后续如需闭合旧6.4GB账本，须在该同步fixture自身采集完整分配追踪或逐真实引擎帧的等价对照；本轮不机械重跑已完成长测，不把诊断flush/trim加入产品逻辑。

## 13. 最终真实610秒长测

`PerformanceAudit_FinalLongRun01` 在已推送8f26fdf文档检查点启动，运行时仍为b03bcfb；无Trace、无截图、真实D3D12/SM6 Standalone、1080p/SP100及原质量。独立进程634.853秒，完整协议、日志正常关闭、exit0、severe0，42,663个LongInteraction帧的环境异常0。

前置ActualNewKnowledge360帧保留6条真实新增未解决知识，p50/p95/p99/max=15.381/19.059/28.494/32.109ms，无>33ms。随后原混合路线达到610秒，首末采样跨度609.975秒；p50/p95/p99/max为 **14.135/15.991/17.538/160.729ms**，18帧>33ms、5帧>100ms、最长慢帧串1。只有该路线的p95/p99数值达标；重复大尖峰及前述Empty/Partial超标使总体FRAME PERFORMANCE仍为FAIL。

路线事件62个，11次显式Reset、10次成功运动启动；前置知识到混合路线之间的Room03 Reset为6→0，清楚区分人为Reset与合法反证。逐帧elapsed_seconds从整个驱动开始计算（此组Long末值616.314），long-events从混合路线起点计算；分钟资源桶使用前者，不能把它误读为长路线运行了616秒。

| 资源 | Long首帧 | Long末帧 | 峰值 | 释放Python样本+GC+60真实帧后 |
| --- | ---: | ---: | ---: | ---: |
| records | 1 | 2 | 4 | 2 |
| proxies | 1 | 2 | 3 | 2 |
| textures | 2 | 12 | 14 | 12 |
| MIDs（历史统计） | 1 | 4 | 5 | 4 |
| caps | 1 | 1 | 2 | 1 |
| fine history bytes | 0 | 2818048 | 6627328 | 2818048 |
| 工作集 bytes | 3363123200 | 3754786816 | 3754786816 | 3434364928 |
| UObject槽位 | 53904 | 53999 | 53999 | 53999 |
| RHI texture bytes | 1946574848 | 1939058688 | 1960042496 | 1939058688 |

分钟末工作集约3.370/3.430/3.488/3.521/3.541/3.572/3.616/3.661/3.706/3.742/3.755GB，最后桶不足一分钟。RHI纹理从约1.960GB下降并在后半程保持1.939GB；释放Python帧数据后工作集下降约320MB，保留历史资源不变。相比旧长测42,663对29,741个样本，采样器自身保存更多dict；不能把运行末WS增加全部称为玩法泄漏，也不能据此证明旧同步6.4GB没有其它来源。

native memory p50/p95/p99/max=.144/4.327/5.304/30.040ms。五个>100ms帧发生在驱动elapsed366.635、427.757、488.877、550.012、611.134秒，间隔约61.12秒；对应native对象memory只有.096–.102ms，无texture upload/cap rebuild，日志的逐帧时序与wall采样有相邻帧延迟。异步engine GT峰值149.008ms，仍不能用计数相减定位所有父子scope。对这一新发现的周期性尖峰补做短时Python GC归因，原长测全部数据与FAIL结论保留。

23行明细保存在 `SIGHTWEAVE_PERFORMANCE_ARCHITECTURE_METRICS.csv`，包含7个独立无Trace进程、全部case/cold/setup、p50/p95/p99/max、慢帧串、资源首尾/峰值、SHA和二进制/驱动/质量哈希。七组所记录的Darkwell DLL、driver、质量分别只有一种哈希；不会用新Standalone局部样本冒充新PIE全矩阵。生成命令：`python Scripts/SummarizeGrayPerformanceAudit.py <七个Saved/Stabilization/PerformanceAudit_Final目录> --csv Docs/SIGHTWEAVE_PERFORMANCE_ARCHITECTURE_METRICS.csv`；输入各目录先经AnalyzeGrayStabilization.py分析，长测资源明细保存为该长测目录long-resource-trends.json。

## 14. 采样器Python GC成本的短时归因

本机BaseEngine.ini:1749的 `gc.TimeBetweenPurgingPendingKillObjects=61.1` 与五个大尖峰的61.12秒周期一致。UnrealEngine.cpp的ConditionalCollectGarbage按该间隔触发；PythonScriptPlugin.cpp:2175的OnPreGarbageCollect获取GIL并调用PyUtil::CollectGarbage；PyUtil.cpp:1798最终执行完整 `PyGC_Collect()`。现有测量驱动既逐帧写JSONL，也把包含engine字典和viewport等列表的每个样本保留在samples直到case结束，造成大量仍可达、仍需GC遍历的对象。

为验证机制，独立 `Saved/GrayObjectPolicy/PerformanceAudit_PythonGcAttribution01/probe.py` 在实际UE Python3.11.8、NullRHI进程中读取**既有**帧文件，分别保留0/4096/16384/42663条字典，对每种规模做四次完整gc.collect(2)。没有重跑游戏路线，没有修改GC间隔或关闭GC；23.048秒、complete、exit0。原命令、script、日志和gc-scaling.json保留。

| 保留样本数 | 四次完整GC ms | tracked对象数 |
| ---: | --- | ---: |
| 0 | 7.140 / 6.678 / 6.757 / 6.713 | 68557 |
| 4096 | 19.322 / 16.709 / 17.128 / 16.760 | 89040 |
| 16384 | 46.873 / 46.397 / 48.444 / 48.288 | 150483 |
| 42663 | 104.361 / 105.735 / 107.753 / 108.632 | 281881 |
| 释放后0 | 6.267 | 68568 |

每次collected=0，所有保留row仍tracked；这是对仍可达采样数据的遍历成本，不是发现了4万条垃圾或泄漏。它确证采样器能额外制造百毫秒GC，并与真实长测的增长周期吻合；真实长测未Trace，不能声称逐帧160.729ms已全部分解为Python GC。应将其标为**已有受控证据的测量开销**，不可删掉原慢帧后重新计算PASS。

下一次修订测量协议时，应只保留运行断言必要的有限数据，全部原始帧继续流式保存，完整分布在计时结束后由外部分析器计算；不得减少样本、禁用GC、丢弃尖峰或改画质。本轮不在最终采集后悄悄修改已冻结驱动，也不为改变成绩再重复十分钟长路线。原Empty/Partial失败和184秒级停顿独立于这一长测采样器问题，仍需解决。

## 15. 最终判定与继续施工的起点

| 维度 | 状态 | 结论 |
| --- | --- | --- |
| ARCHITECTURE AUDIT | PARTIAL | 成本、复杂度和资源生命周期模型已建立并通过规模/线程/分配/GC对照验证；旧26.430ms逐帧因果链、跨运行渲染时间差及旧同步6.4GB完整账本仍未闭合 |
| FRAME PERFORMANCE | FAIL | 新Standalone Empty p95中位18.599ms、Partial21.940ms；新610秒p95=15.991ms但5帧>100ms。旧PIE正式FAIL未被新局部样本替代 |
| INITIALIZATION / BATCH HITCHES | FAIL | 184真实最大整帧1.288–1.355秒；独立同协议Trace从2.426降到1.318秒，仍远未达标 |
| LONG-RUN RESOURCES | PARTIAL | 实际长测资源边界稳定；上传排队、4MiB D3D12页回收、Mimalloc decommit及采样数据保留分别有证据，不能冒充全部内存无泄漏 |
| FUNCTIONAL REGRESSION | PASS | 最终完整Editor build成功，145/145及四套视觉/全部独立oracle通过；冻结灰色规则无降级 |

EXIT STABILITY沿用已验证范围的PASS；本轮全部最终功能、视觉、七个性能进程和短GC探针正常退出。原中断EXIT UNKNOWN及旧失败保留，没有通过强杀或全局快捷键结束进程。总体仍为 **PARTIAL — GRAY_STABILIZATION_BLOCKED**。

继续施工顺序：先设计并验证不可变ownership/cap工作集与GT原子发布，解决批量同步停顿；随后减少完整帧渲染与提交成本，并将长测采样器改为完整流式证据；最后针对同步fixture做分配级闭环。不能重复已完成的功能/视觉/长测来替代实现，不改变Whole每轮100cm、合法历史、采样、cap和质量。构建用 `Scripts/BuildEditor.ps1`，功能用RunGrayObjectPolicyTests.ps1的既有完整selector（最终summary.json保存原串）；视觉用RunGrayMemoryAudit.ps1的Qualification/Contracts/Episodes和Reobservation -WholeSessions -NormalTurns。最终仅文档/分析脚本变更，不再修改已验证运行时。

## 16. 有限额度施工：封存ownership的同帧并行切片

起点cf3a3c1588bc290aa9a42f97f8cd1c8d199432c5，工作树干净。本轮只处理批量停顿，保留上一轮三次184 before：setup561.233/512.112/516.630ms，max1354.999/1288.085/1293.440ms。没有重做大范围审计、正式矩阵或长测。开始时SpaceCraft运行，已请求用户退出；因此尚未开始真实GPU before/after。

选择**同帧join**而非跨帧异步发布：已有历史阶段按升序epoch更新，任何一个record的ownership读阶段只依赖较新的未更新数据。对dirty>=4096、存在原空间索引及geometry snapshot、候选>2、该身份全部record均已封存的场景，GT暂借原CPU数据为只读输入，最多16个任务执行原有完整footprint/区间判据。Current、退化几何索引fallback和小工作量仍串行。原cap、texture、FineHistory更新顺序和全部知识不改变。

任务只读Prop/Visual中的CPU历史、geometry、candidate/index和cache位；GT在ParallelFor join前不推进它们。每个任务写互不重叠的uint8结果段，查询计数用线程本地指针指向任务私有计数，避免共享RuntimeFrame写和TBitArray并发bit写。join后GT顺序合并suppression/cache位和计数，再执行原FineHistory/cap/texture发布。既没有跨帧epoch结果，也没有悬空任务或UObject生命周期延长，所以不引入取消队列和revision补丁。工作线程查询链不能访问live actor/policy，Current查询入口另有assert。UE ParallelFor未启用PumpRenderingThread，GT等待不会泵送其他GT更新。

这是一种借用不可变数据、并行求值和GT提交的完整局部切片，**不是**独立纯数据模块或完整异步capture/cap架构。现有const查询方法仍属于scene类；以后若扩到跨帧，必须先拆出拥有自身数据的输入，再设计失效/取消，不能延长当前借用对象的存活期。默认控制量r.Darkwell.ObjectMemory.JoinedSealedOwnership=1；0保留同二进制串行对照，性能runner新增-SerialSealedOwnership并明确记录metadata。

完整Editor构建 `BatchSlice_JoinedBuild01.log` 成功104.57秒。定向 `BatchSlice_JoinedTarget01` 4/4、3clean/1warning、0failed/not-run/severe、exit0，实际NullRHI；测试15.232秒/进程83.210秒。进程启动与构建尾部重叠，但测试开始在构建完成之后，确实执行新JoinedSealedOwnership测试并报告37个并行批次、1,982,208输入样本；不把此过程时间用作性能对比。唯一warning为引擎网络连通性探测generate_204超时，与测试断言无关。

验证包括原全扫描BatchOwnershipSamplesEquivalent、4140组完整interval/footprint空间索引对照、原slab几何对照，以及新增串行/并行十阶段逐帧比较：全部细样本字段、suppression位、texture signature和cap四边形坐标。覆盖无效coverage、8/16条重新播种、Reset及world销毁。所有任务在每次Step返回前完成；此架构没有跨帧“待取消”状态，不能冒称测试了未来异步方案的取消。真实batch收益、完整阶段回归和必要视觉待后续证据；先推送可构建、已定向验证检查点。

运行时检查点a8bb332已推送。其后完整 `BatchSlice_Functional01` **146/146**（136clean、10warnings、0failed/not-run/severe、exit0），实际NullRHI；测试182.321秒/进程203.854秒。没有再次执行四套视觉或长测。SpaceCraft仍运行，真实GPU A/B保持待测；为先验证主目标的CPU工作，新增独立JoinedDistributedBatch诊断，使用现有原生64+64+56构造器、串行/并行交替四次，报告setup与首个native update而非D3D12完整帧。新增仅测试文件，完整DiagnosticBuild01成功8.45秒，先保存检查点再运行该有界诊断。

`BatchSlice_DistributedCPU01` 失败已保留：测试误用普通PropLab/GrayObjectPolicies世界，生产SetGrayPolicyStressMode按gray_lab=0正确拒绝；没有有效成本样本。诊断改为GrayPolicyLab地图身份，并调用正常ConfigureForGrayPolicyLab配置，未削弱生产guard。另将并行查询计数改为任务栈上累加、末尾一次写回，避免相邻计数槽的false sharing；几何求值不变。完整 `BatchSlice_DiagnosticBuild02.log` 成功20.92秒；先推送可构建修订，再执行修正诊断及受影响parity。

### 16.1 最终运行时6c66747的有界对照

`BatchSlice_DistributedCPU02` **3/3 PASS**（1clean、2warnings、0failed/not-run/severe、exit0），测试19.478秒/进程39.103秒。包含修正184诊断、JoinedSealedOwnershipParityAndLifetime、BatchOwnershipSamplesEquivalent。两条warning均为引擎generate_204 HTTP探测超时。原有功能146/146在a8bb332完成；后续运行时仅将计数移到任务栈，并在此处重新验证受影响parity，未机械重复全套。source.json、原日志、report和提取的native-costs.json均保留于上述Saved/GrayObjectPolicy目录。

四次使用同一二进制、同一原生64+64+56 stress构造器、独立新世界，按串行/并行/串行/并行顺序；seed后不插入预热。**这是NullRHI CPU诊断，SpaceCraft PID18656仍运行，且不含完整GrayPolicyLab渲染环境，不是独占负载的Standalone性能验收。** setup只测构造器，Step测Adapter/Room/Fixture的一次原生更新；memory为其中的对象记忆阶段。以下单位ms，全部保留，不能与旧D3D12整帧相加或混算收益：

| 工作阶段 | 串行0 | 并行1 | 串行2 | 并行3 |
| --- | ---: | ---: | ---: | ---: |
| 184 setup | 473.349 | 482.495 | 474.776 | 518.930 |
| 首次Step | 851.353 | 414.154 | 838.413 | 391.221 |
| 首次native memory | 850.919 | 413.722 | 837.970 | 390.782 |
| Ownership | 583.685 | 115.767 | 568.525 | 115.063 |
| Cap presentation | 79.846 | 83.678 | 80.156 | 82.457 |
| Occupancy | 83.671 | 102.650 | 86.957 | 81.458 |
| Fine history | 43.185 | 49.456 | 42.353 | 49.729 |
| Texture submission CPU | 15.480 | 16.518 | 15.318 | 16.951 |

每次首帧均保留184条record，geometry tests **8,030,933**、record visits **17,100,359**完全相同。并行各181批、1,941,840输入样本；串行0批。后两次更新各约6–7ms，ownership约.01ms、cap=0。两次均值ownership576.105→115.415ms（约80%下降），native844.445→402.252ms（约52%下降）；仅说明该CPU路径的并行收益，不声称通用硬件提升或真实最大整帧下降同样比例。

### 16.2 第一检查点验收边界与下一轮入口（c1b1ebb，真实A/B补证见16.3）

| 本轮重点 | 结果 |
| --- | --- |
| 架构切片 | 已完成：封存ownership只读借用、并行精确求值、join后GT合并；无跨帧任务和待取消结果 |
| 功能正确性 | 完整146/146一次通过，最终计数局部修订另有3/3定向；覆盖旧全扫描oracle、样本/纹理/cap parity及无效coverage、Reset、重播种和world销毁 |
| 184真实setup / 最大整帧 | 新after **待测**。上一轮三次before setup512–561ms、最大整帧1288–1355ms保留；本轮CPU数据不能填入此栏 |
| INITIALIZATION / BATCH HITCHES | **FAIL**。即使CPU ownership显著缩短，setup约半秒且GT仍等待所有任务完成，未消除玩家可感知停顿 |
| 最终视觉 | 本轮未重跑；前轮四套PASS仅作既有基线，不能写成6c66747的新视觉PASS |
| 其它总体状态 | ARCHITECTURE AUDIT PARTIAL / FRAME PERFORMANCE FAIL / LONG-RUN RESOURCES PARTIAL；本轮未重做全系统审计、矩阵、十分钟长测或旧内存账本 |

恢复后先确认SpaceCraft已退出，再在**同一6c66747运行时二进制**做最小Batch A/B：`Scripts/RunGrayPerformanceBaseline.ps1 -RunName <唯一名称> -Mode Standalone -Protocol Batch -NoAuthoringToolsets -SerialSealedOwnership`为串行，不带最后开关为并行；用日志确认串行CVar确实为0，先一对，再按变异决定必要重复。不应重做全矩阵或从零跑145项。用 `Scripts/AnalyzeGrayStabilization.py <Saved目录>` 分析全部setup/冷帧，不删异常。取得真实A/B后补一次与改动相关的历史/cap视觉证据；此前保留最终渲染验证待办。

下一轮最高收益结构入口是**封存capture的CPU输入、occupancy/cap计算与GT资源提交的工作边界**：此切片已将ownership降到约115ms，但setup仍约500ms，occupancy/cap各约80ms。先沿既有Trace定位setup中的捕获/快照/资源创建，做可拥有自身数据的工作单元和GT提交，再决定跨帧调度及失效策略。任何跨帧结果必须有epoch/revision和世界生命周期校验；不能将本轮借用引用直接交给跨帧任务，也不能仅延后压力测试构造器来冒充产品优化。Current分支、capture/cap任务化和发布背压均尚未实现；本轮没有遗留半套异步队列。Python流式采样器低于主任务优先级，本轮未修改。

最后检查工作树/暂存、diff和LFS均通过，所有构建及测试进程正常结束，Saved失败/成功证据完整保留。稳定分支不移动，不创建最终stable、不开始黑色层、不自动关机。

### 16.3 SpaceCraft退出后的最小真实D3D12补证

2026-09-06约19:49–19:50，用户确认退出游戏，进程检查无SpaceCraft/UE/构建等冲突负载。直接使用已构建、已推送的6c66747运行时，没有重新编译或修改代码。执行 `RunGrayPerformanceBaseline.ps1 -RunName BatchSlice_RenderSerial01 -Mode Standalone -Protocol Batch -NoAuthoringToolsets -SerialSealedOwnership`，正常结束后执行不带最后开关的 `BatchSlice_RenderJoined01`；随后用AnalyzeGrayStabilization.py分析两个已有结果目录。

两次均D3D12/SM6、前台1920×1080、SP100、原Epic/TSR/Lumen硬件光追/VSM配置，无Trace、无固定时间步、无截图。所有480个case帧各自环境异常0，valid_normal_sample=true，complete、exit0、severe0、log_closed=true；进程28.826/26.134秒。串行日志实际回显 `r.Darkwell.ObjectMemory.JoinedSealedOwnership = "0"`，并行使用原默认1。两次源SHA均c1b1ebb；DLL SHA256均47D4AD0FC9B558A964E33277D21587C9E071DD0E38A91A4ACAEB28B89CDDAD29，driver SHA256均4889D03A8159A896BE5C742A28E747B5EDF7F594470FCD1CF7E329B78933D670，质量值和DefaultEngine.ini一致。所有原始帧、启动帧、setup和退出证据保留于Saved/Stabilization下对应目录，提取对照另存Joined01/before-after.json；分析器跨运行合并的p95不作为串行/并行效果统计。

| Distributed184阶段，单位ms | 串行01 | 并行01 |
| --- | ---: | ---: |
| Setup | 549.266 | 536.297 |
| 最大完整wall帧（case index0） | **1363.970** | **921.871** |
| 同次最大native memory（index0） | 805.611 | 376.810 |
| Ownership | 566.111 | 107.680 |
| Cap presentation | 67.231 | 74.602 |
| Occupancy | 70.145 | 81.055 |
| Texture submission CPU | 23.403 | 25.514 |

两次setup均创建184条，首个及后续采样均120条，与旧正式路线一样完成合法反证；没有减少seed或合法历史来获得成绩。完整wall尖峰下降442.099ms，约32.4%，ownership下降约81%；这是**一对短同条件实验**的观察值，不代表三次重复统计或所有机器。setup只小幅变化，cap/occupancy/texture没有相同方向的改善；不将这些相互包含/跨流水线阶段相加，也不把CPU节省直接等同GPU收益。初始尖峰完整保留，两次Distributed均1帧>100ms。

同协议附带的SameIdentity64 setup187.181→185.211ms、最大帧457.824→303.663ms；Empty p95 18.489→18.580ms。由此确认切片收益已经体现在真实渲染的批量帧中，同时**INITIALIZATION / BATCH HITCHES仍FAIL，FRAME PERFORMANCE仍FAIL**。无需为确认此明显CPU切片收益重跑完整矩阵或十分钟长测，也没有重跑已通过的146项。

本次只补性能证据并更新文档；相关最终历史/cap图像验证仍待完成，不冒称新的视觉PASS。下一实现入口仍是约536ms的同步capture/setup及occupancy/cap/资源提交工作边界。运行结束再次确认无残留UE/SpaceCraft/测试进程，工作区只改本文与交接；旧失败证据、既有功能PASS、长期资源PARTIAL和stable指针保留。

## 17. Capture准备与最终cap提交切片

起点8fb4d6e，工作树干净，无游戏/UE/构建冲突进程。先执行当前6c66747运行时必要视觉：`InitSlice_BeforeEpisodes01`完成51张图像，六组各896个历史内部表面样本全部无缺失，35.689秒；`InitSlice_BeforeContracts01`完成178张图像及3次PIE生命周期，Whole首次离开图像oracle24/24 PASS，60.168秒。均D3D12/SM6、协议完成、severe0、正常退出；另核看局部外切口与最终完整历史图像，没有发现新增缺面或内部接缝。Episodes与旧Stage2的图像ROI不是逐像素相同，原始比较保留，不把TSR图像误称bitwise parity。本阶段先通过上述视觉门槛，未重新做ownership审计。

读取已有PerformanceAudit_BatchFootprint01/stats_all.csv：248次SealCapture累计550.694ms，其中CaptureFootprint205.980ms；EnsureResources累计144.095ms，CapPresentation累计272.560ms包含setup与后续update，不能把这些父子/跨阶段总量直接相加。184的上一轮无Trace setup536.297ms/最大帧921.871ms仍是改造前参考。

本切片把封存几何footprint准备拆成拥有bounds/size/geometry副本的只读CPU输入、分段uint8输出、join后GT合并packed bits并提交最终FineHistory/texture/cap。使用同一完整中心/边交点谓词和原精度，dirty样本不删减。>=4096个样本最多16任务；小输入走串行。任务不持有record/visual/资源，不访问live actor；场景const几何方法仍提供原谓词和任务局部计数，尚不是独立无scene依赖的几何模块。全部工作在函数返回前join，未引入跨帧队列，GT不推进epoch/世界销毁，无待取消结果。

同时省去同一次封存内先算/提交Current cap、随后立即覆盖为Historical cap的中间调用；既有资源保留至最终结果，Whole的原子资格验证、隐藏原件/历史接管、最终cap计算/资源创建顺序保留。当前texture调用原本直接return，跳过它不计成GPU上传收益。控制量 `r.Darkwell.ObjectMemory.StagedCapturePreparation=0` 保留旧中间cap和串行几何计算；runner增加-LegacyCapturePreparation用于同二进制对照，不改旧ownership默认开关。occupancy和最终cap算法、GT proxy/texture创建本身尚未重构。

完整Editor构建InitSlice_CaptureBuild02成功11.84秒；Build01因新增测试缺少DynamicMeshComponent头文件失败，已修正，原日志保留。`InitSlice_CaptureTarget01` **17/17 PASS**（14clean、3warnings、0failed/not-run/severe、exit0），NullRHI，33.022秒测试/53.306秒进程。新增CapturePreparationParityAndLifetime覆盖27组不同尺寸/旋转/倾斜/反射/薄几何的完整slab oracle，Partial/Whole封存、实际再次观察、失效coverage、Reset、重新播种及世界销毁；比对捕获/geometry掩码、细历史字段、texture signature、cap顶点/可见性。另有旧全扫描、ownership、反复历史对照。先推送可构建可验证阶段，再采集真实before/after及最终视觉；性能尚未判定改善。

运行时59030ab已推送后，首对无Trace `InitSlice_CaptureLegacy01 / CaptureStaged01` 均有效、逐帧环境异常0、正常退出：184 setup558.937→465.343ms，最大整帧935.647→848.486ms；首个native368.894→373.530ms基本未变。控制量0保留旧串行求值和中间cap提交，但仍使用提取后的共同输入/结果封装；不能说该控制二进制完全等于8fb4d6e。两次原始日志、冷帧和setup均保留，没有隐藏新增Empty 52.563ms慢帧。

改造后 `InitSlice_AfterEpisodes01` 51张/六组896样本surface oracle PASS，38.149秒；`InitSlice_AfterContracts01` 178张/24帧Whole image oracle/3次PIE PASS，62.842秒。均severe0、exit0，另核看局部切口图像。Episodes同相机ROI与本轮before的51图比较，最大图像MAE=.305/255、最大p99像素差6/255，保留原图和比较JSON；以独立表面/图像规则及cap几何parity验收，不设置新宽松像素阈值冒充完全一致。

独立短Trace `InitSlice_TraceCaptureLegacy01 / TraceCaptureStaged01` 均完整、exit0、severe0、环境异常0；Trace样本按协议不算valid_normal_sample。使用Insights导出实际GameThread事件，确认恰好248个seal，按协议顺序64+184分组，并只累计嵌套在对应seal内的子scope（Saved中analyze_capture_events.py及capture-attribution.json可复核）。184 seal累计401.933→327.838ms，其中footprint **159.301→56.381ms**；cap调用 **368→184**，但实际cap时间131.458→146.713ms，没有测得cap耗时收益。两次ensure在seal内均368次、2.344→2.862ms；大量首次资源创建在seal外，不得用这个小值宣称资源构造已解决。新的完整stage功能回归正在运行，随后补必要短Batch重复以区分噪声；不重跑完整矩阵和长测。

### 17.1 最终三次真实Batch对照

全部使用59030ab运行时、同一DLL（SHA256 638E31BDB9C63795BAA2ADEEF662D33D46C3A991E1A179D8382ACA8D5E441698）、原Python driver（4889D03A8159A896BE5C742A28E747B5EDF7F594470FCD1CF7E329B78933D670）和DefaultEngine.ini。第一对源SHA59030ab，后两对源SHAf1c2f71仅增加证据文档，未再构建。顺序Legacy01→Staged01→Staged02→Legacy02→Legacy03→Staged03；双方ownership均开启，只切换StagedCapturePreparation。每次独立前台D3D12/SM6 Standalone，1920×1080/SP100、原质量、无Trace/固定步长/截图；均complete、exit0、severe0、log正常关闭，480个case帧环境异常0，六组valid_normal_sample=true。

| 184 distributed，ms | Legacy01 | Legacy02 | Legacy03 | Staged01 | Staged02 | Staged03 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Setup | 558.937 | 538.769 | 525.705 | 465.343 | 444.692 | 451.701 |
| 最大整帧 | 935.647 | 926.468 | 913.484 | 848.486 | 841.894 | 852.615 |

三次中位数setup538.769→451.701ms，减少87.068ms（16.2%）；最大整帧926.468→848.486ms，减少77.982ms（8.4%）。首个native update中位378.550→388.191ms，ownership104.782→106.897、occupancy83.010→83.321、cap73.472→74.913、texture25.031→25.749ms：收益来自setup中的封存准备，未测得历史update阶段改善，不掩盖这些小幅退化。Trace的footprint缩短可解释主要机制，但不能将它与独立无Trace的时间直接相加或逐毫秒归因。

六次setup均184条，首个及末个采样均120条、fine bytes均41,157,632；按原规则完成合法反证，没有减采样、丢历史或调整cap精度。每次184均保留1个>100ms帧。SameIdentity64最大帧中位308.368→278.164ms、setup188.815→158.101ms；其首帧因实际合法反证可为0或64条，原数据保留，不与稳定120条的184成本混用。Empty三次p95中位18.752→19.241ms；Staged01另有52.563ms冷慢帧。完整分布及18个case明细保存在Staged03/six-run-comparison.json，不用分析器跨变体混合p95作为改善结论。

### 17.2 最终验证与剩余工作

`InitSlice_Functional01` **147/147 PASS**（134clean、13warnings、0failed/not-run/severe、exit0），实际NullRHI，测试175.181秒/进程199.474秒；baseline-coverage.json确认旧146项无遗漏，仅新增CapturePreparationParityAndLifetime。17项定向的warnings来自HTTP连通性与EOS网络请求，不是玩法断言失败。代码冻结后只运行这一次完整阶段功能及上述两种必要视觉；未再跑Qualification/WholeSessions全套、PIE×3/Standalone×3完整矩阵或十分钟长测。

| 维度 | 最终状态 | 当前依据 |
| --- | --- | --- |
| FUNCTIONAL REGRESSION | PASS | 147/147、27组geometry oracle、Partial/Whole封存与生命周期parity，改造前后Episodes/Contracts及独立oracle通过 |
| INITIALIZATION / BATCH HITCHES | FAIL | 184三次最大帧仍842–853ms；setup约445–465ms，未接近100ms门槛 |
| FRAME PERFORMANCE | FAIL | 原完整矩阵FAIL未被短Batch替代，当前Empty仍约19ms p95 |
| ARCHITECTURE AUDIT | PARTIAL | capture只读准备/GT合并切片成立，完整cap计算与资源提交仍同步；未重做ownership大审计 |
| LONG-RUN RESOURCES | PARTIAL | 本轮未做长测；任务在函数返回前join，原长期资源证据与归因缺口保留 |

下一轮最高收益入口是**最终cap的纯CPU网格构建与GT SetMesh/资源提交分离**：新Trace在184封存内cap约147ms，后续首个update又约75ms。先把需要的FineHistory/geometry/较新合法贡献作为共享只读输入，形成独立cap结果，再以原序GT提交；避免为每个record复制整套历史造成新的O(H²S)成本。随后处理首帧约83ms occupancy，以及seal外首次proxy/texture/resource构造。不能直接把当前借用场景方法的任务延长到跨帧，跨帧仍须完整epoch/revision/失效协议。本轮没有遗留半套异步队列，也没有提前宣称cap或GT资源创建已解决。

最终检查diff、工作树及LFS正常，测试/Insights/UE进程全部结束；所有Saved证据（含失败构建、旧失败诊断、冷帧）保留。阶段运行时59030ab和中间文档f1c2f71均已推送；stable不移动，不开始黑色层，不自动关机。

## 18. 最终 cap 行计算与 GT 提交切片（19f2e76 之后）

先读取最新交接，保持 capture footprint 与既有 ownership 实现。计时检查点 `5c9e495` 已推送；完整 Editor 构建 `CapSlice_ProbeBuild02` 成功18.60秒。首次 `CapSlice_TraceBefore01` D3D12 启动后 Windows 前台激活失败，90秒前台校验超时，complete=false、exit0、脚本Traceback一条，正常退出；没有有效Batch数据，不作为性能样本。用户随后确认可采集。

为避免等待前台阻断 CPU 归因，使用已有184构造诊断 `CapSlice_CpuProbe01`，NullRHI+CPU Trace，1/1通过、severe0，测试4.112秒/进程34.119秒。Insights按真实GT和184次seal分组，四次各184个封存中的 cap CPU build **75.983–76.717ms**，细胞转换17.850–27.621ms，签名21.074–21.288ms，GT提交10.112–11.449ms，cap总计127.186–136.385ms。原始trace、导出CSV、cap-attribution.json在Saved/GrayObjectPolicy/CapSlice_CpuProbe01。四组诊断切换的是旧ownership控制量，不是cap A/B；此处只用cap分段成本选择切片，不能当真实帧性能。

运行时改造：大网格（至少4096格且至少4行）最多8个连续行任务，借用只读record/geometry/history输入，任务独立生成裁剪后的有序quad和计数；join后按原行顺序合并，构造最终FDynamicMesh3，GT验证epoch、transform revision、visual及cap目标，再发布诊断和SetMesh/visibility。计算阶段不再边算边改Visual。细胞转换、签名、最终mesh materialization和资源操作仍在GT；这不是完整跨帧异步。涉及较新实时Current的历史cap走串行，避免worker进入actor/policy查询。全部任务在返回前join，不复制每条record的整套历史，不引入H²S快照存储、持久任务队列或待取消结果。几何方法仍依赖scene的const CPU查询及任务局部计数，不可直接延长到跨帧。

控制量 r.Darkwell.ObjectMemory.JoinedCapBuild=0 / runner -SerialCapBuild 用于同二进制串行行求值对照；两边都保留当前ownership/capture默认开启，以及新的结果合并封装，因此不把控制量0称为19f2e76原二进制。采样、裁剪断点与精度、完整区间差、候选顺序、quad/vertex/triangle顺序不变。occupancy、seal外首次资源创建本切片尚未修改。

最小验证过程：CapSlice_Build01完整构建成功37.48秒。CapSlice_Target01旧三项cap测试通过，新parity测试在覆盖断言上失败：48次结果一致、96次joined，但初始fixture没有非空cap和live Current。改用真实局部观察宽柜fixture补足，未降低断言。CapSlice_Build02因新增fixture访问lab私有测试辅助函数缺少friend声明失败，已补测试friend；CapSlice_Build03完整构建成功29.73秒，原失败证据保留。最终验证和真实A/B见18.1–18.3。

CapSlice_Target02已补非空cap（30次比较、20次非空、50次joined全部一致），但fixture移除了实际源，live Current覆盖仍缺失。现显式恢复源后，最终 `CapSlice_Target03` **1/1 clean PASS**，31次比较、20次非空、35次joined、8次live Current回退；网格顶点/三角形索引、quad顺序、cap诊断、几何/候选计数一致，每个细历史字段和捕获/抑制掩码不变。覆盖Partial/Whole、真实再观察、失效coverage、Reset、重新播种和世界销毁。最终完整构建 `CapSlice_Build04` 成功11.58秒（仅测试fixture修订，运行时代码在Build01后未变）。先推送该阶段，再做真实D3D12 A/B和必要阶段回归；尚不声称真实帧改善。

运行时检查点 **9c14ecc** 已推送。第一对真实D3D12 Batch CapSlice_Serial01 / CapSlice_Joined01均complete、exit0、severe0、正常退出、480帧环境异常0、valid_normal_sample=true。184 setup **458.850→375.941ms**，最大整帧 **850.725→749.592ms**；首次native381.502→364.698ms，cap76.425→68.775ms，ownership107.099→101.492ms，occupancy82.275→79.461ms，texture25.365→25.106ms。改善初步成立，但尚未用单对样本替代重复和分段证据；初始化仍远超100ms。

### 18.1 两次真实 Batch 对照

运行时9c14ecc冻结，顺序Serial01→Joined01→Joined02→Serial02，均同DLL 3D3FC1128749E474682A94135CE5D956772DC884BB8E368174E01BDD403489C9、driver 4889D03A8159A896BE5C742A28E747B5EDF7F594470FCD1CF7E329B78933D670、同DefaultEngine.ini。正常前台1920×1080/SP100、Epic/TSR/Lumen HWRT/VSM，D3D12/SM6，NoTrace/无固定步长/无截图；每次480帧环境异常0，complete/exit0/severe0/log关闭，valid_normal_sample=true。前台激活返回false时由用户点击建立前台，实际逐帧校验通过，未关闭校验。

| 184 distributed，ms | Serial01 | Serial02 | Joined01 | Joined02 |
| --- | ---: | ---: | ---: | ---: |
| Setup | 458.850 | 440.336 | 375.941 | 418.135 |
| 最大整帧 | 850.725 | 821.656 | 749.592 | 792.716 |

两次中位数setup **449.593→397.038ms**（减少52.555ms，11.7%），最大整帧 **836.191→771.154ms**（减少65.037ms，7.8%）。Joined两次仍有约43ms差异，改善不是固定节省100ms，且均未接近100ms门槛。原始全部冷帧保留；没有用每个case第90帧后的稳定段替代最大帧。

首次native中位376.916→365.052ms，其中cap75.810→68.858、ownership104.947→102.038、occupancy81.144→79.806、texture25.561→25.014ms。未修改后三个算法，不把其小幅变化归因成新优化。setup均184记录，首/末采样均120，fine bytes均41,157,632；四次184均保留1个>100ms帧。SameIdentity64最大帧273ms附近→256–257ms，Empty p95范围17.917–18.543ms，均是短Batch证据，不能覆盖旧完整帧矩阵FAIL。所有12个case指标、原始帧索引及同源hash见Saved/Stabilization/CapSlice_Joined02/four-run-comparison.json。

复杂度仍保留原全格扫描O(S)、边界对历史网格断点扫描及完整ownership裁剪；仅把行计算分配到最多8个CPU任务，不声称消除历史/断点的最坏复杂度。暂存增加单个cap的O(Q+T)输出，Q为实际裁剪后quad数，T≤8；原O(S)细胞转换仍存在，不复制每个依赖的完整历史。旧Visual保持到新结果全部完成，因此事务内会短暂同时持有旧/新quad，不是零额外内存。GT mesh materialization、签名扫描、occupancy与首次资源创建是下一阶段可测边界。

### 18.2 实际 D3D12 分段 Trace

独立CapSlice_TraceSerial01 / CapSlice_TraceJoined01均完整、exit0、severe0、环境异常0；因Trace按协议valid_normal_sample=false，不混入18.1正常样本。两组各248次seal，按64+184分组，CPU timer通过真实GT线程ID筛选，只累计父scope包含的子事件。导出rsp、CSV、cap-attribution.json均保留。

| 184，ms | Serial | Joined |
| --- | ---: | ---: |
| 封存内cap CPU构建（含join和CPU合并） | 85.771 | 29.480 |
| 其中CPU合并/最终mesh materialization | 0.029 | 0.053 |
| 封存内细胞准备 | 18.427 | 20.923 |
| 封存内签名 | 21.399 | 21.759 |
| 封存内GT cap提交 | 9.450 | 10.157 |
| 封存内cap合计 | 136.269 | 83.715 |
| SealCapture总计 | 305.155 | 259.692 |
| 首次更新cap CPU构建（182次重建） | 19.654 | 11.984 |
| 首次更新cap合计（184次调用） | 70.964 | 65.267 |

这份压力数据中最终quad很少/为空，CPU mesh materialization不是主要成本；全部格子的合法边界扫描和诊断仍必须完成，本切片并行的是这部分，不以空cap为理由删除扫描或降低精度。非空cap正确性另由逐顶点/三角形对照与视觉证明。cap提交时间没有改善，不能把整个SetMesh/GPU资源边界称为已消除。seal内EnsureResources仍各368次、2.359/2.357ms；首次创建大部分在seal外，此值不能代表全部资源构造。独立Trace解释计算机制，不将其中毫秒直接加到另一组真实Batch。

### 18.3 阶段正确性与最终范围

运行时冻结后完整阶段 `CapSlice_Functional01` **148/148 PASS**（135 clean、13 warnings、0 failed/not-run/severe、exit0），实际NullRHI，测试173.638秒/进程194.895秒。baseline-coverage.json确认旧147项无遗漏，仅新增JoinedCapMeshParityAndLifetime。定向新增测试保留全部覆盖断言，旧三项裁剪/接触/历史cap规则在CapSlice_Target01已通过；后续修订只补测试fixture，不修改运行时。本轮只追加必要Episodes/Contracts D3D12视觉，未重新跑Qualification/WholeSessions全套、完整性能矩阵或十分钟长测。

必要视觉 `CapSlice_Episodes01` 完成51张图像，38.143秒，六组各896个历史内部表面样本全部无缺失；`CapSlice_Contracts01` 完成178张、三次PIE，59.909秒，Whole离开24帧图像oracle PASS。均D3D12/SM6、协议/teardown完成、exit0、severe0。已核看cycle_1局部历史、cycle_7完整历史、pie2_partial_outer_cut，无新增缺面/内部接缝。Episodes对上一运行时InitSlice_AfterEpisodes01的51图同ROI比较，最大图像MAE0.329210/255、最大p99差6/255；原始PNG与previous-runtime-roi-comparison.json保留，不把时域渲染误称逐像素完全一致，也未添加宽松图像验收门槛。

| 维度 | 最终状态 | 本轮依据及剩余范围 |
| --- | --- | --- |
| ARCHITECTURE AUDIT | PARTIAL | cap只读行计算/有序合并/GT发布生产切片完成；没有重新开展全系统审计，live Current依赖仍串行，无跨帧队列 |
| FRAME PERFORMANCE | FAIL | 原正式矩阵FAIL维持；短Batch改善不能替代全帧矩阵验收 |
| INITIALIZATION / BATCH HITCHES | FAIL | 两次184最大帧749.592/792.716ms，均仍有>100ms尖峰 |
| LONG-RUN RESOURCES | PARTIAL | 短Batch记录/资源规模相同，新增暂存只存在单次join内；未重跑长测，旧长期内存归因缺口仍在 |
| FUNCTIONAL REGRESSION | PASS | 最终148/148、非空cap逐顶点/三角形与生命周期parity、Episodes51/Contracts178及独立oracle通过 |

下一轮最高收益入口：**约80ms首轮occupancy** 的只读几何输入与分块结果，保持完整合法查询/采样，再处理 **seal外首次proxy/texture/resource构造**。cap CPU行工作仍29ms左右，GT细胞准备和签名还在；不要把没有收益证据的SetMesh微优化或继续capture footprint放到前面。若扩展到跨帧必须另建完整epoch/revision/取消/发布协议，本轮安全同帧借用不能直接沿用为异步持有。最终必要回归已完成；未执行的全矩阵、四套视觉全集与长测是明确保留范围，不伪称已重跑。

检查点5c9e495（计时）、9c14ecc（运行时及定向）、9a838fb（两对真实A/B）均已推送。网络推送曾停滞，仅终止已识别的本次git-remote-https子进程，再以30秒低速超时重试成功；未触及Editor、Codex或其他PowerShell。全部失败fixture、无效前台样本、Trace、冷帧及原有Saved证据保留。Git/LFS检查通过，stable两分支保持原SHA；不开始黑色层、不自动关机。

## 19. 阶段性收尾（51eb837 之后，仅文档）

本次以51eb8377f905782a4ee90c5a249bec5435631e90为起点，只核对仓库和既有Saved证据，不进行运行时施工，不构建或重跑功能、视觉、性能矩阵及十分钟长测。当前已完成以下三个生产切片，运行时最终仍为9c14ecc：

| 切片 | 已完成的生产边界 | 各阶段独立证据 |
| --- | --- | --- |
| Ownership（6c66747） | 大批封存历史只读求值、同帧并行join、GT合并 | 184 ownership 566.111→107.680ms；最大真实帧1363.970→921.871ms |
| Capture（59030ab） | 几何footprint只读输入与并行准备、GT合并，省去封存内中间cap提交 | footprint Trace 159.301→56.381ms；三次setup中位538.769→451.701ms；最大帧926.468→848.486ms |
| Cap（9c14ecc） | 只读行计算、按原序合并、GT验证和资源发布；live Current依赖保留串行 | cap CPU Trace 85.771→29.480ms；两次最大真实帧中位836.191→771.154ms |

以上是各阶段独立A/B，不能将不同批次耗时直接相加或宣称精确累计百分比。现有证据已完整记录在16–18节；本次只读取four-run-comparison.json、两份cap-attribution.json、CapSlice_Functional01.summary.json及Episodes/Contracts的summary与oracle，确认148/148功能、Episodes51张/表面oracle、Contracts178张/24帧Whole oracle均PASS。

**初始化仍FAIL**：184仍有749.592–792.716ms完整帧。FRAME PERFORMANCE仍FAIL，LONG-RUN RESOURCES仍PARTIAL；功能与本切片必要视觉验收已完成，不能为收尾机械重跑。原采样、cap精度、合法历史及灰色层规则保持冻结。

下一阶段建议顺序：先处理约80ms occupancy的只读输入/分块计算，再处理seal外首次proxy/texture/resource创建与提交。若这些继续优化后仍有数百毫秒玩家可感知卡顿，再进入跨帧调度/原子发布架构，建立完整不可变输入、epoch/revision校验、取消/失效及GT发布协议；现有同帧借用不能直接延长到跨帧。此顺序仅为后续建议，本次不实现任何部分。

**Large World / 全地图灰色记忆**的分块、流式表现资源和增量空间索引列为后续独立scalability audit，单独建立规模、复杂度与资源生命周期证据，不混入本次初始化收尾，也不视为已实现能力。

收尾核对起点local/upstream/实时remote均为51eb837，工作树与暂存区干净，git lfs status正常、git lfs fsck OK，无遗留测试/构建/Trace进程。两条stable仍分别为7534163b9c5718700b610e7677f47fbaa79cf977与404a5820739638f1097eaae0aa7fba19733298c3。本次仅暂存、提交、推送两份收尾文档，保留全部Saved证据；最终文档SHA以该提交及Git核对为准。

## 20. Occupancy 同帧生产切片（32f6abe 之后）

按上一轮侦察直接施工，不重开旧审计。BuildGeometryDirtyIndices保留原几何/ownership修订失效、精确几何比较、dirty区域和frame geometry复用；先形成有序physical dirty索引，再交给BuildOccupiedSamples。fine与抽出的UpdateCoarseOccupancy共用批量接口：至少4096个查询且具备FrameOccupancy时，最多8个任务借用只读几何、候选、Whole mask和冻结点缓存，写独占逐样本结果/局部计数；同帧join后GT验证geometry revision、输出及cache尺寸，按原索引合并位图、点缓存和统计，随后发布Visual修订，再继续原record证据顺序。小批量、live Actor回退与forced-full oracle串行。没有跨帧持有、异步队列、UObject worker操作或资源创建改动。

点缓存仍精确XY、容量131072、按原序插入且只存center结果，Whole薄边继续检查完整FrameOccupancy，不误用center ROI。worker的TMap只有const读取、没有并发TBitArray写；重复坐标可能在同批多算一次（无并发cache填充），统计报告真实工作量而非虚构串行命中，结果与cache内容不变。暂存O(D)，每个待计算样本保存point及三个bool，生命周期仅本次join；未复制整个history。新增FineOccupancy/CoarseOccupancy/OccupancyCPU/OccupancyMergeGT Trace边界。

控制量 `r.Darkwell.ObjectMemory.JoinedOccupancy=0` / runner `-SerialOccupancy` 保留串行查询；ownership/capture/cap原开关保持开启。串行对照也经过新的有序索引/共用接口，不冒充32f6abe原二进制。没有更改灰层规则、采样精度、Whole/Partial、源隐藏/预备代理/首次显示发布。

完整 `Scripts/BuildEditor.ps1`：Saved/OccupancySlice/Build01.log成功91.08秒（项目/插件依赖重编译），Build02/03分别成功10.82/10.47秒（仅补测试fixture）。Target01实际NullRHI **4/4 clean PASS**：新增occupancy、RepeatedHistoryEvidenceMatchesFullUpdate、FramePhysicalCacheMatchesGeometryOracle、PlanarProjectionMatchesOriginalSlab，测试1.680秒/进程58.053秒、exit0/severe0。补充大coarse fixture的Target02因尚未BeginAbsent就初始化FineHistory触发现有断言，exit3/severe2，失败日志保留；修正测试状态顺序，生产源码未变。最终Target03 **1/1 clean PASS**，23次比较、19个joined批次，测试0.133秒/进程16.532秒、exit0/severe0：逐位fine/coarse、dirty列表、缓存值/容量和修订一致，Whole薄边/掩码、空ROI/物理帧、非空ROI、旋转/倾斜/负缩放/奇异回退、小批/live回退、重复索引、ownership-only/微小位移/物理移除、geometry复用及世界销毁均覆盖；大coarse确有并行dispatch、稀疏映射有独立中心oracle。此前三项规则验证仍适用，未重跑完整148项、全矩阵或长测。

先提交推送该可恢复检查点，再做真实D3D12短Batch A/B与必要首次Whole/cap视觉。此检查点尚未宣称性能收益，初始化/完整帧FAIL、长期资源PARTIAL维持；seal外资源创建是否继续由本切片真实结果决定。stable保持7534163/404a582，不开始黑色层。
### 20.1 首版A/B及空ROI修正

4ccda80首版有效三对（Serial02/03/04、Joined01/02/03）均同二进制、前台D3D12/SM6、1080p/SP100/原Epic/TSR/Lumen/VSM、NoTrace、无固定步长/截图；每次480帧环境异常0、complete/exit0/severe0。双方统一使用已有-NoAuthoringToolsets：原Serial01因364帧失去前台及5条引擎Toolsets在-game缺少Python API错误而无效，已保留，不混入对照。

首版184 occupancy中位62.6565→50.2412ms，但setup282.271→314.794ms、最大整帧599.044→622.071ms，整帧收益不成立。64 occupancy8.1495→9.2162ms有调度退化。独立TraceJoined01（非正常性能样本，环境异常0、complete/exit0/severe0）184 GT专属scope：GeometryDirtyIndices42.083ms，其中FineOccupancy28.793ms，后者CPU/join13.166ms、GT合并15.409ms；GeometryDirtyIndices exclusive13.289ms，coarse另2.824ms。不能重复相加父子scope，不能把独立Trace毫秒加到无Trace样本。导出stats与命令保留Saved/Stabilization/OccupancySlice_TraceJoined01。

据此补严格空ROI分支：原IsOccupiedByActual在空center候选时先于cache直接false；对没有Whole footprint回退的Partial批次，GT按同样索引直接写false并计入全部logical occupancy tests，不分配point/任务/结果数组、不触及点cache。Whole即使center ROI为空也仍走完整footprint查询。没有减少coverage/fine authority采样、改变精度或用空ROI作为VerifiedEmpty；原合法证据仍在随后推进。该分支只随JoinedOccupancy开启，串行对照保留逐点路径。

Build04完整Editor构建成功13.66秒；最终Target04实际NullRHI四项 **4/4 clean PASS**，测试1.488秒/进程18.063秒、exit0/severe0。新增空Partial ROI逐位/零cache写断言，与Whole薄边空ROI正例并存。首版Contracts01也完成178图/三次PIE，24帧Whole oracle PASS、exit0/severe0；它只作为首版证据，最终运行时视觉和重新配对A/B继续补充。seal外资源创建未施工。
### 20.2 最终同二进制真实 D3D12 A/B

最终运行时 **f12c6c1** 冻结，按 Serial01→Joined01→Joined02→Serial02 运行 `OccupancyFinal_*` 四个独立进程。均Standalone Batch、-NoAuthoringToolsets、D3D12/SM6、前台1920×1080/SP100、原Epic/TSR/Lumen HWRT/VSM、NoTrace/无固定步长/无截图；各480帧环境异常0、valid_normal_sample=true、complete/exit0/severe0/log关闭。进程分别19.105/18.924/19.391/19.014秒。本机引擎仍5.8.2 CL56702186；只比较本次同环境A/B，不直接拿旧机器/旧批次约80ms或771ms作本次对照。

四次同DLL SHA256 `AF689CAA3CC76BBC2DC407B131B1DD290DD50971A94A952DB60C5AA3B79261E9`，driver SHA256 `90096AC63F248D5B0AB789F1735FF6613248B3036FD988E83B77784DD7FBDB34`，同DefaultEngine.ini及全部quality配置值。quality.json的initial包含动态帧计时，不把其时长当质量值要求相等；逐帧viewport/foreground/画质均由原分析器严格判定。比较脚本和带全部hash/首末资源/12项case结果的JSON保存在 `Saved/OccupancySlice/compare.py`、`comparison-OccupancyFinal_Serial01.json`；原始逐帧及metadata在各Saved/Stabilization运行目录。

| 184 distributed，ms | Serial01 | Serial02 | Joined01 | Joined02 |
| --- | ---: | ---: | ---: | ---: |
| Setup | 285.192 | 279.223 | 264.319 | 277.329 |
| 首次 occupancy | 64.727 | 65.106 | 37.736 | 38.522 |
| 首次 native memory | 297.824 | 295.445 | 262.604 | 267.777 |
| 最大完整帧 | 591.081 | 584.586 | 535.312 | 552.318 |

两次中位 occupancy **64.916→38.129ms**（减少26.787ms，41.3%），首次native **296.635→265.191ms**，最大完整帧 **587.833→543.815ms**（减少44.018ms，7.5%）。setup中位282.208→270.824ms；没有改seal资源构造，不能把setup及其它阶段的小幅变化都归到occupancy，更不能将父子/跨帧计时相加或声称GPU同等收益。

四次setup均184 records/184 proxies/10 tracked identities；首update均occupancy_tests **2,095,981**、geometry_tests **8,018,953**、samples_scanned **1,972,688**、occupancy_hits0；合法反证后首/末均records/proxies/caps/textures=120、MIDs360、fine bytes41,157,632，resident_samples1,286,176。没有减少seed、合法知识或采样换收益。每次184仍保留1个>100ms帧。64 occupancy中位8.047→6.001ms，但最大帧200.492→201.994ms，未测得64整帧改善；Empty p95中位14.095→13.893ms，不替代旧完整矩阵FAIL。

最终收益同时来自非空查询的同帧并行与空Partial ROI的等价批量回填，不把全部26.787ms归为worker加速。首版三对“CPU有收益、整帧不成立”和初始无效前台样本全部保留，未挑选删除不利结果；最终两对只使用最终二进制，不跨版本混合。

### 20.3 最终正确性、交接决定

最终Build04完整 `DarkwellEditor Win64 Development` 成功；Target04 **4/4 clean PASS**，新增测试24次比较、19批joined，原串行/完整证据oracle通过。没有重跑完整功能manifest，旧148/148仍是上一阶段证据，不冒称本版149/149。最终 `OccupancyFinal_Contracts01`：178图、三次PIE、24/24 Whole离开图像oracle通过，45.428秒；`OccupancyFinal_Episodes01`：51图、六组各896内部样本全无缺失，30.451秒。两组正常D3D12/SM6、protocol/teardown完成、exit0/severe0，已查看首次Whole灰影、完整历史与Partial外部cap原图，未见空帧/内部接缝。固定时间步、2233×911图像只作正确性证据，不混入真实性能。

可复核命令：`Scripts/BuildEditor.ps1`；`Scripts/RunGrayObjectPolicyTests.ps1 -RunName OccupancySlice_Target04 -Tests <summary.json中的四项selector>`；`Scripts/RunGrayPerformanceBaseline.ps1 -RunName <OccupancyFinal运行名> -Mode Standalone -Protocol Batch -NoAuthoringToolsets`（Serial另加-SerialOccupancy）；`python Scripts/AnalyzeGrayStabilization.py <运行目录>`；`Scripts/RunGrayMemoryAudit.ps1 -RunName <上述视觉运行名> -Protocol Contracts/Episodes`，分别经AnalyzeGrayWholeTransitions.py、AnalyzeGrayMemoryEpisodes.py验证。已存在的证据目录不可覆盖，复测应另取唯一RunName。

**本occupancy生产切片完成且收益成立；INITIALIZATION仍FAIL（535–552ms尖峰），FRAME PERFORMANCE仍FAIL，LONG-RUN RESOURCES仍PARTIAL。** 本轮决定不叠加seal外首次proxy/texture/resource施工：GT对象创建/注册/上传和首次显示生命周期是另一完整切片，留作下一入口。现有细胞/签名、资源创建以及约38ms occupancy余量仍在；当前无跨帧队列，不开始黑色层，不移动stable。未重跑完整矩阵、十分钟长测或无关视觉全集，失败fixture和原始Saved证据均保留。

## 21. 首次表现资源切片完成（2026-09-07，起点 a8df0d9，运行时 bf48648）

运行时基线仍为 f12c6c1；中间提交仅维护项目顾问文档。本阶段不重做 occupancy。初始分支干净，开发远端 a8df0d9；远端默认 HEAD 实为 main/46d9f9d，不能与开发分支尖端混称。

最小新增资源 Trace scopes，BuildEditor ProbeBuild01成功22.91秒。`ResourceSlice_TraceBefore01` 是真实前台D3D12/SM6 Batch，exit0/complete/severe0，480帧环境异常0；因Trace开启不作为正常A/B。原始证据在 Saved/Stabilization；Insights stats_Distributed184.csv、events_resources.csv和events_cap_outlier.csv可复查。

184阶段：EnsureResources总70.824ms，seed内552调用70.618ms，其中seal内仅1.490ms，**seal外69.128ms**。首次创建184纹理/184cap/184proxy/552mesh/MID；cap创建25.489ms（注册1.342）、proxy绑定23.437ms（MID创建10.252、参数4.071、mesh注册8.330）、proxy生成11.953ms、历史texture创建6.917ms（allocate2.942/clear1.242/UpdateResource2.668）。父子计时不可累加；当前纹理路径在此seed不执行。TextureSubmission另44.421ms，其中seal内23.163ms，不等于RHI上传成本。

真实异常落点：184首个cap耗时14.760ms，嵌套构造0.146ms、注册0.024ms、LoadObject0.010ms。editor.log明确报告同名MovingCap_Lab.V2.Stress.000_1替换等待清理**14.44ms**，随后同名epoch也有短等待。引擎UObjectGlobals.cpp StaticAllocateObject同名覆盖执行ConditionalBeginDestroy并在IsReadyForFinishDestroy前Sleep(0)。历史销毁/重建复用StableId+Epoch的显式cap名称，使待回收组件被原位替换；这是可安全修复的生命周期成本，不能解释为cap几何生成。

### 生产实施与生命周期边界

归因检查点0899a7f及最终运行时**bf48648ee46e119155e16616c977f3a251bc179b**均已推送。cap分配使用MakeUniqueObjectName取得独立UObject身份，仍在原GT调用中立即创建/注册所有合法cap；已销毁组件沿DestroyComponent/正常GC回收，不再被新对象原位覆盖。没有对象池，旧cap可能短暂存活至GC，不能把“消除覆盖等待”描述为取消清理工作。

同一record所有mesh具有相同parent/texture/bounds/tint/UV/ready参数，仅在record内共享一个MID，禁止跨record共享。OwnedMaterials与Visual.Materials每个MID只登记一次；所有mesh仍在材质完整绑定后注册。prepared Current维持SpatialReady=0；合法seal对唯一MID一次更新最终texture/domain/ready，保留捕获SnapshotTransform和同GT调用原子交接。源Current每part texture、精度、cap算法、ownership、缓存失效及玩家知识规则未改变。

同二进制开关`r.Darkwell.ObjectMemory.RecordScopedResources=0`与runner `-LegacyRecordResources`使用旧cap命名/每part MID路径；默认1启用生产切片。没有创建跨帧任务，也未将首显/合法资源工作挪到预热或未计时帧。

### 正常真实D3D12 A/B（验收数据）

四次ABBA顺序：ResourceSlice_Legacy01 / Scoped01 / Scoped02 / Legacy02。每次480帧、环境异常0、complete/exit0/severe0，正常前台1920×1080/SP100、原质量D3D12/SM6，NoTrace、无固定步长/截图，双方-NoAuthoringToolsets；未修改全局插件/画质。源证据记录HEAD0899a7f与source.patch，补丁与随后提交bf48648的运行时/测试/runner一致；比较脚本另行加入且不改变被测DLL。

| 样本 | 184 setup ms | 184 native ms | 184真实最大整帧 ms | 64最大整帧 ms |
| --- | ---: | ---: | ---: | ---: |
| Legacy01 | 324.900 | 268.836 | 602.475 | 197.236 |
| Scoped01 | 238.567 | 275.185 | 520.370 | 191.714 |
| Scoped02 | 238.337 | 270.024 | 515.810 | 190.924 |
| Legacy02 | 261.376 | 283.921 | 552.176 | 193.416 |
| 旧两次中位 | 293.138 | 276.378 | 577.325 | 195.326 |
| 新两次中位 | 238.452 | 272.604 | 518.090 | 191.319 |

184完整帧两对-13.6%/-6.6%，中位-10.3%（59.236ms）；setup中位-18.7%，native约-1.4%，不宣称native本身显著改善。184每次均保留唯一>100ms完整冷帧。旧路径setup/整帧波动较大，样本只有两对；不把全部中位差归给MID或cap单个函数，也不与旧occupancy的543.815ms跨DLL拼接计算本轮收益。

同DLL SHA256 `A6947266594DA055116A7D4C6AFF6FA79EACF9239A5A38ED76BAEE2E879B3101`；DarkwellEditor模块 `1BC1EE5EAC9CE8B1C7D4B92C31B85FEF00BDEA5C707D3F5FE0298438B10012BF`；driver `90096AC63F248D5B0AB789F1735FF6613248B3036FD988E83B77784DD7FBDB34`；DefaultEngine.ini `9a1584044ffeeee73defc9aca4b908ec68581396e31f100aff9dc7ba644f517e`。比较器校验质量快照时仅排除initial动态计时，保留全部质量项。

setup184记录/proxy；经原合法反证后采样首末120 records/proxies/caps/textures、fine bytes41,157,632、resident_samples1,286,176、samples_scanned1,972,688、occupancy_tests2,095,981、geometry_tests8,018,953、occupancy_hits0，四次一致。MID360→120是同record重复实例合并，全部mesh仍保留。四次日志64+184共248个seal身份/epoch/texture尺寸逐项相同，共2,659,200 texel，尺寸清单在Saved/ResourceSlice/resource_inventory.json。新旧采样UObject数各自平稳（新56751、旧57615）；WorkingSet有启动波动，不作为精确内存节省结论。短样本及GC测试不证明十分钟长期稳定。

复算命令（PowerShell；所有Saved结果只在本机）：

```powershell
./Scripts/BuildEditor.ps1
./Scripts/RunGrayPerformanceBaseline.ps1 -RunName ResourceSlice_Legacy01 -Mode Standalone -Protocol Batch -NoAuthoringToolsets -LegacyRecordResources
./Scripts/RunGrayPerformanceBaseline.ps1 -RunName ResourceSlice_Scoped01 -Mode Standalone -Protocol Batch -NoAuthoringToolsets
./Scripts/RunGrayPerformanceBaseline.ps1 -RunName ResourceSlice_Scoped02 -Mode Standalone -Protocol Batch -NoAuthoringToolsets
./Scripts/RunGrayPerformanceBaseline.ps1 -RunName ResourceSlice_Legacy02 -Mode Standalone -Protocol Batch -NoAuthoringToolsets -LegacyRecordResources
python Scripts/AnalyzeGrayStabilization.py Saved/Stabilization/ResourceSlice_Legacy01 Saved/Stabilization/ResourceSlice_Scoped01 Saved/Stabilization/ResourceSlice_Scoped02 Saved/Stabilization/ResourceSlice_Legacy02
python Scripts/CompareGrayResourceSlice.py Saved/Stabilization/ResourceSlice_Legacy01 Saved/Stabilization/ResourceSlice_Scoped01 Saved/Stabilization/ResourceSlice_Scoped02 Saved/Stabilization/ResourceSlice_Legacy02 --output Saved/ResourceSlice/FinalABBA.json
```

### Trace复核（不混入正常A/B）

ResourceSlice_TraceScoped01同样480帧环境异常0，exit0/severe0；旧TraceBefore01与新Trace均用于定位，不充当同二进制正常验收。两次独立Trace的184资源分项如下：

| 分项 | 旧 ms | 新 ms | 数量/边界 |
| --- | ---: | ---: | --- |
| EnsureResources | 70.824 | 47.504 | 均736调用，包含下列子项 |
| cap创建 | 25.489 | 9.170 | 均184；旧64次覆盖等待，新0 |
| proxy绑定 | 23.437 | 16.081 | 均184；含MID/参数/mesh注册 |
| MID创建 | 10.252 | 5.076 | 552→184 |
| MID参数 | 4.071 | 1.863 | 552→184 |
| proxy mesh注册 | 8.330 | 7.832 | 均552 |
| proxy创建 | 11.953 | 12.341 | 均184 |
| history texture创建 | 6.917 | 7.239 | 均184；格式/尺寸不变 |
| TextureSubmission | 44.421 | 43.463 | 均368；包括CPU内容/签名/Float16，并非纯RHI |
| SealCapture | 206.370 | 198.559 | 均184；与上述部分嵌套 |

正常旧两次也各出现64次同名覆盖等待，新两次与新Trace为0。资源确保下降23.320ms的方向与完整帧改善一致；表中父子计时不相加。Source.Mesh.LoadSynchronous约0.1ms/552次、parent LoadObject约0.5ms/184次，没有证据支持继续堆加载缓存。Current纹理路径在本seed不执行，真实Current准备由下述合同/视觉检查覆盖。

### 最小正确性、生命周期与视觉

- Build02完整DarkwellEditor Win64 Development成功，34.56秒，既有引擎弃用/浮点转换警告；代码之后未修改或重建DLL。
- ResourceSlice_Target01：3/3 clean PASS、exit0/severe0，实际测试1.75秒（进程44.25秒）。新增RecordScopedResourcesParityAndLifetime覆盖两种分配路径×Partial/Whole，Current透明ready、首离开立即发布捕获姿态、全部mesh注册/参数、跨record隔离、证据/texture signature/尺寸/cap signature/三角数一致，同epoch销毁重建采用不同cap对象名、Ensure幂等、MID移出强引用、世界销毁后GC弱引用全部失效。同时运行PlayStopResourceLifetime及StaticPartialEpisodes.ErasedDoesNotRebuild，保护实际释放和旧灰影不复活。
- ResourceSlice_Contracts01：正常D3D12/SM6固定步长视觉合同，178图、三次PIE与GC/teardown，exit0/severe0；whole_handoff_oracle.json 24帧visual_pass=true。实际查看pie0_whole_exit_00.png及pie2_partial_outer_cut.png，首次Whole与Partial外切口正常。此固定步长视觉协议不作性能数据。

```powershell
./Scripts/RunGrayObjectPolicyTests.ps1 -RunName ResourceSlice_Target01 -Tests 'Darkwell.PropLab.ArchitectureAudit.RecordScopedResourcesParityAndLifetime+Darkwell.PropLab.GrayObjectPolicy.PlayStopResourceLifetime+Darkwell.PropLab.ArchitectureAudit.StaticPartialEpisodes.ErasedDoesNotRebuild'
./Scripts/RunGrayMemoryAudit.ps1 -RunName ResourceSlice_Contracts01 -Protocol Contracts
python Scripts/AnalyzeGrayWholeTransitions.py Saved/ArchitectureAudit/ResourceSlice_Contracts01
```

本轮没有失败/裁掉的正常A/B或定向测试；两条Trace明确不属于valid_normal_sample。未跑完整矩阵、十分钟长测、occupancy全集、Episodes/其他无关视觉全集、Shipping打包或独立D3D12 C++自动化；NullRHI C++及真实D3D12视觉/性能证据分别列明。Git diff检查和LFS fsck通过，stable仍404a582/7534163；Docs/AI未维护。

### 验收与下一最高收益入口

**本资源切片收益成立；INITIALIZATION仍FAIL（新516–520ms），完整帧FAIL/长期资源PARTIAL没有升级。** 当前资源创建已分散为proxy约12ms、cap约9ms、texture约7ms、绑定约16ms；大量同帧seal/证据/表现工作远大于任一单独创建函数。新Trace seal约198.6ms，后续native仍约273ms，并且部分scope互相嵌套，不能简单相加做归因。

下一优先级应提升到整批首次历史准备/发布的调度合同：**跨帧调度 + epoch/revision/lifetime校验 + GT原子发布**，先明确合法观察何时形成可展示历史、旧Current/预备资源如何持续有效、reset/销毁/新证据如何取消过期产物，以及整批峰值/最坏首显延迟的双重验收。需要单独授权，不能直接把Freeze或UObject创建搬到worker，不能让晚到结果恢复旧灰影。本轮完全没有开始该系统。

若下一轮仍限定同帧，可评估TextureSubmission内约43ms的像素生成/顺序签名/Float16准备，优先做纯CPU数据路径；其收益上限不足以独自解决半秒首次整帧。共享跨record可变texture/MID、盲目池化、移动Whole首次发布或拆分合法知识都不是本轮建议的安全续作。

## 22. 跨帧首次历史架构定案（2026-09-07，084ab56之后，未改运行时）

完整方案、拟定private接口、源码集成点、状态/失效矩阵、第一生产切片和验收计划见 [SIGHTWEAVE_HISTORY_PREPARATION_SCHEDULING_DESIGN_ZH.md](SIGHTWEAVE_HISTORY_PREPARATION_SCHEDULING_DESIGN_ZH.md)。本阶段只阅读必要源码并提交技术文档；没有新计时、构建、测试或运行时变更。实际运行时仍bf48648，第21节正常两对D3D12数据仍是最新证据，INITIALIZATION仍FAIL。

决策：合法Current期间预算化准备纯数据，原GT合法seal只消费严格匹配的完整结果；未完成或过期就同帧回退，不隐藏源等待。第一切片P1用GT协作续算两种Whole几何mask，不启动worker，不调度历史证据/Partial/UObject生命周期，不重新优化现有几何算法。P1完整包含有界队列、generation/ticket、reset/replace/resume/retire失效、Ready消费与oracle回退，不能只上线影子队列。

关键源码约束：History.Initialize重置epoch，resume复用epoch；SourceReplace保留旧历史；TransformRevision有容差；FrameOccupancy/FrameNewerCandidates只在本帧有效；Visual退休状态参与候选语义。方案因此分离世界/Scene/History/Source/record寿命与精确几何域；未来封存表现还必须独立覆盖evidence/opacity/可逆排除版本。取消旧任务不是取消玩家知识，任务完成不能创建或恢复权威记录。

**对184的结论修正为明确边界，而不是性能承诺**：现有一次调用内同时请求并seal184条，无准备提前量。P1普通Whole路线有机会提前分摊工作，但原冷184应完整保留、可能继续同步回退。若要硬性降低这个冷批次且不增加首显延迟，必须先提供语义上允许的准备阶段；单纯改seed时序不算优化。首显延迟/初始化交互门槛若需要变化，应独立定案，不能在实现时自行放宽。

下一轮验收同时覆盖最大整帧（从合法观察/最初请求起，包含预备帧）、合法seal至首次正确图像的帧差与墙钟时间、AllReady/合法终止、取消/过期结果拒绝、内存高水位与回收。P1额外首显帧延迟要求0；软准备预算不是整帧硬上限。旧冷184与新增有提前量的Whole路线分别同二进制A/B，不跨路线比较。详细拟定测试未执行，不计为通过。


## 23. P1 FirstWholeGeometry：可选生产路径与安全检查点（2026-09-07）

最终运行时 **9382461772468fd481a850783f2eba42a8706653**，首个实现提交33ceecb869d99180fb1a0529d630873c391b0b85；均已推送开发分支。起点8df5665，工作区最初干净。**默认仍为模式0；模式2完整接通，但不宣称整体性能验收通过。INITIALIZATION仍FAIL。** 本阶段不是仅挂后台任务的半套系统：准入、续算、一次消费、拒绝/同帧回退和释放均已接入并测试；没有worker或跨帧发布。

### 实际边界与接口

- `DarkwellHistoryPreparation.h/.cpp`：私有纯票据、两个packed mask、游标、最多128 cell谓词/step、8个推进帧期限、一次Take及跨UpdateMemory保留的frame budget。每cell最多遍历16个primitive，期限失败释放mask并保留有界拒绝描述符，避免相同输入反复重算。
- `DarkwellWholePreparation.cpp`：GT所有者与最多8个驻留包；每包最多65536 fine cells、两组各最多16个descriptor。这些硬数量界限使结果/描述符远小于设计32MiB上限，不是无限字节队列。输入不保存Actor/record/visual指针或借用数组view。
- 只准入首次、合法confirmed Whole、静止策略状态、实际姿态/LastLegalPose/捕获姿态精确一致、没有FineHistory/封存mask的Current。Whole原SAT cell谓词和capture footprint原谓词提取共享，未改容差、分辨率或灰色层规则。
- `UpdateTracked`在原EnsureRecordVisual后申请；`UpdateMemory`末尾按真实GFrameCounter轮转续算，软预算1ms。显式测试frame/work配额不依赖机器速度；同一engine frame、Reset和取消不重置已花预算。多宿主世界保守回退，不给每个宿主各发1ms。没有世界级后台任务服务。
- `FreezeCurrentForHiddenMotion`仍在原合法seal事务内工作。通过精确域验证的Ready只替换Whole mask及后面的capture footprint；fine初始化、限制几何、pixels、proxy/texture/MID/cap、显隐/GT注册提交均留在原调用。未命中、不完整、失效均完整同步回退，不等下一帧。
- 开关`r.Darkwell.ObjectMemory.WholeGeometryPreparation`：0为已有同帧oracle；1为影子计算/两mask比对并仍使用原输出；2为完整P1。之前的ownership/capture/cap/occupancy/resource开关不变。诊断Hold/Invalidate只用于构造Preparing/Stale视觉样本。

### 失效、消费及统计

域同时包含Scene/World/Source/Policy的FObjectKey（含UObject寿命serial）、History.Initialize生成的新FGuid、StableId/Epoch、内容/几何/策略/移动版本，以及精确pose、bounds、size、两组primitive输入。没有只依赖带容差TransformRevision或单个hash判断相等。resume的FineHistory资格检查也排除复用旧epoch。

Reset/EndPlay、释放源/SourceReplace、实际resume、retire、abandon和Lab直接History.Initialize覆盖撤销；源进入Destroy流程即使弱引用尚可解析也拒绝。域变化在请求/推进/消费处重新校验。取出包先从私有队列移除，重复Take及旧ticket失败，失败也撤销旧发布权；不会FindOrAdd权威历史。完成本身不授予确认、知识或空证据，不恢复旧灰。SourceReplace仍保留原合法未反证历史。

两个packed位图只由GT私有游标写。准备查询的统计隔离于权威历史查询统计；`frame_work`计两个mask的cell谓词工作。末次9382461用作用域计账覆盖快照/结果析构及显式取消释放，避免清理被漏在计时末尾之后。`pending`遥测指驻留私有包数（含Ready/Rejected），不是未完成工作数；另有ready/rejected/bytes字段。`bytes`是包内数组采样驻留量，不冒充进程RSS或分配器绝对高水位。

### 构建与定向验证

| 证据 | 结果 |
| --- | --- |
| `Saved/WholePreparation/Build08.log`，`Scripts/BuildEditor.ps1` | 完整DarkwellEditor Win64 Development成功，30.58s；非Live Coding |
| `Saved/GrayObjectPolicy/P1_final_tests` | 5/5，无warning/failure/severe；新增Preparation.Protocol/FirstWholeHandoff，加OrdinaryHost、WholeReobservation、RecordScopedResourcesParityAndLifetime |
| `Saved/GrayObjectPolicy/P1_budget_tests` | 末次释放计账修正后新增协议/交接2/2重验通过 |
| `Saved/ArchitectureAudit/P1_final_contracts0` / `P1_final_contracts2` | 同二进制真实D3D12/SM6 Contracts；各3次PIE、178图，正常teardown，严重错误0 |
| `Scripts/AnalyzeGrayWholeTransitions.py` | 两边各24张首次Whole退出帧全部通过；另人工检查Preparing首帧及Partial外切口 |

新增确定性覆盖：1/127/128/129配额及非word对齐输出；两个独立原包装oracle逐bit比较薄边/旋转/反射；owner/history/record/source/request不匹配、重复消费、回退后拒绝、8推进帧期限；同frame配额不补发；九包以上拒绝（私有快照故障注入，不新增权威身份）；Reset/History.Initialize、极小捕获pose变化、内容/策略变化、Source Destroy、非法capture revision及Preparing同步回退。0/1/2最终capture/fine mask一致，模式1影子命中、模式2实际Ready命中；连续coverage更新只入队一次。

普通宿主回归继续包含真实SourceReplace/Destroy/GC；Contracts覆盖Whole首次离开、StationaryOnly/Never、墙体/视锥、普通相机深度和Partial外切口。没有把这些回归说成任意生命周期排列的穷举证明。

### 最终二进制真实D3D12结果

运行时9382461的四条短测：`Saved/Stabilization/P1_accounted_lead0`、`P1_accounted_lead2`、`P1_accounted_cold0`、`P1_accounted_cold2`。DLL SHA256均为`E65ADD84F9AA45D1089DD2999ECC43C09789CEA6E19770560588F6BAF8DFBB05`；driver SHA256均为`2668B40D2ADDEB5181EE1FAD2339E65F6808505FA4BC97E336A390C77286DFCA`。均Standalone、D3D12/SM6、1080p/SP100、原质量、NoAuthoringToolsets、NoTrace、无固定步长/无截图；lead各180帧、Batch各480帧，记录期前台异常0。cold2启动阶段有9129个等待前台的更新（总进程约104s），保留startup证据，不混入记录窗口；**该最终冷对的启动条件不匹配，只作压力记录，不作为严格收益A/B。** 前述较早同DLL冷对照仍保留，不能用最终这一对更快的数字覆盖反向证据。

| 路线/指标（ms） | 模式0 | 模式2 |
| --- | ---: | ---: |
| 有提前量Whole，整个窗口最大完整帧 | 28.033 | 26.130 |
| 有提前量Whole，setup | 4.616 | 4.022 |
| 有提前量Whole，native最大 | 14.821 | 16.525 |
| 同一合法退出帧，完整帧 | 27.960 | 19.444 |
| 同一合法退出帧，native | 9.216 | 4.513 |
| 原冷184，最大完整帧 | 548.577 | 520.202 |
| 原冷184，setup | 268.380 | 244.542 |
| 原冷184，native最大 | 272.860 | 268.413 |

提前量路线两边同轨迹：先记录未观察状态，index5转入合法观察、65转离；不是给模式2增加预热。模式2在index6请求、10 Ready、66消费，requests=1/hits=1/fallback=0，最后驻留包/bytes为0；两mask共82944 cell工作，准备计账合计7.981ms，单frame最高2.512ms，包内数组采样峰值11840 bytes。**1ms是软预算，实测有超支，不能写成硬上限达成。** 单目标AllReady为index10；没有以此冒充184目标跨帧收敛。

两模式提前量路线最终同为1 record/proxy、0 cap、4 textures、1 MID、fine_bytes=1327104。冷184两边setup均184 records/proxies、10 identities；原后续合法证据推进后均120 records/proxies/caps/textures/MIDs、fine_bytes=41157632。没有减少合法创建或把184 seed拆到多帧；P1不改变该同步压力基线。

**不能据最后一对宣称稳定整体收益。** 此前同二进制P1_final_lead0/2为23.538→26.723ms，最大帧转移到首次Current；P1_final_cold0/2为530.669→557.163ms；更早冷对照520.125→542.256ms。不同修订的样本不合并做中位数、也不裁掉反向结果。可重复的结构性事实是Ready消费能移走部分seal几何工作，但首次Current/resource整帧仍主导；冷184没有提前量，仍约半秒且有明显运行间波动。

### 首显与视觉墙钟证据

Contracts模式2三个周期分别为正常Ready、保持Preparing、显式Invalidate后Stale。对应第一张`whole_exit_00`均正确，后续连续8帧通过，与模式0相同；**本测试范围内首显额外帧延迟0**。Ready周期遥测hits=1；另两周期均原同步fallback，Stale还记录cancelled=1，最终队列/bytes为0。

同次视觉诊断记录seal入口至截图读回完成的墙钟上界：模式0三次61.066/62.546/62.294ms；模式2 Ready/Preparing/Stale为41.143/60.012/59.831ms。三样本最近秩p95等于max（62.546/60.012ms），样本量小。这包含截图读回等待，**不是无读回正常游戏的GPU呈现/屏幕扫描延迟，也不能充当正常性能A/B**。图像/墙钟证据来自33ceecb；最后9382461只补释放计账，完整构建及定向测试重验，未机械重跑178图。

### 判定、剩余风险和下一入口

保留默认0作为安全检查点，模式2完整可选；不升级INITIALIZATION，不开始更大的跨帧系统。准备/消费/失效功能已接通并有实际命中，但**默认启用/整体性能收益门槛尚未通过**。下一轮优先在现有P1内核查首次Current重帧的准备预算、准入/校验开销与真实整体峰值，再补更细的取消原因/多宿主压力及无截图呈现延迟证据；不要因seal变快就直接扩展worker/Partial/资源池。

已知限制：多宿主只做保守回退，未做多宿主性能压测；票据错误/重复/取消由确定性GT测试覆盖，没有真实worker乱序完成测试（本实现没有worker）；没有完整随机生命周期组合、十分钟长测、完整性能矩阵、occupancy全集、Shipping打包或无关视觉全集。没有Large World、黑色层、跨帧evidence/知识/资格/资源创建。Docs/AI未维护，stable未移动。

失败证据保留：最初Build01因不完整类型UniquePtr析构失败后修正；P1_protocol02有一个失效断言失败，后续补Destroy防护并使微位移测试确实修改精确输入，之后通过。初次真实P1_lead_mode2为60次请求/59次撤销/0命中，暴露TryResume探测入口每帧撤销；已移至真正resume前，并补“连续coverage只请求一次”断言。该早期快帧不是有效P1收益。


## 24. P1 首次Current归因与重帧门控（2026-09-07，默认仍0）

起点开发分支/远端dcc0855406f8435c9bc4a94e95bbcb9b5691054d，工作区干净；remote默认HEAD仍main/46d9f9d。已读AGENTS、第23节、交接及设计第8–9节。本轮运行时 **8831d86a3ef0bd8c4a0a6c349c01008de7c30a1b** 已commit/push。没有扩大FirstWholeGeometry、启动worker或改变知识/证据/资格/Partial/资源创建及冷184时序。

### 实际成本与预算归因

固定提前量路线index6首次Current，调用链为UpdateMemory → UpdateTracked → EnsureRecordVisual → BindProxyMaterial → LoadObject(M_MovingAccumulatedMemory)。六次匹配启动的同DLL实测：

| 首次Current分项（ms） | 范围 |
| --- | ---: |
| native memory整次更新 | 11.518–17.216 |
| EnsureRecordVisual（含下面子项） | 10.044–15.530 |
| 历史材质LoadObject | **9.241–14.497** |
| proxy Actor/mesh准备 | 0.242–0.452 |
| 历史透明纹理创建 | 0.155–0.245 |
| MID创建 / mesh注册 | 0.133–0.229 / 0.118–0.210 |
| Whole几何资格块 / Current推进块 | 0.097–0.196 / 0.021–0.043 |
| Current texture / cap调用 | 0.148–0.253 / 0.002–0.007 |

材质同步加载是本路线首次Current的主要GT成本。LoadObject计时包含引擎内部加载/等待，本轮没有Trace深入其依赖序列化、资源初始化或等待占比，不能把全部时间称为磁盘I/O。表中存在嵌套，不能相加；也不能用native分项加总替代wall最大整帧。

旧准备路径在Current资源提交未结束时准入，帧尾再花约1ms；snapshot/匹配后没有检查本作用域尚未入账的耗时，仍可能分配输出或进入128-cell不可抢占chunk。旧计账覆盖作用域析构和取消，但漏掉部分轮转/查找/临时容器等外围开销。本轮新增request/snapshot/admission/step/max_chunk/cancel/take及Current资源分项计时。

**第23节的2.512ms未复现，不能追认其具体函数来源。** 本轮旧调度B/C的snapshot最大0.081/0.288ms，admission最大0.003/0.001ms。C的最大frame_ms=1.1837发生在index65：step=0，request=0.0237，snapshot=0.0069，cancel作用域=1.1618ms；cancelled计数未增加，说明这是失效检查作用域墙钟尖峰，不能说成释放了大量mask。可能包含系统抢占/等待，未进一步Trace确认。新路径仍保留必须立即执行的取消/消费，绝不为了1ms数字推迟失效。

### 实施边界

- 外层WholeGeometryPreparation仍默认0，0/1/2协议及原mask谓词、容量/寿命/精确域校验/一次Take/同步fallback不变。
- 新诊断开关`r.Darkwell.ObjectMemory.WholePreparationFrameGuard=1`：在UpdateMemory帧尾、合法Current及历史/资源工作结束后重新查询并尝试原Snapshot准入。没有跨帧保存Actor引用或引入新发布队列。开关0保留旧调度，runner加`-LegacyWholePreparationBudget`用于同DLL对照。
- 本帧有texture/MID创建、cap rebuild，或UpdateTracked合计达到1ms，就不做可选snapshot/admission/geometry step。按engine frame锁存，重复UpdateMemory/Reset/Invalidate不补发本帧机会；下个空闲帧可继续，既有包消费前仍精确验证。必要取消、合法Current和seal均不等待。
- snapshot/匹配后，使用`Spent + InFlight`再次检查是否还能准入/step；总预算补入帧尾外围开销及清理，扣除已嵌套计账部分，避免重复收费。128-cell chunk仍不可抢占，1ms仍是软预算而非硬保证。
- 新的确定性测试显式注入前台耗时，与已有显式frame/work配额一致；生产使用实际UpdateTracked耗时。首次测试因NullRHI夹具日常工作>1ms导致Ready断言失败，修正测试时钟输入后全部通过，未放宽生产门槛。

### 同二进制D3D12短对照

六次固定WholePreparation路线各180帧，均无等待前台更新、记录期前台异常0，Standalone/D3D12 SM6/1080p/SP100/原质量、NoTrace/无截图/无固定步长、NoAuthoringToolsets。DLL SHA256 `948220F699514AB76A06A84E8679D98332793EB4A56DE19F0DC82E15D7B8C66D`；driver SHA256 `2668B40D2ADDEB5181EE1FAD2339E65F6808505FA4BC97E336A390C77286DFCA`。源对应8831d86；部分记录在commit前采集，source.patch保留同一最终代码。下表保留所有窗口，不删除非Current慢帧，单位ms。

| Saved/Stabilization运行名 | 整段MaxFullFrame | 首次Current | seal | preparation MaxFrame | 最大单chunk |
| --- | ---: | ---: | ---: | ---: | ---: |
| P1Budget_Lead0_B（oracle） | 26.417 | 26.417 | 22.919 | 0 | 0 |
| P1Budget_Lead0_C（oracle） | 27.572 | 27.572 | 20.169 | 0 | 0 |
| P1Budget_Legacy2_B（改前） | 39.533 | 24.653 | 17.214 | 1.005 | 0.073 |
| P1Budget_Legacy2_C（改前） | 32.898 | 32.898 | 16.696 | 1.184 | 0.012 |
| P1Budget_Guard2_A（改后） | 28.404 | 28.404 | 18.239 | 1.007 | 0.017 |
| P1Budget_Guard2_B（改后） | 24.101 | 24.101 | 16.608 | 1.030 | 0.114 |

旧B最大帧在index1、尚无准备请求，不能把该39.533ms的下降归因于门控。两次整段中位：mode0 26.995、新mode2 26.253ms，仅约2.7%且一对反向；**稳定整体收益未成立，默认启用2条件未通过**。seal native两次中位mode0 7.958、新2 4.727ms；seal wall中位21.544→17.423ms，仍不能只报seal。

门控结构效果成立：新两次首次Current都request/snapshot/step=0、work=0，仅门控计账0.0007/0.0003ms，原两次首次Current准备1.005/1.003ms。新单帧超支最大0.0074/0.0303ms，各4帧；最大单chunk反而有0.114ms样本，不能声称所有分项都变快。旧请求在index6，Ready index10/9；新从后续空闲帧准备，Ready index11/12，seal仍index66。双方均完整82944 cell、requests1/hits1/fallback0/cancelled0，峰值包内数组11840 bytes、最终0；新门控帧数2/3。

新全窗口准备账合计11.683/11.049ms，旧5.291/6.516ms：新帧尾逐tracked尝试准入及更完整外围计账有总成本，不能当作等口径纯CPU回归百分比，也不能隐去它。没有减少合法工作或降低采样换成绩。六条路线逐index比较records/proxies/caps/textures/MIDs/fine_bytes/resident_samples/samples_scanned完全相同，最终1 record/proxy、0 cap、4 textures、1 MID、fine_bytes1327104；bit/寿命正确性由下列定向测试补足，计数相同不冒充所有像素逐bit证明。

复算：`python Scripts/CompareWholePreparationBudget.py Saved/Stabilization/P1Budget_Guard2_A Saved/Stabilization/P1Budget_Legacy2_B Saved/Stabilization/P1Budget_Lead0_B Saved/Stabilization/P1Budget_Guard2_B Saved/Stabilization/P1Budget_Legacy2_C Saved/Stabilization/P1Budget_Lead0_C`；本机汇总`Saved/WholePreparationBudget/final_lead.json`。

早期P1Budget_Lead0_A/Legacy2_A分别等待前台2826/3293次更新，最大窗口104.601/131.519ms；保留但不混入匹配启动组。P1Budget_Attribution_Mode2是早期instrumentation DLL，只用于发现binding入口，不混入最终A/B。

### 正确性、视觉和冷184

完整Editor构建`Scripts/BuildEditor.ps1`，最终`Saved/WholePreparationBudget/Build04.log`成功。`RunGrayObjectPolicyTests.ps1 -RunName P1Budget_Regression02 -Tests 'Darkwell.ObjectMemory.Preparation+Darkwell.ObjectMemory.OrdinaryHost+Darkwell.ObjectMemory.WholeReobservation+Darkwell.PropLab.ArchitectureAudit.RecordScopedResourcesParityAndLifetime'`：5/5 clean PASS、severe0。包括0/1/2两mask oracle、精确失效/一次消费/Preparing fallback，以及新增in-flight预算、资源重帧零准入、同frame不能绕过门控、Invalidate不清latch、下一空闲帧恢复及heavy frame零几何工作。实际SourceReplace/Destroy/GC和原资源生命周期回归通过。

`DARKWELL_WHOLE_PREPARATION_MODE=0/2`分别运行`RunGrayMemoryAudit.ps1 -Protocol Contracts`，证据`Saved/ArchitectureAudit/P1Budget_Contracts0/2`，各178图/三次PIE、exit0/severe0/teardown完成。AnalyzeGrayWholeTransitions两边各24张首次Whole退出连续帧全PASS；人工查看新mode2首张Whole及Partial外切口。mode2 Ready首离开hits1，Preparing fallback1，Stale cancelled1/fallback1，三者消费后bytes0。测试范围**首显额外帧延迟仍0**，Current透明预备和原GT原子交接保持。

seal→首图读回墙钟上界mode0三次43.394/45.218/45.847ms，mode2 Ready/Preparing/Stale为43.549/60.586/44.469ms；含截图读回开销，不是正常无截图呈现延迟，不纳入性能A/B。

原Batch输入未改，本轮仅一对压力短测P1Budget_Cold0/Cold2：184 MaxFullFrame535.078/524.639ms，setup251.005/253.790ms，native276.348/263.071ms。mode0等待前台1348次、mode2为0，**启动不匹配，不宣称冷184改善**。两边setup184records/proxies、10 identities；后续原合法反证后均120records/proxies/caps/textures/MIDs、fine_bytes41157632，记录期前台异常0。INITIALIZATION仍FAIL，未升级长期资源判定。

### 下一入口与刻意未做

本轮落地的是重帧避让和计账安全切片，不是首次Current主成本已被消除。下一最高收益入口为BindProxyMaterial内首次历史父材质同步加载及其依赖初始化生命周期；先短Trace拆开LoadObject内部等待/依赖，再评估合法初始化阶段的材质可用性与GC持有。必须把初始化/预备帧纳入完整窗口，不能把这10余ms偷偷移到测量外当作收益。另可在现有P1范围限制帧尾候选扫描的总开销；不扩worker/Partial/evidence/资源池，不因这次seal收益扩大异步协议。

没有重跑完整矩阵、十分钟长测、occupancy全集、无关视觉全集、Shipping、随机生命周期穷举、多宿主压力或无截图GPU呈现延迟。没有证明OS抢占下1ms硬上限，也没有复现归因旧2.512ms。默认仍0；stable、Docs/AI、黑色层、Large World未动。Saved原始证据留本机，源码/复算工具及本技术文档随Git交付。


## 25. 历史父材质首次可用性与Scene生命周期（2026-09-07，43c5748）

开始核验开发分支/远端5881eaa0853b2c9aca5cdbcbf105d44dc1c702ef、干净工作区；remote默认HEAD仍main/46d9f9d。已读AGENTS、交接、第24节及Scene/注册/Reset/EndPlay/Bind资源源码。运行时 **43c5748db9afbd7c861d7a3be7389437ca2a4eab** 已推送。WholeGeometryPreparation仍默认0，未扩大P1、Partial/evidence/worker/资源池，未修改资产或冷184输入时序。

### LoadObject内部：同步完成包和材质PostLoad，不是单纯磁盘I/O

短真实D3D12 Trace `Saved/Stabilization/ParentMaterial_TraceBefore`（起点8831d86的DLL，仅归因，不混入最终A/B）。用UnrealInsights导出timers/threads及事件，按实际GameThread和最大Darkwell_Resources_MaterialLoad时间域筛选：

| 嵌套事件 | inclusive ms |
| --- | ---: |
| Darkwell_Resources_MaterialLoad / LoadObject | 14.767 / 14.764 |
| FlushAsyncLoading | 14.716 |
| Event_DeferredPostLoad | 9.668 |
| UMaterial::CacheResourceShadersForRendering | **9.133** |
| BuildShaderMapIdOverride / FinishCacheShaders | 4.520 / 4.546 |
| 两处FMaterialShaderMapLayoutCache::FindOrCreateLayout | 4.464 / 2.597 |
| FHLSLMaterialTranslatorTranslate | 0.775 |
| Event_PreloadExports / Event_CreateExports | 2.332 / 1.388 |
| Event_ProcessPackageSummary | 0.832 |
| FMaterialShaderMap::Compile | 0.066 |

加载范围39.4295422–39.4443090s，GameThread Id=2，MaterialLoad TimerId=4547。父子scope不能相加；两个layout事件分别属于不同父分支。约9.1ms是材质渲染shader资源缓存/布局和PostLoad CPU工作；其余主要落在包summary、export创建/预加载等。FlushAsyncLoading名称不表示这14.7ms全部空等I/O或GPU；Trace显示GT在同步完成实际加载/PostLoad工作，没有证据把主体归因于GPU fence或ShaderCompileWorker长编译。未逐一分摊依赖包I/O字节或所有引擎内部小事件。该结果来自uncooked Editor -game D3D12路径，包含EditorOnlyData和shader缓存工作，不外推Shipping相同成本。

原始capture.utrace、all_events.csv、timers.csv、threads.csv和material-export.rsp/log保留本机，脚本导出使用TimingInsights.ExportTimingEvents及实际ThreadId过滤。

### 最小生产生命周期方案

父材质是不可变的表现依赖，可以在合法source接入Scene时持有，无需等知识/首次Current。新增`InitializeHistoryPresentationResources()`，GT幂等接口；在RegisterRememberable通过源/策略/世界/身份合法性和重复注册检查后调用。空Scene的构造/BeginPlay不加载；调用本身不创建record/proxy/MID/texture、不确认知识，也不等待下一帧显示。

Scene用`UPROPERTY(Transient) TObjectPtr<UMaterialInterface> HistoryParentMaterial`强持有。首次source注册或显式资源初始化成功后，Bind只取现有父对象；没有经过注册/显式准备的兼容宿主仍在Bind内同调用同步fallback。缺失资产记录Error并返回false；注册不会静默成功，Bind不会拿nullptr继续创建MID。每record的MID及其参数/姿态/SpatialReady仍独立，父材质没有被修改，不是MID池或跨record可变状态共享。

Reset清的是知识和表现实例，保留同Scene不可变父材质；SourceReplace复用同一父对象，旧历史仍按原证据处理。EndPlay先按原Reset释放表现，再清父引用；新Scene单独持有/登记自己的依赖。没有static裸指针、AddToRoot资产或持有旧World/Source回调。资源即便在不同world由引擎解析为同一全局资产对象，也不携带旧world知识；世界相关MID仍属于各Scene。未进入正常Play生命周期的临时对象最终由Scene自身GC释放引用。

`r.Darkwell.ObjectMemory.SceneHistoryParent=1`默认启用本资源切片；0保留原每次Bind的LoadObject路径。**这与WholeGeometryPreparation无关，后者仍0。** runner的`-LegacyHistoryParent`用启动期DPCvars在世界/注册之前设0，避免ExecCmds执行过晚形成假oracle。

### 成本判定与完整测量窗口

结论是 **交互卡顿改善，但总成本迁移**。首次包/材质初始化没有被消除：旧Current LoadObject约9.24/9.42ms，新显式初始化约10.29/10.42ms。Scene存活期间不再重复字符串LoadObject、且Reset/GC不会因Scene主动放弃引用而需要重新取得父资源；未通过真实强制卸载对照证明多次9ms初始化被消除，不作此宣称。

新增`-Protocol ParentMaterial`复用原180帧固定合法Whole路线，P1=0，两边都先用启动期DPCvars禁用自动取得父材质；index0断言object_loaded=0且loads=0，再在已记录窗口内按A/B开关调用同一个显式生产资源初始化接口。新路径初始化完整包含在index1的wall interval；Current仍index6，seal仍index66，窗口持续到index179稳定状态。旧路径接口为no-op，加载仍在Current。

这是**显式资源准入开始至稳定的完整窗口**，不是整个进程启动/地图加载时间。生产默认将该幂等接口接到首次合法Register；其更早的自动注册初始化总启动帧没有被拿来宣称性能收益。独立窗口保证任何父材质预加载帧不被裁掉，默认自动注册的正确性由真实普通宿主/PIE测试覆盖。原Batch输入与计时保持不变。

四次匹配启动采集次序Scene2 → Legacy2 → Legacy3 → Scene3，均180帧、等待前台0、记录期前台异常0；同DLL、同driver，D3D12/SM6、1080p/SP100/原质量、NoAuthoringToolsets、NoTrace/无截图/无固定步长。DLL SHA256 `9AA94315A6F945DE3D852CEC8D90DE07DCA81687D5B648F405B1AFC5BA31241D`；driver SHA256 `0D522FF1A258F609F881FAFF4A7DBD0EBAF29324F221BBD118D5DB1DB87C4AC7`。单位ms，所有index0及其他慢帧保留：

| Saved/Stabilization运行名 | 完整MaxFullFrame | 初始化帧index1 | 首次Current | seal |
| --- | ---: | ---: | ---: | ---: |
| ParentMaterial_FinalLegacy2 | 22.834 | 13.646（无父预加载） | 22.834 | 22.782 |
| ParentMaterial_FinalScene2 | 17.778 | 16.399 | 11.562 | 17.778 |
| ParentMaterial_FinalLegacy3 | 21.944 | 7.751（无父预加载） | 21.063 | 21.879 |
| ParentMaterial_FinalScene3 | 22.199 | 19.172 | 12.809 | 17.228 |

第二对完整峰值分别在index0，不删除它们。完整窗口一对降低、另一对基本持平，**不宣称总工作量或稳定整体性能收益**。seal路径未改，不把其运行间波动归给本切片。

| 首次Current分项，两次ms | Legacy | Scene持有 |
| --- | --- | --- |
| native memory更新 | 10.565 / 10.932 | 0.992 / 1.095 |
| BindProxyMaterial | 9.552 / 10.030 | 0.181 / 0.198 |
| ParentMaterialLoad/取得父对象 | 9.239 / 9.423 | 0.0012 / 0.0011 |
| MID创建 | 0.131 / 0.225 | 0.077 / 0.103 |
| mesh注册 | 0.108 / 0.246 | 0.079 / 0.070 |
| 历史透明texture创建 | 0.294 / 0.172 | 0.233 / 0.254 |
| Current texture | 0.102 / 0.187 | 0.080 / 0.071 |

新Scene各loads=1、held=1贯穿窗口；旧Scene专有loads计数为0，不能解释为旧路径没做LoadObject。四次逐index的records/proxies/textures/MIDs/caps/fine_bytes/resident_samples/samples_scanned一致。最终仍1record/proxy、4textures、1MID、0cap、fine_bytes1327104，不减少合法资源。复算入口`Scripts/CompareHistoryParentMaterial.py`，汇总`Saved/ParentMaterial/final_abba.json`。

FinalLegacy1/FinalScene1分别9473/1133次前台等待，完整窗口292.027/46.132ms，保留但不混入匹配组；早期LegacyA属于测试夹具修正前DLL，仅验证协议。未用不同二进制结果拼成A/B。

### 构建、生命周期和视觉

最终`Scripts/BuildEditor.ps1`完整DarkwellEditor Win64 Development成功，Saved/ParentMaterial/Build04.log；ParentMaterial_Target03 **3/3 clean PASS、severe0**：HistoryParentLifetime、OrdinaryHost、RecordScopedResourcesParityAndLifetime。新测试覆盖空Scene无BeginPlay加载、显式初始化幂等/无知识、旧路径不强持有、Scene强引用跨GC、Reset保留、SourceReplace只加载一次、Destroy/EndPlay清引用、world teardown/GC及下一世界重新初始化。使用临时材质实例作GC探针，避免真实资产被编辑器其他引用持有而掩盖UPROPERTY缺陷；不声称真实材质包必然已卸载。

最初Target因测试世界未InitializeActorsForPlay而不路由EndPlay、释放断言失败；修正后Target02通过但缺WorldContext产生warning，再补正常context创建/销毁，Target03 clean通过。这是夹具修正，失败证据保留。

视觉：ParentMaterial_ContractsLegacy/ParentMaterial_ContractsScene，P1均0，旧父路径由RunGrayMemoryAudit.ps1 -LegacyHistoryParent控制。各三次真实PIE、178图、正常world进入/退出与资源重建，exit0/severe0/teardown完成；两边各24张首次Whole退出连续帧全部通过，首图正确，额外首显延迟0帧。人工检查新路径首次Whole及Partial外切口。seal→首图读回墙钟上界旧58.019/43.069/44.277ms，新43.024/44.108/42.131ms，含截图读回开销；与无截图性能窗口分别记账，不当作GPU正常呈现延迟。

原冷184独立新版本压力`ParentMaterial_ColdPressure`：MaxFullFrame **533.639ms**、setup257.921ms、native268.504ms，无前台等待/记录异常。setup184records/proxies、10 identities；合法后续反证后120records/proxies/caps/textures/MIDs、fine_bytes41157632。只有单条压力样本，不与旧版拼收益；约0.5s同步压力不变，**INITIALIZATION仍FAIL**。

### PropLab资产边界与下一入口

通用生产Scene确实硬编码依赖`/Game/Darkwell/Vision/PropLab/M_MovingAccumulatedMemory`。项目运行时使用项目材质符合C++规则/资产负责表现的架构，但该路径及制作脚本create_moving_history_ownership_material.py仍标为Lab derivative，资产归属和生产命名存在遗留边界问题。字符串LoadObject加Transient运行时引用也不是显式cook依赖声明，本轮未做Shipping cook验证，不能据Editor成功确认打包完整性。另有source/cap的PropLab材质依赖，不在本轮扩展整理。

后续单独明确生产材质归属、用Unreal资产工具迁移及修复引用/建立可审查的资产引用与cook合同；本轮不重命名、复制或改写任何uasset。不要将材质中的灰层表现公式擅自改成新的知识规则。

交互热点移除后，最高收益下一入口是用本版父材质就绪路径重新做既有P1 mode0/2联合验收，判断seal是否重新成为主峰；不扩大协议或直接默认2。冷184仍是整批同步建立架构问题，不能以这10ms迁移升级初始化评级。未做矩阵/十分钟/occupancy全集/随机生命周期穷举/Shipping/进程冷启动总时间/无截图GPU呈现延迟/真实材质反复卸载压力。stable、Docs/AI、黑色层、Large World未动。

## 26. 批量历史表现架构判断（2026-09-07，仅文档）

基线cad92f014cfb88d4ba9e333eae5814184a9ba5a6，实际运行时仍43c5748db9afbd7c861d7a3be7389437ca2a4eab。本节没有新benchmark、构建或运行时改动；复核第25节原始ParentMaterial_ColdPressure/performance.json、frames.jsonl及生产Scene、Lab seed和SaveGame源码。详见[批量历史表现架构判断](SIGHTWEAVE_BATCH_PRESENTATION_ARCHITECTURE_ZH.md)，其中第6节是下一轮A0生产合同。本节的下一入口替代第25节末尾继续P1微项验收的建议。

### 两阶段同步成本与测试含义

cold184为SetGrayPolicyStressMode(6)的三身份64+64+56条SpatialPartial/StationaryOnly历史。ConfigureHistoricalEpochCountForTesting在同调用内人工FullCoverage、AdvanceCurrent(0.20)、逐epoch记录/seal；不是184个正常Whole离开或已接入的真实读档。Batch先Empty、SameIdentity64再Distributed184，所谓cold是历史资源冷建立，不是全进程/磁盘/驱动冷启动。setup要求184套资源，未要求或验证同帧184条可见像素；实际可见K未知。

原完整wall533.639ms，setup257.921、native268.504ms。首次native的Historical父scope240.040ms；ownership99.499、occupancy39.179、texture20.335、cap51.006ms为主要分项，不能与父scope重复加总。扫描1,972,688样本，occupancy tests2,095,981、geometry tests8,018,953；184次texture/cap更新、各182次upload/rebuild。合法反证后120条和41,157,632 fine bytes；后续sleeping=120仍驻留120proxy。稳定样本约12–13ms不能替代冷帧。

成本模型为seed CPU(N,S,G) + evidence(S_dirty,重叠I) + materialize(必显K,parts,texels,cap几何) + submit + other。对象/组件/每record资源及固定提交近似O(N+parts)，fine/像素按总samples/texels增长，ownership密集候选可能超线性。不能把所有工作称作O(N)资源注册，更不能认为atlas可消除约100ms ownership或所有capture/fine工作。setup后首次证据使表现再次变化，只有另行定义bulk-import中间状态不发布的合同并证明知识等价，才可能合并阶段；本轮不改seed时序。

### 推荐与边界

A按需物化有利于大量旧历史、少量必显K，CPU知识/证据始终推进；B atlas/实例化/共享renderer合并对象及提交，适合真正大量同屏同mesh历史。两者互补。规划敏感性而非实测：假设A可延期纯表现P=80–160ms、K/N=25–50%，对应40–120ms窗口减负；假设B固定管理/提交F=30–80ms可消除50–80%，对应15–64ms。P/F尚未测定，不能作为收益承诺；原cold184最近seal全部保守pin时A可能零收益。两者都不能据此承诺533ms到帧预算。

首切片A0只分离FRecordVisual内CPU状态与可回收GPU资源，默认全驻留、预期性能收益0。禁止以Visual缺席/retired替代驻留状态，因为当前这些条件影响ownership、证据和终结释放。提供单旧record的开发释放/重建oracle，期间推进真实新证据，证明重建来自当前CPU revision、不复活旧历史；Current与最近seal不逐出，同帧fallback保留首显。完整边界、幂等/Reset/SourceReplace/Destroy/GC/world和Whole/Partial视觉验收见设计第6节。A0不包含自动距离流式、worker、资源池或自定义scene proxy。

冻结已有P1及ownership/capture/cap/occupancy/resource/parent-material局部优化，默认P1仍0、旧路径作oracle。建议新增独立GameplayFirstHistory、SceneRestoreInitialization产品验收，原cold184保留stress身份和完整输入；真实历史restore合同目前未定义，不能填PASS。**原INITIALIZATION仍FAIL，本轮没有修改gate或宣称新性能收益。** 未重跑任何测试，没有新视觉/生命周期/性能通过声明；stable、Docs/AI、黑色层、Large World未动。

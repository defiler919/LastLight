# SightWeave 灰色层性能架构审计

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

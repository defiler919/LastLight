# 灰色层功能检查点与稳定化施工

## 首次表现资源生产切片完成（2026-09-07，a8df0d9之后）

最终运行时 **bf48648ee46e119155e16616c977f3a251bc179b** 已推送；最小归因检查点0899a7f。启动核验开发分支/远端a8df0d9、工作区干净；remote默认HEAD是main/46d9f9d，并非开发分支。只沿第20节之后处理首次表现资源，没有重做occupancy、维护Docs/AI、启动黑色层或移动stable。

定位到两个资源生命周期成本：①cap用StableId+Epoch固定UObject名称，销毁后同epoch重建会触发引擎原位覆盖和渲染清理等待；Trace首个cap14.760ms，日志明确同名覆盖等待14.44ms。②同record三个proxy mesh的MID参数完全相同却分别创建。现在cap使用MakeUniqueObjectName，保留旧组件正常销毁/GC生命周期；MID仅在**同record内部**共享，GC强引用OwnedMaterials及Visual.Materials各登记一次，seal按唯一MID更新最终texture/bounds/SpatialReady。不同record不共享，Current仍SpatialReady=0透明预备，首次离开在原GT调用内发布捕获姿态及完整资源。没有跨帧队列、池化、降低精度或省略合法创建/提交。诊断开关r.Darkwell.ObjectMemory.RecordScopedResources=0保留同二进制旧分配对照。

正常真实D3D12/SM6两对ABBA，四次各480帧环境异常0，1080p/SP100/原质量、前台、NoTrace/无固定步长/无截图，双方-NoAuthoringToolsets，同DLL/driver/config。结果如下（毫秒）：

| 184 distributed | 旧分配 | 新分配 |
| --- | ---: | ---: |
| 第1对最大完整帧 | 602.475 | 520.370 |
| 第2对最大完整帧 | 552.176 | 515.810 |
| 最大完整帧两次中位 | 577.325 | 518.090 |
| setup中位 | 293.138 | 238.452 |
| native memory更新中位 | 276.378 | 272.604 |

整帧两对分别-13.6%/-6.6%，中位-10.3%；旧路径波动明显，不将全部59.236ms中位差归给某个局部函数。native基本持平。独立归因Trace的EnsureResources70.824→47.504ms、cap创建25.489→9.170、proxy绑定23.437→16.081；旧路径64次同名覆盖等待，新路径0次。texture创建仍约7ms，不是剩余唯一瓶颈。

setup仍184条/proxy，首次采样经原合法反证后120条/proxy/texture/cap、fine bytes41,157,632；MID360→120，减少的是重复材质实例。184首次创建Trace仍184texture/184cap/184proxy/552mesh，MID552→184。四次64+184共248次seal逐条身份/epoch/纹理尺寸相同，共2,659,200 texel；所有既有occupancy/geometry查询及resident/scanned计数相同。新组件不覆盖尚待回收的旧对象，短期可能共存至正常GC；未创建资源池，也未把首显推迟到GC之后。

验证：BuildEditor Build02完整成功34.56秒；ResourceSlice_Target01 **3/3 clean PASS**（新资源oracle/隔离/透明准备/首离开/重建/GC，既有PlayStopResourceLifetime和ErasedDoesNotRebuild）。ResourceSlice_Contracts01 **178图、三次PIE、24帧Whole交接oracle PASS**，Partial外切口及首次Whole原图已查看，exit0/severe0/teardown完成。测试比较证据/texture signature/尺寸/cap signature/三角数，检查所有mesh已注册且绑定正确，重建Ensure幂等、旧MID移出强引用、销毁后弱引用经GC失效。短样本UObject计数平稳，不能据此升级长期资源结论。

**INITIALIZATION仍FAIL（新最大整帧516–520ms）；完整帧FAIL/长期资源PARTIAL沿用未升级。** 下一最高收益方向是整批首次历史seal/证据/表现准备与GT发布的同步架构；建议下一轮先明确跨帧调度+epoch/revision/lifetime校验+GT原子发布合同，须另获授权，本轮未实施。现有新Trace seal198.559ms、资源47.504ms，另有历史更新；单个texture创建约7ms、纹理内容与提交43.463ms，继续局部优化的上限不足以消除约半秒峰值。若坚持同帧边界，可另评估texture像素/签名/Float16纯CPU准备，但不优先堆小型NewObject/LoadObject缓存。

证据：Saved/Stabilization/ResourceSlice_{Legacy01,Scoped01,Scoped02,Legacy02,TraceBefore01,TraceScoped01}，Saved/ResourceSlice/FinalABBA.json及resource_inventory.json；复算工具Scripts/CompareGrayResourceSlice.py。完整命令/hash/限制见性能审计第21节。未重跑完整矩阵、十分钟长测、occupancy全集、Episodes/无关视觉全集或打包Shipping；这些不是本版通过证据。Git LFS fsck OK，两个stable SHA仍为404a582/7534163。原始Saved证据仅在本机，代码/验证工具/本技术交接随Git交付。

## Occupancy 生产切片完成（2026-09-07，32f6abe之后）

最终运行时 **f12c6c1**（首版4ccda80）均已推送。BuildGeometryDirtyIndices维持原失效/候选/精度规则，fine/coarse共用只读快照批量求值：大批最多8任务同帧join，GT按原序写bit/cache/统计及发布修订；小批/live Actor回退串行。空Partial ROI沿原谓词直接false的语义批量回填，避免无效任务开销；Whole空center ROI仍查完整footprint，薄边合同不变。worker不写共享TMap/TBitArray，不改资源准备/首次显示/灰层规则，无跨帧队列。

最终 `Scripts/BuildEditor.ps1` Build04成功13.66秒；Target04 **4/4 clean PASS**，包括原完整证据/物理几何/精确slab oracle及新增occupancy（24次比较、19批joined，含大coarse/稀疏回填、缓存容量/复用/失效、Whole薄边/空ROI、微小位移、碰撞几何移除及live回退）。最终Contracts178图/三次PIE/24帧Whole oracle、Episodes51图/六组896内部样本全PASS，exit0/severe0，原图已查看。没有宣称本版完整149项通过；旧148项不重复跑。

最终四组 `Saved/Stabilization/OccupancyFinal_Serial01 / Joined01 / Joined02 / Serial02` 同DLL/driver/配置，正常前台1080p/SP100、原质量D3D12/SM6，双方-NoAuthoringToolsets，NoTrace/无固定步长/无截图；每次480帧环境异常0、complete/exit0/severe0。两次中位：184 occupancy **64.916→38.129ms（-41.3%）**、native296.635→265.191ms、最大整帧 **587.833→543.815ms（-7.5%）**。setup282.208→270.824ms变化不全归因于本切片。setup184条，采样首末120条/fine bytes41,157,632；occupancy查询2,095,981、geometry tests8,018,953、samples_scanned1,972,688一致，冷帧完整保留。64 occupancy8.047→6.001ms，但整帧200.492→201.994ms，没有宣称64整帧改善。

首版三对CPU有改善而整帧未改善，促成空ROI修正；Target02测试fixture缺少BeginAbsent断言失败和Serial01前台/引擎Toolsets启动错误的无效样本均保留。最终只用最终版本两对作结论，不跨版本/机器拼接旧约80ms与新值。首次普通-game的Toolsets Python API错误通过双方使用既有-NoAuthoringToolsets隔离，没有改项目画质或全局插件配置。完整表格/hash/命令与限制见性能审计第20节；可复算JSON在Saved/OccupancySlice/comparison-OccupancyFinal_Serial01.json。

**该生产切片已完成且收益成立，初始化仍FAIL（最终535–552ms峰值）/完整帧FAIL/长期资源PARTIAL。** 本轮不继续叠加资源创建修改；下一入口是seal外首次proxy/texture/resource的GT创建、注册及提交，保持透明预备/Whole首次离开原子交接；无需再做本轮已完成的occupancy侦察、完整矩阵或十分钟长测。最终必要定向和视觉验证已完成。跨帧与Large World继续独立留待后续，不开始黑色层、stable不移动。源码/测试/runner与本文均随Git交付，Saved原始证据仅在本机。

## 下一阶段定向侦察（2026-09-07，起点 a41928beec12f56fa6d68887b1669ea8bf7915e7）

本轮仅阅读 AGENTS.md、最新性能审计第18–19节、交接和相关源码；不改运行时、不构建、不运行测试、不引入跨帧任务。下列行号对应起点源码。最新审计引用的 Saved/Stabilization/CapSlice_Joined02 与 CapSlice_TraceJoined01 在本机不存在，因此计时沿用已提交证据，不声称重新核验原始 Trace。此前148/148和必要视觉结果不重复执行。

### Occupancy 的实际边界、规模与待确认项

入口在 `Source/Darkwell/Private/VisionPresentation/DarkwellObjectMemoryScene.cpp`：`UpdateMemory`（5511）在GT采集 FrameOccupancy（5524–5545，独立 OccupancySnapshotUs），然后 `UpdateTracked`（2256）按历史 epoch 原序处理；2701按 record bounds筛候选；2734–2756的 **OccupancyUs** 包括 `BuildGeometryDirtyIndices`（1419）以及 coarse dirty映射/占据回填。它不包括前面的候选/coverage准备，也不等于某个几何函数自身耗时。最新无Trace occupancy中位79.806ms是此复合区间；尚无足够证据给内部函数排毫秒名次。

- `BuildGeometryDirtyIndices`：收集/复制当前物理与较新历史几何；查 FrameHistoryGeometry复用；比较修订和前后几何，构造 Dirty/PhysicalDirty位图；按索引计算或复用 fine occupancy，并生成两份dirty索引。首次 ProcessedGeometryRevision=0 或尺寸不符时全格标脏。即使候选为空，仍支付位图、循环、索引输出及缓存写入成本；复用命中也复制 Occupied和两份索引数组。
- 精细查询链：`IsOccupiedByActual`（1282）→ exact XY点缓存/候选bounds → `QueryVerticalInterval`（338，缓存投影早退及精确local slab）；Whole仅在中心未占据且 LastLegalCaptureMask允许时走 `IsOccupiedWithinWholeFootprint`（1330）→ AABB/中心包含/四边 `ClipSegmentToGeometryProjection`（421）。Whole footprint现在遍历完整FrameOccupancy，不能不加证明地复用中心点候选缩小它。
- coarse部分将PhysicalDirty映射到coarse位图，再对coarse中心调用同一占据函数。ownership-only dirty仍必须进入后续合法证据处理，但不得重新推翻物理占据缓存。
- 规模：压力是三个身份64+64+56=184条姿态，yaw每次17度、X每次7cm，绝非184个当前物理对象；seed后合法反证的采样记录为120，fine分配41,157,632 bytes（既有审计）。fixture CellSize=2.5cm，Fine每轴4倍，即每coarse cell 16样本、名义约0.625cm细度；每条实际尺寸随旋转后bounds变化。工作量应记录 sum(S_i)、physical-dirty数D_i、coarse-dirty数C_i与primitive候选P_i，不能用分配字节反推准确样本数，也不能挪用旧ownership的1,941,840样本/8,030,933 geometry tests当本阶段计数。
- `DarkwellMovingPropLabRoom.cpp:769–852`播种结束将该身份碰撞关闭、bExists=false；FrameOccupancy只纳入当前存在且有碰撞的Actor。空候选时中心查询立即false，Whole仍有自身检查。故80ms可能有显著的CPU数组/扫描成本；本轮不能证明narrow-phase为主耗时。通用成本含O(sum S_i)扫描/初始化、O(sum(D_i+C_i)*P_i)几何查询，加较新历史几何收集/比较/复制（历史规模增长时可有二次工作），并非单一O(H)算法。点缓存上限131072、geometry复用条目上限64，满后保留完整路径。

### 可施工边界与正确性约束

适合继续“只读输入→并行CPU→GT合并”，但**不适合直接并行整段BuildGeometryDirtyIndices或跨record的UpdateTracked**。推荐先按单record分块、同帧join，维持record证据/ownership/退役的原顺序：GT完成revision判定、复用命中、候选与输入固定，任务只求fine/coarse中心和Whole footprint结果，GT按原索引写缓存/dirty列表/统计，再推进原合法证据与ownership。进一步并行dirty计算或减少复制应由分段结果决定，不能先假设纯查询占满80ms。

输入保留精确bounds/size、已投影几何、Whole mask、旧occupancy及dirty/revision上下文；同帧借用期间禁止容器变动。任务用独占字节输出或独占完整bit word，GT压回TBitArray；不同bit也可能共用机器字，不能并发写同一TBitArray。FrameOccupancyPoints是共享TMap，FrameHistoryGeometry与Visual数组/修订也是共享可变状态；需GT管理cache命中/发布，或设计有界局部cache后实测重复查询损失，不能给worker直接调用当前共享查询。`QueryVerticalInterval`已有thread_local GOwnershipQueryCounts路线，可复用局部计数思想；IsOccupiedByActual自身的OccupancyTests/CacheHits仍须拆出。没有FrameOccupancy快照的live Actor/UObject回退继续GT。

主要风险：精确浮点点key及边界容差改变导致薄边误擦；Whole中心空但footprint有物理占据；ownership-only变化、亚容差物理位移、collision/销毁导致的缓存失效；任务并发bit/TMap写与悬空候选；提前发布修订、改变record处理顺序导致旧历史复活或提前VerifiedEmpty。不得混淆占据事实与合法coverage/ownership，不能让物理位置本身授予玩家知识。

### Seal外首次 proxy / texture / resource 创建

可确认的调用点（同一Scene.cpp，另注明Lab文件）：

| 入口/阶段 | 主要创建或提交 | 结论 |
| --- | --- | --- |
| Lab `ConfigureHistoricalEpochCountForTesting`，MovingPropLabRoom.cpp:837 | EnsureRecordVisual先执行，下一行才FreezeCurrentForHiddenMotion | seed的首次创建位于seal外；不是seal内Ensure的368次约2.357ms所覆盖的总成本 |
| 生产 `UpdateTracked:2626` Current | EnsureRecordVisual；2631附近UpdateCurrentPartTextures | 合格观察时预备透明历史代理，并更新当前表现 |
| `EnsureRecordVisual:3153` | 3194 CreateTransient(PF_FloatRGBA)→Bulk锁定清零→UpdateResource；3233附近NewObject DynamicMesh cap→RegisterComponent/LoadObject材质 | 纹理按coarse尺寸×4的每轴分辨率；CPU分配/清零与GT对象及后续渲染创建须分开量 |
| `SpawnMemoryProxy:3461`→`BindProxyMaterial:3514` | SpawnActor、Root注册、逐primitive LoadSynchronous/NewObject/SetStaticMesh；LoadObject父材质、每mesh创建MID、绑定后RegisterComponent | 后者才注册mesh；已有“先材质后注册”优化，不重复施工。实际render proxy/RHI成本可晚于GT调用，本轮未取得其线程耗时 |
| `UpdateCurrentPartTextures:3302` | 3327 CreateTransient/UpdateResource；后续CPU像素/签名/FFloat16Color staging→UpdateTextureRegions | 与历史预备纹理是两条路径；Whole有1×1与spare复用，不能把全部上传算首次创建 |
| 历史 `UpdateTracked:2706`，seal内部3029/3077 | EnsureRecordVisual缓存命中、必要尺寸重建/最终姿态及SpatialReady绑定 | 完整资源成本必须同时统计seal外/内，不能只计ensure总次数 |

资源整段不能直接搬到worker：UObject创建、加载、Actor/component注册、MID参数、OwnedTextures/Materials/Caps和显示发布保持GT；纯像素/几何准备才是CPU任务候选。必须保留Current预备代理SpatialReady=0、最终合法捕获姿态、源隐藏/历史接管顺序与GC可见owner；不能通过推迟预备来重新引入首次Whole离开的空白帧。不可把UpdateResource返回等同GPU就绪，也不新增Flush或降低采样换成绩。

### 下一轮顺序、收益预算与最小验收

1. **P0：先给occupancy现有复合区间做最小分段**（dirty准备/复用复制、fine查询含Whole、coarse、合并），同时记录上述S/D/C/P和空候选/cache命中。仅需下一轮施工时一次短归因，不重开旧审计。若查询/独立扫描可并行部分占80ms的50%–80%，且这部分获得3倍加速，理论净节省约27–43ms，再扣调度/暂存/GT合并；这是条件预算，不是实测收益或承诺。若复制/串行准备占主导，则先减少精确等价的重复收集/复制，避免做低收益任务化。80ms只是整个当前occupancy区间的绝对消除上界，不能预期因此将约771ms整帧压到100ms。
2. **P1：资源首次创建分段归因后再选切片**。按历史/Current、首次/复用/resize、seal内外记录次数、texels/bytes、mesh/MID数，拆texture分配清零、proxy构造、MID/注册与渲染线程资源事件。旧144.095ms Ensure累计跨setup/update且来自旧版，不能当本版可节省预算；当前无法负责任给资源收益毫秒数。缓存父材质或减少重复资源提交需证明确有成本，优先保持现有预备/原子交接。
3. 下一轮实际修改后才运行要求的完整Editor构建及必要定向比较：串行/并行逐位fine/coarse occupancy和dirty列表、修订/计数、Whole薄边、旋转/倾斜回退、空候选、ownership-only/微小位移、销毁碰撞变化与缓存容量回退；保留真实短D3D12 A/B的setup/首update/完整最大帧，资源改动补首次Whole离开及cap视觉。已有阶段148项/视觉/长测不因本轮侦察重新跑；实现后的回归按实际影响安排。

没有新性能PASS。初始化/完整帧FAIL、长期资源PARTIAL保持。跨帧epoch/revision/取消/原子发布及Large World独立审计继续留待后续，本轮没有半成品任务队列。仅本文补充定向结论，提交前检查文档diff；未修改生产代码或资产。

## 阶段性收尾（51eb837 之后，仅文档）

Ownership（6c66747）、capture（59030ab）、cap（9c14ecc）三个生产切片均已完成，运行时保持9c14ecc。各阶段独立证据：ownership约566→108ms；capture footprint约159→56ms、setup约539→452ms；cap CPU约86→29ms，本轮cap最大真实帧两次中位约836→771ms。跨批次数据不直接累加。

已只读复核既有Saved结果：148/148功能PASS，Episodes51张/表面oracle与Contracts178张/24帧Whole oracle PASS。**初始化仍FAIL**，FRAME PERFORMANCE仍FAIL，LONG-RUN RESOURCES仍PARTIAL；本切片必要功能/视觉回归已完成。本次没有运行时修改、构建或测试重跑。

后续顺序：**occupancy → seal外首次proxy/texture/resource创建 → 若仍有数百毫秒卡顿，再进入跨帧调度/原子发布架构**。跨帧须另建完整epoch/revision、取消/失效及发布协议。**Large World / 全地图灰色记忆的分块、流式表现资源、增量空间索引**另列独立scalability audit，本次不实现。完整阶段表与依据见架构审计第19节。

本次起点local/upstream/remote一致于51eb837，工作树干净、LFS检查正常、无遗留测试/构建/Trace进程；仅提交推送本次两份文档，最终SHA以最新Git提交为准。stable保持原SHA，Saved证据保留，不开始黑色层、不自动关机。

## 已完成阶段证据（保留历史记录）

当前19f2e76之后的cap运行时切片 **9c14ecc** 已推送并完成必要最终回归：只读CPU行任务→按原序join/网格合并→GT验证与SetMesh发布；live Current依赖保留串行，无跨帧遗留任务。完整构建CapSlice_Build04通过，定向31次比较含20非空cap、35joined、8 live Current回退；最终 **148/148功能PASS**（旧147项无遗漏），Episodes51张/六组896样本surface oracle及Contracts178张/Whole离开24帧oracle/三次PIE均PASS、正常退出。

两次真实D3D12 A/B中位数：184 setup **449.593→397.038ms**（-11.7%），最大整帧 **836.191→771.154ms**（-7.8%）；并行两次实际749.592/792.716ms，初始化仍FAIL。同二进制/driver/原质量、四次各480帧环境异常0、完整冷帧保留，setup184记录、采样120记录、fine bytes41,157,632均一致。短Trace确认封存cap CPU **85.771→29.480ms**、cap总计136.269→83.715ms；GT提交仍约10ms，未消除。详细成本、原始路径、控制开关-SerialCapBuild及失败fixture修正见架构审计第18节。

本阶段最终：**ARCHITECTURE AUDIT PARTIAL / FRAME PERFORMANCE FAIL / INITIALIZATION-BATCH HITCHES FAIL / LONG-RUN RESOURCES PARTIAL / FUNCTIONAL REGRESSION PASS**；已验证退出路径维持PASS。下一轮优先约80ms occupancy的只读输入/分块计算，然后seal外首次proxy/texture/resource创建；cap细胞准备与签名仍同步。capture footprint未再改，无减少采样/精度/历史或灰色规则变更。未重跑完整矩阵、Qualification/WholeSessions全集或十分钟长测；本切片必要最终功能/视觉已完成，不再作为待办重复跑。计时5c9e495、运行时9c14ecc、性能证据9a838fb均已推送，最终文档提交在其后；所有Saved证据保留，stable不移动。以下是此前已完成阶段。

最新初始化阶段（8fb4d6e之后）已完成，运行时 **59030ab** 已推送：capture几何只读输入→并行准备→GT合并，并移除封存内随即被最终cap覆盖的中间提交。三次真实D3D12 Batch中位数：184 setup **538.769→451.701ms**（-16.2%），最大整帧 **926.468→848.486ms**（-8.4%）；新三次842–853ms，初始化仍FAIL。改造前后均完成必要Episodes51张/表面oracle、Contracts178张/Whole离开24帧oracle/3次PIE；最终完整构建、17/17定向和 **147/147** 阶段功能通过，旧146项无遗漏。

控制量r.Darkwell.ObjectMemory.StagedCapturePreparation；runner的-LegacyCapturePreparation保留旧串行准备与中间cap、ownership仍开启。两个短Trace按真实GT嵌套事件确认184 footprint **159.301→56.381ms**；cap调用368→184但耗时131.458→146.713ms，未解决cap瓶颈。下一入口为最终cap纯CPU网格结果与GT SetMesh/资源提交分离，其次约83ms occupancy及seal外proxy/texture创建。六个真实Batch、两个Trace与完整功能/视觉证据均保留Saved，运行时冻结后未再修改代码；未跑完整矩阵或十分钟长测。详细三次数据、范围和剩余工作见架构审计17.1–17.2。最终状态FUNCTIONAL PASS / INITIALIZATION FAIL / FRAME FAIL / ARCHITECTURE PARTIAL / LONG-RUN PARTIAL；进程均正常结束、stable未移动。下文“最新”均为更早阶段历史记录。

最新继续施工（cf3a3c1之后）：已完成大批封存历史ownership的同帧并行求值/GT合并切片，最终运行时 **6c66747** 已推送。四项定向及一次完整146/146功能回归通过；最终仅任务计数局部修订的完整Editor build成功20.92秒，另有3/3定向通过。原生184 CPU对照ownership **569–584→115–116ms**、首次native update **838–851→391–414ms**，几何/record访问计数完全相同。用户随后退出SpaceCraft，已补一次真实D3D12 Batch A/B：184最大整帧 **1363.970→921.871ms**、setup **549.266→536.297ms**、ownership **566.111→107.680ms**、cap **67.231→74.602ms**；同二进制/driver/质量、两次环境异常0、正常退出。初始化仍FAIL，并行同帧join仍有近一秒停顿。详细证据及范围见 [架构审计第16节](SIGHTWEAVE_PERFORMANCE_ARCHITECTURE_AUDIT_ZH.md#16-有限额度施工封存ownership的同帧并行切片)，新增真实样本见16.3。

本轮待办：相关最终历史/cap视觉；最小真实A/B已完成，结果在Saved/Stabilization/BatchSlice_RenderSerial01与BatchSlice_RenderJoined01，不再以“GPU被SpaceCraft占用”作为未采集理由。此补证未修改代码、未重跑功能/全矩阵/长测；完整146/146在a8bb332通过，6c66747仅计数栈化与测试fixture修正后按影响范围验证。Current、capture/cap/resources仍同步，跨帧任务/失效队列尚未引入；因此没有悬空任务需要恢复。下一结构入口是约半秒setup的封存capture工作单元，以及occupancy/cap计算与GT资源提交边界。Python流式采样器、旧6.4GB账本和Empty GPU成本留待后续。最后检查无残留UE/SpaceCraft/测试进程，Saved证据保留，stable未移动。下文“最新续工完成”指上一轮。

最新续工完成：从 `d986b53` 开始的两阶段局部重构，运行时冻结并推送于 `b03bcfb`：ownership空间候选与旧表面提前排除、cap依赖摘要、地面记忆饱和行/扫描线交点复用。最终145/145原生功能、四套视觉及全部独立oracle PASS。三次无Trace Batch的184初始化峰值1.288–1.355秒；FrameAudit的Empty p95中位18.599ms、Partial21.940ms；SourceUpdate独立Trace均值5.50→2.59ms。最终真实610秒长测42,663帧、p95=15.991ms，但5帧>100ms，性能仍FAIL。工作集3.363→3.755GB，释放采样数据+GC+60真实帧后3.434GB，历史/纹理资源边界稳定。短GC规模对照证实采样器保留4.27万条字典可产生约107ms完整Python GC，原慢帧不删除。分配探针区分了用户buffer、D3D12页池与Mimalloc预留/decommit；旧同步6.4GB仍未全部闭环。已有正式全矩阵和54,000步同步长测未重跑。完整证据、限制和后续架构方向见 [性能架构审计](SIGHTWEAVE_PERFORMANCE_ARCHITECTURE_AUDIT_ZH.md) 第5–15节，以及新增23行 [阶段明细](SIGHTWEAVE_PERFORMANCE_ARCHITECTURE_METRICS.csv)。

本次性能架构阶段最终分项：**ARCHITECTURE AUDIT PARTIAL / FRAME PERFORMANCE FAIL / INITIALIZATION-BATCH HITCHES FAIL / LONG-RUN RESOURCES PARTIAL / FUNCTIONAL REGRESSION PASS**。EXIT STABILITY在已验证范围内维持PASS。下一步优先运行时ownership/cap工作单元与原子发布，不能把延迟压力脚本当作初始化优化；随后改进完整帧成本和流式采样，保留全部证据。下文“最终”及其普通Editor打开现场属于上一轮稳定化基线，本轮新增进程均已正常结束，未重新打开普通Editor。

2026-09-06，最终状态：**PARTIAL — GRAY_STABILIZATION_BLOCKED**。功能与当前可复现退出路径的回归通过；完整帧、批量尖峰未达标，长期资源归因仍有缺口。不得据此创建发布 stable 或开始黑色层。

## 起点和人工验收边界

- 实际起点：`a6e4324839acb9b3be7e7c5ea14c0bb4d994735b`，本地/上游/实时远端一致，工作树及暂存区为空，stash 列表为空，`git diff --check` 和 `git lfs fsck` 通过。
- 用户在本任务明确确认该交接状态的 Whole 达标交接退灰测试通过，并表示当前人工路线灰色层行为可接受。最终运行时是 `a4a17a412a35d2e9a0c7dd8917965d6e8c9323f5`；a6e4324 为文档交接，不能说它又修改了运行时。
- 验收覆盖用户的 Whole 局部→整件达标连续显示人工路线及其目前使用过的灰色层交互；不是所有几何的穷尽证明。上一轮自动化 142/142、连续帧 oracle 8632/8632 是既有证据，不能作为本轮重跑结果。
- 两条远端 stable 固定为 `stable/sightweave-gray-core-20260903` → `7534163b9c5718700b610e7677f47fbaa79cf977`、`stable/moving-history-grid-v2-20260902` → `404a5820739638f1097eaae0aa7fba19733298c3`，本轮不移动。
- 非发布 tag `checkpoint/gray-functional-accepted-20260906` 已推送，解析到 a6e4324；注释为“灰色层功能人工验收检查点；性能和退出稳定性仍开放；不是发布 Stable。”
- 实际引擎 `D:\UE_5.8`：Build.version 为 **5.8.2 CL56702186**。AGENTS 的 5.8.1 是环境说明差异，实验按实际二进制记录。

## 冻结的外部规则

1. Whole 每次连续合法观察都用对象自己的 MinimumObservedSpanCm（默认约 100 cm）重新确认；旧历史/缓存不授予资格，不跨轮累计。真正失联才结束；无效 Coverage、revision mismatch、未就绪不等于失联。达标后仍有合法接触就保持整件。
2. 局部→Whole 达标交接必须承接已显露表面：首次与再确认均不得退灰、消失、重新点阵、空白帧或整体闪烁。
3. 合格观察先完成正确历史交接再结束资格；首次离开不空白、后续不回退残缺历史。未达标不能生成完整 Whole 历史、覆盖或删除旧合法知识。
4. 视锥/距离/墙体控制合法接触和跨度；Whole 达标后的统一展示不再按玩家到墙边界逐块裁切，相机深度遮挡保留；不得穿墙发现、扩张地面探索或确认其它/完全不可见对象。
5. SpatialPartial 看到多少显示多少，允许的历史累计局部；补全无内部旧接缝，未观察区域不出现，合法外切口和深灰 cap 保留。
6. StationaryOnly 移动中 Live 但不生成运动历史；隐藏停止不自动记终点，必须重新合法观察。Never 不留灰影；Always 保持兼容语义。
7. StableID、真实 Transform、内部销毁、隐藏换模型/颜色不等于合法反证。Superseded 不伪装 VerifiedEmpty；合法擦除不复活，重建不读取未见隐藏状态。
8. 相同未反证状态允许复用捕获/proxy/texture/geometry snapshot，不无意义逐轮增长；真实新增未解决知识允许增长，不为常量内存丢弃。

## 施工与验收协议

先建立退出最小复现和受控性能入口。所有样本独立命名并保留失败；图像、功能、退出分开判定。关键同条件前后实验串行重复至少三次，记录实际游戏视口、质量、运行方式、前后台和硬件。冷启动及初始化帧单独保留，不藏进预热。

完整帧目标 p95 ≤16.6 ms、p99 ≤33 ms；普通交互不得可重复 >100 ms 或持续慢帧。Editor PIE 与 Standalone 各自报告；模拟 54,000+600 与至少十分钟真实 D3D12 wall-clock 各自报告。若空场景失败先归因基础成本。

禁止强杀/立即 ExitProcess/忽略退出码伪装稳定，禁止修改系统配置、降低画质、改变规则或牺牲合法知识。所有有效阶段明确暂存、commit、push 后核对 local/upstream/remote。生成证据保留 Saved，不提交资产或生成目录。

## 上一轮稳定化基线分项结论

| 维度 | 状态 | 本轮证据 |
| --- | --- | --- |
| FUNCTIONAL REGRESSION | PASS | 最终原生 143/143，四个最终视觉协议及全部对应 oracle 通过；见阶段 7–8 |
| EXIT STABILITY | PASS | 旧失败路径、多进程 Contracts/Episodes，以及普通 Editor 一次/两次 PIE 后正常关闭均通过；中断样本 EXIT UNKNOWN 保留 |
| FRAME PERFORMANCE | FAIL | 正式前后各 3 PIE + 3 Standalone；空场景仍超 16.6 ms，部分案例退化 |
| INITIALIZATION / BATCH HITCHES | FAIL | 184 distributed setup 约减半，整帧仍存在约 2.5–2.6 秒尖峰 |
| LONG-RUN RESOURCES | PARTIAL | 两种完整长测均已完成；真实资源有界，模拟工作集 4.77→11.20 GB 的增长归因仍开放 |

启动时的功能检查点仅新增文档；后续 C++、构建、测试与证据见分阶段记录。SightWeave 插件和二进制资产未改动，本轮未运行 BuildPlugin。


## 阶段 1：执行器完成与退出所有权

原 `RunGrayMemoryAudit` 图形驱动把 `set_keep_python_script_alive(True)` 保留到退出，却直接发送 `CLOSE_SLATE_MAINFRAME`。实际引擎源码 `EditorPythonExecuter.cpp` 显示：正常 keep-alive 结束会先 DestroyNotification 再排入 QUIT_EDITOR；提前请求引擎退出会让执行器 Tick 跳过该分支，Notification 到 Python 插件 OnShutdownModule 才 SetComplete。`LaunchEngineLoop.cpp` 的顺序是初始 Core ticker Reset → GEngine PreExit → Slate Shutdown → AppPreExit / 模块 Shutdown。Slate async notification 的 UpdateNotification 会把强持有 OwningNotification 的委托排回 ticker。

这解释了与旧 dump 相符的迟到通知→文本析构链。进一步反汇编核对 Core 的队列/函数对象析构与 Slate 捕获参数的析构位置，不能把旧 nearest-export 名称（例如 HazardPointer）当精确函数名。**目前还没有新的失败 dump 把原通知标题或生产者直接读出，不能扩大为每个 0xC0000005 都已证明同根因。** 新造的晚通知实验未稳定触发 AV，其正常结果也保留。

修复已有 audit/gray performance 驱动：原回调注销和 PIE 停止流程完成后，设置 keep-alive=False，把完成/关闭交还执行器；不强杀、不延迟很多秒、不忽略退出码、不清空全局 ticker，不改玩法。受控 `-LegacyDirectClose` 能在同一版本/同一路线上恢复旧关闭方式作为负对照，默认关闭。

| 本轮样本 | 调试器 | 真实退出码 | 内容 |
| --- | --- | --- | --- |
| ExitBefore_Main01 | 无 | 0 | 无 Python，真实主窗口 Alt+F4，未进 PIE |
| ExitBefore_Python0/1/2_01 | 无 | 各 0 | 旧直接关闭；0/1/2 次 PIE 最小路径 |
| Stabilization_ExitBefore_Contracts01 | 无 | **0xC0000005** | 178 帧、3 PIE 完成、severe 0，退出失败 |
| Stabilization_ExitBefore_ContractsDebug01 | 退出前附加 | 0 | 同旧 Contracts；不能覆盖非调试器失败 |
| Stabilization_ExitAfter_Contracts_013849 / _014027 | 无 | 各 0 | 修复后原 Contracts，59.919 / 58.935 秒 |
| Stabilization_ExitAfter_Episodes_013950 / _014127 | 无 | 各 0 | 修复后原 Episodes，35.915 / 43.173 秒 |

运行目录均在 Saved/ArchitectureAudit；最小路径在 Saved/Stabilization。新造通知探针 ExitNotification_Before01、Before02、LateDebug01 均退出 0（最后一项调试器从启动附加），不算已重现根因的证明。退出验收继续开放，需追加最终正常主窗口、多 PIE 和失败路线覆盖。

阶段 1 只提交驱动所有权修复与此处进度，场景/资源算法不变。性能元数据与 Editor 专用诊断入口仍在下一阶段施工；已有烟测包含脚本变量错误、Standalone 内置 ToolsetRegistry PythonTestRunner 初始化错误和后台 PIE，均明确不是正式基线。单独禁用 ToolsetRegistry 会被依赖重新启用，不能声称该开关已生效。当前机器 CPU Ryzen 9 3900X、GPU RTX 2070 SUPER、驱动 32.0.16.1088、RAM 34,305,445,888 字节。用户已退出 SpaceCraft，后续正式实验串行独占 UE/构建负载。

## 阶段 2：中断恢复与可构建测量入口

用户实体 Esc 中断后恢复，起点仍为 `2344b1a4829e4ca4ac60250cedc4df453ee34ea2`，fetch 后 local/upstream/remote 一致。未提交文件均来自本轮，未使用 reset/restore/clean/stash。没有残留 UE、Python、Trace、UBT 或测试进程；用户自己的普通 PowerShell 保留。LFS fsck 通过。

- `Scripts/TestManifests/GrayFunctional.json` 固定原 142 项完整名称与 selector；`RunGrayFunctionalRegression.ps1` 验证新报告不得遗漏原名。`Stabilization_HarnessFunctional` 实际 142 项（131 clean、11 warnings）、0 failed、0 not-run、severe 0、exit 0，194.741 秒；coverage 文件确认 missing/new 均空。
- `RunGrayExitProbe.ps1` / `audit_gray_exit.py` 保存最小 0/N PIE 生命周期、真实退出码及源码状态；MainWindow 模式用于外部真实主窗口操作。
- `RunGrayPerformanceBaseline.ps1` / `profile_gray_stabilization.py` 保存逐帧 JSONL、冷启动、case setup、资源与质量/硬件/进程元数据，支持 PIE、Standalone、Smoke/Matrix/LongRun 和独立 Trace。尚未完成正式矩阵/长测验收。
- 新 `DarkwellEditor` 模块只承载临时 PIE 浮动窗口、实时视口 override 恢复和退出顺序诊断；uproject/Editor target 增加该 Editor 模块，SightWeave 插件无改动。人工晚通知 probe 仅显式参数启用，已有负结果保留，不作为退出修复。
- Runtime Lab 新只读实际视口/窗口、CVar 和线程/GPU计数接口。计数异步且不可相加；特别是 PIE 的全局 Render/RHI 读数可能被后续 Slate 窗口更新覆盖，不能当该游戏帧精确归因。下一阶段用独立 Insights 样本核对。
- Smoke 实际 viewport 1920×1080、SP100、TSR、sg 全 3、硬件光追和 VSM 开启。Standalone 空场景 p95 约 28–30 ms；浮动 PIE 的 OS foreground 仍为 0（即使 Slate window_active 为 1），因此不是正式前台 baseline。所有异常烟测保留，不能选最后一次代替对照。
- `-NoAuthoringToolsets` 是显式诊断环境，逐次记录全部禁用项；解决 UE 内置 ToolsetRegistry 在 Standalone `-game -EnablePython` 中引用缺失 PythonTestRunner 的初始化错误。普通项目配置不改变，该环境不能冒充默认 Editor。

`Scripts/BuildEditor.ps1` 恢复检查成功（Saved/Logs/Stabilization_ResumeCheckpointBuild.log，1.32 秒 up-to-date；此前对应 C++ 完整构建 6.86 秒成功）。该检查点保存现有入口，并不声称功能最终验收、退出全面稳定、性能达标或尖峰已修复。先 push 本阶段再进行长实验。

阶段 2 已推送为 `c2ee6575e3caff69f858ec1faad3a2b17785c4a6`，local/upstream/remote 核对一致。

## 阶段 3：空场景归因与正式对照前的入口修正

- `Attribution_Standalone_Matrix01`：旧 Hidden 启动无法建立 OS 前台；90 秒谓词超时，协议失败、exit 0，保存 failed.txt/trace。改为用户要求的可见前台测试窗口，并通过 Computer Use 激活实际返回的窗口；Slate active 不能代替 OS foreground。前台等待帧另存 startup，正式帧逐帧核对 viewport/画质/前台，分析器不删除离群帧。
- `Attribution_Standalone_Matrix02`：前台有效，完成 Empty 到 LongRepeatDistributed，但 ActualNewKnowledge 使用编辑器 snake-case 属性别名，在 -game 无法读取，协议失败、exit 0。前十二案例和完整 trace 保留，仅用于诊断，不算完整正常矩阵。改用真实 native `StableId` 名称；`Harness_Knowledge01` 独立非 Trace 实际新增六条未解决记录，360 帧断言通过，exit 0、severe 0；p95 30.202、p99 44.232、max 98.346 ms。
- Insights CPU/GPU/region 独立采样：首份忘启用 region channel，因此只导出全程统计；后续明确开启 region。`ExportGrayTrace.ps1` 可无 UI 导出区域与总计。全程 trace 的 GPU DrawMaterialToRenderTarget 平均 9.330 ms，不能冒充空场景单独结果。
- `Attribution_Standalone_Controls01`：1080p、SP100、TSR、sg 全 3、VSync/FPS limit/fixed/smooth 均关闭、实际前台；六段各 300 帧，exit 0、severe 0。Empty / NoGuidance / NoWorldLabels / NoUi / NoCoverageDraw / Restored 的 wall p95 分别为 **31.493 / 31.342 / 29.946 / 29.742 / 19.867 / 31.777 ms**。这是一轮带 Trace 的干预归因，不是优化后的正常达标成绩。
- Empty 区域 Insights：GPU 覆盖绘制平均 **10.294 ms**，TSR **5.153 ms**，LumenScreenProbeGather **3.259 ms**；存在明确 GPU occlusion-query wait。覆盖纹理真实 **6240×6320、R16F、2.5 cm/texel**，每次转头按全图绘制。CPU memory p95 约 0.203 ms。由干预和 GPU track 共同支持优先减少确定为零的覆盖像素工作，不降低纹理密度、TSR、光照或合法采样。
- 新诊断开关 `r.Darkwell.FogVisual.Diagnostic.SkipCoverageDraw` 默认 0，只在 Attribution 的 NoCoverageDraw 段故意冻结 GPU 场、保留 CPU 语义；正常结果不使用。Lab UI 可显式隐藏/恢复；RHI texture bytes 为设备统计，不能当全部显存或泄漏证明。
- 构建：TraceScopes 18.56 秒成功；Attribution 首次编译因 TObjectPtr range auto* 推导失败，明确改为具体指针类型后完整 Editor 构建成功 7.89 秒。Python AST 与 diff --check 通过。代码目前仅增加诊断，不改变正常覆盖绘制和玩法。

正式 before/after 使用 `RunGrayPerformanceBaseline.ps1 -RunName UNIQUE -Mode PIE或Standalone -Protocol Matrix -NoAuthoringToolsets`，每次为独立进程，激活其实际窗口后自动采集。`AnalyzeGrayStabilization.py` 输出所有帧及单独 steady_after_90、setup/资源数量、>33/>100、最长慢帧串；trace、干预或条件变化不混入 normal 汇总。首次批量 setup 的即时 records/proxies/identities 与运行中资源分别记录，避免将合法反证后的压力下降当满负载通过。

## 阶段 4：六组正式 before 与第一批等价优化

Before 固定在 `194a0dbddacf9c5ea5d6b19f66773c2effb984c0`，六组过程未修改源码/二进制。全部使用上述 Matrix、NoAuthoringToolsets、实际前台 1920×1080/SP100、相同质量，无 Trace/截图/调试器；逐帧环境无异常。Saved/Stabilization 下有效样本为 Before_PIE_01/02/03、Before_Standalone_01/02/04，全部 complete、exit 0、severe 0。Standalone_03 因未在 90 秒内激活 OS 前台而协议失败（exit 0）；原始失败保留，不计有效样本。

| Empty p95 ms | 1 | 2 | 3 |
| --- | ---: | ---: | ---: |
| PIE | 33.708 | 33.083 | 33.528 |
| Standalone | 31.693 | 31.720 | 31.710 |

逐 case 的 p50/p95/p99/max、所有尖峰和资源见各目录 analysis.json。以 Standalone_04 为例：184 distributed setup 1073.318 ms、整帧最大 3527.221 ms。批量构造即时 65/185（含先前局部背景记录）不等于热态仍有相同压力：原生合法反证会让 64 案例降至 1 条，distributed 保留约 121/122 条；不得据此宣称 64 条持续活跃压力通过。

第一批实际优化：

- GPU current coverage 保持原 R16F 6240×6320、2.5 cm/texel、全 mip 和原全图 UV；每次清除当前场，再用硬件 scissor 只绘制 body/cone 半径加过渡宽度与浮点余量内的保守区域。历史数据独立，不延迟本帧结果。`Diagnostic.FullCoverageDraw=1` 保留原全图绘制作为 oracle，正常默认 0。
- 归属布尔查询在首个合法重叠证明后停止；cap 减法仍取完整区间，测试完整路径保留。捕获 footprint 先做保守平面包围盒拒绝，几何并集命中后停止；原参考路径保持可运行。
- 静态 world-label 纹理改为 Configure 时 RequestRedraw，朝向相机的组件变换仍正常更新，内容/质量不变。

完整 Editor 构建 `Stabilization_OptimizationBuild01.log` 成功，28.33 秒。定向 `Stabilization_OptimizationTarget01` 实际 2 项通过（ConservativeDrawSupport、BatchOwnershipSamplesEquivalent）；第三个指定的 Incremental selector 前缀写错，未运行，不冒充通过，最终完整 manifest 将覆盖正确名称。

独立真实 D3D12 `Stabilization_CoverageGPU01`：1 项通过，5 种 source/墙体/边缘情形，逐个读取全部 13 层 mip，与原全图路径完整像素 CRC 一致；先污染旧场再绘制同时验证清除。35.982 秒，exit 0、severe 0。这是 GPU 正确性 oracle，不是性能或正常 Editor 关闭证据。新增 `RunGrayObjectPolicyTests -Rendering` 显式启用真实 RHI，GPU oracle 在 NullRHI 下报错。

`Optimization_SmokeStandalone01` 前台条件有效、complete、exit 0、severe 0；Empty p95 20.198、p99 21.194、max 56.501 ms，OneWhole p95 21.128 ms。说明改动有收益，但这是烟测，**仍未满足完整帧 16.6 ms 目标**，也不是正式三组 after 或最终回归通过。

## 阶段 5：剩余成本、退化几何保护与最终原生回归

`1e89e63ca98c80843e2a47a548d87f2b96a0ed62` 已推送并核对 local/upstream/remote；首次 push 网络 TLS 失败，重试成功，没有 force push。

`Optimization_AttributionMatrix01` 为该提交的独立 Trace，138.135 秒、完整协议、exit 0、severe 0。空场景 GPU DrawCanvasToTarget 平均约 0.240 ms（原全图约 10.294 ms），TSR 5.122 ms、LumenScreenProbeGather 3.301 ms；SourceUpdate CPU 约 5.166 ms，不能把它全算作灰色对象更新。184 distributed setup 553.849 ms、最大整帧 2515.789 ms；首个 native memory update 1949.427 ms，其中 ownership 1493.731 ms、cap 253.628 ms。收益真实，批量尖峰仍失败。

进一步在平面竖向区间查询中，用包含原容差的 world AABB 先拒绝无交集点。零 XY 缩放保留原 slab 行为（逆变换可能接受折叠 AABB 外点），捕获 footprint 的 bounds 捷径也排除这种退化情形。原 PlanarProjectionMatchesOriginalSlab 增加独立 world offset，实际覆盖 **65,610** 次点/容差查询以及平面、倾斜、负缩放和零缩放。

构建均为完整 Editor：ReferenceBuild01 8.42 秒、ProjectionBuild01 20.96 秒、SingularGuardBuild01 22.03 秒成功。`Stabilization_FinalFunctional01` 143/143、208.768 秒通过，但之后补充了零缩放保护；最终以 **Stabilization_FinalFunctional02** 为准：**143 项（132 clean、11 warnings）、0 failed、0 not-run、severe 0、exit 0、212.211 秒**，manifest 原 142 项无遗漏，新增 ConservativeDrawSupport。包括 OrdinaryHost、Whole/Partial/cap/合法反证和完整参考路径；它不是画面或普通退出证据。

普通地图对照单列为 Reference 协议，Matrix 的地图/案例不变。只读环境查询允许在 CDO 上读取实际 game viewport；原生 ExecuteMenuAction 暴露给 UI/Python，逻辑不变。Reference 对已加载世界调用 ResumeGame，不读写存档。LongRun 补充 reset/重新进入/实际运动成功的事件记录与释放 Python 帧缓冲后的 native GC 资源快照；不改变 Matrix 分支。

Reference 的失败/局限全部保留：ProjectReference_Standalone01 停在原生暂停主菜单，正常 Alt+F4，exit 0、complete false、190.107 秒。Standalone02 原生启动后在等待前台期间继续模拟，完成 720 帧、exit 0，但最终实际截图为 YOU DIED，**不能作为正常游玩基线**。该独立 GPU profile 的格式化事件确证 TSR `1920x1080 -> 1920x1080`。后续驱动保持原暂停菜单直到前台建立，采集 240 帧并逐帧断言 Health>0；截图使用显式 nosuffix 文件名。

ProjectReference_Standalone03 的 240 帧生命值断言通过，但 Shot 的 nosuffix 缺少命令参数前缀，图片实际为 viewport00000.png，末尾文件名断言失败（exit 0、severe 1）；原图已查看，玩家存活、原生敌人 Hunting、HUD 正常。修正为 `Shot -nosuffix -showui` 后 **ProjectReference_Standalone04** 完整通过，实际截图 viewport.png；240 帧 p50/p95/p99/max 为 **25.208/33.485/39.913/50.409 ms**，15 帧>33、0 帧>100、最长慢帧串 1。该普通 L_Prototype 使用默认玩法和 legacy 呈现（project fog_extent 为 0），是基础项目对照，不冒充 Lab 空场景同负载或灰色层性能通过。独立 GPU profile/截图均在这些计时样本之后。

## 阶段 6：正式 after 与完整前后比较

正式 after 固定 `5ec64c1d8dfef1e20d13ed98fc3c6617828d9170`；所有六组完成前没有编辑源码。有效样本 After_PIE_01/02/03 与 After_Standalone_02/03/04 全部 complete、exit 0、severe 0，逐帧实际前台/1920×1080/SP100/全部记录的质量设置检查通过，与 before 设置相同。After_Standalone_01 最后 ActualNewKnowledge 有 357 帧失去 OS 前台，整组排除正式汇总，原始数据保留；协议 complete、退出 0 不能代替前台有效性。

下表每阶段、每模式各三次独立进程；p95 包含全部 case 帧，不剔除冷帧、初始化尖峰或离群值；setup 为三次中位数，最大整帧为该阶段三次最大值。完整 p50/p95/p99/max、慢帧串和资源计数在 Saved/Stabilization/paired-comparison.json 及各运行 analysis.json；聚合原始表在 paired-comparison.md。
| 模式 / 案例 | before p95 中位数 [范围] ms | after p95 中位数 [范围] ms | before → after setup 中位数 ms | before → after 最大整帧 ms |
| --- | ---: | ---: | ---: | ---: |
| PIE / Empty | 33.528 [33.083, 33.708] | 32.584 [31.386, 32.788] | 0.133 → 0.121 | 122.421 → 50.923 |
| PIE / OneWhole | 33.863 [33.510, 34.004] | 31.072 [30.495, 31.125] | 2.020 → 2.224 | 70.512 → 61.545 |
| PIE / EightWhole | 35.985 [35.014, 36.079] | 32.122 [31.623, 33.121] | 13.724 → 14.863 | 57.442 → 51.475 |
| PIE / ThirtyTwoWhole | 40.244 [38.533, 41.732] | 41.263 [38.021, 42.231] | 56.380 → 61.875 | 117.140 → 125.685 |
| PIE / PartialNewThenRepeat | 31.958 [31.875, 32.258] | 31.014 [30.512, 31.871] | 23.657 → 24.725 | 53.827 → 58.653 |
| PIE / Overlap64 | 33.572 [33.461, 34.009] | 31.321 [31.140, 31.369] | 348.403 → 184.862 | 802.464 → 551.491 |
| PIE / SameIdentity64 | 33.283 [33.266, 33.703] | 31.409 [30.964, 32.295] | 388.439 → 205.087 | 1315.046 → 934.409 |
| PIE / Distributed184 | 34.158 [34.064, 34.331] | 30.882 [30.675, 31.261] | 1085.538 → 580.757 | 3641.627 → 2633.466 |
| PIE / FastSweep90 | 21.171 [20.828, 21.358] | 27.877 [27.371, 28.728] | 392.491 → 204.827 | 844.810 → 574.466 |
| PIE / FastSweep160 | 20.917 [20.830, 21.258] | 27.770 [27.145, 28.100] | 376.251 → 213.227 | 823.881 → 580.680 |
| PIE / StationaryStop | 21.940 [21.903, 21.964] | 27.635 [27.582, 29.451] | 8.581 → 9.103 | 34.423 → 36.808 |
| PIE / LongRepeatDistributed | 33.573 [33.421, 33.973] | 31.456 [31.236, 32.148] | 1090.884 → 602.258 | 3647.831 → 2639.522 |
| PIE / ActualNewKnowledge | 31.933 [31.614, 32.277] | 29.739 [29.212, 29.851] | 28.310 → 29.063 | 55.467 → 66.507 |
| Standalone / Empty | 31.710 [31.693, 31.720] | 28.429 [27.842, 29.858] | 0.133 → 0.120 | 111.688 → 37.630 |
| Standalone / OneWhole | 31.855 [31.551, 32.234] | 28.787 [27.780, 29.050] | 1.728 → 2.020 | 44.154 → 35.890 |
| Standalone / EightWhole | 32.655 [32.572, 34.736] | 29.993 [29.894, 30.125] | 12.057 → 12.839 | 60.995 → 49.428 |
| Standalone / ThirtyTwoWhole | 41.168 [39.716, 43.620] | 37.385 [36.832, 38.448] | 46.850 → 52.159 | 83.263 → 86.875 |
| Standalone / PartialNewThenRepeat | 30.239 [29.867, 30.898] | 28.433 [28.364, 29.714] | 4.586 → 4.908 | 54.733 → 48.992 |
| Standalone / Overlap64 | 31.485 [31.460, 31.872] | 28.859 [28.835, 29.055] | 342.539 → 181.511 | 739.421 → 511.476 |
| Standalone / SameIdentity64 | 31.458 [31.362, 31.630] | 28.175 [27.817, 28.203] | 383.868 → 197.108 | 1250.710 → 883.026 |
| Standalone / Distributed184 | 31.896 [31.583, 32.357] | 29.107 [27.970, 29.373] | 1073.318 → 553.043 | 3533.005 → 2566.444 |
| Standalone / FastSweep90 | 19.297 [19.068, 19.391] | 27.796 [27.448, 28.267] | 394.377 → 186.729 | 792.784 → 516.010 |
| Standalone / FastSweep160 | 19.176 [19.088, 19.291] | 27.835 [27.583, 28.491] | 376.719 → 207.590 | 774.887 → 537.854 |
| Standalone / StationaryStop | 19.918 [19.765, 20.017] | 27.904 [27.347, 27.909] | 4.295 → 3.766 | 35.029 → 35.422 |
| Standalone / LongRepeatDistributed | 31.742 [31.412, 31.816] | 27.911 [27.589, 28.776] | 1080.001 → 607.273 | 3537.339 → 2538.341 |
| Standalone / ActualNewKnowledge | 30.264 [30.096, 30.336] | 28.645 [28.108, 29.296] | 6.077 → 7.293 | 46.207 → 52.158 |

性能结论明确为 FAIL：空场景和所有正式案例的 p95 都未达到 16.6 ms；PIE 32 Whole 每次仍有 123–126 ms 尖峰。快速扫视与停止案例的正式 after 比 before 更慢，不能选择早期 20 ms 烟测覆盖这些退化结果。批量 setup 约减半，但 184 条 distributed 的首帧仍超过 2.5 秒。

可比性边界：相同脚本路线按真实 delta 运动，原生捕获/反证的时序会改变背景记录数；Standalone OneWhole 的 after 资源最大数 2、before 1，不能声称各帧状态完全相同。64 压力样本的即时 65 条很快被合法反证降到 1，distributed 热态约 121/122，不能声称 64 条持续活跃压力已通过。等待前台的冷启动帧单独保留；before/after 激活延迟不同，不把等待时间伪装为可比启动性能。

FinalAttribution_Standalone01 为独立 Trace（不并入正式数据）：58.392 秒、完整协议、exit 0、severe 0、环境异常 0。Empty / NoGuidance / NoWorldLabels / NoUi / NoCoverageDraw / Restored 的 wall p95 为 29.498 / 28.635 / 28.102 / 26.879 / 25.101 / 27.004 ms。Empty GPU coverage 平均 0.243 ms，TSR 5.345 ms，LumenScreenProbeGather 3.485 ms，SourceUpdate CPU 5.193 ms。覆盖优化收益持续存在；剩余基础渲染和等待成本仍高，其增长原因尚未完全归因。现场 NVML 记录 GPU 97%、71°C、1920 MHz、169 W、P0；没有证据把退化归咎于温度或后台应用，也没有关闭用户其他程序。

Stabilization_FinalQualification01 最终 D3D12/SM6 视觉流程：906 帧、8632 个检查全部通过，九次独立资格会话 current color ratio 最低 1.0，无图像 oracle 失败；151.949 秒、完整协议/清理、exit 0、severe 0。Contracts、Episodes、Reobservation/WholeSessions 和长测继续。

长测驱动只在 ActualNewKnowledge 之后显式 Reset Room 03，恢复该案例实际移走的 source，再进入混合交互路线；记录 reset 前后资源，保持真实运动/可见范围。此变更发生在六组正式 after 结束之后，未改变 Matrix 或 C++ 二进制。先提交推送该检查点，再开始长测。

## 阶段 7：最终完整视觉回归

运行时代码保持最终构建不变。四个独立 D3D12/SM6 图形协议全部完成，清理标记完整、非调试器实际 exit 0、severe 0；图像原件已查看，协议完成与画面判据分别核验。

| Saved/ArchitectureAudit 运行目录 | 协议证据 | 独立画面/资源 oracle | 进程 wall 秒 |
| --- | --- | --- | ---: |
| Stabilization_FinalQualification01 | 906 帧、九次独立资格会话 | 8632 检查 PASS | 151.949 |
| Stabilization_FinalContracts01 | 178 帧、三次 PIE | Whole 首次离开 24/24 图像可见度 PASS | 71.858 |
| Stabilization_FinalEpisodes01 | 八轮观察和 cap 隐藏/恢复诊断 | 六组各 896 个内部样本，无缺口 | 42.211 |
| Stabilization_FinalWholeSessions01 | Reobservation + WholeSessions + NormalTurns，118 取证帧 | Reobservation 192、ConfirmedWholeCurrent 97、WholeSessions 332 检查全 PASS | 74.009 |

最后一组同时覆盖原连续四阶段观察、四次重复远近观察、四次重新取得 Whole 资格、相同小接触达标后保持彩色、旧完整历史不回退，以及真实相机墙体深度/隐藏墙负对照。图像 oracle 比较原始截图、实际材质/纹理绑定与独立解析内部样本，不能以原生测试成功替代。所用分析命令为 Scripts/AnalyzeWholeQualification.py、AnalyzeGrayWholeTransitions.py、AnalyzeGrayMemoryEpisodes.py、AnalyzeGrayReobservation.py、AnalyzeConfirmedWholeCurrent.py、AnalyzeWholeSessions.py，参数为表中对应目录。

这些视觉协议采用固定时间步，截图实际 2233×911；其用途是表面正确性和生命周期，不冒充正式 1080p 无截图性能矩阵。所有计时门槛仍以独立正式性能与真实长测为准。

## 阶段 8：完整模拟长测及普通 PIE 尺寸修正

`RunGrayObjectPolicyTests.ps1 -RunName Stabilization_FinalSoak01 -Tests Darkwell.PropLab.GrayHomeBaseline.FifteenMinuteInteractiveSoak` 完整完成 54,000 active、60 settle、600 idle；无 wall budget 中断。测试耗时 328.048 秒，进程 wall 358.431 秒、exit 0、severe 0，但测试本身 **FAIL**，脚本正确返回 1。失败均来自原性能门槛：active 共 1308 步 >33 ms、2 步 >100 ms、最长连续慢步 14；不能用正常退出覆盖测试失败。

每 3600 步的原始分布与资源记录在 log 和 soak-trends.json；active 最后一段 step p50/p95/p99/max 为 3.764/15.908/67.808/101.872 ms（该段分位数不能冒充完整 54,000 分位数）。Idle 600 步为 0.266/0.277/0.379/0.469 ms，0 次 >33/>100，零纹理上传、创建和历史扫描。模拟保持 9 个身份，资源峰值 records 4、textures 21、MIDs 36、caps 0、显示丢失检查 0，原记录/呈现资源边界断言通过。

**工作集异常未解决**：第 3600 步 4,770,422,784 字节，54000 步 11,194,589,184 字节，idle 结束 11,201,318,912 字节；同期存活 UObject 在周期 GC 后约 63,161–63,174，idle 63,057。不能由 UObject/纹理个数稳定推出内存稳定，也不能直接把进程工作集增长定性为灰色历史泄漏。该测试在同一个同步 Automation 调用里连续推进子系统，不运行普通引擎完整帧；引擎 Texture2D.cpp 的 UpdateTextureRegions 会把上传缓冲清理排到 RHI command lambda。队列是否积压尚无直接分配证据，故仅作为需验证的线索，不能当已证实根因。

普通默认 Editor `ProjectReference_PIE01` 实际尺寸为 1920×1082，被严格条件断言拒绝（complete false、exit 0、severe 1、27.596 秒），失败保留。新增 Editor 模块专用 SetPerformanceViewportSize，在普通 PIE 中固定实际 1920×1080，结束时恢复；不生成 Lab actor、不改普通玩法、不改变 Matrix。完整 Editor 构建 Stabilization_ReferenceViewportBuild01.log 成功，24.74 秒；UBT 因 adaptive unity 工作集同时重链 runtime DLL，runtime 源码没有新改动。此前六组正式 after 的原二进制/源码元数据继续保留，新增构建不冒充那些样本的二进制。

ProjectReference_PIE02 使用默认 Editor 工具集、普通 L_Prototype 原生玩法，实际 1920×1080/SP100、240 帧存活断言通过，complete/exit 0/severe 0，32.546 秒。p50/p95/p99/max 为 27.543/36.634/47.321/79.908 ms，66 帧 >33、0 帧 >100、最长 3 帧。原图显示 Health 36%、Stalker Hunting；计时后 GPU profile 的 TSR 事件确证实际 1920×1080。截图 showui 可包含窗口边框，不能把 PNG 外框尺寸当内部渲染尺寸。

LabRenderReference_PIE01 为独立 Lab 大厅 render-size 取证，不混入 Matrix：34.921 秒、complete、exit 0、severe 0，实际 1920×1080/SP100、6240×6320 coverage。GPU 原始事件 TSR MeasureFlickeringLuma / SpatialAntiAliasing / ResolveHistory 为 1920×1080；Epic TSR UpdateHistory 为 3840×2160，保留当前默认高密度历史。240 帧 p50/p95/p99/max 23.130/26.188/27.691/146.271 ms，2 帧 >33、1 帧 >100、最长 2；该独立大厅路线与压力房 Empty 不同，不用于替换正式矩阵。两份 PIE Reference 截图均已查看，尺寸以实际 viewport 与 GPU 事件为准。

最新 Editor 构建追加完整 manifest 回归 Stabilization_FinalFunctional03：**143/143（134 clean、9 warnings）、0 failed、0 not-run、severe 0、进程 exit 0**；原 142 个名称无遗漏，新增仍为 ConservativeDrawSupport。测试总时长 178.692 秒、进程 wall 211.329 秒。运行时源码自 5ec64c1 以来无变化，本次额外复核包含最终重链后的二进制；完整日志与 baseline-coverage.json 保留。

已提交明细表 `Docs/SIGHTWEAVE_GRAY_STABILIZATION_METRICS.csv` 包含全部 12 个有效进程 × 13 案例 = 156 行：逐次 SHA/二进制哈希、p50/p95/p99/max、setup、>33/>100/慢帧串、资源首尾/峰值，以及异步线程和灰色阶段分位数。线程列存在重叠和延迟，不能相加或直接相减归因；它们与前文独立 Trace 互为补充。无效样本仍在原目录并在本文逐项记录，不混入该有效明细表。

## 阶段 9：真实十分钟长测与第二次中断恢复

`RunGrayPerformanceBaseline.ps1 -RunName Stabilization_FinalLongRun01 -Mode Standalone -Protocol LongRun -NoAuthoringToolsets` 已在 Codex 意外关闭前完整完成。Saved/Stabilization/Stabilization_FinalLongRun01 的 complete.json、summary.json、analysis.json、long-resource-trends.json 和原始 frames.jsonl 均存在。进程 wall 636.739 秒，完整协议、非调试器 exit 0、severe 0、日志正常关闭；全部逐帧环境检查通过，异常前台/尺寸/画质帧为 0。没有重跑这份长测。

前置 ActualNewKnowledge 360 帧通过真实新增六条未解决知识断言；p50/p95/p99/max 22.057/29.101/44.393/50.300 ms，5 帧 >33、0 帧 >100。随后 LongInteraction 按协议达到 610.004 秒，首末已记录帧跨度 609.967 秒；共 29,741 帧，p50/p95/p99/max **21.004/26.430/28.963/112.532 ms**，57 帧 >33、2 帧 >100、最长慢帧串 1，性能仍 FAIL。共有 62 个房间阶段事件、11 次显式 Reset、10 次成功物理运动启动，覆盖 Room 1/2/3/5 的 Whole、Partial、运动、重复观察和重入。前置六条知识到混合路线之间的 Room 03 Reset 明确记录为 6→0，不伪装自然遗忘。

| LongInteraction 资源 | 首帧 | 末帧 | 峰值 |
| --- | ---: | ---: | ---: |
| records | 1 | 2 | 4 |
| proxies | 1 | 2 | 3 |
| textures | 2 | 12 | 14 |
| MIDs（历史计数） | 1 | 4 | 5 |
| caps | 1 | 1 | 2 |
| fine history bytes | 0 | 2,818,048 | 6,627,328 |
| 工作集 bytes | 3,235,434,496 | 3,575,353,344 | 3,575,353,344 |
| UObject 槽位计数 | 53,904 | 53,999 | 53,999 |

分钟末 RHI texture bytes：前四分钟 1,948,364,800，之后 1,939,058,688，末尾清理仍为后者；这是设备纹理统计，不等于全部显存。独立 NVML 观测运行中约 3919–3981 MiB，退出后 1694 MiB（读数时下一普通 Editor 正在启动，不能声称是完全无 UE 的桌面基线）。工作集随 Python 保存逐帧字典增加；释放 samples、Python GC、native GC 并再运行 60 个真实帧后降至 **3,385,679,872** 字节，records/proxies/textures/MIDs/caps/fine history 保持 2/2/12/4/1/2,818,048。真实运行未见持续增长的灰色对象资源或 GPU 纹理数量，但此结果不能解释或抵消同步模拟中的 6.4 GB 工作集增长，故 LONG-RUN RESOURCES 保持 PARTIAL。

用户随后报告：关闭 Unreal Editor 过程中 Codex 窗口也意外关闭。恢复时 local/upstream/实际 remote 均为 e358e3eb2504be56af929f20ce8d61525f7d4c52，未提交仅本文两行明细说明和新 CSV，暂存区/stash 为空，LFS fsck 通过。没有残留 UnrealEditor、Python、Trace、UBT 或 AutomationTool；原有普通 PowerShell 保留。保存所有 Saved 证据，未 reset/clean/restore，也未重跑已完成测试。

中断发生于 `Stabilization_FinalMainPlayStop01`：普通 Editor 已启动，最后日志为 12:24:44.406；没有 summary.json、正常关闭标记、已完成 PIE 或可靠退出码。最后原生窗口操作调用被中断，不能推定键盘操作已执行。该样本标为 **INTERRUPTED / EXIT UNKNOWN**，不算 PASS，也不伪造 0xC0000005。恢复查询最近一小时 Windows Application 1000/1001/1002 未找到匹配 Codex/UE 事件，项目最新 crash 目录仍为前一日；这不证明没有异常，只说明目前没有新崩溃证据可归因。最终普通 Editor 退出扩展验证仍待完成。

本次普通 Editor 的额外启动日志 verbosity 明确识别出早前反复出现的 13 条 generic Condition failed 属于引擎 smoke 测试 FUnifiedErrorTest_CreateErrorMessage、FUnifiedErrorTest_CreateErrorMessageWithContext、FStructuredLogFormatTest。它们发生在本任务指定测试队列之前，原日志保留，不能称整个引擎所有测试均通过；灰色层 143 项结果按其独立报告判定。尚未把这些引擎测试失败与退出事件建立因果关系。

## 阶段 10：普通 Editor 的原生接口退出扩展

为避免再次依赖中断时的系统级窗口键盘输入，改用 UE 自带 ModelContextProtocol（本机 127.0.0.1:8000）的 EditorAppToolset.StartPIE / StopPIE / IsPIERunning，以及 SlateInspectorToolset.Windows。每次请求、结果和时间先落盘；普通 Editor 没有 -ExecutePythonScript、keep-alive audit 或调试器。StartPIE 使用标准 in-viewport、非 Simulate，返回后 BeginPlay 已完成；Stop 后再独立查询 false。Windows(close) 的引擎实现调用目标 SWindow::RequestDestroyWindow，是正常窗口关闭请求，不强杀进程，不发送全局按键。

Stabilization_FinalMainApi01：IsPIERunning false→true→false，官方窗口列表只有 Darkwell - 虚幻编辑器，针对该窗口关闭，返回 OK；实际 exit 0、log_closed true、severe 0、182.914 秒（包含接口发现期间等待）。API 回执与引擎 PIE world 创建/清理日志共同证明一次 Play→Stop→Close，不能只依赖 MainWindow runner 的 protocol_complete 字段。中断的旧 FinalMainPlayStop01 继续保留为 EXIT UNKNOWN。

Stabilization_FinalMainApi02：false→true→false→true→false 的完整官方回执和两次 PIE world 创建日志，关闭前 editor_pre_exit pie=0，随后 slate_pre_shutdown、日志正常结束；实际 exit 0、severe 0、122.200 秒。两组普通 Editor 都没有 -ExecutePythonScript 或调试器，关闭仅通过该 UE 进程内 SWindow 的正常 RequestDestroyWindow。Codex 在这两组操作后保持正常。

## 阶段 11：短时上传清理归因与 RHI 参数错误

新增独立 `Darkwell.Stabilization.Diagnostics.UploadCleanup`，不改玩法代码，不重跑已完成测试。它向一个 256×144 PF_FloatRGBA 临时纹理提交两组各 2048 次上传，直接跟踪用户上传缓冲的 pending bytes / cleanup 完成数；第二组每 32 次显式 FlushRenderingCommands，属于诊断干预，绝不加进运行时作为优化或性能达标证据。最终还在正常引擎继续 12 帧后记录工作集，区分同步测试期间的滞留与恢复真实帧后的状态。

首个 `Stabilization_UploadCleanupNull01` 虽然请求 NullRHI，但探针记录 GUsingNullRHI=0。核查发现 `RunGrayObjectPolicyTests.ps1` 的条件表达式返回单字符串后，native splat 把 `-NullRHI` 展开为 `- N u l l R H I`；完整原始命令行、Using Default RHI: D3D12 以及探针三者吻合。**Stabilization_FinalSoak01 与 FinalFunctional03 的原日志同样证明实际为 D3D12/SM6**。其测试结果仍真实，但不能称它们为 NullRHI；54,000 步仍是同步模拟子系统调用，不是 900 秒真实引擎帧。已改为显式 string[] 保存参数，并在摘要保存请求 RHI 与原始日志证据。Rendering 分支本来有两个参数，未受这一单字符串问题影响；GPU oracle 与正式性能 runner 不受影响。

首个探针 1 项 PASS、exit 0、severe 0、测试 7.385 秒/进程 37.113 秒：无排空时 2048 个缓冲全部待释放，共 603,979,776 bytes（576 MiB）；排空后 pending=0、completed=2048。每 32 次排空把 pending 峰值限制到 9,437,184 bytes（9 MiB），最终也全部清理。但工作集仍从 4.23 GB 上升至 6.03 GB，证明不能仅看 CPU 上传回调就宣布内存问题解决，还需观察 D3D12 暂存资源/池的逐帧回收。该样本源文件与 hash 已额外保存在原目录。

修复参数后的 `Stabilization_UploadCleanupTrueNull01` 确认实际命令为完整 -NullRHI，但新探针原先错误地要求 NullRHI 也创建纹理资源，断言失败（测试 FAIL、exit 0、severe 0、19.518 秒）。失败保留；改为在 GUsingNullRHI 时明确验证资源为空、上传为零，D3D12 路径继续验证全部清理。三次完整 Editor 构建 UploadProbeBuild01/02/03 分别成功 6.76/5.48/5.57 秒，均只编译新增测试文件并重链。

最终短对照：`Stabilization_UploadCleanupD3D12Frames01` 1 项 PASS、exit 0、severe 0，测试 3.091 秒/进程 35.251 秒，显式 -d3d12 -sm6 且 GUsingNullRHI=0。用户缓冲 pending 576 MiB→0；每 32 次排空峰值 9 MiB，全部 4096 个缓冲最终清理。工作集 4,066,603,008→5,857,345,536 bytes，恢复 12 个真实引擎帧后 5,858,992,128 bytes，未回到起点。`Stabilization_UploadCleanupTrueNull02` 1 项 PASS、exit 0、severe 0，测试 0.008 秒/进程 23.271 秒，实际 GUsingNullRHI=1、texture_resource=0、uploads=0。

该对照确证同步提交会积压用户上传缓冲，以及短时间的 RHI 排空不保证进程工作集下降；尚未用分配调用栈区分全部 allocator 缓存、D3D12 暂存池和永久保留分配。因此不把 54,000 步的 6.4 GB 增长全部归因于已证明的单一原因，也不把定期 Flush 加入玩法来换取虚假通过。LONG-RUN RESOURCES 保持 PARTIAL。后续应针对保留分配做 Memory Insights/LLM 与 D3D12 暂存分配归因，再决定是否需要修改资源生命周期或把同步测试改为逐引擎帧推进；现有两份完整长测不需要因本次恢复而重跑。

## 交接结论与剩余阻塞

- FUNCTIONAL REGRESSION：PASS。原 142 项无遗漏，最终功能报告 143/143；最终四个视觉协议及全部 oracle 通过；新增上传生命周期诊断在真实 D3D12 和正确 NullRHI 两分支均通过。恢复后没有修改产品运行逻辑，没有重跑已完成的功能、视觉、矩阵或长测。新构建只加入测试代码；原功能报告对应原有玩法代码，不能伪称在新 runner 的 NullRHI 下重新跑过 143 项。
- EXIT STABILITY：PASS，范围限定为已验证路径。**已证实并修复当前可复现的 audit 生命周期错误；不能证明历史上所有相同退出码必然具有相同根因。** 旧 0xC0000005、中断无退出码及所有协议失败均保留。Codex 意外关闭的确切机制没有新证据，不将其归入同一个 UE 崩溃根因。最后两组通过官方 UE 接口完成普通主窗口正常关闭，非全局快捷键、非强杀。
- FRAME PERFORMANCE：FAIL。三次正式 after 的 Empty p95 中位数 Standalone 28.429 ms、PIE 32.584 ms，均超 16.6 ms；快速扫视/停止存在退化，真实十分钟 p95 26.430 ms 且有两帧 >100 ms。基础 GPU/等待仍高，当前证据不支持把全部成本归于灰色对象。
- INITIALIZATION / BATCH HITCHES：FAIL。覆盖 GPU 绘制平均约 10.294→0.243 ms、批量 setup 约减半，但 184 distributed 整帧峰值仍约 2.5–2.6 秒；剩余 ownership/cap 等首批处理成本开放。64 合法反证后的压力下降和前台等待差异已注明，不能冒充持续满负载或严格冷启动达标。
- LONG-RUN RESOURCES：PARTIAL。两种完整长测已完成；真实 610 秒对象/纹理计数有界且 GPU 纹理统计稳定，释放采样缓冲后工作集下降；同步 D3D12 模拟工作集的大幅增长只得到部分机制证据，未完全闭环。

最终维持 **PARTIAL — GRAY_STABILIZATION_BLOCKED**。下一步工程重点为完整帧基础成本/退化归因、批量历史归属计算及保留内存的分配级归因；不得降低画质、改变每轮 100 cm、丢弃合法历史或取消 cap。两条 stable 与非发布功能检查点均核对未移动。最终 diff --check、LFS fsck、工作树及 local/upstream/remote 将在交付前核对；正常打开 Lab，PIE 停止，电脑保持开启。

收尾现场已核对：普通 UnrealEditor 加载 /Game/Maps/L_SightWeaveGrayPolicyLab，官方 IsPIERunning 两次返回 false，实际 Editor 截图保存在 Saved/Stabilization/FinalHandoff01/editor.png。测试、构建和 Insights 进程均已结束；普通 Editor 自启动的 Trace Server 已使用自身 `kill` 子命令触发 quit event 并等待服务退出，未强杀；最终只保留用户请求的普通 Editor。diff --check 与 LFS fsck 通过，工作树和暂存区干净，local/upstream/实际 remote 一致；最终提交及进程清单另存 FinalHandoff01/handoff-final.json。未改变 stable，未开始黑色层，未关机。

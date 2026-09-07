# SightWeave 批量历史表现架构判断

日期：2026-09-07。审计基线：cad92f014cfb88d4ba9e333eae5814184a9ba5a6；实际运行时43c5748db9afbd7c861d7a3be7389437ca2a4eab。

本轮只读源码和已存证据，没有运行UE、重新计时、构建、新增instrumentation或修改生产代码。开发分支已核验、初始工作区干净。本文是架构决策与下一切片合同，不是已实现或已测得的收益。旧INITIALIZATION仍FAIL，WholeGeometryPreparation默认0；stable、黑色层、Large World不动。

## 1. 决策

推荐 **先分离CPU历史状态与GPU表现寿命，再优先A按需物化；B作为高可见密度场景的后续后端**。两者互补，不是任选一个就能消除533ms。

先冻结已有微项。下一生产切片A0只完成资源可回收/可重建的语义隔离及oracle验证，默认仍全部物化，不立即实现距离流式、队列或跨帧证据。A0性能收益预算为0，不以重构冒充提速。A1再按明确的可见需求服务旧历史；如果产品确实要求大量历史同时可见，A的优势趋近0，届时选择B批量后端，而不是提高流式延迟或降低精度。

**必须澄清产品合同，但本轮不修改合同**：合成cold184不能自动代表shipping初始化；应保留原测试为独立压力回归，同时建立真实游戏/读档初始化验收。未定义或未验证的新门槛不能填PASS。

## 2. cold184实际上测什么

源码：`DarkwellMovingPropLabRoom.cpp::SetGrayPolicyStressMode(6)` → `ConfigureHistoricalEpochCountForTesting`；驱动`Content/Python/profile_gray_stabilization.py`的Batch。

- 不是184个身份，也不是184个首次Whole：3个新压力身份，**64+64+56条SpatialPartial / StationaryOnly历史epoch**；其余7个Lab身份使setup的tracked identities计数为10。
- 三组起点为(6000,3850)、(-6000,-3150)、(-6000,3850)cm。每epoch捕获姿态yaw增加17°、X增加7cm，同身份多epoch重叠密集；这不是均匀分布且互不干涉的N对象。
- 一个同步调用内反复BeginObservedLocation → 全粗网格coverage=1 → AdvanceCurrent(0.20s) → NotifyLegalObservation/ObservedArmed → EnsureRecordVisual → FreezeCurrentForHiddenMotion(TEST_EPOCH_SCALING)。0.20是传入模拟推进量，不是等待0.20秒或走真实相机路线。
- seed完成后隐藏实际源、关闭碰撞、bExists=false，随后原生产历史更新执行合法反证。Batch先Empty、再SameIdentity64、再Distributed184；因此这里的“cold”指历史资源首次冷建立，不是进程/驱动/材质/磁盘全部冷启动。
- setup明确要求该调用完成时184条record和proxy已创建。**它没有证明184条同一渲染帧都贡献可见像素**。三组空间分离、相机/墙体/视锥及ownership另行决定实际显示；不能把proxy数、候选数、纹理数当作可见数。
- 首次采样已只剩120条；不是只创建120。该帧处理184次texture/cap调用，随后64条合法消除。后续样本candidates=0、sleeping=120但proxies仍120：现有“sleeping”是CPU候选调度休眠，不是GPU资源卸载，也不是相机不可见证明。
- SaveGame已有玩家、探索cells、容器/拾取物/敌人等保存；当前`DarkwellSaveGame.h`和SaveSubsystem没有接通这套184 record/FineHistory的保存恢复入口。不能把此seed helper直接称作真实历史读档性能。

结论：这是有效的高重叠、多epoch、同调用全物化压力测试，适合检验复杂度和最坏同步成本；不是已证实的正常shipping交互/读档输入。若以后产品允许bulk restore仅先建立CPU历史、按显示需求物化，这是**新产品/导入合同**，不是把原seed切成多帧的等价优化。不得根据TEST_EPOCH_SCALING字符串偷偷少做工作。

## 3. 剩余约500ms的成本模型

直接复核现有`Saved/Stabilization/ParentMaterial_ColdPressure/performance.json`及`frames.jsonl`，运行时43c5748；无新增采样：

| 层级 | 时间/规模 | 含义 |
| --- | --- | --- |
| 完整冷帧wall | 533.639ms | 包含setup与随后游戏更新/流水线，不能只看native |
| 同步setup | 257.921ms | 三源构造、184次记录生成/seal、CPU capture/fine/几何及表现准备 |
| 首次native memory更新 | 268.504ms | 全局dirty后的证据/ownership/occupancy/表现更新等 |
| 其中UpdateTracked / Historical | 242.899 / 240.040ms | 父scope，不能再加下面子项 |
| ownership / occupancy | 99.499 / 39.179ms | 语义相关空间工作，换renderer不自动消失 |
| texture提交 / cap表现 | 20.335 / 51.006ms | 当前CPU路径分项，含生成/准备等，不等于纯RHI调用 |
| coverage / fine advance计时 | 4.919 / 23.649ms | 嵌套相关工作，不能与父scope重复加总 |
| 工作规模 | 扫描1,972,688；occupancy tests2,095,981；geometry tests8,018,953 | 不是184次简单NewObject而已 |
| 首帧更新 | texture calls184/uploads182，cap calls184/rebuilds182 | seed已经建过表现，首次证据/ownership更新又提交变化 |
| 更新后驻留 | 120条，1,286,176 fine samples，41,157,632 fine bytes | 仅fine样本CPU字节，不是总内存 |

257.921+268.504=526.425ms，与wall差7.214ms只作时间账量级核对；不能把差值精确标为GPU或Python，独立timer边界/流水线并不提供此归因。setup本轮没有再次拆Trace；第21节旧版本Trace显示资源Ensure约47.5ms、seal约198.6ms且嵌套，不能拿来与本版分项拼成精确饼图。它足以说明“全部换掉Actor创建就省500ms”不成立。

用符号表示：

`T_cold ≈ T_seedCPU(N,S,G) + T_evidence(S_dirty,I) + T_materialize(K,P,U,H) + T_submit + T_other`。

N=历史record数；S=总样本数；G=捕获几何量；I=实际重叠/occupancy候选交互；K=本帧必须有可提交表现的record数（当前seed强制K=N）；P=mesh part数；U=纹理texel/脏区域；H=cap几何量。不是只由N决定。

| 工作 | 增长方式 | 可否合并/避免 |
| --- | --- | --- |
| Actor、每part component、每record MID/texture/cap对象 | 近似O(N+P)，注册/GC/渲染线程命令也随实例增加 | A减少驻留K；B减少组件/材质/提交对象 |
| fine初始化、capture mask/footprint、存储 | O(S)，细节/包围尺寸越大越重 | 不能因不显示省掉必要知识；未来压缩需独立等价证明 |
| pixels/Float16/签名/上传 | O(U_dirty)，另有O(提交次数)固定开销 | A不生成不需要显示的可重建产物；atlas合提交，不消除texel工作 |
| cap生成 | O(边界/几何候选H)及相交查询 | 可延后纯绘制网格，保留定义它的CPU状态；批量buffer不消除合法切口复杂度 |
| ownership/旧新epoch排除 | 随候选重叠I增加，密集时可超线性；朴素对比上界含同身份N²关系 | 索引已优化但语义未消失；共享renderer也必须完成相同判定 |
| dirty传播、空间索引首次构造 | O(S+候选/格子引用)，首次全局dirty无热缓存 | 普通增量场景可摊薄，不能将新合法证据跨帧漏算 |

重复阶段不是可随意删除的“重复工作”：seed时每次seal构成有效当前状态，之后出现更新的历史/合法空证据，必须再更新ownership/表现。只有显式bulk-import事务规定“中间状态不对外发布”时，才能合并最终表现提交；若导入语义不是纯最终state，跳过中间证据可能改变知识，须单独证明。

## 4. A：知识立即成立，表现按需物化

### 可行边界

保留record/FineHistory/capture/pose/content和全部权威证据推进；仅让proxy/纹理/MID/渲染cap网格成为可重建资源。渲染范围不得反向决定合法观察、ownership、空间反证或记录寿命。远处record仍须参加所需的新旧知识/物理dirty查询。

当前不可直接落地的原因：`FRecordVisual`混合GPU引用与PartGeometry、SuppressedByCurrentEvidence、TransientCurrentSuppression、coverage/evidence/occupancy缓存及Processed*Revision。UpdateTracked先EnsureRecordVisual再推进证据；ownership候选还检查Visual存在/retired。`RetireHistoricalPresentation`含终结语义，之后可ReleaseTerminalRecord。**Dormant不能等于retired，DestroyVisual不能充当资源逐出。** 渲染资源缺席必须不影响CPU贡献者的存在。

### 需求与零延迟

- 当前透明预备资源、最近刚离开视野的捕获先保守pin住；原Whole Current→Gray同GT事务发布，0额外帧。第一阶段不逐出它们。
- 对旧sealed历史，表现需求是保守的相机可贡献范围+预测/滞回范围，不是灯光合法证据范围。不要以“黑暗”“墙后”“LastRenderTime未更新”直接断定不会再次需要；正常深度遮挡仍交给renderer。
- 尚未Ready而相机已经需要：同帧同步物化fallback。保证首显而允许hitch，不能等待预算后续帧。任意瞬移/快速转身和所有资源同时需要时，零延迟与严格帧预算不能同时保证；不允许隐含保证二者。
- off-range释放只释放GPU表现；重入先基于当前CPU revision重建，不恢复旧像素/旧cap。未来若有跨帧准备，必须检查world/Scene lifetime、History incarnation、epoch、source/content/capture和presentation revision，再GT发布；迟到结果不得FindOrAdd历史。
- 场景加载若允许在解除输入/首次画面前建立必显K条，应明确loading barrier的产品合同、最坏ready时间及失败处理，不能把这段初始化从总窗口排除。

### 收益、风险、复杂度

资源驻留可从O(N+P)变成O(K+P_K)，CPU知识仍O(S)。大量旧历史离当前显示域很远时最有价值；全体同屏或最近capture全被pin时收益可为0。现有cold184逐条Current→Gray触发保护，**保守A实现可能仍全物化，原533ms不保证下降**。新bulk-old-history输入可展示A的价值，但必须另命名，不能替换cold184。

规划敏感性（非测量/承诺）：若真正可推迟的纯表现部分P为80–160ms，且旧历史必显比例K/N=25–50%，可避免当前窗口约40–120ms（约8–22%）；P尚需在数据解耦后量出，不能把此假设当成已归因金额。权威CPU成本保留，无法据此承诺16ms。驻留GPU对象可在相同K假设下减少50–75%，fine CPU字节不因此减少。复杂度中到高，最高风险是误删CPU状态、迟到复活、重入空白、逐出/重建抖动，以及把CPU证据候选索引错用成显示索引。

## 5. B：批量Presentation backend

| 结构 | 适用性与真正收益 | 主要风险/复杂度 |
| --- | --- | --- |
| 共享历史纹理atlas/array页 | 保留每record原texel密度、格式、无效区；用region/slice索引替代独立texture参数，可合并上传次数 | 不减少U；边界padding/过滤/寻址、槽位复用revision、碎片/resize和旧渲染命令仍使用旧页；中高 |
| 按mesh/material/render-state分组实例化 | 同mesh重复epoch（cold184很适合评估），instance transform+record索引代替Actor/多component/MID | 先有shader per-record参数/atlas支持，不能直接跨record共享当前MID；透明排序、每实例裁剪、bounds/深度/姿态正确性；中高 |
| shared component/custom scene proxy | 一个或少数组件持有多个record绘制项，独立CPU记录/边界和资源handle | GT/RTbuffer寿命与fence、重建/注册、批次失效扩大、调试与编辑器适配；高，不作为首切片 |
| cap共享mesh buffers/间接绘制 | 合并对象/提交，允许每record索引区间和局部更新 | 合法外切口/3Downership仍须生成，不能以一个大包围cap替代；局部更新变全批上传可能倒退；高 |

不可仅合并的内容：不同record的texture domain、captured pose、tint/UV、ready、永久排除和可逆Current exclusion；它们必须成为明确的per-record数据，而非被共享MID最后写入覆盖。几何内容相同才可共享mesh；snapshot mesh资产的持有寿命不能跟随现存source消失。批量bounds也不能破坏正常相机深度或把整批错误剔除。

收益规划：B先攻击O(N+P)对象和O(提交次数)，不自动消除本版约100ms ownership、39ms occupancy和capture/fine CPU工作。假设可合并固定管理/提交部分F=30–80ms、消除50–80%，得到15–64ms（约3–12%冷帧）的第一阶段规划范围；F未测、不是预测实测值。要取得更大收益需连同纯表现cap/pixel后端改造，复杂度和风险显著上升；不能许诺一次atlas省500ms。K≈N、同mesh密集可见群是B的最佳产品场景；材质/mesh高度异构时批次碎片会压低收益。

## 6. 最小第一生产切片A0：资源寿命解耦闭环

下一轮中档可直接执行，**不再重新设计架构**：

1. 阅读本文、AGENTS、Scene的FRecordVisual/EnsureRecordVisual/UpdateTracked/Freeze/Retire/Destroy及已有资源oracle测试。保持所有既有开关默认值，P1仍0。
2. 引入CPU per-record状态与GPU资源子结构的明确边界。第一版保守保留现有所有bits、PartGeometry、coverage/occupancy缓存、ProcessedRevision、cap描述、SubmittedPresentation在CPU侧；不要顺便优化/删除缓存或改算法。GPU侧只拥有proxy/component、texture、MID及其注册/释放状态。每record CPU状态在资源未驻留时仍存在。
3. 分开`ReleaseRenderResources`、`EnsureRenderResourcesFromCurrentCpuState`与现有终结retire。普通释放不能设置bPresentationRetired、调用ReleaseTerminalRecord、改FineHistory/ownership或清CPU revision。强引用OwnedTextures/OwnedMaterials/OwnedCaps及proxy销毁各自成对维护；无池、无后台任务。
4. 正常路径仍全部驻留，保留原同DLLoracle。仅提供开发测试控制：对一个旧sealed record强制release → 在无GPU资源状态下推进原合法证据/新epoch排除 → 重建 → 同帧首次显示。要覆盖Partial外切口和Whole旧epoch，但不改它们规则；Current和最近首次seal不进入自动逐出。
5. 清除把GPU驻留性当作贡献者资格的检查；Visual/CPU状态存在和语义终结仍按原合同判断。先逐一审计所有Visual存在/retired分支，不允许全局替换成“资源存在”条件。
6. 最小测试：相同输入的CPU record/fine/永久及可逆排除/姿态/revision逐项oracle；离线期间新证据后重建像素和cap一致；Reset同epoch复用、SourceReplace/Destroy、terminal后禁止复活、双release/ensure幂等、GC/world teardown；真实Whole首离开+旧历史重入首帧及Partial外切口。确认最近Whole0额外首显帧。
7. full Editor Build及定向测试；同DLL默认路径短压力仅作回归、维持原cold184时序/计数，完整窗口包含释放/重建与同步fallback。记录资源数、驻留bytes、重建耗时、最大完整帧及首显延迟。A0不设“必须优化533ms”的目标。

A0验收通过再进入A1保守需求分类/旧历史驻留策略；先记录所需K/可逐出集合，再启用自动物化。若隔离过程中必须改变权威知识/采样/Partial算法，停止扩展，缩回纯GPU子结构；不要留下半套异步发布。B1在真正密集必显场景成立时再选一个mesh组+per-record参数后端验证，不能和A0同轮施工。

## 7. INITIALIZATION验收应如何拆分

建议产品合同分三条，**本轮只提出、未替换原gate**：

- `GameplayFirstHistory`：正常相机、合法Current→Gray、最近捕获0额外首显，记录从可用资源准备起完整最大帧以及Current/seal、可见K和最坏重入。不能只测已热缓存帧。
- `SceneRestoreInitialization`：先明确shipping是否恢复这套历史、最大N/S/K、输入解锁/首次画面边界、允许的loading总延迟，再固定真实save/import输入。当前这条合同/实现未建立，应标未定义/未验收，不是PASS。不能用现有人工FullCoverage seed代替真实保存数据语义。
- `SyntheticCold184Stress`：永久保留原三身份184 Partial、同调用全物化、相同seed顺序/资源数/窗口及后续120结果，单独记录533ms级FAIL。也可另加新backend的全驻留等价版本，但不得改旧版本输入或用A流式版本冒名。

在产品确认之前，原INITIALIZATION仍FAIL，不删除历史失败记录，也不偷偷改阈值。确认后可以明确把“shipping gate未通过/未定义”与“stress失败”分开，但不能因为重分类宣称性能优化。需要的产品决定仅是正式场景/预算/可见合同，不是批准违反知识语义。

## 8. 冻结与下一阶段停手线

冻结ownership/capture/cap/occupancy现有CPU算法及同帧join；它们仍是正确性oracle，只为新后端适配或真实回归修复而动。冻结P1范围及模式0默认，不再优先投入其1ms预算/首次Whole两个mask微调；保留P1协议/故障注入作为参考，不能把它当Partial资源streaming的现成完整协议。冻结record内MID共享、唯一cap名称、Scene父材质强持有；保留各旧路径开关。父材质资产归属/cook声明是独立工程边界，不与本架构切片捆绑。

本轮无需新instrumentation即可证明两阶段同步放大及CPU/GPU寿命耦合。尚未量出实际必显K、纯可推迟P、固定提交F、GPU draw/overdraw和各方案真实收益；上述范围都是有条件规划。没有运行新benchmark/测试，不冒充架构已验证。下一轮唯一建议入口为A0，交付可释放/可重建且不改知识的完整小切片；Large World、黑色层、跨帧证据、完整自定义renderer均不在首轮范围。

## 9. A0落地检查点（2026-09-07，b29557d）

上方第1–8节是161b57a的设计判断；本节记录后续A0实际实现，运行时b29557d84b1cb0dad79e00b53991ce492a2a0058。默认全驻留，开发测试才显式释放单旧record；没有实现A1。验收和证据见性能审计第27节。

### 已落地边界

| 寿命 | 字段/所有者 | 释放Presentation的影响 |
| --- | --- | --- |
| 玩家知识及capture | History内record、SpatialMemory、FineHistory、capture mask/footprint、observed primitives/pose/content/tint/UV | 不改、不重建知识 |
| CPU record状态 | FRecordVisual的PartBounds/PartGeometry、两种suppression、全部coverage/occupancy/geometry缓存、Processed*Revision/最大epoch、dirty/active状态、尺寸锁 | 保留，继续原UpdateTracked推进 |
| CPU表现结果 | SubmittedPresentation、TextureSignature、CapSignature/CapQuads/CapSamplePoints/CapTriangles及cap计数 | 继续更新；CapTriangles仍用于判断历史是否真正终结 |
| 可释放资源 | FRecordRenderResources的Proxy、Texture、Cap、Materials；UploadedTextureSignature、cap待提交/原子发布/透明预备/可见性状态 | ReleaseRenderResources解除Owned*引用并Destroy proxy/component，整体重置Render |
| 显式请求控制 | bRenderResourcesReleased、PresentationRequestSerial，测试票据 | 不是知识/资格/retired，不作为ownership候选条件 |

资源强引用仍通过原Scene UPROPERTY OwnedTextures/OwnedMaterials/OwnedCaps及world actor寿命维护；没有static缓存、AddToRoot资源或资源池。原观察mesh仍以record的TSoftObjectPtr资产路径描述，不把当前source mesh当重建输入；重建先检查全部捕获mesh可取得，缺失时Error+false，保留CPU历史。普通资产路径跨GC后同步重取经过测试；尚未证明任意无持久路径的临时mesh都能卸载后恢复，不能据此启用任意资产流式。

ReleaseRenderResources与DestroyVisual分开：前者只释放Render；后者仍用于原Reset/销毁/terminal清理，保留既有bDiscardEvidence行为。没有用bPresentationRetired表达非驻留，也没有改ReleaseTerminalRecord判定。默认Never/未创建资源路径仍按原条件提前返回，只有显式离线的旧历史继续无GPU的CPU输出生成。

### 请求、推进和发布

1. 开发测试对现存、非Current、非retired且已初始化fine历史调用ReleaseHistoricalPresentationForTesting，取得Scene/source弱身份、History.GetPreparationLifetime、epoch、单调请求序号。API不接受只有id/epoch的重建，也不创造record。
2. 相同record重复release安全，但新票据撤销旧请求；表现资源被回收，CPU Visual保持存在。ownership贡献者和空间索引沿用CPU状态及真正终结条件，完全不检查Render驻留。
3. 无资源期间按原时序推进合法证据/反证、ownership/occupancy和CPU表现结果。没有跨帧证据、隐藏的fine写入或按相机需求减少知识；保留cap计算是本轮安全边界，不是A1性能收益。
4. 重建校验存活Scene/world、相同History寿命、source身份、epoch和请求序号；检查当前record非Current/terminal。直接取调用时最新CPU结果的输入重新生成，票据不携带过期像素或prepared snapshot，所以普通新证据不必撤销该请求。
5. 创建时proxy隐藏、MID SpatialReady=0，cap不显示；最新像素及cap在GT提交后才同调用设Ready/显示。不等待下一帧，没有任务回调。失败保留CPU状态并清理创建中的资源；成功重复调用不重复注册。
6. Reset/Initialize后不能复用票据，SourceReplace旧票据失败但可对仍合法的旧历史取得新票据；合法resume回到同步Current路径时取消显式非驻留并撤销旧请求。terminal或world销毁后查找失败，不会FindOrAdd复活。

PIE桥接ReleaseOldestPresentationForTesting/RebuildPresentationForTesting只是诊断：每Scene一个待重建票据，以随机GUID匹配请求；非WITH_DEV_AUTOMATION_TESTS构建返回空/false。没有距离、相机、预算、队列或定时触发者，因此最近capture不会被自动逐出。

### 验收结论与下一入口

完整Editor Build、最终D3D12定向3/3、两PIE/32图通过。12个无资源更新帧中8帧证据实际改变，Whole/Partial逐帧CPU/最终表现oracle一致；两次D3D12纹理读回完全一致，Partial实际mesh匹配当前CPU拓扑。Whole首次离开及resume再seal0额外帧；显式重建同GT调用完成并在首张后续渲染图出现正确表现。生命周期、失败及取消边界有定向覆盖，没有随机穷举或Shipping证据。

可以进入A1的保守需求分类及单旧历史自动驻留小切片，先记录所需K/可逐出集合、保护Current/最近capture、明确同步fallback，再考虑启用策略。A0不代表自动流式已经安全，不保证任意瞬移同时满足硬帧预算与0延迟。P1仍默认0，现有微项冻结；冷184输入未动、本轮未跑性能，INITIALIZATION仍FAIL。

# SightWeave 观察高度 / 表面可见性正式审计

日期：2026-09-10。代码基线：`3447970d799a09080e5bdb91255ed07b4a881bbd`。本轮只审计，不实现业务功能。

## 仓库核验与证据范围

- 分支 `codex/darkwell-prop-memory-gameplay-lab`；HEAD 为上述基线；开始时 working tree 干净。
- origin：`https://github.com/defiler919/LastLight.git`。`git ls-remote` 实时核验远端同名分支也为上述 commit。
- stable tag `stable/sightweave-hard-effective-live-unified-20260910` 为 annotated tag，tag object 是 `f53d832b3a035bf4413109e3619f1687c27715f2`，本地及远端 peeled commit 均为上述基线。不存在基线偏移。
- 已读根 AGENTS.md、当前统一合同、相关实现、自动测试和已提交 probes.csv。旧 Static 文档的“单层”描述已被当前代码覆盖，以代码为准。
- 本轮未运行 Editor、构建或新自动化测试；没有将以前的运行结果冒充本轮测试。本轮证据是源码控制流与仓库已有自动测试记录。无 C++、构建配置或资产变更，故未触发全量 build 要求。

## 结论

当前是**按高度谓词分区的二维合法空间场，加物体二维投影记忆**，不是角色视点下的表面知识。高柜顶面问题不是单纯摄像机取错位置，也不是 140cm 常量缺失：缺少接收表面身份、真实采样位置、朝向和源到表面的有限高度遮挡语义。

推荐有限表面片（surface patches）配简化实体遮挡代理，在 Runtime 的同一 EffectiveLive 裁决链内实现。无需体素、全场景三角形扫描或第二套下游合法算法。保留平面场作为能证明等价的特例与快速路径。

## 当前真实模型

| 链路 | 代码事实 | 能力边界 |
|---|---|---|
| 观察源 | `DarkwellSightWeaveWorldSubsystem.cpp:101` 使用角色位置 + 世界 Z 52；`DarkwellCharacter.cpp:33` 胶囊半高 88 | 默认站立几何关系约为地面上 140cm，实际取决于角色位置；不是独立眼点接口，也不是摄像机位置 |
| Runtime | `SightWeaveWorldSubsystem.cpp:1454` 接收 FVector，但形状/多边形测试消费 XY，Z 用于 floor/source/light/suppression 高度范围 | Source Transform 有 Z，不等于遮挡已经使用眼点到目标的斜射线；没有 receiver normal/SurfaceId |
| 遮挡求解 | `SightWeaveGeometry.cpp:1779` 按 segment 与 source HeightRange 是否重叠选线段，再做二维射线求交 | 不是按交点参数计算射线高度；每个 source 的二维多边形不随目标 Z 连续改变 |
| HardCoverage | `SightWeaveHardCoverage.h` 的 AtHeight 与 `SightWeaveHardCoverage.cpp:78` 的 captured query，共享不可变 revision | 分层解决合法高度带不一致、端点及 suppression；不能凭分层恢复表面或有限高度阴影 |
| Object Current | `DarkwellCurrentLiveGrid.cpp:237`、`:328` 在 primitive 的 LocalBounds 中心 Z 上采局部 XY，再变换到世界 | 修正了 pivot 与 primitive 高度差；并非顶面、侧面各自采样。生产路径是 upright 模型，不能据此宣称任意三维姿态支持 |
| Object History | `DarkwellSpatialObservationHistory.h` 保存 mesh、bounds、pose、epoch 和二维 masks；`DarkwellObjectMemoryScene.cpp:1193` 使用捕获 primitive 中心平面，多平面历史要求全部贡献平面具备空证据 | 有三维代理几何、重叠/ownership/cap 运算，不代表其观察证据是三维表面；同 XY 的不同面不能独立记忆 |
| Static Knowledge | `DarkwellStaticEnvironmentSubsystem.cpp:41` 仅声明 mesh 世界 AABB 的 XY tiles；`DarkwellStaticKnowledge.cpp:110` 按 Runtime cuts 拆层、等价层共享/COW | 2.5cm cells、4×4 fine support，记的是分层空间 bit；没有静态表面注册、面朝向和表面占用。声明 tile 也不是精确表面域 |
| P4 / 材质 | `DarkwellHardCoveragePresentation.cpp` 发布高度 atlas；`create_darkwell_project_fog_materials.py:145` 按世界 Z 选 slice，`:653` 消费 surface world position | 有世界 Z 选择和 4×4 表现面积采样；未按眼点判断表面。材质法线明暗不构成 Knowledge 证据 |
| Whole | `DarkwellObjectMemoryScene.cpp:3670`、`:3712` 使用 full geometry mask 捕获；材质脚本 `:689` 附近 Whole 可把 coverage 提到 1 | 这是既有整物体识别/表现合同，不能解释成每个面均已亲眼观察 |

插件另有 `FSightWeaveStaticEnvironmentDescription::WorldFootprint + HeightRange`；它同样不是表面模型，不能因名字相近而当作 DARKWELL 当前 surface store。

Apartment 遮挡也是显式代理：墙/门以及三条高家具中心线；`DarkwellApartmentLab.cpp:109`。普通 mesh 碰撞、渲染阴影不会自动注册为合法遮挡。低桌与柜体也没有由完整实体自动生成的观察遮挡。

## 哪些正确，哪些无法表达

**在现有二维代理合同内正确：**角色 XY/朝向主导视野；远处仅有照明不足以观察；Legal Illumination capability、Bypass、墙门遮挡、height mismatch、suppression；Hard 接受的支撑才写 Memory；Clear/Block/解除不恢复；历史多平面不能被低层空证据误清。平坦、无遮挡、处于合法带内的低表面可获得期望结果，但这不是“低表面真实可见性”已实现的证明。

仓库 `Docs/Evidence/SIGHTWEAVE_HARD_AUTHORITY_20260909/probes.csv:5` 起的四行记录，同一 XY、Z=75/140/240/290 都得到 Hard、Object、Static、P4 为 1。对应测试 `DarkwellSightWeaveAdapterTests.cpp:247` 明确写着无顶面法向/观察高度检查。这是当前语义的已存证据，不是本轮新执行，也不是四块实体表面的实景测试。

**无法正确表达：**

1. 看到柜体侧面但不知道高顶面；同一 XY 顶面、底面及不同朝向侧面独立记忆。
2. 眼点看向低桌面的自遮挡、柜后表面的部分遮挡、越过矮障碍的可见区。仅判断 `target.Z < eye.Z` 不足以判断这些关系；高于眼点的正对侧面也可能可见。
3. 真实有限高度墙的斜射线遮挡。旧 solver 将高度范围有重叠的段用于整个二维 source polygon，可能过度遮挡；未注册为代理的实体又可能漏挡。
4. 在 Whole 确认后仍保证“未观察顶面没有表面知识”。当前全几何 capture 本身没有这一表达维度，不能仅靠材质补丁解决。
5. 仅移动摄像机不会改变 Runtime Hard 点结果，但顶视图会显露上述投影缺陷。原因不是当前 Runtime 从 camera depth 回写知识。

## 推荐 production 模型

### 1. 有限表面片，不做全网格可见性

每个可观察对象声明少量稳定表面片：世界/局部平面、二维域、多边形边界、几何外法向、SurfaceId、所属 geometry/content revision。矩形柜体可由 box 代理生成顶面及各侧面；墙面用沿墙距离 × 高度的局部坐标；地面继续 XY。有斜面需要时增加少量 authored 平面；曲面采用显式近似，超出支持范围不得默默退回整物体知识。

表面片内部保留空间细分，不能一个点通过就整面记忆。沿用当前支撑精度作为首轮验收基准。静态存储按不可变 surface domain/page 共享，不需要给每面墙创建 Actor/epoch/proxy；动态表面片挂现有 primitive/epoch。

眼点由角色 C++ 提供世界位置及 revision，默认可先保持现有站立位置；52/140 只作为初始参数依据。灯源位置与眼点分开表达，不能默认永远共用同一高度。观察范围/水平朝向继续原设计，不需要引入摄像机视锥或强制拟真垂直 FOV。

### 2. 同一 Runtime 裁决，替换不适用的几何谓词

表面样本 P、外法向 N、观察源 E：在该 source 的贡献内部检查正面性 `dot(N, E-P) > epsilon`，再检查 E→P 的代理可达性；随后沿原 source-compatible light 关系完成合法照明、Bypass 和 suppression。水平顶面自然得到眼点在其上方才可观察的规则，侧面不受统一高度上限误杀。使用几何法向，不用 normal map。

近身圆保留 120cm、全向及 BypassLegalIllumination 的现有特权；Bypass 仍只绕过照明，不扩大成忽略背面/实体遮挡。这样可保持近身实际可达表面可见，同时不靠近身圆泄露高柜顶面。

表面朝向/遮挡必须在逐 source 判断中完成，不能先 OR 所有 source 再任选眼点后过滤。照明几何也应接收同一个目标样本及灯源位置；首轮不需要引入物理照度、PBR 或渲染阴影回读。

**不能实现成旧 Hard(P) AND 新 raycast(P)。**旧二维 polygon 可能已经把越过矮柜的合法样本拒绝，新增 gate 无法恢复。Runtime 内必须将受影响的二维遮挡谓词替换为支持有限高度的代理几何；旧多边形只有经证明等价时才可作精确快速路径，不能无条件作为拒绝 broad phase。PossibleSupport、区域证明及缓存也必须一起审视，不能只改最终点查询。

### 3. 少量实体代理与按需批处理

遮挡用挤出 footprint/OBB/少量凸体；墙的现有 XY segment + Z 区间可保留为薄墙特例。对挤出实体，在 XY 相交参数区间内检查 `z(t)=Ez+t(Pz-Ez)` 是否进入实体高度区间。处理实体入口、出口、目标端点接触容差及自遮挡；不能把目标所属整个物体排除，否则远侧面/凹结构会漏挡。

先用现有空间索引的 XY/floor broad phase 筛候选，再按需精确检查。批量表面区域证明由同一 Runtime 几何实现提供；大块明确区域共享结果，边界才细分。可按源/代理/表面 revision 缓存或生成表面域投影遮挡，不应每个 fine sample 调 UE 世界碰撞 trace。

两点必须分开：表面片是可观察接收域，遮挡代理是实体。只有法向没有实体不够；只有一个标量高度图也表达不了墙侧面和同 XY 堆叠的地面/桌面。

### 4. Memory 和 Presentation 同时闭合

表面 Knowledge 按稳定表面身份/局部坐标保存，不按“当前眼高切出的一层”保存。离开后灰色来自曾观察的表面证据，不能用当前眼点的正面性再次抹掉历史事实。Clear/Block 仍在原事务入口，把既有世界 XY 区域映射到相关表面样本，保留解除不恢复和重观察合同。

动态物体继续 last-observed pose/epoch；不可见移动不得更新旧表面姿态。看不到旧位置不等于旧表面已不存在。旧空间 verified-empty/占用反证不能直接用某个面背向观察者来成立。

Whole 建议保留“物体识别/整物体呈现资格”，但与“已观察表面事实”分开；Whole 确认不得批量写未观察表面。若要求最终图像也不能显露未知顶面，则 Whole 表现须受表面 Unknown mask 限制。这是既有 Whole 全几何合同的显式边界调整，应先验收，不能伪称完全无行为变化。近身、Legal Illumination、Clear/Block 和历史事实保持不变。

P4 消费 Runtime 发布的 surface-domain hard 结果和对应历史 bit，AA 只影响显示。通用 XY×height atlas 保留给平面域；不要让材质自行计算第二套合法观察。生产上线时 Object/Static/P4 必须切到同一发布 revision，不能长期一条链有新规则、另一条链仍按旧平面写记忆。

## 性能和未来边界

- 真正的视点遮挡随目标高度、眼点位置连续改变；旧“相同 HeightRange 谓词 ⇒ 平面等价”的缓存前提不再一般成立。把眼高每帧加入 HeightCuts 会导致 persistent layers 只拆不并、COW 和 atlas 放大，禁止作为方案。
- 现有 Hard atlas 要求 `(H+2)*N <= 16384`；Static 有页表/atlas 容量限制。复制全世界纹理到每个面/眼高不可扩展。表面页按实际 domain/residency 管理，不能全场景逐面逐帧扫描。
- CPU 工作量风险是“未知支撑数 × 灯源候选 × 遮挡候选”，尤其门移动、眼点高度变化、长墙、很多局部光与历史更新叠加时。保留空间候选、脏区、区域证明、相同 revision 共享；预算不足不写伪知识，也不把未查询当空证据。若延期更新，要测出 Live 延迟并明确验收，不得靠降精度隐藏成本。
- 既有统一报告的同机 GT 平均 16.61ms、P99 29.84ms 说明余量有限；这是旧报告，不是新方案性能预测。新方案须同机同协议测 GT P95/P99、查询数、dirty pages、upload bytes、驻留增长，不只看平均 FPS。
- SurfaceId/geometry revision 可为未来楼板上下表面及洞口局部变更提供边界；观察/光传播连通关系仍是另一接口。当前 FloorId 继续是 scope/索引约束，不自动开放跨楼层。未来破洞会使该建筑不再满足现有不可变注册合同，需要几何失效与知识迁移，不能仅删除 blocker 或扩大 HeightRange。本轮不实现楼板/洞口/跨层传播。

## 施工阶段和第一刀

1. **先固定 receiver 合同与反例 oracle。**补 SurfaceId/局部域/眼点语义设计和有界 C++ 测试夹具；独立解析 box/prism 相交作为测试 oracle。覆盖同 XY 不同面、高顶/低顶、近身高顶拒绝、可见高侧面、越过矮柜、目标自遮挡、旋转37°、眼高变化、compatible light/Bypass/suppression、Whole 与表面事实分离。现有 surface_height 测试继续标为旧点场事实，不冒充 surface acceptance。
2. **Runtime 最小闭环。**一种 box surface domain + 有限高度代理，同一 EffectiveLive point/batch/proof 出口；实现眼点/几何 revision 和严格快速路径条件。验证候选上界与区域证明差分，再扩复杂形状。可保留未启用的旧路径做对照，禁止两套生产写入 authority 并存。
3. **完整垂直切片。**一张低桌、一只高柜、一面墙，贯通 Object、Static、P4、Whole、历史反证及 Clear/Block。C++ 变更后运行 `Scripts/BuildEditor.ps1` 完整 DarkwellEditor Win64 Development，再运行相关自动测试和 D3D12 表面读回/画面验证。
4. **扩展与性能验收。**转换 Apartment 的显式代理、测试脏区/门/多光/历史组合压力，做同机 benchmark、资源上限和长时间增长检查；通过后才推广到更多资产类型。资产接入使用 Unreal API。

**下一步第一刀不是设置全局 140cm 上限，也不是改 shader。应先提交“有限表面 receiver 合同 + 解析反例测试夹具”，特别固定 Whole 身份知识与表面知识的边界。**它能在不大范围重构的情况下证明所选模型确实表达产品意图，防止先修画面、最后才发现两类 Memory 数据模型无法承载。

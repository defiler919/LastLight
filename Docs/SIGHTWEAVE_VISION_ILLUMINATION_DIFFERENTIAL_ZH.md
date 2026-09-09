# Vision × Legal Illumination 跨路径差分（2026-09-09）

后续现有语义收口与高度分层见 [Hard EffectiveLive 下游收口](SIGHTWEAVE_HARD_EFFECTIVE_LIVE_UNIFICATION_ZH.md)。
本文件及其 Evidence 保留原始差分阶段的事实，不回写为当时已经通过正式验收。

基线：04b01b02cf180ae340795fa369b9c1a367c8ce12；稳定 tag
`stable/sightweave-pre-formal-vision-illumination-20260909` 未移动。
本切片只新增 Development automation 和证据，不修改业务规则、材质或地图。

## 测量边界

Selector：`Darkwell.SightWeave.Differential.VisionIllumination`。
使用现有真实 Adapter / IntegrationFixture / Character，玩家位置 (-650,0,92)，
Adapter 观察源 Z=144（角色 Z+52），不取顶视摄像机位置。
26 个确定性探针比较同一发布 revision 的 Runtime Hard EffectiveLive、P4 CPU Coverage、
生产 FDarkwellCurrentLiveGrid 的存储查询、生产 FDarkwellStaticKnowledge 的 Known bit、
真实 D3D12 P4 R16F texel readback。

这不是完整 ObjectMemoryScene historical proxy / Whole 确认 / 最终屏幕材质验收。
Object 测试对象是 10×10cm 的 Partial 平面支撑，直接使用现行生产存储算法；
Static 声明小区域（内部仍按生产 tile 更新）。每个探针使用新存储，排除旧灰干扰。
GPU 列是包含探针的真实 texel 值，不是最终材质双线性结果，也不是精确世界点硬判定。
额外记录 Static 的 0.625cm fine-cell 五点硬支撑、Object 实际 local-cell 五点硬支撑与
生产最小 Coverage，避免把不同 footprint 或 GPU texel 中心误认为同一空间点。

自动测试 assert 有效查询、revision 一致、读回成功、行数、近身圆/合法内点真正存储、
无光/光照独存/关闭/注销不写入。其他差分原样输出，不将错误行为写成 expected PASS。
**测试执行通过仅代表测量可靠完成，不代表 Vision × Illumination 产品验收通过。**

## 结果与归因

- 近身圆在黑暗中 Hard/两类存储/P4 均为 1；合法近身观察可以建立灰。
- 无光 Cone、光照独存、光源关闭及注销，相关内点均为 0；复开后新观察均为 1。
  光照独存探针另外 assert QueryLegalIllumination 为真，避免用“根本没有光”假通过。
- 同层同高度合法内点、既有墙/门段遮挡路线的三个点，各路径一致。
- **确定越权差分**：光源 HeightRange=[200,290]，目标 Z=92。
  Hard 点及两套硬支撑均为 0，但 Object 与 Static 实际存储均为 1，CPU/GPU Coverage=1。
  Adapter BuildFogVisualSourceSnapshot 只复制光源 XY/方向/范围/角度；高度带在转换中丢失。
  Revision 已同步变化，故不是旧 revision 或旧存储复活。
- **合法区域收缩**：光源圆心 X=750、半径300，在 X=450.25 和451，
  Hard 点及两套完整硬支撑均为1，但两类存储为0；CPU 点 Coverage 分别 .6/.9。
  SignedLinearCoverage = clamp(.5+signedDistance/2.5)，存储再要求 >=.99。
  这使 AA 过渡侵入合法写入条件。X=450 的 Object 五点硬支撑为0，不能用该点单独证明 Object 错误。
- 视锥近边界 (-350,380)：点及两套硬支撑均合法，Object 为0、Static为1。
  Object 的完整 footprint 更大且使用最小软 Coverage；实测最小值为0.865076。
  必须按实际支撑比对，不应扩大 reveal 或降低阈值来掩盖。
- GPU 在 X=450附近读到 .998535，CPU 点为 .5/.6/.9，不能直接称为 GPU 越权：
  texel中心与点坐标、硬判定与AA权重是不同量。本次没有完整最终材质的越权像素验收。

## 一致但仍不满足新产品合同

1. 目标 Z=75/140/240/290，各路径都接受。当前 Hard 是高度带内的 XY 点多边形查询，
   没有输入表面法向、顶面/侧面或观察者到表面三维可见关系。
   因而这不能证明“高柜/高墙顶面不应因顶视摄像机获知”已经实现。
   本测试用不同 Z 的同一空间探针暴露数据表达限制，没有伪造完整高柜人工画面。
2. 独立 FloorId 的光即使未提供楼板也全部不贡献。
   Runtime 只允许一个 query-active floor；测试将另一层登记为非 query-active，并保留光源启用。
   兼容映射仍硬要求同 FloorId，与本轮“光照不能按楼层编号硬隔离”的合同不同。
3. 水平楼板及开口传播：当前源/遮挡接口是 floor+height-band 的 XY 线段/多边形，
   不能表达射线穿过水平楼板开口的三维事实。本轮明确标为 **unsupported / 未运行物理楼板验收**，
   不把二维墙段当楼板、不添加会被忽略的 mesh 后声称通过。

Runtime Hard 是本轮跨路径数值基准，但其现有高度/楼层模型不是新产品合同的最终正确答案。
正式阶段须先补齐这些空间事实的权威表达，再让消费者统一使用它。

## 测试与证据

命令：`Scripts/BuildEditor.ps1`；`Scripts/RunUnknownPartialCutTests.ps1 -RunName VisionDifferential_20260909_final -Tests 'Darkwell.SightWeave.Differential.VisionIllumination+Darkwell.SightWeave.Closure.VisionIlluminationBoundary+SightWeave.M2.Query.EffectiveLive'`。

完整 Editor Development Build 成功；最终 D3D12/SM6 7/7 执行通过、0测试警告、0 severe。
26行差分测量完成不等于26场景产品语义通过。工具链版本和既有弃用警告未作为产品错误处理。
本地完整报告位于 Saved/GrayObjectPolicy；版本化 CSV/summary 位于 Docs/Evidence/VisionIlluminationDifferential_20260909。
初次运行 6成功/1失败：第二个 query-active floor 注册被拒，是测试夹具错误；
修正为非 query-active 后执行通过，初次 summary 保留。未修改业务逻辑获得通过。

## 下一阶段与性能机会

先定义表面合法观察和不按 FloorId 隔离的传播合同，补上楼板/开口权威测试；
同时把已明确的高度带和硬边界差分作为消费者收口的验收条件。
不要仅用当前二维 Hard 查询替换所有调用并宣告三维合同完成。

现有 Runtime 求遮挡多边形后，P4又按光源/线段求覆盖，Object/Static还反复查询同一覆盖。
存在共用 immutable revision、硬覆盖批查询/区域证明/空间缓存的机会。
这是代码路径确认的重复工作机会，本轮不提供未测量的毫秒收益，未实施优化。

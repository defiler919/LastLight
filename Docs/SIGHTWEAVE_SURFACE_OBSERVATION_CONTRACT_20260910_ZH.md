# Surface Observation 产品合同与反例

状态：产品合同冻结；生产 Surface Receiver 尚未实现。本切片不改变 Runtime、Memory、P4 或 Unreal 资产。
基线：`3447970d799a09080e5bdb91255ed07b4a881bbd`，stable tag `stable/sightweave-hard-effective-live-unified-20260910`。

## 冻结合同

1. **身份与域。**物体提供少量稳定的表面身份及局部接收域，包含几何朝向、变换和 content/geometry revision。知识键至少能区分 scope、物体/静态域、表面、域内样本；动态历史还要绑定捕获 epoch/pose。相同 XY，甚至同 XYZ 的相反朝向表面，不是同一知识。一个点合法不得解锁整面。
2. **世界眼点。**角色 C++ 提供世界空间观察点；相机不参与授权。140cm 只是当前站立参考，没有全局目标高度上限。水平观察方向/范围保持现有设计，不因引入表面而强制增加垂直 FOV。
3. **表面几何。**有限表面片的正面朝向观察点，且观察线段未穿过自身或外部实体，才有几何支撑。几何法向不用 normal map。同平面掠视不建立正面证据；接触终点不算穿过自身，不能排除目标所属整个实体。容差必须明确且与单位有关，不能用于扩大可观察域。
4. **单一权威。**生产观察仍由 SightWeave Runtime 原 EffectiveLive 链裁决。在每个 source 贡献内部完成方向、表面及遮挡判定，再与其 compatible illumination/Bypass 合并，最后遵守 suppression；不能先混合不同 source 再借用另一个眼点。灯源到接收表面的有限高度遮挡属于同一链，首轮不要求物理照度/PBR。
5. **近身。**保留 120cm 全向、黑暗下合法照明 Bypass；它只绕过照明，不绕过表面背向、自遮挡、外部遮挡或 suppression。近身高顶不能泄露；可达低表面不能因无灯而退化。
6. **表面记忆。**只凭 Runtime 发布的合法表面支撑写入。眼点移开、表面暂时背向、光灭，都不能删除已观察事实；隐藏移动不得刷新历史姿态。未看到一个表面不能作为它已不存在的反证。
7. **Whole 分离。**Whole 识别的是物体身份/整体识别事实，不是“所有表面均已观察”。它不填充 Surface Known bits，不绕过 surface admission。严格表面表现下，Whole 可保持整物体识别资格，但未知顶面仍使用 Unknown 表现，不得渲染成已观察/记忆细节。既有 full-geometry capture 必须在下一阶段显式适配，这不是声称旧 Whole 图像完全不变。
8. **Clear/Block。**沿用现有事务和世界区域语义映射到表面样本；Clear 清知识，Block 阻止写入但不关闭合法 Live；解除不恢复旧 bit，重新合法观察才写回。表面清理不凭空制造 Whole 识别，Whole 自身遗忘沿用其既有策略。
9. **范围。**有限平面片 + box/prism 简化实体即可开启最小闭环。无体素、逐三角形、摄像机深度回写、第二套生产可见性服务。本切片不实现跨楼层/洞口传播。

## 测试层次：不得混淆绿色结果

测试在 `Source/Darkwell/Private/Tests/DarkwellSurfaceObservationTests.cpp`，独立 helper 为同目录 `DarkwellSurfaceObservationFixture.h`，全部受 `WITH_DEV_AUTOMATION_TESTS` 保护，无生产调用。

- `Spec.Geometry`：独立解析线段/盒体 slab 相交与有限平面五点支撑。每条断言给出确定的几何结果。测试自己的实体端点、内部穿越、部分遮挡、刚性37°变换，避免只测“高度小于眼点”。这是未来 Runtime 差分 oracle，不是另一套上线算法。
- `Spec.Knowledge`：单物体/单 content revision、每面一个支撑 cell 的参考 ledger，验证 Whole 与 surface 分离、绕行累积、旧面保留、Clear/Block/解除/重观察。它不替代生产 Memory 验收，不实现完整 persistence/epoch/region 索引。
- `CurrentModel.KnownGaps`：调用真正 Runtime 和 Static CPU store，明确断言现有错误授权和错误拒绝。绿色表示成功复现缺口，**绝不是 Surface Observation 已完成**；下一阶段应将相应断言迁移为 receiver acceptance，不能永久保留旧错误作为产品要求。
- `CurrentModel.PolicyAndMemory`：真实 Runtime directional vision、compatible/incompatible illumination、body bypass、suppression 和真实 Static Clear/Block；锁住当前可保留合同。新表面系统尚未接入，因此这里只证明旧合同没被本轮改动。

五点是本夹具的小面积支撑样本，不宣称对任意夹在样本之间的薄遮挡构成全域证明。未来生产区域加速必须提供严格保守证明或按既有密度细分，不能直接把此 helper 当整面判定。

## 最小场景与确定结果

单位 cm。默认眼点 (-200,0,140)，柜体 bounds [-40,-30,0]..[40,30,200]；front x=-40，side y=30；顶面中心 Z=200；低桌同 footprint、高75。每个 receiver 用中心与 U/V 各10cm 的四角支撑。

| 反例 | 产品/解析结果 | 当前系统证据 |
|---|---|---|
| 低桌顶75 | 可见 | 当前点场也可接受，但没有表面语义 |
| 高柜正面100、170 | 可见，即使高于140 | 当前点场可接受；不能据此等同于侧面已实现 |
| 高柜顶200、眼140 | 拒绝 | 真实 Runtime 仍给 memory eligibility，真实 Static 高层也写入 |
| 眼点升到260，同顶面 | 从不可见变可见 | 真实 Runtime 前后都为 true，没有表达这个转变 |
| 绕到 (0,200,140) | side 新获得知识，front 旧知识保留 | 当前 Runtime 绕行前已授权 side 的点 |
| 同 XY 顶75/200 | 一个可见、一个不可见（独立实体场景） | 真实 Static 两个高度都写入 |
| 同 XYZ 相反面 | 只有朝向眼点的一面合法 | 当前 point/store 无 receiver 参数，无法区分 |
| Whole 先确认 | 不新增任何 surface fact | 参考 ledger 验证；旧 Whole full geometry 捕获仍须改造，本轮没有伪造集成通过 |
| 越过 x=-120..-100、高80障碍看低顶 | 斜射线可越过 | 对应旧 XY segment + [0,80] 与 source band 重叠导致 Runtime 拒绝 |
| 同障碍升至180、目标自遮挡、局部遮挡 | 必须拒绝相应支撑 | 解析 oracle 覆盖，未宣称旧 Runtime 已支持 |

## 下一阶段最小 Runtime Surface Receiver 闭环

先一种 box 接收域：注册稳定 surface identity、局部坐标与几何 revision；角色/光源分别提供真实 origin；在 Runtime 同一裁决内支持 source→receiver 及 light→receiver 的有限实体几何。点/批量/区域证明共享实现与 revision，Object/Static/P4 只消费结果。

不能用 `旧 Hard(P) && 新几何检查(P)` 当通解：本轮越矮柜反例证明旧 polygon 的 false 会误杀合法表面。同步检查 PossibleSupport、候选剔除、height-class 等价证明和缓存 key；二维路径只在可证明等价时保留。一个低桌/高柜的垂直切片必须含真实 Memory 与 Whole 适配及 P4 Unknown 遮罩，才能称闭环完成。

性能边界：按源/空间索引筛选少量代理，批量域证明、边界细分、revision 脏区；禁用全场景逐物体/逐fine样本世界 trace。眼高变化不是新增永久 height slice 的理由；连续 Z 会破坏旧分层等价前提并放大 COW/atlas。测查询数、未知支撑、灯与遮挡候选、P95/P99、页上传与长期驻留。本轮无性能改善或生产容量承诺。

## 验证记录

执行结果和可复现命令见 `Docs/Evidence/SIGHTWEAVE_SURFACE_OBSERVATION_20260910/RESULTS.md`。未修改 `.uasset/.umap`，未更改 stable tag。

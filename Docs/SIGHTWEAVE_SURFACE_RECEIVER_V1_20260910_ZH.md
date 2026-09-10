# SightWeave Surface Receiver V1 — Runtime 闭环

基线 `a3ac3d955a9f05b3ee5cfdd5659425e7fc2d33e1`。本片开始实现上一轮的表面合同；未迁移 ObjectMemory、Static Knowledge 和 P4。旧点场不能冒充表面知识，但在迁移前保持原消费合同。

## 最终接口与职责

- `SightWeaveSurface.h`：`FSightWeaveSurfaceBox` 显式提供 world-unique Id、Floor、刚性 Pose、HalfExtent；六个面有稳定 native face 编号，样本是 `(Receiver, Face, UV[-1,1])`。非单位缩放拒绝，尺寸通过 HalfExtent 提供；支持刚性旋转，不扫描 mesh、碰撞体或三角形。
- `USightWeaveWorldSubsystem::Register/Update/UnregisterSurfaceBox` 管理 receiver 同时也是不透明遮挡实体。V1 没有单独透明材质规则或 blocker-only 类型。更新尺寸/pose/floor 或重新注册产生新的 ReceiverRevision；重用名称不重用注册 lifetime。显式 owner 可以由 `UnregisterAllForOwner` 清理，接入组件须在 EndPlay 调用；不依靠弱引用 GC 自动改变权威。
- `QuerySurfaceSample/QuerySurfaceSamples` 查询当前不可变发布帧，返回稳定样本身份、世界点、receiver revision 和原 Hard result（含 scope、snapshot revision、source/light attribution、eligibility）。只计算事实，不写 Memory、不做 Whole 解锁。消失的 receiver、非法 UV/face/owner/floor 明确拒绝。
- Surface 查询最终调用**同一个 `QueryEffectiveLiveValidated`**。新增的是接收表面的几何上下文；复用原 source-owner-floor-height、compatible illumination、Bypass、suppression 和最终 eligibility 逻辑，不在 DARKWELL 或材质重写合法算法。旧 point API 仍使用旧 polygon 几何，新 receiver 使用其实际几何域，二者是同一裁决的不同目标语义，不是两套消费者可任选的 surface 权威。
- 旧 HardCoverage/PossibleSupport/height equivalence 没有应用到新表面查询。新几何不能先经过旧 polygon 的 false 剔除，否则越过矮障碍反例会失败。旧区域证明也不得拿来证明新 surface domain。

## 真实空间判定

每个 source 分别检查水平 nominal shape/range、接收面几何法向 `dot(N,E-P)>0.0001cm`、E→P 的有限实体遮挡。灯源按它自己的 origin、nominal shape/height、接收面正面性及有限遮挡判定，再沿原 compatible source 索引合并。近身 radial 的 illumination bypass 不能绕过面朝向、实体或 suppression。Source Direction 从 source Transform 消费，V1 保持现有水平观察角，不引入垂直 FOV。

OBB 在局部做解析 slab 相交，包含自身实体；终点接触容差为 0.0001cm，不把目标实体整体排除。现有已注册 XY segment + 高度区间作为有限竖直墙矩形，按射线相交参数的实际 Z 判定，含 coplanar/vertical 分支。既有墙 segment 和新 box 不相互转换；同一物理家具迁移时避免遗留重复代理，尤其原中心线不能替代新实体外表面。

**未声称任意场景精确几何**：简化 OBB/墙就是当前产品代理；极薄/透明/复杂凹结构、光强衰减不在 V1。图形法线和相机不参与合法写入。

## Observer Pose 接入

`UDarkwellObserverComponent` 是角色默认 C++ component，无 Tick。默认 world eye = capsule feet + `StandingHeightCm`，配置默认 **162cm**；SightWeave 无眼高常量。

组件支持动态 `SetObserverWorldPose` 和独立 `SetObserverWorldDirection`，以及显式清除 override。默认驱动跟随角色，方向 override 为绝对世界方向，不被 Body Yaw 覆盖；姿态 override 后方向 override 优先。默认眼点跟随角色脚底升降。未来蹲伏/翻滚/骨骼驱动可每帧提供 pose，V1 不实现这些玩法。相机没有参与接口。

Adapter 用 observer pose 更新 body/cone source，火炬仍沿原 actor+52cm 的独立驱动。新增 `UpdateSourceGroupPoses` 原子更新两套姿态，只发布一个 revision；旧同姿态 API 转发到此入口。避免分开更新导致每次移动发布两帧。D3D12 差分测试包含真实 Adapter 姿态 override、独立37°方向、灯源不跟随眼点覆盖及默认恢复。

## 缓存、批量与性能边界

- Snapshot 持有不可变 `SurfaceScene`，BVH 对有限盒体及墙段 AABB 建索引。眼点、方向、灯源、suppression 改变只换权威帧，复用几何 scene；盒体或墙段变更才重建。没有任何 receiver 时不分配 scene/BVH。
- Caller-owned cache 绑定不可变 frame 对象和 owner，最多4096个精确样本；任一发布帧改变即清理，拒绝旧眼点/旧光/旧几何结果。性能计数区分 exact sample、cache hit、BVH nodes 和 primitive tests。缓存不跨世界；不要长期保留很多旧 cache 导致旧 scene 驻留。
- Batch 同步读取同一帧，但**不声称整域证明**。局部 UV 域和 immutable scene 为后续 conservative region proof 提供接口基础；现在五个样本只能证明五个样本，不能自动填整面或整 cell。下一阶段需要在同一几何实现上加入保守区域加速。
- 注册/几何更新当前重建 BVH，批量注册没有 transaction：大批动态盒体逐个更新会放大构建和发布成本。V1 面向少量显式 receiver；移动主体多、密集墙段、多光下要加入批量几何提交/局部 refit，并量测。禁止用降精度或全场景逐样本物理 trace 作为捷径。
- 测量区分冷批量、同帧缓存与眼点变化后的批量，另列 source publication 成本。它是 Runtime microbenchmark，不是全 Apartment GT/GPU 帧率承诺。

## 哪些完成、哪些继续保留缺口

RuntimeV1 的真查询已覆盖低顶/高顶、升高眼点、绕行侧面、六面独立结果、有限矮墙/实体、高障碍、自遮挡、只遮住灯源的外部实体、方向/多source/兼容性/Bypass/suppression、旋转姿态、注册/撤销/owner清理和cache失效。解析 oracle 与真实 API 对照，不以 expected failure 代替 acceptance。

`LegacyConsumers.KnownGaps` 继续记录旧 XY point/Static 的错误表面推论，因为本轮明确不迁移这些消费者。Whole 身份知识与 surface result 无调用联系，新 API 没有 Whole 参数；旧 full-geometry history 和图像遮罩仍需下一阶段处理。本片不把 surface eligibility 自动写入旧投影 store，不宣称 Clear/Block 已有 surface-aware 存储实现。

下一阶段最小纵向接入：一个低桌/高柜接入稳定 surface sample/domain，Whole 识别与 surface bits 分开，Static/domain 与 Object epoch 对应各自稳定身份，P4 只消费同一发布 revision 的表面结果。维持旧区域事务/历史空证据规则，并补 GPU/Memory 新表面验收。楼板/洞口/跨层传播继续独立。

构建、测试、性能数据和失败迭代记录见 `Docs/Evidence/SIGHTWEAVE_SURFACE_V1_20260910/RESULTS.md`。

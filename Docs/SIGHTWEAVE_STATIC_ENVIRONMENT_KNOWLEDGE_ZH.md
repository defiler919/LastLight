# Static Environment Knowledge：第一个 production 架构切片

基于 `b1fb4ee6ac644fe4dd28779b07e0a1635c148bd1`。Apartment 不可变建筑退出 ObjectMemory；既有 stable tag 不变。本片不是剩余 ObjectMemory 的性能重构，也不是人工 gameplay 验收。

## 静态与动态边界

`ADarkwellApartmentLab::Box(..., Immutable=true)` 显式注册 `UDarkwellStaticEnvironmentSubsystem`。16 块地板、10 面静态墙、3 个过梁共 29 个 mesh 使用共享空间知识；这些 mesh 没有 Rememberable component、object epoch、historical proxy、cap 或 current texture。原 mesh 的碰撞和几何保留。

14 个门/家具对象继续使用原 ObjectMemory：三扇可开关门及可变化家具、Whole 茶几/餐桌/床头柜/开关、SpatialPartial 家具和 37° 衣柜。不是根据 UE Mobility 自动判断不可变性；以后可能移动、破坏、替换、独立遗忘的建筑不能注册到此路径。没有修改 ObjectMemory history index/current texture/cap 实现。

## 空间知识与 authority

- 世界 XY 网格：2.5 cm cell，每格保留 4×4 fine support，fine spacing 0.625 cm。80×80 cm tile 含 128×128 个 bit。负坐标采用 floor；Clear/Block 使用现有 `DarkwellMemoryRegionSamples` 的半开区间和 fine 中心归属。
- 注册仅声明可用空间 tile，不写知识。第一次合法观察才分配 tile bitset。已知 bit 在离开视野后保留，Clear 才清零；Block 不允许再写，解除不会恢复旧 bit。
- `UDarkwellFogVisualSubsystem` 发布的同一 source、16 条 occluder segments 与 coverage draw revision 驱动更新，不改变 P4 Vision / Legal Illumination。只有新 coverage revision 才处理当前 draw rect 的候选 tile；完全已知 tile 跳过，只有变化 tile 上传。没有每帧遍历所有静态 actor 或全地图 fine samples。
- 边界 sample 调用原 `QuerySourceCoverage`，以四角＋中心均合法（coverage ≥ .99）为证据。大块内部复用原 `TryUniformCoverage`；额外的全遮挡证明要求同一线段阻挡矩形四角，对 body/cone 分别成立。不把四个不同遮挡物的角点结果当作内部证明。自动测试对 0°/37°/90° 与逐 fine oracle 比较。
- 公寓较小且视锥范围较大，当前 draw AABB 有时覆盖全部 224 个已声明 tile；这是视野范围内的哈希候选查询，不能把它报告成只访问十几个 tile。`touched_tiles` 指实际测试未知 fine 样本的 tile，另有 `candidate_tiles`、`queries`、`written_samples`。

## Presentation 与事务

原不可变 mesh 使用 `M_DarkwellStaticKnowledge`：共享 sparse hash 页表和 atlas 提供 Remembered bit，Current Live 直接使用原 P4 raw field；未知处为黑，记忆使用 Lab 中性灰色/法线明暗。原碰撞和深度保留。没有扩大 bounds、视野、sample 或关闭 AA。

页表为 4096 entries（64 KiB）；atlas 初始 2048² BGRA8（16 MiB），每 tile 一页，128² texels。只上传 changed tiles；容量增长时才重建 atlas 并重传已有页。没有每帧 resource recreate、同步 readback 或 FlushRenderingCommands。页表探测长度取实际最大链长，CPU/材质一致。

`UDarkwellMemoryRegionSubsystem` 在原 Clear/Block/Release 事务中调用静态 subsystem；Trigger/EventAdapter/F 仍只调用现有 Region API。CPU Clear 当次调用完成，GPU tile 更新同帧 enqueue；没有增加 Knowledge 成立延迟。Live 不受 Block 影响。World teardown 清理 subsystem 的知识和资源。

## 本片明确限制

共享知识采用现有单层 XY 语义，首个有效 Runtime exploration scope 被绑定；其它 scope fail closed，不跨层共享记忆。这不是多楼层或 World Partition streaming 实现。不可变 mesh 的取消注册、编辑运行时几何、页驱逐和多 scope residency 不是本片接口。

CPU sparse map 随实际探索区域增长；GPU 第一版最多 4096 resident tiles（约 2621 m² tile 面积，最大 8192² atlas），没有冒充无限地图支持。超限在所有 build 中明确报错并关闭该 presentation，CPU 知识保留。未来应增加 residency/streaming，而不是扩大 authority cell 或丢弃已知 bit。

## 可重复验证

`Scripts/BuildEditor.ps1` 完整 Editor Development build。

`Darkwell.SightWeave.StaticKnowledge` 三项 C++ 自动测试覆盖 Unknown/Clear/Block/reobserve、20 次重复操作、遮挡/旋转 oracle、负坐标、四条边界/角点、sparse 和已知 tile 无重采样。

`Content/Python/verify_static_knowledge_apartment.py` 在真实 D3D12 PIE 中脚本定位角色、调用原门/Trigger 接口，检查关门遮挡、开门观察、墙后 Unknown、Blocked Live、解除不恢复、重新观察、20 次重复及 Active teardown；截图异步完成后才进入下一阶段。它不是人工 WASD/F 验收。

`Scripts/RunApartmentSightWeaveBenchmark.ps1 -RunName <唯一名称>`：D3D12 1280×720、100% screen percentage、现有 TSR、30 秒预热＋30 秒连续真实 WASD/转向；走原 EnhancedInput/CharacterMovement，保留碰撞/速度。`-Reference` 在同一构建恢复原 43 ObjectMemory 架构，不改变原始 tag/源码；`-Still` 是静止对照。所有诊断默认关闭且不进入 Shipping/Test。测量记录实际位移、转角、前台有效性、GT/RT/RHI/GPU 与两路 telemetry。原始数据在 `Saved/StaticKnowledge/<RunName>`。

最终数据与结果见 `Evidence/SIGHTWEAVE_STATIC_KNOWLEDGE_20260909/RESULTS.md`。

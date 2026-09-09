# Historical Coverage 遮挡退化修复

基线 `501a18c5ea6f838f9a352058971c7afa622a87b7`。本片只处理 Apartment 朝大量 Gray 时的 CPU 退化，没有修门、材质或其它视觉 Bug；stable tag 不变。

## 已隔离的根因

`UpdateTracked → AdvanceFineHistory → SampleConservativeCoverage → QueryCanonicalCoverageRaster` 在 draw revision 变化时仍会请求历史 raster。原 `TryUniformCoverage` 能证明视锥外的整块零覆盖和无遮挡的整块满覆盖，却不能证明视锥内、完全位于墙后的一整块零覆盖。这样的区域递归到 fine 叶子，逐 sample 查询四角和中心；即使最终全为零，仍做大量点查询。

固定厨房横移、朝向 Gray 的修复前配对结果：Historical 平均 100.56 ms，Coverage 平均 109.49 ms，约 830348 次查询／帧。反向少历史方向 Historical 1.02 ms。两组每步玩家及发布的视野源 XY 一致，排除了不同移动路线造成的数量级差异。

## 修复与正确性

在共享 CPU uniform builder 中增加共同阴影证明：**同一条**遮挡线段必须阻挡矩形四角，对合法 body/cone 分别证明不可达。单线段后方的阴影是凸区域，四角成立即整个矩形成立。使用原 `IsBlockedBySegments` 的交点条件，不新增 epsilon、bounds 扩张、采样降级或缓存旧视野。四角被不同墙挡住不成立，窄门洞继续逐点求解。

无法证明的叶子仍使用原坐标和精度；五点最小值一旦为零，利用 coverage 非负性提前结束剩余点计算。这与完整五点最小值完全相同，不延迟任何 Knowledge。

没有改变 Local/world 映射、2.5 cm cell／4×4 fine support、P4 shader、Legal Illumination、Memory、Clear/Block、Whole/Partial、ownership、cap、历史索引或 resource 生命周期。新增自动测试包含旋转、窄缝、body/cone 来源不同，以及原 point oracle 精确比较。

实测证明本次根因可以通过补全精确求解算法关闭，因此没有继续扩大到历史索引、dirty/cache 或全局 ObjectMemory 数据结构重构。它们的大地图扩展风险仍存在；本片不是“已完成全部 production 架构”的声明。

## 最终证据

完整 Editor Build 成功；最终真实 D3D12 定向回归 19/19。严格位置配对和两次原生 WASD 30 秒测试见 [Evidence](Evidence/SIGHTWEAVE_HISTORY_COVERAGE_20260909/RESULTS.md)。

修复后 Gray 配对 Historical 平均 1.08 ms、最大 1.81 ms；原生移动 Historical 最大 10.54～11.05 ms，无 ≥50 ms 样本。原生整帧 GT 均值 16.70～16.72 ms，但 P99 25.62～26.25 ms；严重历史退化已关闭，**不是稳定 60 fps 全帧 gate 已通过**。

复跑：`Scripts/RunApartmentSightWeaveBenchmark.ps1 -RunName <唯一名称> -Pair PairGray` / `-Pair PairUnknown`。不带 Pair 为原生 WASD＋转向、30 秒预热＋30 秒测量。配对是固定步骤 pose replay，保留真实场景和正常 Knowledge 更新；不是人工试玩，也不冒充固定 wall-clock 速度测试。详细限制见 Evidence。

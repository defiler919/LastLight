# SpatialPartial 部分显现栅栏（2026-09-08，完成）

后续 F 键控制台交接见 [Black Region F 开关](SIGHTWEAVE_BLACK_REGION_F_SWITCH_ZH.md)。

起点 `842c66c2948fbf1a73aa0074ec32ac069b99c880`。公司 `D:\UE_projects\LastLight`，UE 5.8.2 / RTX 4060 / D3D12 SM6。本轮只修视觉 blocker，没有 F 键施工。最终 SHA 为本文件所属提交。

## 复现与分类

使用独立 Clean Black Region Lab 的原 fixture：玩家 (-200,-130,92)，Partial 中心 (130,80,55)，尺寸 140×60×110 cm，yaw=37°，一个 mesh primitive；Whole 和地面仍在。固定玩家朝向 -15° 保持部分观察 180 个真实引擎帧，转到 -90° 保持到第 210 帧，再转到 45° 观察到第 270 帧。第二组以 85° 从另一侧切入。

证据纠正了最初的阶段假设：纯 Current 的 coverage/mask 和提交 R/A 不是栅栏；-15° Current 外表面可见两条错误 cap 细线，转开封存后才产生与用户截图一致的整面灰色百叶窗。85° 另一侧同样可以复现部分体积和外侧条纹。没有把它当成旧的 B 双线性 padding 问题直接处理。

## 精确根因

1. `WritePartRasters` 的 Current 源纹理按 primitive Local 状态采样，并在物理轮廓外钳到该 part 的边缘，因此源纹理连续。
2. `WriteWorldSnapshot` 是几何裁切的 coarse world AABB 快照。旋转边缘的 coarse 格子中心落在实体外时写 0，虽然格子的一部分与实体相交，且对应 Local 边缘已经被合法观察。
3. Current cap 把这些几何外零格子误认为观察 cut，产生少量假竖线。
4. 封存把 coarse `DiscoveredPresent` 原样复制到 4×4 fine capture mask，又从相同零格子生成 FrozenAAEnvelope。结果是合法实体边缘被周期性挖掉：A 硬门有缺口，B 也有错误 AA 衰减。37° 竖直面共享 XY，把这些列沿整个 Z 拉成条纹。
5. 完整观察后旧代码会填充 `CompleteEnvelope`，所以继续观察足够区域又恢复实体。这解释了人工现象。

因此是 **Local→capture 转换有损，加上显示边界输入混淆**；Local authority 本身没坏，也不是多 primitive 接缝（复现对象只有一个 primitive）。TAA/双线性只是把已有间断显示出来。此前 B 外环 padding 无法补回 A 中已丢失的 capture 位，也无法修复几何内已冻结的零 AA。

## 修复和授权边界

用户明确确认：“允许修正映射，保留原知识规则”。不能将本修改宣称为所有 CPU capture 位不变。

- 原 `WriteWorldSnapshot` 的结果保持不变，新增 `WritePresentationSnapshot` 提供与 Current 源纹理一致的物理边缘延拓。单 primitive Partial Current cap 使用这个显示副本，不写回知识；复合 primitive Current cap 保留原 union 路径，避免跨 part 的 AABB padding 借用。
- `ExtendCaptureAtPhysicalEdges` 只检查 coarse 中心位于某个 primitive 实体外的格子。修复位必须与**同一 primitive** 的 fine 几何相交，而且原 Local 边缘及 fine 中心映射都已有合法 capture。真实内部观察 cut 不变，不能借另一 part 的 AABB 观察未知物体。
- 封存 AA 从物理边缘显示副本生成；最终 capture 继续经过 geometry footprint、fine Clear/Block、retained sample 和既有 ownership 门。Whole 及无对应 Local epoch 的合成记录仍走原初始化路径。
- 不修改 Local coverage 阈值、五点合法观察、采样分辨率、Current 源纹理、材质、AA 开关、Whole span/原子规则或 cap 封口语义。不强制 alpha、不全物体显示、不延迟首显，也没有隐藏 cap 验收。

## 数据对照

- Local=56×24，world raster=60×53，fine=240×212。
- 修复前后 `probe_180_local.csv`（含 coverage、两个 mask）、`probe_180_world.csv`、Current 提交 R/G/B/A 通道图 SHA256 全部一致。
- 部分观察封存 capture：20,866 → 21,235，补回 369 个已证明的物理边缘 fine 位。geometry footprint 为 22,312 位，仍有 **1,077 位 Unknown**，没有变成 Whole。
- 已 capture 的零 AA 样本：710 → 284；保留真实观察 cut 的保守 inward AA，未强制清空所有零值。
- 自动检查 46 个已证明的物理边缘点：封存 mask / 最终 A / B 缺口为 **0**。

## 验收和证据

完整 `Scripts/BuildEditor.ps1`：DarkwellEditor Win64 Development **Succeeded**。最终 `Scripts/RunBlackRegionTriggerTests.ps1 -RunName PartialFenceFinal`：**21/21 通过，20 clean、1 warning、0 failed、0 not-run、exit 0**。唯一测试警告为 Google generate_204 HTTP 超时，与渲染断言无关；编译有既有 Engine 弃用及工具链 preferred 提示。

另一侧独立 `PartialFenceOppositeFinal`：1/1 clean。全部使用真实 D3D12/SM6；捕获保留 temporal AA 和持续 view state，等待正常资源提交，并检查截图不是空帧。原 Unknown Whole/Partial、37° Clear/Block/解除首帧/重观察、Trigger 生命周期、presentation residency、Whole 快速路径、pose bounds/topology reset、此前 crash 生命周期均通过。当前表面和部分封存表面均无重复竖条；另一侧仍有正确连续 cut，解除后无旧灰复活。

调查失败记录保留：75° 覆盖太多、95° 未有效看到物体，不能作为复现；-25° 太窄；-15°/85° 成功复现。仅修 AA/Current cap 的 `PartialPresentationFix1` 仍有历史 A 栅栏，没有当作 PASS。`PartialFenceRegression1` 拦到新路径错误作用于 Whole 合成 fixture 的断言；入口限定为相应 Local epoch 的 Partial 后完整回归通过，没有移除该回归场景。

证据目录：[SIGHTWEAVE_PARTIAL_REVEAL_FENCE_20260908](Evidence/SIGHTWEAVE_PARTIAL_REVEAL_FENCE_20260908/README.md)。Before/After 保留原始正常 D3D12 画面、Current R/A 与历史 B/A；raw.zip 是无损 CSV 与 before source patch，包含 raw Local masks、world raster、fine capture/geometry/opacity/AA/state。最终构建、自动报告、DLL SHA256 和文件哈希对照一起提交；本机 Saved 完整试验没有覆盖。

## 人工复查 / 精确自动复现

运行 `Scripts/LaunchBlackRegionLab.ps1`，保持物体 37°。从初始位置缓慢移动视野边界经过蓝色 Partial，只观察一部分；转开查看部分灰，再观察更多。应看到连续外表面与真实 cut，而不是栅栏。activate/deactivate 仍为既有 `Darkwell.BlackRegionLab` 控制台命令，无需 setup。

精确帧序列由以下命令在同一 fixture 运行并导出截图：

```powershell
& Scripts/RunUnknownPartialCutTests.ps1 -RunName MyPartialNear -Tests Darkwell.BlackRegion.CurrentPartialProbe
$env:DARKWELL_PARTIAL_PROBE_YAW='85'
try {
    & Scripts/RunUnknownPartialCutTests.ps1 -RunName MyPartialFar -Tests Darkwell.BlackRegion.CurrentPartialProbe
} finally {
    Remove-Item Env:DARKWELL_PARTIAL_PROBE_YAW
}
```

本视觉 blocker 已关闭。下一最小切片仍为固定黑区的 F 键启停；需用户下一轮启动，本轮未扩展。

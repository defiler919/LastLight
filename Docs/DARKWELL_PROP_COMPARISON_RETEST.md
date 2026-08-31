# 家具呈现比较修正记录

状态：PARTIAL — VERIFICATION IN PROGRESS。用户最终模式尚未选择。
本轮从 `049f59be42bca1c3f139985ee4c31248ca082d75` 原分支继续，不改命令 World 解析。

## 实测根因

`RetestBeforeGeometry.json` 通过官方 Editor Python 读取全部原生家具／结构网格的 Transform、Bounds、可见性、材质和 StableID。
`Lab.Island/Part0` 与另一 actor 的 `LabStructure8` 相交 323400 cm³；左右共面面积各 1540 cm²。
岛台顶板 Part3 与同一结构块相交 16380 cm³，顶面共面面积 4095 cm²。
压缩块实际属于结构 fixture，没有家具 StableID，使用结构的逐像素材质。家具则使用其物体级状态和 MIDs。
因此根因是跨 actor 几何交叠／共面 Z-fighting，叠加两个不同呈现对象争夺同一表面；不是同一 StableID 的 Primitive 独立执行 .50/.25 门限。
原 `EvaluateMaximumCoverage` 对所有注册组件取 max 后仅调用一次 Observe；`ApplySourceLiveState` 将结果统一施加到所有组件。这条规则没有改写。

限定复查还发现：货架层板／立柱共面交叠；独立连续台面和矮柜自身顶板体积重叠；冰箱／高柜把手嵌入门板 0.5cm（无共面外表面）。
返向柜排的 Python Rotator 位置参数意外生成 pitch=90，已改用明确的 yaw 关键字并恢复直立。
修复为：岛台单主体；结构块移离家具；柜排仅保留独立连续台面；货架层板接触立柱内面；把手接触门板而不嵌入。
`RetestAfterGeometry.json` 确认所审计家具无正相交体积或共面外表面。场景远端两段墙的角部相交仍保留，未扩大到无关建筑清理。

## 人工环境和路线

地图没有已放置 Stalker，也没有默认生成逻辑。`enemy 1` 显式生成；`enemy 0` 移除。路线 1–6 移除测试敌人且拒绝生成。
仅实验地图允许 SightWeave 项目适配器零敌人启动，并为显式敌人维护原 NeverRemember 注册；正式地图仍要求原来的单 Stalker。
重置后满生命、双发霰弹枪、Torch、满 Torch/Lantern 能源、零热量。实验工具自动恢复，不改变正式耐久规则；`dark` 显式关闭自动补充，`torch`/`lantern` 恢复。

Route 1：Lab.Island 单块 1400×110×90cm 长比较表面；玩家固定 (400,-700,92)，相机固定，只有 Torch 朝向变化。
0–2s 全灰；2–8s yaw -30→40；8–10s 停留；10–16s yaw 40→110；16–28s 反向回 -30；28–30s 全灰。
扫描速度为 11.6667 度/秒。重新输入 `route 1` 从同一起点重播，不改变 mode 或 policy。
Mode 0 仍整物体 .50/.25；Mode 1 连续原覆盖；Mode 2 同一空间覆盖加 .20s 渐入、失去 Raw 立即归灰。未改 blur、feather、迟滞、depth bias 或正式默认值。
Route 3 为静止旋转；11/12/13 分别为水平／垂直／对角移动，用于岛台连续帧检查。

## 检查点与待完成验证

RetestBuild04 完整 Editor Development 构建通过。RetestAutomation01 的 DARKWELL 过滤器 17/17 通过，含新增固定轨迹、零敌人／显式敌人往返生命周期、三模式完整轮廓与物体级状态。
RetestBuild01 的 TObjectPtr 自动类型推导编译错误已修正；RetestProbe01 因原适配器强制 Stalker 而失败，已保留失败日志并添加仅实验地图的可选主体支持。
RetestProbe02 正在确认可见对照；双分辨率正式矩阵、匹配帧、连续帧、实际 PIE 与 Git 闭合尚待完成。

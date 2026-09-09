# SightWeave Apartment Lab

正式人工入口：`/Game/Maps/L_SightWeaveApartmentLab`。基于 `87f9a96e5c99f3ffe1c6fa274c41b853dfe56112`，不移动既有 stable tag。

## 布局与玩法

约 12×10 m、墙高 2.4 m，无顶盖供俯视观察。地图保存当前 Integration GameMode、原生 `ADarkwellApartmentLab` 和 PlayerStart；灰盒几何在 BeginPlay 由 C++ 创建，编辑器未 PIE 时不会显示全部运行时家具。没有自动 Moving/Multi 路线。

```text
北 y=500
┌──────────┬──────────┬──────────┐
│ 厨房     │ 客厅     │ 卧室黑区 │
│ 操作台   │ 沙发     │ 床/床头柜│
│ 餐桌     │ 茶几Whole│          │
│ 冰箱     门         门 衣柜37° │
├──────────┴────门────┴──────────┤ y=-140
│ 玄关高柜      入口 / 短走廊    │
└───────────────────────────────┘ y=-500
 x=-600    -200       200       600
```

三扇门复用 `ADarkwellDoor`、原生 Door Open/Closed Gameplay Tags 和现有 F 交互组件；宽 1.2 m、高 2.2 m，门楣补足墙高。三扇门的实际门板姿态都参与当前遮挡，不把逻辑 Open 状态直接当作整扇门消失。

家具采用 UE Cube：沙发座/靠背、茶几、厨房操作台和餐桌、冰箱、床和床头柜、37° 衣柜、玄关高柜。茶几、餐桌、床头柜、绿色开关为 Whole，其余主要为 SpatialPartial；门复用已有移动历史策略，其余静态物体 StationaryOnly。全部 `bRememberFromStart=false`。

卧室固定 BlackRegion AABB：XY `[200,600] × [-140,500]`，400×640 cm，完整沿房间分隔线定义。现有绿色 `ADarkwellBlackRegionSwitch` 位于客厅 `(100,-55)`，指向现有 Trigger；靠近 150 cm 内并面向它按 F。这里只调用现有 Activate/Deactivate，没有复制 Clear/Block/Knowledge。

客厅注册一盏现有 `Darkwell.Visible.Environment` 合法径向光，中心 `(0,250)`、范围 350 cm；同时有对应暖色 PointLight。卧室不注册环境光。玩家原固定视锥/近身 awareness/随身工具规则保持不变，环境光不能扩大视锥。没有新增照明系统或新开关灯按键。

## 当前运行路径与遮挡

`ADarkwellVisionIntegrationGameMode → UDarkwellSightWeaveWorldSubsystem → SightWeave Runtime + UDarkwellFogVisualSubsystem P4 → ADarkwellObjectMemoryScene`。

P4 raw coverage：488×408 R16F、2.5 cm/texel；对象局部 cell 2.5 cm、原细样本精度不变。旧 HUD `FogTexture/FogCompositeMID=None`，Legacy Visibility tick 关闭；旧插件 renderer 被抑制。旧组件类型可能由共有角色/HUD构造，但不执行 Legacy Fog 路径。

沿用当前 2D segment 模型：10 条墙轴线、3 条高家具遮挡平面、3 条实际旋转门板中心线，共 16 条，未扩大 P4 上限。墙、门与家具仍有实际碰撞几何。不是任意三角网格或所有低矮家具的完整 3D 遮挡系统；低家具用于 Partial 表面观察，三件高家具额外参与合法视线遮挡。

新增 fixture opt-in `HasDynamicSightWeaveOccluders()`：adapter 仅在门几何变化时调用现有 Runtime `UpdateOccluder`，同步 P4 参数后发布对应 revision。不重建 render target、不降低精度；几何变化时中断旧几何上的历史旋转 sweep。原固定 Lab 的注册与验证保持原分支。

## L_Prototype 废弃处理

**Legacy Deprecated，保留资产，未迁移。** 检查代码、脚本、测试和文档后确认仍有冻结历史性能 Reference 依赖：

- `Scripts/RunGrayPerformanceBaseline.ps1 -Protocol Reference` 默认目标为 L_Prototype；它与 `Content/Python/profile_gray_project_reference.py` 及历史 stabilization 报告构成基准回放链。现在必须显式加 `-AllowLegacyReference`，否则拒绝并指向公寓入口。
- `DarkwellGameplayRuleTests.cpp` 的 SaveRoundTrip 保存/比较字符串 `L_Prototype`，不加载地图；保留历史序列化断言。
- 历史 handoff/evidence 文档保留当时记录，不改写为新版已经验证。

`Config/DefaultEngine.ini` 的 GameDefaultMap 和 EditorStartupMap 均改为公寓；README 当前入口已更新，旧完整玩法介绍明确置于 Legacy Deprecated 历史节。未删除 L_Prototype.umap、未修改其内容或偷偷升级 Fog。引用清单保存在本轮 Evidence。

## 人工试玩

运行 `Scripts/LaunchApartmentSightWeaveLab.ps1`：直接进入 D3D12 手动游戏窗口，没有自动路线、自动 Blackout 或存档加载。也可在 Editor 打开地图并按 Play 进行 PIE。

1. 出生于南侧玄关，初始无预写 Memory；近身/合法视野随正常首帧显现。
2. 靠近正前方门，面向门按 F；进入客厅。重复开关，观察门后 Live 是否随实际开口变化。
3. 分别进入西侧厨房、东侧卧室；贴墙角、门洞观察，再转开形成 Gray。检查茶几/餐桌等 Whole 与衣柜/墙面等 Partial。
4. 观察 37° 衣柜后返回客厅，靠近绿色控制台按 F。HUD 显示 BEDROOM BLACKOUT ACTIVE，卧室旧灰被 Clear+Block。
5. 再进卧室，Live 正常；离开合法视野后保持 Unknown。回开关按 F 停用；旧灰不能自动恢复，重新观察才重建。
6. 观察客厅暖光与卧室较暗的区别；原随身光仍可能合法照亮卧室，不应把“房间暗”理解成强制禁止 Live。

本轮没有重构 AA、ownership、cap、Whole 或 Unknown 语义。当前已有阶梯和时域颗粒可在室内边界继续观察和记录，不能把它们描述为本轮已修复。

## 验证边界

完整 Editor Development Build、C++ 定向测试和真实 D3D12 PIE 的最终结果见 `Evidence/SIGHTWEAVE_APARTMENT_20260909/RESULTS.md`。

`verify_apartment_sightweave_lab.py` 是有界自动 PIE 证据采集：脚本定位玩家、固定拍摄朝向并调用现有 Trigger API，保存初始/观察/离开/Blackout/BlockedLive/解除/重观察序列。它不模拟用户完整 WASD/F 试玩，不作为人工视觉通过结论。最终人工试玩和视觉验收留给用户。

# Mode 2 双向深灰剖面复测交接

状态：`PARTIAL — READY_FOR_USER_MODE2_SYMMETRIC_DARK_GRAY_CAP_RETEST`

本轮从 `855eb87ad047fc0b75e63a9479413efd21d9ffa7` 继续，分支始终为
`codex/darkwell-prop-memory-gameplay-lab`。没有创建或切换分支，没有 merge、rebase、reset、clean、
restore 或 force-push。`Darkwell.uproject` 的本机 EngineAssociation GUID 差异保持未提交。

## 修正范围与根因

用户已确认 ABSENT 残影擦除方向能看到封口，但 PRESENT 实体重新显露方向没有封口。准确根因是
`ADarkwellManualStaleRoom::UpdateStaleCap()` 明确要求 `SpatialMemory.IsAbsent()`，剖面签名和边界分类也只读取
`InitialRemembered / VerifiedEmpty`，因此 PRESENT 代的 `DiscoveredPresent` 边界从未进入同一表现组件。

本轮只扩展既有 Lab-only `UDynamicMeshComponent Mode2StaleCutCap`：

- ABSENT：继续在 retained stale / verified empty 的相邻权威格之间生成封口。
- PRESENT：在 discovered present / undiscovered present 的相邻权威格之间生成同样的封口。
- 两个方向使用同一 2.5 cm 权威网格、同一原始三部件 frozen world Bounds、同一四边形生成函数、同一材质。
- 零覆盖、完整显露、完整擦除、Mode 0/1 或无有效状态时清空并隐藏动态网格。
- 签名只加入实际代别和对应离散 D/V 状态，不读取相机、时间 opacity 或 4×4 AA 表现采样。

剖面仍是独立表现层：无碰撞、无 overlap、无阴影、无 decal，不属于真实柜子或记忆代理，不参与
StableID、快照、占用、D/V/R 或空位验证。原柜体组件、Transform、Bounds、角点、LOD0 顶点和阴影源没有修改。

## 固定深灰材质

`M_ManualStaleCutCap` 保留原独立 `Opaque + Unlit + Two-Sided` 路径。没有为了填写无效的 lit PBR 槽位重构
材质。最终颜色通过 Emissive 常量稳定输出，避免高光或阴影漂移：

- 目标 sRGB：`#343A40`
- 线性常量：`(0.0343398068, 0.0423114106, 0.0512694584)`
- 非 Metallic、Roughness 光照计算不参与该 Unlit 路径；视觉结果等价于固定中性深灰。
- 不读取柜体颜色，不做平均色识别，没有新增颜色 override。

自动化直接读取材质表达式，要求 Emissive 常量等于
`FLinearColor::FromSRGBColor(FColor(0x34,0x3A,0x40))`，并继续验证 Opaque、Unlit、Two-Sided、无 WPO、无
Opacity 驱动整柜替换。

## 冻结合同

没有修改 `FDarkwellSpatialPropMemory`、D/V/R 累计规则、`.20` 秒进入、`.18` 秒退出、退出 hold、空位确认、
4×4 保守采样、bilinear、非法区域零值、StableID、SightWeave、隐藏同源阴影或 Mode 0/1 分支。没有修改
`L_Prototype`、正式地图或 SightWeave 插件。

## 构建与自动化

所有 Editor、UBT、dotnet 和 ShaderCompileWorker 调用串行。

- `Saved/PropGameplayLab/Mode2SymmetricCap/Build01.log`：标准 DarkwellEditor Win64 Development 成功，5 actions。
- `Saved/PropGameplayLab/Mode2SymmetricCap/Build02.log`：修正测试 tolerance 字面量后最终标准构建成功，4 actions。
- `Saved/AutomationReports/Mode2SymmetricCap01`：双向封口、固定深灰材质、保守 AA，3/3 成功。
- `Saved/AutomationReports/Mode2SymmetricCapFinal`：16 项全部 Success，13 clean、3 项带既有外部 HTTP 警告，
  0 失败、0 未运行。

完整套件覆盖 PRESENT/ABSENT 部分边界、零/完整状态、Mode 0/1、十次开关、固定原始几何、隐藏同源阴影、
D/V/R 世代与不复活、`.20/.18` 时间合同、保守 AA 和 Lab 隔离。新增测试还在相同 D/V/R 快照上切换
Mode 1/2，逐格确认剖面清空/重建不会修改 `DiscoveredPresent`、`VerifiedEmpty`、`InitialRemembered` 或
`RemainingStale`。

## 最终 D3D12 / SM6 / TSR 证据

最终两次运行均使用 D3D12、SM6、正常 TSR（`r.AntiAliasingMethod=4`）、100% Screen Percentage，三方向、
三完整开关循环。两次均逐 Tick 检查权威格、原几何、组件和阴影源，并在 PRESENT/ABSENT 的 25/50/75%
要求 cap visible 且 triangle count > 0；完整显露和完整擦除要求 cap 为零。

| 运行 | 根目录 | 截图 | 几何检查 | 逐格检查 |
|---|---|---:|---:|---:|
| 1920×1080 | `Saved/PropGameplayLab/AccumulatedMemory/PIE_20260901_100957` | 149 | 1,536 | 40,547,584 |
| 2560×1440 | `Saved/PropGameplayLab/AccumulatedMemory/PIE_20260901_101253` | 149 | 1,465 | 38,884,032 |

合计 298 张原生截图、3,001 次固定几何检查、79,431,616 次逐格检查。最终日志：

- `Saved/PropGameplayLab/Mode2SymmetricCap/GPU1080StaticFixed.log`
- `Saved/PropGameplayLab/Mode2SymmetricCap/GPU1440StaticFixed.log`

Agent 实际打开了两分辨率的 `final_discover_*_contact.png`、`final_erase_*_contact.png`、
`final_motion_*_contact.png`、`final_stationary_*_contact.png`、`final_full_*_contact.png`，以及四张原生 50%
左右/斜向帧。所见结果：两个方向均有相同中性深灰封口，明显深于灰色记忆和灰色地面且不是纯黑；左右和
斜向边界单调推进；静止三帧稳定；连续帧无随机点阵、闪烁或 TSR 拖尾；无整柜弹出、绿色一闪即灰、提前
整柜、几何变形、Z-fighting、重复柜子或重复阴影。完整显露与完整擦除均无封口残留，中墙遮挡保持正确。

首轮证据目录 `PIE_20260901_095752` 和 `PIE_20260901_100448` 的驱动统计均通过，但静止组三张中的最后
一个异步 `Shot` 在相机转离后才落盘，不能作为合格静止对照。没有用它冒充最终证据。采证脚本只增加每个
静止截图后的 0.12 秒同姿态保持，随后分别一次复跑得到上述最终目录；未修改路线、时钟或玩法代码。

## 日志与限制

最终 Build02、AutomationFinal、GPU1080StaticFixed 和 GPU1440StaticFixed 的 severe scan 均为 0。保留：

- Visual Studio 14.51 新于 UE 偏好 14.50。
- UE / GeometryFramework / Chaos 头文件 C4996 弃用警告。
- 完整自动化中三项外部 `google.com/generate_204` HTTP 警告；测试状态仍为 Success。
- 启动期既有 Condition failed、MCP、Recast/Crowd、MotionVectorSimulation 和截图命令提示。

剖面仍只服务手动 Lab 的三个轴对齐灰盒柜体部件，不是任意复杂网格通用封口器。自动截图不能替代用户在
真实鼠标速度下判断最终手感，本轮没有选择最终 Mode。

## 用户 PIE 复测

打开 `/Game/Maps/L_ProjectFogPropGameplayLab`，Play 后执行：

```text
Darkwell.PropLab stalemanual reset
Darkwell.PropLab stalemanual mode 2
Darkwell.PropLab stalemanual teleport top
Darkwell.PropLab stalemanual teleport bottom
```

先建立完整柜体记忆，到下层踩开关使其 ABSENT，返回上层检查擦除封口；再踩开关使其 PRESENT，返回上层
检查重新显露封口。分别执行左→右、右→左、斜向、静止和小幅往返，确认两个方向颜色及边界一致。最后确认
完整显露/擦除无残留，并切 Mode 0/1 检查不会留下 Mode 2 封口。

用户人工 PIE 仍待执行，不能宣布 Mode 2 最终完成。

## 用户人工验收结果

```text
USER MANUAL PIE: PASSED
Date: 2026-09-01
Accepted commit: 39908cc67eb91d72a7d5ac35fe813367b41a7919
Decision: Mode 2 is the canonical prop-memory presentation baseline.
```

用户已在真实 PIE 中确认累计空间记忆、PRESENT/ABSENT 双向深灰色剖面、当前 4×4 保守采样和抗锯齿表现
通过。Mode 0/1 保留为 Lab 对照、调试和回归模式，不再作为玩家默认候选。本次决定只冻结呈现稳定点，
没有把实验系统迁入正式地图，也没有设置正式运行时默认模式。

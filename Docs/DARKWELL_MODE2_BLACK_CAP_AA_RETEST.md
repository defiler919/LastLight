# Mode 2 黑色剖面封口与裁切抗锯齿交接

状态：`PARTIAL — READY_FOR_USER_MODE2_BLACK_CAP_AA_RETEST`

本轮从家中已验收节点 `a1210901bc7b76d881438f33808a47a9f6df9336` 续接，分支始终为
`codex/darkwell-prop-memory-gameplay-lab`。开始时 local HEAD、upstream、remote 三处一致，工作树只有
`Darkwell.uproject` 的本机 EngineAssociation GUID 差异。没有切换分支，没有 merge、rebase、reset、clean
或 force-push。

用户已经验收的累计 D/V/R、0.20 秒进入、0.18 秒退出、固定原始几何、隐藏真实阴影、Mode 0/1、
StableID 和 SightWeave 公共合同均未重新设计。本轮没有修改 `L_Prototype`、正式地图、SightWeave 插件或
`Darkwell.uproject`。

## 可靠提交

- `af2b16b` — `Add fixed-grid black cut caps for stale prop erasure`
- `a3fd51e` — `Smooth Mode 2 cuts with conservative dense sampling`

两个检查点均在对应构建、自动化和真实 GPU 检查后立即推送。本文档作为最后的交接提交单独推送，最终
完整 SHA 在最终报告的 Git 闭合结果中给出。

## 黑色剖面实现

masked 原表面只能丢弃像素，不能为任意裁切产生新的封闭面。仅修改原材质无法得到真实剖面。因此 Lab
手动房间新增一个 `UDynamicMeshComponent`，但它只承担剖面表现：

- 组件属于 `ADarkwellManualStaleRoom`，不属于真实柜子或记忆代理。
- 只在 Mode 2、ABSENT 且同一已记忆区域存在相邻的 `VerifiedEmpty` / retained stale 格时生成。
- 边界来自既有 2.5 cm 权威网格；摄像机位置和角度不参与网格或顶点计算。
- 每条边界与初始化时冻结的原三个柜子部件 world Bounds 求交，再生成对应竖直四边形。
- 剖面使用独立的双面、不透明、unlit 黑色材质 `M_ManualStaleCutCap`。
- 组件关闭碰撞、overlap、阴影和 decal，不参与 StableID、快照、占用、空位验证或玩家知识。
- PRESENT、Mode 0/1、完整未切割和完整清空状态会清空网格并隐藏组件。

这不会重现 `0032533`：原柜子的三个真实组件、12 个原生槽位、StaticMesh、actor/component Transform、
Bounds、八角点、全部 LOD0 世界顶点和材质槽位合同都没有被剖面组件修改。没有缩放、WPO、动态柜体代理、
重建原柜子、移动切面代理或替换整柜几何。剖面组件不投射第二份阴影。

剖面自动化还逐顶点证明：所有剖面顶点落在固定权威网格上，并处于原三个固体 Bounds 之一；剖面三角形
遥测等于实际动态网格三角形数。

## Mode 2 裁切 AA

权威状态仍是原来的 2.5 cm D/V/R 网格。新增的 `BuildConservativePresentation()` 是 const 表现函数，
不会写入 `CurrentLegalCoverage`、D、V、R、时间混合、世代或 StableID。

每个权威格展开为 4×4 RGBA16F 表现采样，手动柜子的表现纹理由原来的 `176×68` 提高到
`704×272`：

- R、G、B 分别继续表示 source opacity、Live/gray blend、stale proxy opacity。
- 内部可见边界在可见一侧以零值保护采样结束，再向内部形成短空间坡度。
- 隐藏或已验证为空的格保持全零，因此双线性过滤没有可扩散到非法格的种子。
- 纹理改为 bilinear；既有局部 temporal opacity dithering 和 TSR 仍负责最终像素采样。
- 0.20 秒进入和 0.18 秒退出的逐位置时间合同没有改变。
- Mode 0/1 不启用手动 fixed-reveal 分支，剖面也被强制清空。

新增 `ConservativePresentationAA` 自动化用两个相邻权威格直接验证：已知一侧内部保持 1、边界保护样本为
0、未知/已清空一侧全部为 0，并逐字段比较调用前后的 D/V/R 和时间状态完全相同。

## 修改范围

相对 `a1210901` 共修改 10 个受控文件：

- `Content/Darkwell/Vision/PropLab/M_ManualStaleCutCap.uasset`
- `Content/Python/create_manual_stale_cut_cap_material.py`
- `Content/Python/verify_accumulated_spatial_memory.py`
- `Source/Darkwell/Darkwell.Build.cs`
- `Source/Darkwell/Private/Tests/DarkwellSightWeaveAdapterTests.cpp`
- `Source/Darkwell/Private/Tests/DarkwellSpatialPropMemoryTests.cpp`
- `Source/Darkwell/Private/VisionPresentation/DarkwellManualStaleRoom.cpp`
- `Source/Darkwell/Private/VisionPresentation/DarkwellSpatialPropMemory.cpp`
- `Source/Darkwell/Public/VisionPresentation/DarkwellManualStaleRoom.h`
- `Source/Darkwell/Public/VisionPresentation/DarkwellSpatialPropMemory.h`

运行时 C++（不含测试、脚本、Build.cs 和资产）为 176 行增加、11 行删除，净增加 165 行。新 `.uasset`
由 Git LFS 管理。

## 构建与自动化

所有 UBT、dotnet、Editor 和 ShaderCompileWorker 调用均串行。

黑色剖面阶段执行 6 次标准 `DarkwellEditor Win64 Development` 构建，全部成功。多次构建用于把首轮组合
断言拆开，发现它把“分离的旧记忆岛还没有相邻实体切面”误计为失败；运行时实现未崩溃。原失败报告保留：

- `Saved/AutomationReports/BlackCap01`：15 项中 14 成功、1 失败。
- `Saved/AutomationReports/BlackCap02` / `BlackCap03`：定向断言仍失败，定位为上述测试假设。
- `Saved/AutomationReports/BlackCap04`：2/2 成功。
- `Saved/AutomationReports/BlackCapFinal`：15 项全部成功（14 clean、1 带外部 HTTP 超时警告）。

AA 阶段第一次 shell 重定向因 `Saved/PropGameplayLab/Mode2AA` 尚不存在而失败，未启动构建；创建忽略目录后：

- `Saved/PropGameplayLab/Mode2AA/Build01.log`：标准 Editor Development 构建成功，10 actions。
- `Saved/PropGameplayLab/Mode2AA/BuildFinal.log`：最终标准构建成功，target up to date。
- `Saved/AutomationReports/Mode2AA01`：新增 AA + 固定几何 2/2 成功。
- `Saved/AutomationReports/Mode2AAFinal`：16 项全部成功（13 clean、3 带外部 HTTP 超时警告），0 失败、0 未运行。

最终套件覆盖 Mode 0/1/2、十次开关、隐藏同源阴影、固定几何、空位不复活、世代隔离、黑色剖面、
保守 AA、Relocation 既有合同和 Lab 隔离。

## D3D12 / SM6 / TSR 证据

全部成功 GPU 运行使用 D3D12、SM6、正常 TSR（`r.AntiAliasingMethod=4`）和 100% Screen Percentage。

黑色剖面检查点：

- `Saved/PropGameplayLab/BlackCap/GPU1080b.log`
- `Saved/PropGameplayLab/AccumulatedMemory/PIE_20260901_084853`
- 1920×1080，三方向、三完整循环、96 张原生截图。
- 1,766 次固定几何检查；47,046,208 次逐格知识检查；驱动无失败。
- 实际打开 `black_cap_1080_contact.png`、25/50/75% 九个匹配帧及
  `0090_diagonal_erase_50_held.png`。

第一次黑色 GPU 尝试误用嵌入式 PIE；驱动要求新窗口并在采样前失败，零截图、零状态写入。日志为
`Saved/PropGameplayLab/BlackCap/GPU1080.log`，失败 JSON 在
`PIE_20260901_084353/failed_checks.json`。随后以正确的新编辑器窗口一次通过，没有用代码修改掩盖失败。

最终 AA 证据：

| 运行 | 分辨率 | 循环 / 方向 | 截图 | 固定几何检查 | 逐格检查 |
|---|---:|---:|---:|---:|---:|
| `GPU1080Final.log` / `PIE_20260901_090341` | 1920×1080 | 3 / 左右双向及斜向 | 113 | 1,331 | 34,372,096 |
| `GPU1440.log` / `PIE_20260901_090134` | 2560×1440 | 1 / 左→右补充 | 44 | 536 | 14,158,144 |

两次最终运行合计 157 张原生截图、1,867 次固定几何检查、48,530,240 次逐格检查。25/50/75%、
完整发现、完整擦除、转头保留和两类阴影状态均通过。局部擦除时 `capVisible=true` 且 cap triangles > 0；
完整清空为 0。

Agent 实际打开：

- `PIE_20260901_085753/mode2_aa_1080_contact.png` 与 `mode2_aa_before_after.png`。
- `PIE_20260901_090134/aa_motion_adjacent_1440_contact.png` 及原生连续请求帧 `0034...png`。
- `PIE_20260901_090341/aa_motion_all_directions_1080_contact.png`，包含三方向各六帧。

所看帧中边界单调推进，没有整件弹出、随机棋盘格破碎、已擦除区域复活、黑片穿墙或重复阴影。截图请求
本身有开销，不能代替正常交互帧率下的最终主观判断，也不作为性能声明。

## 警告、限制和待人工判断

最终 build、automation、1080p 和 1440p 日志 severe scan 均为 0。保留的非 severe 信息包括：

- Visual Studio `14.51.36256` 新于 UE 偏好版本 `14.50.35717`。
- UE / GeometryFramework / Chaos 头文件 C4996 弃用警告。
- 自动化的 `google.com/generate_204` 三次外部 HTTP 超时；相关测试仍成功。
- 既有阴影 CVar 优先级、MCP EULA、Recast/Crowd、MotionVectorSimulation 和截图命令提示。
- 引擎启动前测试队列外的既有 `Condition failed` 诊断仍保留，没有静默处理。

剖面当前只为手动房间的三个轴对齐灰盒部件按 Bounds 求交，不是任意复杂网格的通用封口器。薄结构、
凹网格、旋转网格和大批家具未验证。`704×272` RGBA16F 每 Tick 上传只验证了单柜 Lab，没有完整性能矩阵。
高对比黑色斜边仍可能让用户在真实鼠标慢扫中感知轻微栅格阶梯；自动截图不能替用户选择是否接受。

用户人工 PIE 仍待执行，因此不能宣称 Mode 2 最终完成，也没有替用户选择最终 Mode。

## 用户复测

打开 `/Game/Maps/L_ProjectFogPropGameplayLab`，Play 后执行：

```text
Darkwell.PropLab stalemanual reset
Darkwell.PropLab stalemanual mode 2
Darkwell.PropLab stalemanual teleport top
Darkwell.PropLab stalemanual teleport bottom
```

先完整记住柜子，再到下层踩开关使其 ABSENT。返回上层分别慢速左→右、右→左和斜向擦除，重点观察
25/50/75% 边界、黑色剖面、静止边缘和小幅往返。完全擦除后确认正常地面、无黑片；重新 PRESENT 后
确认隐藏真实阴影和局部显露仍正常。再切换 Mode 0/1 确认没有残留剖面，然后回 Mode 2；模式切换不应
重置实际柜子或记忆。至少完成两个开关循环。

最终保持 PIE 未运行、Play 可点击，等待用户执行上述真实交互判断。

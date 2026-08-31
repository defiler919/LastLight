# 家具呈现比较修正记录

状态：**PARTIAL — READY_FOR_USER_PROP_PRESENTATION_COMPARISON_RETEST**。用户最终模式尚未选择。
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
岛台只有 Part0 启用并注册记忆，Bounds 为 X[-300,1100]、Y[-355,-245]、Z[0,90] cm；类中其余预留部件槽保持隐藏、无碰撞、不参与记忆采样。运行日志的 `LAB_PART` 逐帧记录其 actor Live、组件覆盖、可见性、Transform、Bounds 和 MID。
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

## 三种呈现与身份的准确合同

| 模式 | 行为 |
|---|---|
| 0 AcceptedWholeObject | 一个 StableID 的全部注册组件共用一次 actor-level 观察结果；任一权威样本达到 .50 整件进入 Live，按原 .25 退出规则整件归灰。已知物品未 Live 时由完整灰色快照表示。 |
| 1 SurfaceSweepHard | 身份可见性仍统一；已知且位置合法的源物体保持完整轮廓。连续世界空间 LiveCoverage 只改变覆盖表面的原材质／合法光照，未覆盖部分是无光照灰色。没有 Opacity Mask 或半物体裁切。 |
| 2 SurfaceSweepSoft | 使用与 Mode 1 相同的 Raw 空间场；显示强度受 Raw 上限约束，新进入区域约 .20s 渐入，失去 Raw 当帧归灰。只有短显示状态，无长期历史，不改变物品、敌人、HUD 或 relocation 权威。 |

两种记忆策略未修改：policy 0 保留旧位置快照直至旧位置被验证为空；policy 1 认出同一 StableID 的新位置时更新记忆，外形相似的另一 StableID 不清除原身份。
路线重播不改变 policy。重置和退出实验世界恢复默认 0/0；正式地图默认合同没有变化。

## 验证和证据

最新运行代码检查点为 `f7cde9fd61ce5eb2280471d88c14cca2800f4b35`；后续只有报告／离线证据审计脚本变更。
本地证据目录：`D:/UE_projects/LastLight/Saved/PropGameplayLab/`，全部被 Git 忽略。

| 验证 | 结果／日志 |
|---|---|
| 完整 DarkwellEditor Win64 Development | `Scripts/BuildEditor.ps1 -Configuration Development`；`RetestBuild05.log` 通过。构建始终串行。 |
| C++ 自动化 | `RetestAutomation02.log`，过滤器 `Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1`，17/17 通过。 |
| GPU 材质 | `RetestGPU01.log`，33/33 检查通过；D3D12/SM6，Opaque、稳态 Hard=Soft、12 个 1/60s 渐入步、Raw 撤销即归零。 |
| 真实 PIE | `RetestPIE02.log` 通过；零默认敌人、100 Torch、同帧敌人拒绝、policy=1 的路线重播、显式敌人开关、受伤／dark 后 reset 满血并恢复 mode/policy/route=0。 |
| 定向矩阵 | `RetestFinal01`：1080p、1440p各15组，合计30/30通过、9720张连续原始帧；Route 1三模式、Route 3静止旋转、Route 11/12/13水平／垂直／对角移动。 |

自动化覆盖独立测试世界中的可选 Stalker 两代生成／销毁、NeverRemember、HUD snapshot 清除、六种模式／策略组合、完整轮廓、统一组件状态、同 ID 迁移及相似物品区分，并包含正式 SightWeave 集成／多世界隔离回归。

Route 1 每模式每分辨率保存 900 张原始 PNG，30s；旋转和三个方向移动每组 180 张，12s。
采集采用固定模拟步长（Route 1 30fps，其余 15fps），避免截图开销使三段路线时刻错位；这是确定性动态呈现证据，**不声称实时性能达到该帧率**。人工 PIE 不使用固定步长。
所有运行均为 D3D12/SM6、`r.AntiAliasingMethod=4` 的正常 TSR；没有关闭 TSR、增加 blur 或 depth bias 来隐藏瑕疵。

三模式匹配帧为 f128 / f232 / f403，对应 4.300 / 7.767 / 13.467s，权威顶面网格覆盖约 25.2% / 49.8% / 75.0%。
该百分比来自顶面 201×9 个 Raw 样本，不是 actor 的 .50 进入门限；Mode 0 的任一权威样本门限在更早时刻触发。

| 1080p 匹配点 | Mode 0 蓝色顶面 | Mode 1 蓝色顶面 | Mode 2 蓝色顶面 |
|---|---:|---:|---:|
| 约25%覆盖 | 100.00% | 23.80% | 23.10% |
| 约50%覆盖 | 100.00% | 49.61% | 49.26% |
| 约75%覆盖 | 100.00% | 77.24% | 76.58% |

| 1440p 匹配点 | Mode 0 蓝色顶面 | Mode 1 蓝色顶面 | Mode 2 蓝色顶面 |
|---|---:|---:|---:|
| 约25%覆盖 | 100.00% | 23.81% | 23.11% |
| 约50%覆盖 | 100.00% | 49.56% | 49.24% |
| 约75%覆盖 | 100.00% | 77.26% | 76.60% |

颜色统计使用实际截图投影顶面内缩95%的区域、B−R>40 分类，仅作辅证，不代替权威覆盖或人工查看。
两种分辨率的 Mode 0 各900帧：各141帧全灰、759帧整件 Live，零帧中间比例（2%–98%）。f69→70 显示明确整件跳变；Mode 1/2 同期仍只有左端逐步进入。
已实际打开两种分辨率的三模式并排全图、25/50/75%细节、三种HUD原始帧、f68–74整件跳变、f230–236半覆盖和同一屏幕像素边界放大图，以及反向f663–669和退出f827–833连续帧。
此外，实际打开全部24组旋转／移动contact sheets，以及每种分辨率、每种运动的三模式并排f87–93相邻帧。单块岛台没有原来的独立压缩组件闪烁；9720帧的组件统一可见性检查零失败。未把正常的整件门限切换算成组件闪烁，也未声称逐帧人工观看了全部录像。
Mode 1 和2的 Raw／玩家／相机／yaw／时间逐帧一致；固定屏幕点的短渐入不同。灰色表面持续完整，未观察到独立侧块闪烁。

`RetestFinal01_audit.json`、`RetestFinal01_pixels.json` 和 `RetestFinal01_closure.json` 保存完整逐帧数值、像素辅证、错误／警告清单及录像时长检查。30段MP4时长全部符合要求，其中六段Route 1严格同为30.00s，其余24段同为12.00s。
两种分辨率的并排证据为 `RetestFinal01_1920_matched.jpg` / `RetestFinal01_2560_matched.jpg`，顶面细节为同名前缀的 `_matched_detail.jpg`。
各模式录像位于 `RetestFinal01_<1920或2560>_M<0或1或2>_P0_R1/review.mp4`。相邻帧以 `_adjacent_jump/half/boundary/withdrawal/exit` 命名；运动三模式连续帧为 `_R3/11/12/13_adjacent.jpg`。

## 保留失败、警告与范围

- `RetestBuild01`：TObjectPtr 自动类型推导编译失败，已改为明确指针类型，后续完整构建通过。
- `RetestProbe01`：零敌人使旧项目适配器拒绝激活。已添加严格限于实验地图的零主体支持；失败日志保留，不计入最终矩阵。
- `RetestPIE01`：同帧 route 1 / enemy 1 曾短暂生成敌人；已直接检查请求路线修复。测试脚本也改用 ApplyDamage，避免写只读 Health 属性；PIE02 通过。
- 30组日志共有1590行 Error：390行初始化前 LogAutomationTest `Condition failed`，1200行 Experimental Toolsets 的 Python API 缺失 traceback（AgentSkill、ToolsetDefinition、PythonTestRunner）。按发生阶段、引擎路径和准确异常类型分类保留；零未分类 Error，不能将全日志宣称为零 Error。
- Severe扫描零匹配：Fatal、Assertion、Ensure、GPU crash/device loss、材质编译失败和实验合同失败。最新Build、C++自动化、GPU、真实PIE、地图修改／几何审计日志也单独扫描，零severe匹配。
- 保留 EditorDataStorageUI purpose=0、MotionVectorSimulation 渲染线程标志、Scalability 优先级、LabRoute 多次 FindConsoleObject、退出时 CrowdManager/RecastNavMesh 等警告；没有改动正式系统去消音。
- 30组共有728行 Warning，包括两条Zen旧锁恢复警告；完整清单保存在closure JSON。编译器偏好／废弃头文件等构建诊断另保留在构建日志。
- 运动采集的最初渲染帧可见 TSR 初始化模糊，未抹除。Route 1 的两秒全灰起点之后再开始比较。
- 远端墙角的两个结构块仍相交，限定家具审计已无共面／正体积相交；本次没有扩大为全地图几何清理。
- 未执行 BuildPlugin、Cook、Package、Shipping 或全性能矩阵。没有修改 SightWeave 插件、正式 Stalker 实现、正式地图或 Darkwell.uproject。

## 用户真实 PIE 复测

打开 `/Game/Maps/L_ProjectFogPropGameplayLab` 后点 Play，在**活动 Lab PIE**控制台逐行执行。不要在没有 PIE World 时用编辑器控制台判断命令失效。

```text
Darkwell.PropLab reset
Darkwell.PropLab mode 0
Darkwell.PropLab route 1
```

观看30秒，再分别执行下面两组，每组都等待30秒：

```text
Darkwell.PropLab mode 1
Darkwell.PropLab route 1
```

```text
Darkwell.PropLab mode 2
Darkwell.PropLab route 1
```

每次 `route 1` 会回到同一固定相机和两秒全灰起点；不用先 reset，因此 policy 不会被隐式改动。
顶部青色文字始终显示 MODE、名称、POLICY、Route、t 和 ENEMY。默认无敌人、无 STALKER HUNTING、Torch 自动补满。
`route 0` 返回手动移动；`route 3/11/12/13` 分别复查旋转／水平／垂直／对角移动。
只有明确需要敌人检查时，在手动路线执行 `Darkwell.PropLab enemy 1`；`enemy 0` 移除，路线1–6拒绝生成。
`reset` 恢复满血及固定工具并恢复0/0/0；`policy 0` / `policy 1` 仍独立可用，不在此任务替用户选定。

## 重现采集与提交

```powershell
./Scripts/RunPropPresentationComparison.ps1 -Widths 1920,2560 -Modes 0,1,2 -Routes 1,3,11,12,13 -Label NewUniqueLabel
python Scripts/ReviewPropPresentationComparison.py --label NewUniqueLabel --video
python Scripts/MeasurePropComparisonPixels.py --label NewUniqueLabel
python Scripts/AuditPropComparisonRetest.py --label NewUniqueLabel
```

采集脚本拒绝覆盖已有 label。MP4 是原始连续 PNG 顺序编码，没有插帧；匹配图和连续帧都保留原始来源。

| 提交 | 内容 |
|---|---|
| `5e21e1798ab68bcb0a050bae877bd1b95ee89547` | 默认敌人隔离、几何修复、固定Route 1、统一状态／空敌人自动化、地图LFS更新。 |
| `f7cde9fd61ce5eb2280471d88c14cca2800f4b35` | 同帧敌人拒绝修复、真实PIE重置验证、匹配像素和连续帧证据脚本。 |

两次运行代码检查点均完整构建后提交并立即推送原分支。最后另提交 `docs: close prop presentation comparison evidence and handoff`，只包含本报告、旧交接记录的过期提示、离线图像／日志审计脚本；完整SHA见最终答复和 `RetestGitClosure.log`，不把该文档提交当成另一套运行代码。

Git闭合前复核通过：仅允许的本机EngineAssociation GUID差异保留；LFS fsck OK；Git fsck退出0，但保留20个dangling对象，不执行清理。实验地图由LFS管理；无Saved、录像、截图、Binaries、Intermediate、DDC或AutomationReports提交。受保护地图、SightWeave插件及Darkwell.uproject的已提交差异均为空。
最后推送后再次逐项执行用户指定的status／diff／LFS／fsck／HEAD／upstream／ls-remote检查，结果保存在 `Saved/PropGameplayLab/RetestGitClosure.log`。
已打开正常实验地图编辑器并确认Play可用，未启动采集参数或固定步长；电脑保持开启，等待用户PIE选择，不把任何实验模式设为正式默认。

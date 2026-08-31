# DARKWELL 家具记忆玩法实验室

状态：**PARTIAL — READY_FOR_USER_PROP_GAMEPLAY_POLICY_LAB**。
本任务不选择最终家具规则，所有实验仍以真实 PIE 中的用户比较为准。

## 基线与隔离

- 基线分支：`codex/darkwell-project-fog-visual-rebuild`。
- 基线 SHA：`0d5e6cba7bb9ed4c2c605d7e9cf4281b38e902e1`。
- 实验分支：`codex/darkwell-prop-memory-gameplay-lab`，从准确基线创建并推送。
- 独立地图：`/Game/Maps/L_ProjectFogPropGameplayLab`。
- `L_Prototype`、`L_VisionIntegration`、SightWeave 插件和正式默认配置未修改。
- 复用 DARKWELL/SightWeave 的连续世界空间 LiveCoverage；无 screen-space composite、CustomStencil、黑色 Unknown 或材质裁切方案。
- 本机 `Darkwell.uproject` EngineAssociation GUID 差异始终保留，不提交。

**基线事实差异必须保留：**实际基线代码在发现同一物品 B 位置时已经更新唯一快照。
因此，用户要求的 VerifyOldLocation 被实现为独立的、仅限实验地图的旧快照保留分支。
“AcceptedWholeObject”只表示视觉对照组保持原门限／整物体呈现，并不声称新加入的 policy 0 就是基线原有的身份更新行为。实验地图之外保留基线行为。

## 地图结构

地图以厘米为单位，地面约 24×18 米，25 个家具 StableID。

| 区域 | 内容与比例 |
|---|---|
| L 形厨房 | 相邻矮柜 8+4 个，每个约 60×60×88；两段连续台面；冰箱 82×76×190；高柜 70×65×210；岛台 210×85×88 |
| 储藏区 | 开放货架 150×55×190；大、中、小箱；可移动柜 90×65×130；外形相同的 TwinA/TwinB，各自拥有不同 StableID |
| 替换对象 | ReplaceOld 柜子替换为尺寸、形状和 StableID 都不同的 ReplaceNew 货架；新对象默认未被记住 |
| 遮挡 | 门洞、墙角、104 高的半墙、柜后遮挡；8 条原生遮挡线段；可从角落、斜向和柜前平行路线观察 |
| 导航与威胁 | 保存的动态 RecastNavMesh、全场导航边界；Stalker 使用原有 C++ AI/感知与 NeverRemember 合同 |

固定脚本路线只在播放期间接管 Stalker 运动，回到 route 0 恢复原 AI。手动 PIE 中保留正常移动、工具、敌人和威胁 HUD。

## 两个独立维度

`r.Darkwell.ProjectFogVisual.PropPresentationMode`，默认 **0**：

| 模式 | 准确合同 |
|---|---|
| 0 AcceptedWholeObject | 保留原有物体采样门限：进入 .50、退出 .25；任一权威采样达到门限后，完整当前物体使用实时材质。对照行为不改成逐表面扫描。 |
| 1 SurfaceSweepHard | 已知且位姿、外观未改变的完整网格可在身份尚未进入 Live 时参与表面呈现。每像素 RawCoverage 决定实时材质与固定灰色的比例；整个网格保持不透明。这个操作不会授予身份 Live、不会启用 LiveOnly 效果。未知新位置或外观改变仍必须先通过身份确认。 |
| 2 SurfaceSweepSoft | 与模式 1 相同的原始覆盖和身份判断，只增加约 .20 秒的短视觉上升过程。输出为 `min(Raw, Soft)`，稳定后与硬扫描相同；Raw 丢失时立即归零，下降绝不越过当前合法覆盖。不会在已失去覆盖的位置保留淡出余光。 |

软化缓存只包含短期覆盖权重，不存颜色、灯光、阴影、敌人或物体变换。灰色部分的 BaseColor/Specular/AO 实时贡献为零，由曝光稳定、不参与 GI 的记忆显示提供颜色和固定几何明暗。灰色部分不靠 opacity mask 隐藏。

`r.Darkwell.ProjectFogVisual.PropRelocationPolicy`，默认 **0**：

| 策略 | 准确合同 |
|---|---|
| 0 VerifyOldLocation | 物体在视野外 A→B 后保留 A。先发现 B 也不会自动删除未验证的 A；重新观察 A、确认该物品已不在原处后清除旧快照。 |
| 1 RecognizedIdentityRelocation | 在 B 重新看到同一 StableID，立即更新到 B 并删除该身份的 A 旧快照；外形相同但 StableID 不同的家具不能清除 A。 |

身份与快照更新由 C++ 物体级采样和 StableID 记录决定，材质像素不拥有这些规则。两组 CVar 可独立组合。
退出实验地图、停止 PIE、重置地图都恢复 0/0；非实验地图的有效模式始终为 0。
实验控制仅在 Development/Editor、准确实验地图的游戏世界中生效，Shipping/Test 不启用。

## 用户 PIE 操作

1. 打开 `L_ProjectFogPropGameplayLab`，点击 **Play**。可按 **F11** 放大 PIE 视口。
2. WASD 移动，鼠标瞄准。用反引号／`~` 打开游戏控制台。
3. 先 `Darkwell.PropLab reset`，再设置需要比较的 mode 和 policy；重置会把两个值恢复为 0。
4. 顶部调试行与日志显示当前模式、策略、路线和事件。手动比较不受自动采证退出计时影响。

```text
Darkwell.PropLab help
Darkwell.PropLab mode 0        // 改为 1 或 2 比较呈现
Darkwell.PropLab policy 0      // 改为 1 比较身份更新
Darkwell.PropLab route 0       // 手动控制，恢复正常 Stalker AI
Darkwell.PropLab reset
Darkwell.PropLab fridge        // 冰箱 A→B
Darkwell.PropLab cabinet       // 可移动柜 A→B
Darkwell.PropLab destroy       // 销毁中号箱
Darkwell.PropLab replace       // 柜子替换为另一 StableID 的货架
Darkwell.PropLab swap          // TwinA / TwinB 交换位置
Darkwell.PropLab torch
Darkwell.PropLab lantern
Darkwell.PropLab dark
```

输入命令时只输入 `//` 之前的部分。独立事件试验之前先重置；移动／销毁／替换命令本身不会帮玩家转身，手动试验应先确保目标在视野外。
`dark` 清空 Torch/Lantern 可用能源，但不修改已验收基线的近身覆盖机制。

| route | 可复现观察 |
|---|---|
| 1 | Torch 横扫整排 8 个柜子 |
| 2 | 斜向扫冰箱，由局部逐步扫过表面 |
| 3 | 原地慢速旋转 |
| 4 | 沿柜子平行移动 |
| 5 | 冰箱视野外 A→B，先看空 A，再发现 B |
| 6 | 冰箱视野外 A→B，先发现 B，再看 A |
| 7 | Torch→Lantern→无合法光→Torch；最后 Torch 阶段含可见 Stalker/HUD 正对照，再回到柜后 |
| 8 | 相似家具交换位置，分别追踪 TwinA / TwinB |
| 9 | 视野外销毁箱子，再检查原位 |
| 10 | 视野外替换家具，再识别新身份 |

使用 `Darkwell.PropLab route N` 播放。路线 5/6/8/9/10 在约第 4 秒触发事件，约第 8 秒开始重新观察；5/6 约第 12 秒检查另一位置。路线结束后保持末端视点，输入 route 0 回到手动。

## 验证与证据

核心实现检查点：`a38dcfe3a17c544957c84a064e4a2a30b9a10e22`。

| 项目 | 结果 |
|---|---|
| `Scripts/BuildEditor.ps1` | Build16：DarkwellEditor Win64 Development 成功；正常 UBT 构建，非 Live Coding |
| C++ 自动化 | Automation07：15/15 通过，覆盖六种组合、已知几何与身份分离、新位置隐藏、旧位置顺序、相似身份隔离、销毁、默认隔离、Stalker/HUD/NeverRemember |
| 实际 GPU 材质读回 | MaterialGPU03：33 项检查通过，D3D12/SM6；包括不透明、部分覆盖、软硬稳定值相同、Raw 失效归零、12 步短上升 |
| 真实 PIE | PIE02：mode/policy 切换、柜子 A→B、重置原位和 0/0、停止 PIE 恢复 0/0、非游戏世界拒绝实验命令均通过；实际点击 Play/Stop，已打开 PIE 图片检查 |
| 最终方向矩阵 | Visual04：24/24 组通过，2298 帧；两个分辨率×三个呈现模式×四条运动路线。八张跨模式比较图已逐张打开检查 |
| 最终位置顺序 | Relocation03：16/16 通过，3026 帧；1080p 全六种组合×两种顺序，加 1440p 模式 2 的两种策略×两种顺序；八张策略比较图全部实际打开检查 |
| 最终动态事件 | Events03：10/10 通过，1900 帧；两种策略下的交换、销毁、替换、灯具切换，以及三个呈现模式的灯具／威胁正对照；十张 contact sheets 全部实际打开检查 |
| 最终完整性／severe scan | 50/50 组、7224 帧；文件数量／连续编号／尺寸／全时段／导航／D3D12/SM6/TSR 校验通过；无 LAB_CONTRACT_FAIL、Fatal、Assert、Ensure、设备崩溃、内存耗尽或材质编译失败；保留的非零启动错误详见下方 |

所有动态采集使用 D3D12、SM6、正常 TSR（`r.AntiAliasingMethod 4`），逐帧记录权威版本、敌人隐藏状态、HUD 条件和家具快照。
脚本路线先于权威计算执行，材质更新和截图在其后执行。截图 PNG 压缩在后台完成，进程退出前等待写入完成。

已实际查看 8 张呈现比较图、8 张位置策略比较图、10 张事件 contact sheets 和真实 PIE 图片。
观察到局部覆盖下仍保留完整灰色家具；先发现 B 时两种策略的 A 快照结果不同，先观察空 A 时都清除；箱子销毁、不同家具替换和敌人退出后没有保留敌人灰层。
这是对所列采样图片的检查，不宣称覆盖每个中间渲染帧或替代用户的最终体验判断。
全部 50 组均有 `review.mp4`，使用实际帧时间戳编码；原始 PNG、日志、索引及 `FinalEvidenceAudit.json` 均保留。

证据仅存于被忽略的 `Saved/PropGameplayLab`，不提交图片、录像、自动化报告、Binaries、Intermediate 或 DDC。
可用以下脚本复核；Pillow 用于生成 contact sheets，视频编码为可选项。

```powershell
./Scripts/BuildEditor.ps1
# C++ Automation RunTests 过滤器：
# Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1
& 'D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UE_projects/LastLight/Darkwell.uproject' -unattended -nop4 -nosound -d3d12 -sm6 '-ExecCmds=Automation RunTests Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1;Quit' '-TestExit=Automation Test Queue Empty'
# 定向动态矩阵示例；Label 必须未使用，已有证据不会被覆盖：
./Scripts/RunPropGameplayLabEvidence.ps1 -Widths 1920,2560 -Modes 0,1,2 -Policies 0 -Routes 1,2,3,4 -Label UserVisualReview
python Scripts/SummarizePropGameplayLab.py --label Visual04
python Scripts/ComparePropGameplayLab.py --label Visual04
python Scripts/ValidatePropRelocationEvidence.py --label Relocation03
python Scripts/ValidatePropRelocationEvidence.py --label Events03
python Scripts/ComparePropRelocation.py --prefix Relocation03
python Scripts/AuditPropGameplayLabEvidence.py
```

不把此轮方向采证声明为完整性能认证。未执行 BuildPlugin、Cook、Package、Shipping 或全项目性能矩阵，本轮没有把实验模式纳入正式合同。
呈现矩阵的实际采样限制：1080p 每条 100 帧；1440p 最少 76 帧，部分软扫描为 76–79 帧。
各运行中位采样间隔约 .097–.129 秒，最大间隔 .40 秒。视频按实际时间戳编码，不伪装成 60 fps；短过渡手感仍应在真实 PIE 中判断。

## 保留的失败、警告与边界

- Probe01：主菜单阻止路线；Probe02：旧 fixture 声明数量／缺少 Stalker ID 导致激活失败；已修正，原始失败日志保留。
- Visual01：同步 PNG 压缩拖慢采样，被替代；后续改成后台压缩。
- Automation03：测试世界缺少 Actor 初始化，未触发 EndPlay；补齐生命周期后 Automation04 及后续通过。
- MaterialGPU01：Python API 名称错误；MaterialGPU02：实际复现软化重复乘 Raw 的错误。修复后 MaterialGPU03 的 33 项检查通过。
- Visual02/Visual03、Relocation01、Events01/Events02 等早期图片保留为开发证据，不混入本轮最终矩阵；修正涵盖软化稳定值、脚本时序、几何呈现与身份分离。
- PIE01：启动脚本返回后编辑器自动退出；PIE02 使用官方 keep-alive API 完成真实 PIE 生命周期验证。
- 编译器版本高于 UE 推荐版本的提示、引擎头文件弃用警告保留。自动化中的重复 StableID 拒绝属于专门的回滚测试。
- 最终 50 组共 2000 行引擎可选 Python Toolsets 初始化 Error（每组 40 行，包括 traceback；缺少 AgentSkill、ToolsetDefinition、PythonTestRunner）。没有修改引擎或关闭插件来隐藏这些诊断。
- 最终 50 组另有 650 行 `LogAutomationTest: Error: Condition failed`（每组 13 行），全部位于 `Initializing Engine` 之前，紧随 UnifiedError 测试输出；Automation07 和 PIE02 启动也各有 13 行。本轮未修复这些引擎启动自检失败，不能称为全引擎测试通过；15/15 仅指所列 DARKWELL 自动化过滤器。
- EditorDataStorageUI 注册目的 0、MotionVectorSimulation 渲染线程标志、Scalability CVar 优先级警告保留。20 组记录到实验 `LabRoute` 达到 500 次 FindConsoleObject 查询的性能提示；这是保留的实验调试开销，未进行全性能验收。
- 最终 50 组各有一次世界清理阶段 CrowdManager 找不到 RecastNavMesh 的警告；运行中的导航投射每组均为 `LAB_NAV_READY=1`，不把退出阶段警告当作导航已完全无警告。
- 软化不会在 Raw 失效后继续淡出；这是防止灰区余光和拖影的硬边界。最终体验、家具规则和 policy 选择仍等待用户。

## 提交与 Git/LFS 闭合

每个可构建检查点均已提交并立即推送，未 merge、rebase、reset、clean 或 force-push。

| SHA | 内容 |
|---|---|
| `d18959d5a683b446339028391ab9990e31d903ea` | 独立原生实验室、地图、两组控制 |
| `02e345eddaa51a8c8cf210263bbe18157cef56bd` | 后台 PNG 采证，避免同步编码阻塞 |
| `4eb4226117a161fd72dab70159a16f0fb1db6728` | 生命周期、导航与六种组合验证 |
| `0a646effa8b00970c408432c5e41d5e91e469125` | 软化 Raw 权重修正及 GPU 读回回归 |
| `3bce5802ded052880257be654db9544c8976ee42` | 路线先于权威计算执行 |
| `a38dcfe3a17c544957c84a064e4a2a30b9a10e22` | 已知几何呈现与身份 Live 分离 |

最后另有报告与审计脚本提交 `docs: hand off verified prop memory gameplay policy lab`，不改变 C++、材质或地图；该提交的完整 SHA 在最终交接消息和本机 `Saved/PropGameplayLab/GitClosure.log` 中记录（避免提交自引用 SHA）。
地图和两个实验材质通过 Unreal Editor Python 创建／更新，并由 Git LFS 管理。
`git diff --check`、`git lfs fsck` 通过；`git fsck --no-reflogs` 返回成功但列出 19 个 dangling 对象，未清理或改写历史。
最终提交推送后再次执行用户要求的全部闭合命令，以 `GitClosure.log` 的 HEAD/upstream/远端 SHA 为准。
工作区预期仅有 `Darkwell.uproject` 本机 GUID 差异；该文件不在任何实验提交中。截图、录像及生成目录均不入库。

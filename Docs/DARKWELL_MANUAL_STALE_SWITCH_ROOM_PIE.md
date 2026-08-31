# DARKWELL 柜子残影开关房间

恢复基线：`1746c747a4136915350e559ebae2c517a183d5e0`。
分支：`codex/darkwell-prop-memory-gameplay-lab`。
地图：`/Game/Maps/L_ProjectFogPropGameplayLab`，没有新建分支或正式地图。

这是无限时的手动房间，不是原来的 36 秒路线。最终玩法 Mode 和 Policy 1 均未决定。

**PARTIAL — READY_FOR_USER_MANUAL_STALE_PROP_SWITCH_ROOM_PIE**

## 玩家操作

打开 Lab 后点击 Play，默认进入手动房间上层。WASD 移动，鼠标瞄准；先指向面前柜子，确认 HUD 为 `Cabinet Actual: PRESENT`、`Remembered Snapshot: VALID`。

从画面右侧门洞进入走廊，沿走廊向下，再经下层门洞进入下房。踩画面左侧的圆形开关：第一次柜子消失；站在开关上不会连发。走出圆盘范围再进入，柜子出现，后续一直交替。下房开关处柜子的合法覆盖必须为 0。

柜子消失后，自行沿右侧走廊返回上房，用任意角度和速度扫描旧位置。观察整件清除、局部残留和短渐隐的感受。再回下房让柜子出现，返回上房重新观察。房间没有倒计时、自动移动、强制朝向、自动完成或每帧复位；除了初次进入和明确 reset/teleport，代码不会重新放置玩家。

```text
Darkwell.PropLab stalemanual reset
Darkwell.PropLab stalemanual mode 0
Darkwell.PropLab stalemanual mode 1
Darkwell.PropLab stalemanual mode 2
Darkwell.PropLab stalemanual teleport top
Darkwell.PropLab stalemanual teleport bottom
Darkwell.PropLab stalemanual help
```

- `reset`：真实柜子恢复 PRESENT，清空本次观察记录和空位证据，开关重新 ARMED，计数归零，玩家回上房，满生命、Torch 100、Lantern 100、固定初始武器；**保留当前 Mode**，Policy 0、LabRoute 0。随后必须实际看柜子才能得到 VALID。
- `mode`：只改残影表现；不会改变柜子存在状态、开关次数、快照或累计空位证据。
- `teleport top/bottom`：仅方便用户手动测试，移动并朝向玩家，不改变柜子或记忆。bottom 落点在圆盘之外；需要自己向前走上圆盘。
- 命令必须在活动 Lab PIE World 中执行，Editor 的非 PIE 控制台不是有效游戏世界。
- 建议公平比较时用 `mode N`、`reset`，随后重复同样的自行走动过程；不会自动替用户重演。

Torch/Lantern 的消耗率只在该玩家实例 reset 时设为 0，避免长时间自由测试耗尽；不是每帧回填状态。不自动治疗，不自动改回柜子。工具的其他正式行为仍存在。

## 空间结构与隔离

新原生 Actor `ADarkwellManualStaleRoom` 放在 `(4000,0,0)`，与原自动实验区域分离。仅通过 Unreal Editor Python API 向已有 Lab 增加这个 Actor，原 25 件家具和自动路线资产均保留。

- 两房合计约 16.5 m 宽、16 m 深，中间 30 cm 厚、2.5 m 高的不透明墙；仅经右侧走廊连接。
- 走廊净宽约 2.7 m；上下门洞各约 2.4 m。中间隔墙与门框阻挡从开关到柜子的所有视线。
- 柜子 440×160×190 cm，统一身份 `Lab.ManualStale.Cabinet`；柜体、门和把手使用已审查的 Lab 家具绑定。
- 圆盘半径 100 cm，低矮、无阻挡碰撞。触发以玩家 XY 中心进入圆盘且位于同一楼层为准；必须退出后才能再次触发。
- 当前相机 yaw=90 时，世界 +X 对应画面左侧。因此最终柜子位于 `(4500,500,0)`，圆盘位于 `(4500,-450,0)`；右走廊在世界 X=3000–3300。第一次实际截图发现左右镜像后已按相机方向纠正，失败布局资料保留。

手动布局通过现有 Lab fixture 提供自己的 8 条遮挡线和 1 个地面描述，保持项目已有覆盖/遮挡权威与插件接口。原布局被停用时不参与碰撞/呈现，不改正式地图、SightWeave 插件、正式 0/0 默认或 uproject。

## 开关、实际物体和记忆的边界

`ToggleActualCabinet()` 只执行真实 Actor 的销毁或重建。销毁同步移除渲染、碰撞和占用；重建使用同一 StableID、位置和几何，先隐藏组件，等待正常合法 Live 权威。不存在一个可以碰撞的隐藏“已消失柜子”。

销毁通过正常组件注销只解除源 Actor；重建的注册关联到同一条保留记录。开关不调用 ClearMemory / InvalidateSnapshot / Freeze / Finish 等记忆捷径，也不写空位格子或不透明度。C++ Actor 实例可以更换，逻辑家具身份始终不变。

手动房间明确 opt-in 到原有 Lab 记忆接口：

1. PRESENT 的合法观察继续走正常对象身份权威，才能创建或更新快照。
2. ABSENT 不走普通 `.50/.25` 的旧位置清除路径；只由独立空位网格确认。
3. 每格采用已有 10 cm、四角加中心、合法覆盖 ≥ .99、无遮挡、无真实柜子占用、连续 .10 秒规则；单帧确认贡献上限 1/30 秒。
4. 这是有限采样的 2.5D 占用足迹实验，并非完整三维体积可见性证明。列状擦除和小台阶仍是可见实验参数。
5. 玩家未返回前消失又出现，保留原灰色快照和证据，没有凭系统知识制造虚假变化。重新合法看到真实柜子后才更新观察。
6. 已清空记忆后在下房让柜子出现，快照仍为空，源组件仍不可见；再次合法观察才能恢复实时呈现和记忆。

## 三种表现与中途切换

| Mode | 新鲜一轮的消除方式 |
|---|---|
| 0 | 所有占用格子合法确认为空（100%），整件残影一次消失；不是看到一角达到 .50 就清除。 |
| 1 | 已确认格子立即擦除，未观察或被遮挡格子保留。 |
| 2 | 使用 Mode 1 相同的空间证据，每格确认后约 .20 秒渐隐。 |

中途切换不会倒退玩家知识，也不会把已经擦掉的区域画回来。尤其从 Mode 1/2 切到 0 时，之前已擦掉的部分继续保持消失；Mode 0 的整件清除规则只作用于剩余部分。已经开始的 .20 秒渐隐会完成，不停在半透明状态。要比较 Mode 0 的完整整件跳变，请在新鲜 reset 后观察。

HUD 显示 Mode、Policy 0、Cabinet Actual、Remembered Snapshot、StableID、Old Occupancy Verified、Object Empty Confirmed、Pressure Switch、LiveCoverage、Source Live、切换次数和 ENEMY 0。只用于 Lab。默认没有敌人；手动房间拒绝旧 enemy/policy/route 变更命令。

## 原实验如何进入

```text
Darkwell.PropLab original
Darkwell.PropLab stale 0 C
Darkwell.PropLab stale 1 C
Darkwell.PropLab stale 2 C
```

`original` 重新进入原布局；也可以直接从手动房间执行 `stale N C`，加载原布局后启动原路线。原布局的 `reset` 保持原布局；`stalemanual reset` 回到手动房间。

已有采集脚本的 `StaleLabAuto` / `PropLabCapture` / `PropLabComparisonCapture` 参数自动选择原布局，保留原地面边界、相机、几何和路线。没有重采或覆盖先前 C/F 图片、录像和日志。

## 构建与验证

- `ManualSwitchBuild01` 标准 Editor Development 构建成功；实际图片发现布局左右镜像后，修正局部布局，`ManualSwitchBuild02` 最终构建成功。没有使用 Live Coding 作为最终证据。
- `Saved/AutomationReports/ManualSwitch02/index.json`：原 21 项及新增 2 项，共 **23 项通过、0 失败**（22 无警告、1 原有预期重复 Fixture 警告）。首轮 ManualSwitch01 也为 23 项通过。
- 新增 `Darkwell.PropLab.ManualSwitch.IsolationAndTenCycles`：十次完整循环、开关停留不连发、下房多朝向覆盖为 0、Mode 不修改存在/记忆、未观察重建不泄漏、原布局边界和非 Lab 隔离。
- 新增 `Darkwell.PropLab.ManualSwitch.ErasureAndModeChanges`：三模式从初始观察到视野外销毁、合法局部证据、整件/局部清除、离开不复现、再次出现与合法重新发现。
- `ManualSwitchPIE01.log`：首次真实 D3D12/SM6 PIE 功能通过，但截图左右与需求相反，因此不是最终布局证据。原图保留在 `Saved/PropGameplayLab/ManualSwitchPIE01/`。
- `ManualSwitchPIE02.log`：最终真实 D3D12 / PCD3D_SM6、TSR=4 Editor PIE **通过**。十个完整循环（20 次踩踏），开关处 coverage=0；Mode 0/1/2 合法扫描清除、未观察重建隐藏、合法重新发现均通过。局部已验证 72.2% 后在下房切换 0/2/1，证据和剩余 27.8% 不变，没有复现已清除区域。
- 测试对玩家造成 10 点伤害并等待 37 秒，生命仍为 90，柜子仍 ABSENT，Torch 仍 100；只有随后明确 reset 才恢复。证明确实没有原 36 秒自动完成或自动复位。
- 最终测试还使用真实 CharacterMovement 输入：中墙挡住直接向下行走；经右侧上门、走廊、下门可以走到圆盘，并由实际移动触发一次开关，覆盖为 0、快照保留。不是只靠 teleport 宣称路线可走。
- 已实际打开最终上房、下房踩踏截图及三模式部分擦除 contact sheet，确认柜子/圆盘在画面左侧、通道在右侧，HUD 与实际状态一致。图片为 Editor 窗口/PIE 视口截图，不冒称全屏 1080p 动态矩阵。
- 真实 PIE 图像和 42 条检查记录：`Saved/PropGameplayLab/ManualSwitchPIE_20260831_170029/`，包括 `checks.json`、`01_top_present00000.png`、`04_right_corridor_walk_switch00000.png`、`three_modes_partial_contact.png`。旧自动证据一律未重做或删除。
- 测试脚本暂时暂停测试 Controller 的鼠标朝向更新，由测试驱动定位/瞄准；退出前恢复。运行时房间始终为 LabRoute 0，自由输入规则没有修改。该脚本不能代替用户自己的无限时动态比较。
- 最新构建/资产编辑/自动化/PIE 的 severe 和未分类 Error 均为 0，但不是日志零错误：后三份日志各保留 13 条引擎启动 `Condition failed`。Warning 行分别为构建 6、自动化 4、资产编辑 4、PIE 7，包含引擎弃用、MSVC 推荐版本、NavMesh/Crowd、CVar 查找性能与渲染线程标记提示。完整清单为 `Saved/PropGameplayLab/ManualSwitchFinalLogInventory.json`；未扩修引擎或正式合同。

工具：`Content/Python/add_manual_stale_switch_room.py`（仅编辑独立 Lab）、`Content/Python/verify_manual_stale_switch_room.py`（真实 Editor PIE）；生成物全部留在忽略的 Saved/AutomationReports/DerivedDataCache，不提交。

Git 提交仅包含本手动房间相关原生源码、两个验证/编辑脚本、最小自动化、文档和已通过 LFS 管理的独立 Lab 地图。保留 `Darkwell.uproject` 本机 EngineAssociation GUID，不提交。最终 SHA / upstream / 远端和 LFS 核对记录见 `Saved/PropGameplayLab/ManualSwitchFinalGit.log` 及本轮用户报告。

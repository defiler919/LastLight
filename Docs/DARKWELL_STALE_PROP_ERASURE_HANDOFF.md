# DARKWELL stale prop erasure — 恢复验证交接

## 2026-08-31 恢复记录（优先于下方历史暂停记录）

用户已明确授权从 `f667527ce3fc3c3cd9db44a6fa4324a1b14381db` 和保留工作树继续。本次没有重新实现残影实验，没有变更空位判据、最终 Mode、Policy 1、正式地图或插件合同。

**PARTIAL — READY_FOR_USER_STALE_PROP_ERASURE_PIE**

C 单路线重复性、三模式同轨动态复证、标准构建、规则自动化和真实 Editor PIE 均已通过。可以开始用户人工 PIE 对照，但不代表选定最终 Mode，也不代表全部 A–F / 分辨率矩阵已验收。下方旧的 TIMING EVIDENCE PENDING 仅为历史状态。

### 权威起点与改动范围

- 恢复开始实际确认 HEAD / upstream / 远端均为 `f667527ce3fc3c3cd9db44a6fa4324a1b14381db`，当前分支仍为 `codex/darkwell-prop-memory-gameplay-lab`。
- 上次列出的 7 个保留文件全部存在，并与 `Saved/PropGameplayLab/StaleSafeClosePreserved.json` 的 SHA256 一致。未 restore / reset / clean / stash / 覆盖 / 丢弃。
- 本次没有新增运行时代码改动，沿用上次保留的 route 14 输入保护、实际朝向诊断、旧 SoftCoverage 隔离和对应测试。最小复现先使用已经通过 Build07 的本机 DLL，没有先构建或改代码。
- 验证脚本增加路线公式、帧间隔、扫描时相机固定检查；相邻帧标签改用日志时间，不能用 `帧号 / 30` 忽略初始偏移。日志审计只分类具体引擎启动错误，拒绝笼统忽略 Python 错误。
- 真实 PIE 脚本检查相机旋转、世界时钟与路线时间、三模式完成、无敌人、固定工具和 reset；不改变运行时规则。
- `Darkwell.uproject` 的 EngineAssociation 本机 GUID 保留且不提交。正式地图、SightWeave 插件和已有 LFS 材质未改动。

### 实际根因与最小复证

旧 C 三模式的 1080 个时间戳、相机位置和事件时刻原本已经一致；差异出现在玩家实际朝向。`LabRoute=0` 允许现有 PlayerController 在路线设定朝向后继续用鼠标瞄准覆盖它，进而改变合法覆盖与已验证格子。保留修正用内部 route 14 复用已有输入保护，结束恢复 0。本次证据支持修复的是**朝向输入干扰**，不声称发现或修复了引擎时钟漂移。

1. `StaleResumeProbe01_1920_M0_C`：单独 Mode 0 / C，1080 帧；全程原生朝向诊断失败 0，最大路线公式误差 `0.000104°`，相机位置 `(400,-828.273,1224.885)` 固定。
2. `StaleResumeRepeat01_1920_M0_C`：同一路线再跑一次，1080 帧全部元数据和格子 hash 与第一次完全一致。
3. 在重复性成立后才采 `StaleResumeProbe01_1920_M1_C` / `M2_C`，没有批量重跑 C/F 六条。
4. 三模式每帧玩家位置、实际 yaw、相机、时间、阶段、empty、验证比例和格子 hash 完全一致。首帧 `0.066667 s`，末帧 `36.033127 s`，固定步长 1/30；变更事件 `8.033327 s`，全格确认 `22.999992 s`。

GPU 采集均为 1920×1080、D3D12 / PCD3D_SM6、TSR (`r.AntiAliasingMethod=4`)。三模式均 ENEMY 0、Torch 100，移动后的 B 没有进入 Live，已验证比例单调且视野离开后不恢复。

### 实际画面审查与准确合同

已实际打开三模式并排 `1920_C_matched.png`（含约 25% / 50% / 75% 同帧对照）、`1920_C_adjacent.png` 的 9 个连续帧，以及原始 `frame_0480.png`。视频由同一批真实帧组成，没有生成替代玩法画面。

| 共同空位证据 | 时间 | Mode 0 | Mode 1 | Mode 2 |
|---|---|---|---|---|
| 约 25% | 12.97 s | 完整灰色长柜 | 左侧已验证部分擦除 | 相同区域约 .20 秒渐隐 |
| 50% | 16.10 s | 完整灰色长柜 | 半件灰色残影 | 同一擦除边界的短渐隐 |
| 约 75% | 19.30 s | 完整灰色长柜 | 约四分之一残影 | 同一证据，短暂透明过渡 |
| 100% | 约 23 s 起 | 整件一次消失 | 剩余格子擦除 | 最后格子完成短渐隐 |
| 离开视野 | 34 s | 不恢复 | 不恢复 | 不恢复 |

Mode 0 的消除仍要求独立空位网格 **全部确认**，不是普通呈现 `.50/.25`；Mode 1/2 同一空间证据，已经消除的格子永久清除。相邻帧可见 Mode 2 的短透明边缘，没有看到整件反复出现；10 cm 格子边缘的小台阶仍可见。是否接受“半个柜子”和“幽灵溶解感”留给用户真实动态 PIE，不由代理决定。

### 构建、自动化、失败与日志

- `StaleResumeBuild01.log`：仅一次 `Scripts/BuildEditor.ps1 -Configuration Development`，**Succeeded**；目标 up to date / 0 actions，沿用已编译的保留源码，不把它说成一次全量重编译。保留 MSVC 14.51 新于引擎推荐 14.50 的警告。
- `Saved/AutomationReports/StaleResume01/index.json`：**21 项通过，0 失败**（20 无警告、1 有预期警告）。过滤 `Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1`；预期警告仍为 `DuplicateFixtureRollback`。
- 三模式 GPU 日志审计：**severe 0、未分类错误 0**，但保留 **159 条已分类启动 Error、75 条 Warning**。Error 为启动 Condition failed / Experimental Toolsets 缺少 Python 属性；不能报告日志零错误。全量清单在 `StaleResumeProbe01_Review/log_inventory.json`。
- `StaleResumePIE01.log`：Mode 0 已正常结束，检查脚本却把 `ghost=0` 的末尾 `t=0` 当作时间，错误检查结束后相机。正则增加单词边界；运行时代码未改。
- `StaleResumePIE02.log`：实际 `t=9.995548` / yaw=-90，HUD 四舍五入为 `10.00`，脚本误用下一阶段 yaw。脚本改为检查 HUD ±.005 秒舍入区间两侧；原生全精度 .01° 朝向诊断不变。对这两次现有日志的 **2075 帧离线回放通过**，并确认原有鼠标覆盖差异仍会被拒绝。不是再次发现无法解释的时钟漂移。
- `StaleResumePIE03.log`：真实 Editor PIE、D3D12/SM6、TSR 通过。Mode 0 / 1 / 2 分别记录 1858 / 1937 / 1798 个可变帧率游戏帧，各运行约 36 秒；这里不冒充固定帧率同轨采集。相机旋转 `(-65,90,0)` 和位置检查、route/world 时钟偏移检查通过；三轮均 empty=1、ghost=0、恢复家具 25。policy 1 / enemy 1 在专用路线中被拒绝；reset 经过实际伤害和 dark 后恢复满生命、Torch 100、0/0、route 0、ENEMY 0；结束 PIE 成功。
- 最新构建 / 自动化 / PIE03 severe 均为 0。自动化、PIE03 各保留 13 条引擎启动 Condition failed；Warning 分别为构建 1、自动化 4（重复 Fixture 在不同日志类别重复输出）、PIE03 8。PIE 包含 NavMesh/Crowd、MotionVectorSimulation 线程标记、CVar 查找性能警告；未扩修这些正式或引擎问题。清单为 `StaleResumeVerificationLogInventory.json`。PIE01/02 失败仍保留，不计入通过结果。
- 上次 `StaleFinal01`、`StaleSmoke01`、构建/材质/测试失败记录全部保留；旧 C/F 六次采集仍无效，不以本次 C 通过追认旧 F。A/B/D/E/F 和 1440p 本次没有重采，不宣称完整动态矩阵通过。

新证据均在本机忽略目录 `D:/UE_projects/LastLight/Saved/PropGameplayLab/`：

- `StaleResumeProbe01_check.json`、`StaleResumeRepeat01_check.json`。
- `StaleResumeProbe01_1920_M{0,1,2}_C.log` 及同名图像目录，`StaleResumeRepeat01_1920_M0_C.log`。
- `StaleResumeProbe01_Review/1920_summary.json`、`1920_C_matched.png`、`1920_C_adjacent.png`、`1920_C_three_modes.mp4`、`log_inventory.json`。
- `StaleResumeBuild01.log`、`StaleResumeAutomation01.log`、`StaleResumePIE01.log` / `02.log` / `03.log`、`StaleResumePIEScriptReplay.json`、`StaleResumeVerificationLogInventory.json`。

### 用户 PIE 操作与提交边界

打开 `/Game/Maps/L_ProjectFogPropGameplayLab`，点击 Play，在**活动 Lab PIE** 控制台依次输入：

```text
Darkwell.PropLab reset
Darkwell.PropLab stale 0 C
Darkwell.PropLab stale 1 C
Darkwell.PropLab stale 2 C
```

reset 后等地图重新进入 PIE。每条 stale 等约 36 秒、看到 `ROUND FINISHED` 后再输下一条；用相同 C 路线比较。顶部应显示对应 Mode、Policy 0、ENEMY 0；不要用上一轮普通家具的 mode 命令代替 stale 命令。需要中止或重来使用 reset；help 列出 A–F。用户重点判断整件突然消失、半件残影及短渐隐的动态感受，不由自动截图替代。

本次恢复检查点由包含本节的提交标识，提交并立即推送当前分支。仅提交已验证的 3 个 C++ 文件、3 个验证脚本和两份文档；不夹带 uproject / 地图 / 插件 / Saved / 构建产物。开始提交前 `git diff --check` 和 `git lfs fsck` 通过；最终 Git 核对另存 `Saved/PropGameplayLab/StaleResumeFinalGit.log`，最终 SHA 以该记录和用户报告为准。

完成推送后打开 Lab Editor 等待用户 Play；保留电脑开启。不启动其他案例、Policy 1 决策或正式地图施工。用户下一轮可说：“从当前已推送检查点继续，先读取本交接顶部恢复记录和我的 PIE 反馈；不要重新重跑旧 C/F 矩阵或选择最终 Mode。”

### 复现命令（后续明确授权验证时使用，不自动扩跑）

```powershell
# 全新 Label，先只跑一个 Mode / C；不得用默认参数扩跑 A–F。
./Scripts/RunStalePropLab.ps1 -Widths 1920 -Modes 0 -Cases 2 -Label NewUniqueProbe -ZenDataPath D:/UE_projects/LastLight/DerivedDataCache/StaleLabZen
# 单路线可信之后才补相同 Label 的 Mode 1 / 2。
./Scripts/RunStalePropLab.ps1 -Widths 1920 -Modes 1,2 -Cases 2 -Label NewUniqueProbe -ZenDataPath D:/UE_projects/LastLight/DerivedDataCache/StaleLabZen
python Scripts/ReviewStalePropLab.py --label NewUniqueProbe --width 1920 --cases C --video
python Scripts/AuditStalePropLab.py --label NewUniqueProbe
```

本机验证 Python 为 `C:/Users/defiler919/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`，Review 需要 Pillow；视频工具已在忽略目录，不安装新全局依赖。

---

# 历史记录：上次安全收尾（保留原文，不代表当前恢复状态）

记录时间：2026-08-31，Asia/Shanghai。

**PARTIAL — STALE PROP ERASURE LAB CHECKPOINTED / TIMING EVIDENCE PENDING**

用户已明确要求停止实现、修复、构建、测试、自动采证及继续追查时钟差异。本次收尾只核实已有状态、检查遗留进程、保存交接文档。未选择最终 Mode，未进入正式地图施工，未决策 Policy 1。后续代理不得把本文当作自动恢复施工的授权。

## Git 与代码检查点

- 工作区：`D:\UE_projects\LastLight`。
- 分支：`codex/darkwell-prop-memory-gameplay-lab`。
- 实验起点：`b3bafd34ddb1c2b0431f287dc5b154bdeb05f4df`。
- 已推送的实现检查点：**`9023a8f43e7b3b6972d00cf60df414ac188bbe93`**，`Add isolated stale-prop empty verification PIE lab`。
- 收尾开始时实际查询的 HEAD、upstream 和远端分支 SHA 三者均为上述 `9023a8f...`，暂存区为空。
- 本次只将**本交接文件**作为单独文档提交推送；文档提交不会改变代码检查点。不要把新的文档 HEAD 当作后续源码修复已经提交。
- 当前工作树故意保留后续未提交修改；不是干净工作树。禁止丢弃、restore、reset、clean、覆盖、切换分支、merge、rebase 或 force-push。
- 未修改受保护正式地图、SightWeave 插件或正式默认合同。新增实验材质已在 `9023a8f` 中通过 Git LFS 提交推送。

## 已实现，但尚未完成玩法验收

`9023a8f` 包含独立 Lab 原生组件、10 cm 旧占用足迹网格、独立合法空位证据、三个残影消除方式及 A–F 六个案例。空位证据与普通家具 `.50/.25` 呈现门限分开；固定 Policy 0，移动后的 B 在路线视野范围外。实验期间无敌人、工具状态固定，HUD 显示阶段、StableID、验证比例、物体级 empty、残影和原因；提供 `Darkwell.PropLab stale N [A..F]`、`reset`、`help`。

空位规则是有限采样的 **2.5D 占用足迹实验**，不是完整三维体积可见性证明：每格四角与中心均取得连续合法覆盖 ≥ .99、且无真实实验家具占用，短暂确认后永久记录为空。Mode 0 等全部格子确认再整件消失；Mode 1 局部擦除；Mode 2 使用同一证据约 .20 秒渐隐。参数和最终玩法均未由代理决定。

已有实现文档：`Docs/DARKWELL_STALE_PROP_ERASURE_PIE.md`。其中命令和设计描述不代表已经达到“可公平人工对照”的交付状态；以本交接文件的暂停状态为准。

## 保留的未提交文件

以下文件全部保持收尾前内容，不夹带进文档提交：

| 文件 | 实际修改与不提交原因 |
|---|---|
| `Source/Darkwell/Private/VisionPresentation/DarkwellStalePropLabComponent.cpp` | 尝试用内部 route 14 复用已有鼠标输入保护，停止时恢复 route 0；缓存 CVar 指针；增加实际朝向被覆盖的诊断。虽然 Build07 / Automation04 通过，但未重新取得三模式动态一致性证据，属于未完成修正。 |
| `Source/Darkwell/Private/VisionPresentation/DarkwellPropGameplayLab.cpp` | 残影实验期间跳过原有实时材质 SoftCoverage 更新；与未完成隔离修正一同保留，不拆分冒充已闭合结果。 |
| `Source/Darkwell/Private/Tests/DarkwellSightWeaveAdapterTests.cpp` | 新增 route 14 启动、停止恢复 route 0 的断言；通过自动化，但不能证明实际动态扫描一致。 |
| `Docs/DARKWELL_STALE_PROP_ERASURE_PIE.md` | 两条未提交失败记录：测试原点误判，以及 StaleFinal01 动态证据不一致。保持原样，本次另写交接文件。 |
| `Content/Python/verify_stale_prop_lab_pie.py`（未跟踪） | 已写但未运行的真实 Editor PIE 命令、生命周期和复位脚本；没有对应通过证据。 |
| `Scripts/AuditStalePropLab.py`（未跟踪） | 已写但未运行的日志清点脚本；其中启动 Python 错误分类仍较宽泛，不能据此宣布 severe scan 已闭合。 |
| `Darkwell.uproject` | 必须保留的本机差异：`EngineAssociation` 从 `5.8` 到 `{1C0F19FD-493D-6BCD-A0CC-9FAB451BA183}`。不得提交、恢复或覆盖。 |

**本机编译产物已包含未提交源码。** 当前 DLL 对应 Build07，不等同于远端 `9023a8f` 的源码状态；不能仅看 HEAD 判断本机将运行哪个版本。未提交文件目前只在本机，文档提交不会把它们备份到远端。

## 时序 / 公平证据未闭合

停止前已有读取结果如下，不在收尾阶段继续追查：

- `StaleFinal01` 取得了 1080p、D3D12/SM6、TSR 的 C / F × Mode 0 / 1 / 2 六次采集，运行脚本均打印了完成。
- 三模式对照检查却在 `Scripts/ReviewStalePropLab.py` 断言失败：`different spatial evidence between modes`。
- C 的首个已记录差异在帧 **579**：脚本时间均为 **19.366714 s**，玩家位置和相机相同；Mode 0 实际 yaw 为 **89.832558**、验证比例 **0.853896**，Mode 1 / 2 yaw 为 **91.100624**、验证比例 **0.859091**，已验证格子的 hash 不同。
- 因此，已证实的是**实际朝向与空间证据不一致**。这不是已经证明的纯时钟漂移，也不能宣称“时钟同步已修复”。停止前发现原 `LabRoute=0` 仍允许鼠标瞄准更新，后续 route 14 修正只通过构建和规则测试，尚无动态复证。
- `StaleFinal01` 不能作为公平对照交付。其文件名中的 `Final` 只是采集标签，不代表验收通过。
- `StaleSmoke01` 另有一轮 Mode 1 / C 采集，初始呈现隔离尚不充分，同样不作最终证据。代理曾实际打开其 `frame_0480.png`，看到局部灰色残影；这不等于三模式并排、匹配帧、连续帧审查完成。
- 尚未完成：修正后的 C 三模式重采与一致性验证、其他 A/B/D/E 场景动态采集、1440p 定向证据、可靠并排 contact sheets / 视频审查、真实 Editor PIE 脚本验证、最终 severe log 闭合。

**用户人工 PIE 还不能作为可靠的三模式对照开始。** 当前不是 `READY_FOR_USER_STALE_PROP_ERASURE_PIE`。技术上已有实验入口，但公平性和本机未提交版本尚未闭合；本次不启动 Editor，也不让用户据此选择最终模式。

## 已有构建与自动化结果（仅读取历史，不重跑）

所有构建均为 `Scripts/BuildEditor.ps1 -Configuration Development`，目标 `DarkwellEditor Win64 Development`。

| 记录 | 结果和范围 |
|---|---|
| StaleBuild01 | 失败：`auto*` 无法从 `TObjectPtr` 推导组件指针；后续已修正。 |
| StaleBuild02–07 | **6 次构建成功**。Build05 对应提交前检查点；最新 Build07 对应保留的未提交修正，不能代替动态验收。 |
| StaleAutomation01 / Stale01 | 20 项通过：19 无警告、1 有预期警告，0 失败。 |
| StaleAutomation02 / Stale02 | 20 项通过、1 失败；新增测试把主体观察中心 Z=45 当作 Actor 地面 Z=0，随后修正测试比较点。 |
| StaleAutomation03 / Stale03 | 21 项通过：20 无警告、1 有预期警告，0 失败；支持 `9023a8f` 实现检查点。 |
| StaleAutomation04 / Stale04 | 21 项通过：20 无警告、1 有预期警告，0 失败；含未提交的 route 14 启停断言。 |

有警告但通过的项目：`Darkwell.SightWeave.M6P1.Lifecycle.DuplicateFixtureRollback`，1 条预期重复 Fixture 回退警告。

自动化过滤为 `Darkwell.PropLab+Darkwell.FogVisual+Darkwell.SightWeave.M6P1`，在 NullRHI Editor 自动化中运行。上述计数不是 GPU 动态验收计数。退出码为 0 也不能单独证明测试通过，StaleAutomation02 的报告明确有失败。

其他保留失败 / 环境警告：

- StaleMaterial01：默认纹理和 Linear Grayscale sampler 不匹配。StaleMaterial02 加载旧资产时仍记录旧编译警告，随后重建保存了专用材质。
- C 盘当时约剩 1.6 GB，默认 Zen 返回 HTTP 507；后续运行仅通过进程参数 `-ZenDataPath=D:/UE_projects/LastLight/DerivedDataCache/StaleLabZen` 改用 D 盘忽略目录，没有改全局配置或删除旧数据。
- 已有日志还包含引擎启动 Condition failed、Experimental Toolsets Python 属性缺失、工具链版本 / 弃用提示、CVar 查找性能警告和退出时 RecastNavMesh 警告。完整分类尚未闭合，不能报告“日志零错误”。

## 日志与证据位置

均为本机忽略目录，不提交截图、录像、缓存或 AutomationReports：

- `D:\UE_projects\LastLight\Saved\PropGameplayLab\StaleBuild01.log` … `StaleBuild07.log`
- 同目录 `StaleAutomation01.log` … `StaleAutomation04.log`，以及各自 `.console.log`
- `D:\UE_projects\LastLight\Saved\AutomationReports\Stale01\index.json` … `Stale04\index.json`
- `Saved/PropGameplayLab/StaleMaterial01.log`、`StaleMaterial02.log` 及 `.console.log`
- `Saved/PropGameplayLab/StaleSmoke01_1920_M1_C.log` 和同名图像目录
- `Saved/PropGameplayLab/StaleFinal01_CF.runner.log`
- `Saved/PropGameplayLab/StaleFinal01_1920_M{0,1,2}_{C,F}.log` 和六个同名图像目录
- 对照检查失败发生在 hash 断言，不能把 `StaleFinal01_Review` 目录存在当成 review 已完成。
- 本次最终 Git / 进程检查记录放在 `Saved/PropGameplayLab/StaleSafeCloseGit.log`、`StaleSafeCloseProcesses.json`。

## 进程与安全收尾

2026-08-31 15:19（本机时区）读取进程名、完整命令行、父 PID 和创建时间：没有仍在运行的本任务 UnrealEditor、UnrealEditor-Cmd、UBT / dotnet、ShaderCompileWorker、采证脚本、ffmpeg；扩展检查也未发现 Zen、UBA 或 UnrealTraceServer 遗留。无需终止任何进程。

保留无关的 Chrome / Codex 工具宿主、早于本轮启动的 PowerShell 和 Godot Python 服务，不根据进程名称批量结束。此次未启动新构建、测试、采证、Editor，未关机。最终进程清单只用于确认，不触发任务重启。

## 下次恢复

先由用户明确重新授权继续。推荐下一条对话命令：

> 继续 DARKWELL 灰色残影实验。先读取 Docs/DARKWELL_STALE_PROP_ERASURE_HANDOFF.md，核对 Git 和保留的未提交修改，从 9023a8f 实现检查点及当前工作树继续。只处理尚未闭合的三模式时序和动态证据；不得丢弃修改，不得选择最终 Mode 或进入正式地图。

恢复时先做只读检查：

```powershell
Set-Location D:\UE_projects\LastLight
git status --short --branch
git rev-parse HEAD
git rev-parse '@{upstream}'
git ls-remote origin refs/heads/codex/darkwell-prop-memory-gameplay-lab
git diff -- Source/Darkwell Docs Content/Python/verify_stale_prop_lab_pie.py Scripts/AuditStalePropLab.py
Get-Content Docs/DARKWELL_STALE_PROP_ERASURE_HANDOFF.md
```

审查未提交输入保护与诊断、确认本机 DLL 对应源码后，再按新的授权决定最小验证步骤。若重采，应使用**全新标签**，先完成同一 C 路线三模式逐帧时钟、相机、实际朝向和格子 hash 一致性，再考虑其他案例，不能仅凭脚本打印 PASS 或截图数量升级状态。现有失败资料全部保留。Build / test / capture 命令不在本次收尾执行。

在公平对照和真实 PIE 验证补齐前，维持本页的 PARTIAL / TIMING EVIDENCE PENDING 状态。

# 灰色层功能检查点与稳定化施工

2026-09-06 开始，状态：施工中；性能、首次/批量尖峰、长期资源和 Editor 退出均未验收。不得据此创建发布 stable 或开始黑色层。

## 起点和人工验收边界

- 实际起点：`a6e4324839acb9b3be7e7c5ea14c0bb4d994735b`，本地/上游/实时远端一致，工作树及暂存区为空，stash 列表为空，`git diff --check` 和 `git lfs fsck` 通过。
- 用户在本任务明确确认该交接状态的 Whole 达标交接退灰测试通过，并表示当前人工路线灰色层行为可接受。最终运行时是 `a4a17a412a35d2e9a0c7dd8917965d6e8c9323f5`；a6e4324 为文档交接，不能说它又修改了运行时。
- 验收覆盖用户的 Whole 局部→整件达标连续显示人工路线及其目前使用过的灰色层交互；不是所有几何的穷尽证明。上一轮自动化 142/142、连续帧 oracle 8632/8632 是既有证据，不能作为本轮重跑结果。
- 两条远端 stable 固定为 `stable/sightweave-gray-core-20260903` → `7534163b9c5718700b610e7677f47fbaa79cf977`、`stable/moving-history-grid-v2-20260902` → `404a5820739638f1097eaae0aa7fba19733298c3`，本轮不移动。
- 非发布 tag `checkpoint/gray-functional-accepted-20260906`：启动时本地及远端不存在，拟指向 a6e4324；注释为“灰色层功能人工验收检查点；性能和退出稳定性仍开放；不是发布 Stable。”
- 实际引擎 `D:\UE_5.8`：Build.version 为 **5.8.2 CL56702186**。AGENTS 的 5.8.1 是环境说明差异，实验按实际二进制记录。

## 冻结的外部规则

1. Whole 每次连续合法观察都用对象自己的 MinimumObservedSpanCm（默认约 100 cm）重新确认；旧历史/缓存不授予资格，不跨轮累计。真正失联才结束；无效 Coverage、revision mismatch、未就绪不等于失联。达标后仍有合法接触就保持整件。
2. 局部→Whole 达标交接必须承接已显露表面：首次与再确认均不得退灰、消失、重新点阵、空白帧或整体闪烁。
3. 合格观察先完成正确历史交接再结束资格；首次离开不空白、后续不回退残缺历史。未达标不能生成完整 Whole 历史、覆盖或删除旧合法知识。
4. 视锥/距离/墙体控制合法接触和跨度；Whole 达标后的统一展示不再按玩家到墙边界逐块裁切，相机深度遮挡保留；不得穿墙发现、扩张地面探索或确认其它/完全不可见对象。
5. SpatialPartial 看到多少显示多少，允许的历史累计局部；补全无内部旧接缝，未观察区域不出现，合法外切口和深灰 cap 保留。
6. StationaryOnly 移动中 Live 但不生成运动历史；隐藏停止不自动记终点，必须重新合法观察。Never 不留灰影；Always 保持兼容语义。
7. StableID、真实 Transform、内部销毁、隐藏换模型/颜色不等于合法反证。Superseded 不伪装 VerifiedEmpty；合法擦除不复活，重建不读取未见隐藏状态。
8. 相同未反证状态允许复用捕获/proxy/texture/geometry snapshot，不无意义逐轮增长；真实新增未解决知识允许增长，不为常量内存丢弃。

## 施工与验收协议

先建立退出最小复现和受控性能入口。所有样本独立命名并保留失败；图像、功能、退出分开判定。关键同条件前后实验串行重复至少三次，记录实际游戏视口、质量、运行方式、前后台和硬件。冷启动及初始化帧单独保留，不藏进预热。

完整帧目标 p95 ≤16.6 ms、p99 ≤33 ms；普通交互不得可重复 >100 ms 或持续慢帧。Editor PIE 与 Standalone 各自报告；模拟 54,000+600 与至少十分钟真实 D3D12 wall-clock 各自报告。若空场景失败先归因基础成本。

禁止强杀/立即 ExitProcess/忽略退出码伪装稳定，禁止修改系统配置、降低画质、改变规则或牺牲合法知识。所有有效阶段明确暂存、commit、push 后核对 local/upstream/remote。生成证据保留 Saved，不提交资产或生成目录。

## 本轮结果（持续更新）

| 维度 | 状态 | 本轮证据 |
| --- | --- | --- |
| FUNCTIONAL REGRESSION | PARTIAL | 人工功能检查点已记录，待本轮受控完整回归 |
| EXIT STABILITY | PARTIAL | 既有间歇 0xC0000005 开放；旧 Slate 栈不是已确证根因 |
| FRAME PERFORMANCE | PARTIAL | 历史 23.950→44.743 ms 不具严格同视口对照，待新基线 |
| INITIALIZATION / BATCH HITCHES | PARTIAL | 待冷/热与真实有效历史数量复测 |
| LONG-RUN RESOURCES | PARTIAL | 待本轮模拟长测和十分钟真实运行 |

本检查点只新增文档，不改 C++、构建配置、插件或资产，因此无需在该文档阶段重新 BuildPlugin；正式运行前核验/构建 Editor。


## 阶段 1：执行器完成与退出所有权

原 `RunGrayMemoryAudit` 图形驱动把 `set_keep_python_script_alive(True)` 保留到退出，却直接发送 `CLOSE_SLATE_MAINFRAME`。实际引擎源码 `EditorPythonExecuter.cpp` 显示：正常 keep-alive 结束会先 DestroyNotification 再排入 QUIT_EDITOR；提前请求引擎退出会让执行器 Tick 跳过该分支，Notification 到 Python 插件 OnShutdownModule 才 SetComplete。`LaunchEngineLoop.cpp` 的顺序是初始 Core ticker Reset → GEngine PreExit → Slate Shutdown → AppPreExit / 模块 Shutdown。Slate async notification 的 UpdateNotification 会把强持有 OwningNotification 的委托排回 ticker。

这解释了与旧 dump 相符的迟到通知→文本析构链。进一步反汇编核对 Core 的队列/函数对象析构与 Slate 捕获参数的析构位置，不能把旧 nearest-export 名称（例如 HazardPointer）当精确函数名。**目前还没有新的失败 dump 把原通知标题或生产者直接读出，不能扩大为每个 0xC0000005 都已证明同根因。** 新造的晚通知实验未稳定触发 AV，其正常结果也保留。

修复已有 audit/gray performance 驱动：原回调注销和 PIE 停止流程完成后，设置 keep-alive=False，把完成/关闭交还执行器；不强杀、不延迟很多秒、不忽略退出码、不清空全局 ticker，不改玩法。受控 `-LegacyDirectClose` 能在同一版本/同一路线上恢复旧关闭方式作为负对照，默认关闭。

| 本轮样本 | 调试器 | 真实退出码 | 内容 |
| --- | --- | --- | --- |
| ExitBefore_Main01 | 无 | 0 | 无 Python，真实主窗口 Alt+F4，未进 PIE |
| ExitBefore_Python0/1/2_01 | 无 | 各 0 | 旧直接关闭；0/1/2 次 PIE 最小路径 |
| Stabilization_ExitBefore_Contracts01 | 无 | **0xC0000005** | 178 帧、3 PIE 完成、severe 0，退出失败 |
| Stabilization_ExitBefore_ContractsDebug01 | 退出前附加 | 0 | 同旧 Contracts；不能覆盖非调试器失败 |
| Stabilization_ExitAfter_Contracts_013849 / _014027 | 无 | 各 0 | 修复后原 Contracts，59.919 / 58.935 秒 |
| Stabilization_ExitAfter_Episodes_013950 / _014127 | 无 | 各 0 | 修复后原 Episodes，35.915 / 43.173 秒 |

运行目录均在 Saved/ArchitectureAudit；最小路径在 Saved/Stabilization。新造通知探针 ExitNotification_Before01、Before02、LateDebug01 均退出 0（最后一项调试器从启动附加），不算已重现根因的证明。退出验收继续开放，需追加最终正常主窗口、多 PIE 和失败路线覆盖。

阶段 1 只提交驱动所有权修复与此处进度，场景/资源算法不变。性能元数据与 Editor 专用诊断入口仍在下一阶段施工；已有烟测包含脚本变量错误、Standalone 内置 ToolsetRegistry PythonTestRunner 初始化错误和后台 PIE，均明确不是正式基线。单独禁用 ToolsetRegistry 会被依赖重新启用，不能声称该开关已生效。当前机器 CPU Ryzen 9 3900X、GPU RTX 2070 SUPER、驱动 32.0.16.1088、RAM 34,305,445,888 字节。用户已退出 SpaceCraft，后续正式实验串行独占 UE/构建负载。

## 阶段 2：中断恢复与可构建测量入口

用户实体 Esc 中断后恢复，起点仍为 `2344b1a4829e4ca4ac60250cedc4df453ee34ea2`，fetch 后 local/upstream/remote 一致。未提交文件均来自本轮，未使用 reset/restore/clean/stash。没有残留 UE、Python、Trace、UBT 或测试进程；用户自己的普通 PowerShell 保留。LFS fsck 通过。

- `Scripts/TestManifests/GrayFunctional.json` 固定原 142 项完整名称与 selector；`RunGrayFunctionalRegression.ps1` 验证新报告不得遗漏原名。`Stabilization_HarnessFunctional` 实际 142 项（131 clean、11 warnings）、0 failed、0 not-run、severe 0、exit 0，194.741 秒；coverage 文件确认 missing/new 均空。
- `RunGrayExitProbe.ps1` / `audit_gray_exit.py` 保存最小 0/N PIE 生命周期、真实退出码及源码状态；MainWindow 模式用于外部真实主窗口操作。
- `RunGrayPerformanceBaseline.ps1` / `profile_gray_stabilization.py` 保存逐帧 JSONL、冷启动、case setup、资源与质量/硬件/进程元数据，支持 PIE、Standalone、Smoke/Matrix/LongRun 和独立 Trace。尚未完成正式矩阵/长测验收。
- 新 `DarkwellEditor` 模块只承载临时 PIE 浮动窗口、实时视口 override 恢复和退出顺序诊断；uproject/Editor target 增加该 Editor 模块，SightWeave 插件无改动。人工晚通知 probe 仅显式参数启用，已有负结果保留，不作为退出修复。
- Runtime Lab 新只读实际视口/窗口、CVar 和线程/GPU计数接口。计数异步且不可相加；特别是 PIE 的全局 Render/RHI 读数可能被后续 Slate 窗口更新覆盖，不能当该游戏帧精确归因。下一阶段用独立 Insights 样本核对。
- Smoke 实际 viewport 1920×1080、SP100、TSR、sg 全 3、硬件光追和 VSM 开启。Standalone 空场景 p95 约 28–30 ms；浮动 PIE 的 OS foreground 仍为 0（即使 Slate window_active 为 1），因此不是正式前台 baseline。所有异常烟测保留，不能选最后一次代替对照。
- `-NoAuthoringToolsets` 是显式诊断环境，逐次记录全部禁用项；解决 UE 内置 ToolsetRegistry 在 Standalone `-game -EnablePython` 中引用缺失 PythonTestRunner 的初始化错误。普通项目配置不改变，该环境不能冒充默认 Editor。

`Scripts/BuildEditor.ps1` 恢复检查成功（Saved/Logs/Stabilization_ResumeCheckpointBuild.log，1.32 秒 up-to-date；此前对应 C++ 完整构建 6.86 秒成功）。该检查点保存现有入口，并不声称功能最终验收、退出全面稳定、性能达标或尖峰已修复。先 push 本阶段再进行长实验。

阶段 2 已推送为 `c2ee6575e3caff69f858ec1faad3a2b17785c4a6`，local/upstream/remote 核对一致。

## 阶段 3：空场景归因与正式对照前的入口修正

- `Attribution_Standalone_Matrix01`：旧 Hidden 启动无法建立 OS 前台；90 秒谓词超时，协议失败、exit 0，保存 failed.txt/trace。改为用户要求的可见前台测试窗口，并通过 Computer Use 激活实际返回的窗口；Slate active 不能代替 OS foreground。前台等待帧另存 startup，正式帧逐帧核对 viewport/画质/前台，分析器不删除离群帧。
- `Attribution_Standalone_Matrix02`：前台有效，完成 Empty 到 LongRepeatDistributed，但 ActualNewKnowledge 使用编辑器 snake-case 属性别名，在 -game 无法读取，协议失败、exit 0。前十二案例和完整 trace 保留，仅用于诊断，不算完整正常矩阵。改用真实 native `StableId` 名称；`Harness_Knowledge01` 独立非 Trace 实际新增六条未解决记录，360 帧断言通过，exit 0、severe 0；p95 30.202、p99 44.232、max 98.346 ms。
- Insights CPU/GPU/region 独立采样：首份忘启用 region channel，因此只导出全程统计；后续明确开启 region。`ExportGrayTrace.ps1` 可无 UI 导出区域与总计。全程 trace 的 GPU DrawMaterialToRenderTarget 平均 9.330 ms，不能冒充空场景单独结果。
- `Attribution_Standalone_Controls01`：1080p、SP100、TSR、sg 全 3、VSync/FPS limit/fixed/smooth 均关闭、实际前台；六段各 300 帧，exit 0、severe 0。Empty / NoGuidance / NoWorldLabels / NoUi / NoCoverageDraw / Restored 的 wall p95 分别为 **31.493 / 31.342 / 29.946 / 29.742 / 19.867 / 31.777 ms**。这是一轮带 Trace 的干预归因，不是优化后的正常达标成绩。
- Empty 区域 Insights：GPU 覆盖绘制平均 **10.294 ms**，TSR **5.153 ms**，LumenScreenProbeGather **3.259 ms**；存在明确 GPU occlusion-query wait。覆盖纹理真实 **6240×6320、R16F、2.5 cm/texel**，每次转头按全图绘制。CPU memory p95 约 0.203 ms。由干预和 GPU track 共同支持优先减少确定为零的覆盖像素工作，不降低纹理密度、TSR、光照或合法采样。
- 新诊断开关 `r.Darkwell.FogVisual.Diagnostic.SkipCoverageDraw` 默认 0，只在 Attribution 的 NoCoverageDraw 段故意冻结 GPU 场、保留 CPU 语义；正常结果不使用。Lab UI 可显式隐藏/恢复；RHI texture bytes 为设备统计，不能当全部显存或泄漏证明。
- 构建：TraceScopes 18.56 秒成功；Attribution 首次编译因 TObjectPtr range auto* 推导失败，明确改为具体指针类型后完整 Editor 构建成功 7.89 秒。Python AST 与 diff --check 通过。代码目前仅增加诊断，不改变正常覆盖绘制和玩法。

正式 before/after 使用 `RunGrayPerformanceBaseline.ps1 -RunName UNIQUE -Mode PIE或Standalone -Protocol Matrix -NoAuthoringToolsets`，每次为独立进程，激活其实际窗口后自动采集。`AnalyzeGrayStabilization.py` 输出所有帧及单独 steady_after_90、setup/资源数量、>33/>100、最长慢帧串；trace、干预或条件变化不混入 normal 汇总。首次批量 setup 的即时 records/proxies/identities 与运行中资源分别记录，避免将合法反证后的压力下降当满负载通过。

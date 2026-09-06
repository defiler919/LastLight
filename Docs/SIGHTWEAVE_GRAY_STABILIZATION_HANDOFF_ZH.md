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

## 本轮结果（持续更新；最终验收尚未完成）

| 维度 | 状态 | 本轮证据 |
| --- | --- | --- |
| FUNCTIONAL REGRESSION | PARTIAL | 最终原生 143/143；Qualification 906 帧 / 8632 检查通过，其余视觉流程继续 |
| EXIT STABILITY | PARTIAL | 当前可复现的 audit 生命周期错误已修复，最终主窗口、多 PIE 复核继续 |
| FRAME PERFORMANCE | FAIL | 正式前后各 3 PIE + 3 Standalone；空场景仍超 16.6 ms，部分案例退化 |
| INITIALIZATION / BATCH HITCHES | FAIL | 184 distributed setup 约减半，整帧仍存在约 2.5–2.6 秒尖峰 |
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

## 阶段 4：六组正式 before 与第一批等价优化

Before 固定在 `194a0dbddacf9c5ea5d6b19f66773c2effb984c0`，六组过程未修改源码/二进制。全部使用上述 Matrix、NoAuthoringToolsets、实际前台 1920×1080/SP100、相同质量，无 Trace/截图/调试器；逐帧环境无异常。Saved/Stabilization 下有效样本为 Before_PIE_01/02/03、Before_Standalone_01/02/04，全部 complete、exit 0、severe 0。Standalone_03 因未在 90 秒内激活 OS 前台而协议失败（exit 0）；原始失败保留，不计有效样本。

| Empty p95 ms | 1 | 2 | 3 |
| --- | ---: | ---: | ---: |
| PIE | 33.708 | 33.083 | 33.528 |
| Standalone | 31.693 | 31.720 | 31.710 |

逐 case 的 p50/p95/p99/max、所有尖峰和资源见各目录 analysis.json。以 Standalone_04 为例：184 distributed setup 1073.318 ms、整帧最大 3527.221 ms。批量构造即时 65/185（含先前局部背景记录）不等于热态仍有相同压力：原生合法反证会让 64 案例降至 1 条，distributed 保留约 121/122 条；不得据此宣称 64 条持续活跃压力通过。

第一批实际优化：

- GPU current coverage 保持原 R16F 6240×6320、2.5 cm/texel、全 mip 和原全图 UV；每次清除当前场，再用硬件 scissor 只绘制 body/cone 半径加过渡宽度与浮点余量内的保守区域。历史数据独立，不延迟本帧结果。`Diagnostic.FullCoverageDraw=1` 保留原全图绘制作为 oracle，正常默认 0。
- 归属布尔查询在首个合法重叠证明后停止；cap 减法仍取完整区间，测试完整路径保留。捕获 footprint 先做保守平面包围盒拒绝，几何并集命中后停止；原参考路径保持可运行。
- 静态 world-label 纹理改为 Configure 时 RequestRedraw，朝向相机的组件变换仍正常更新，内容/质量不变。

完整 Editor 构建 `Stabilization_OptimizationBuild01.log` 成功，28.33 秒。定向 `Stabilization_OptimizationTarget01` 实际 2 项通过（ConservativeDrawSupport、BatchOwnershipSamplesEquivalent）；第三个指定的 Incremental selector 前缀写错，未运行，不冒充通过，最终完整 manifest 将覆盖正确名称。

独立真实 D3D12 `Stabilization_CoverageGPU01`：1 项通过，5 种 source/墙体/边缘情形，逐个读取全部 13 层 mip，与原全图路径完整像素 CRC 一致；先污染旧场再绘制同时验证清除。35.982 秒，exit 0、severe 0。这是 GPU 正确性 oracle，不是性能或正常 Editor 关闭证据。新增 `RunGrayObjectPolicyTests -Rendering` 显式启用真实 RHI，GPU oracle 在 NullRHI 下报错。

`Optimization_SmokeStandalone01` 前台条件有效、complete、exit 0、severe 0；Empty p95 20.198、p99 21.194、max 56.501 ms，OneWhole p95 21.128 ms。说明改动有收益，但这是烟测，**仍未满足完整帧 16.6 ms 目标**，也不是正式三组 after 或最终回归通过。

## 阶段 5：剩余成本、退化几何保护与最终原生回归

`1e89e63ca98c80843e2a47a548d87f2b96a0ed62` 已推送并核对 local/upstream/remote；首次 push 网络 TLS 失败，重试成功，没有 force push。

`Optimization_AttributionMatrix01` 为该提交的独立 Trace，138.135 秒、完整协议、exit 0、severe 0。空场景 GPU DrawCanvasToTarget 平均约 0.240 ms（原全图约 10.294 ms），TSR 5.122 ms、LumenScreenProbeGather 3.301 ms；SourceUpdate CPU 约 5.166 ms，不能把它全算作灰色对象更新。184 distributed setup 553.849 ms、最大整帧 2515.789 ms；首个 native memory update 1949.427 ms，其中 ownership 1493.731 ms、cap 253.628 ms。收益真实，批量尖峰仍失败。

进一步在平面竖向区间查询中，用包含原容差的 world AABB 先拒绝无交集点。零 XY 缩放保留原 slab 行为（逆变换可能接受折叠 AABB 外点），捕获 footprint 的 bounds 捷径也排除这种退化情形。原 PlanarProjectionMatchesOriginalSlab 增加独立 world offset，实际覆盖 **65,610** 次点/容差查询以及平面、倾斜、负缩放和零缩放。

构建均为完整 Editor：ReferenceBuild01 8.42 秒、ProjectionBuild01 20.96 秒、SingularGuardBuild01 22.03 秒成功。`Stabilization_FinalFunctional01` 143/143、208.768 秒通过，但之后补充了零缩放保护；最终以 **Stabilization_FinalFunctional02** 为准：**143 项（132 clean、11 warnings）、0 failed、0 not-run、severe 0、exit 0、212.211 秒**，manifest 原 142 项无遗漏，新增 ConservativeDrawSupport。包括 OrdinaryHost、Whole/Partial/cap/合法反证和完整参考路径；它不是画面或普通退出证据。

普通地图对照单列为 Reference 协议，Matrix 的地图/案例不变。只读环境查询允许在 CDO 上读取实际 game viewport；原生 ExecuteMenuAction 暴露给 UI/Python，逻辑不变。Reference 对已加载世界调用 ResumeGame，不读写存档。LongRun 补充 reset/重新进入/实际运动成功的事件记录与释放 Python 帧缓冲后的 native GC 资源快照；不改变 Matrix 分支。

Reference 的失败/局限全部保留：ProjectReference_Standalone01 停在原生暂停主菜单，正常 Alt+F4，exit 0、complete false、190.107 秒。Standalone02 原生启动后在等待前台期间继续模拟，完成 720 帧、exit 0，但最终实际截图为 YOU DIED，**不能作为正常游玩基线**。该独立 GPU profile 的格式化事件确证 TSR `1920x1080 -> 1920x1080`。后续驱动保持原暂停菜单直到前台建立，采集 240 帧并逐帧断言 Health>0；截图使用显式 nosuffix 文件名。

ProjectReference_Standalone03 的 240 帧生命值断言通过，但 Shot 的 nosuffix 缺少命令参数前缀，图片实际为 viewport00000.png，末尾文件名断言失败（exit 0、severe 1）；原图已查看，玩家存活、原生敌人 Hunting、HUD 正常。修正为 `Shot -nosuffix -showui` 后 **ProjectReference_Standalone04** 完整通过，实际截图 viewport.png；240 帧 p50/p95/p99/max 为 **25.208/33.485/39.913/50.409 ms**，15 帧>33、0 帧>100、最长慢帧串 1。该普通 L_Prototype 使用默认玩法和 legacy 呈现（project fog_extent 为 0），是基础项目对照，不冒充 Lab 空场景同负载或灰色层性能通过。独立 GPU profile/截图均在这些计时样本之后。

## 阶段 6：正式 after 与完整前后比较

正式 after 固定 `5ec64c1d8dfef1e20d13ed98fc3c6617828d9170`；所有六组完成前没有编辑源码。有效样本 After_PIE_01/02/03 与 After_Standalone_02/03/04 全部 complete、exit 0、severe 0，逐帧实际前台/1920×1080/SP100/全部记录的质量设置检查通过，与 before 设置相同。After_Standalone_01 最后 ActualNewKnowledge 有 357 帧失去 OS 前台，整组排除正式汇总，原始数据保留；协议 complete、退出 0 不能代替前台有效性。

下表每阶段、每模式各三次独立进程；p95 包含全部 case 帧，不剔除冷帧、初始化尖峰或离群值；setup 为三次中位数，最大整帧为该阶段三次最大值。完整 p50/p95/p99/max、慢帧串和资源计数在 Saved/Stabilization/paired-comparison.json 及各运行 analysis.json；聚合原始表在 paired-comparison.md。
| 模式 / 案例 | before p95 中位数 [范围] ms | after p95 中位数 [范围] ms | before → after setup 中位数 ms | before → after 最大整帧 ms |
| --- | ---: | ---: | ---: | ---: |
| PIE / Empty | 33.528 [33.083, 33.708] | 32.584 [31.386, 32.788] | 0.133 → 0.121 | 122.421 → 50.923 |
| PIE / OneWhole | 33.863 [33.510, 34.004] | 31.072 [30.495, 31.125] | 2.020 → 2.224 | 70.512 → 61.545 |
| PIE / EightWhole | 35.985 [35.014, 36.079] | 32.122 [31.623, 33.121] | 13.724 → 14.863 | 57.442 → 51.475 |
| PIE / ThirtyTwoWhole | 40.244 [38.533, 41.732] | 41.263 [38.021, 42.231] | 56.380 → 61.875 | 117.140 → 125.685 |
| PIE / PartialNewThenRepeat | 31.958 [31.875, 32.258] | 31.014 [30.512, 31.871] | 23.657 → 24.725 | 53.827 → 58.653 |
| PIE / Overlap64 | 33.572 [33.461, 34.009] | 31.321 [31.140, 31.369] | 348.403 → 184.862 | 802.464 → 551.491 |
| PIE / SameIdentity64 | 33.283 [33.266, 33.703] | 31.409 [30.964, 32.295] | 388.439 → 205.087 | 1315.046 → 934.409 |
| PIE / Distributed184 | 34.158 [34.064, 34.331] | 30.882 [30.675, 31.261] | 1085.538 → 580.757 | 3641.627 → 2633.466 |
| PIE / FastSweep90 | 21.171 [20.828, 21.358] | 27.877 [27.371, 28.728] | 392.491 → 204.827 | 844.810 → 574.466 |
| PIE / FastSweep160 | 20.917 [20.830, 21.258] | 27.770 [27.145, 28.100] | 376.251 → 213.227 | 823.881 → 580.680 |
| PIE / StationaryStop | 21.940 [21.903, 21.964] | 27.635 [27.582, 29.451] | 8.581 → 9.103 | 34.423 → 36.808 |
| PIE / LongRepeatDistributed | 33.573 [33.421, 33.973] | 31.456 [31.236, 32.148] | 1090.884 → 602.258 | 3647.831 → 2639.522 |
| PIE / ActualNewKnowledge | 31.933 [31.614, 32.277] | 29.739 [29.212, 29.851] | 28.310 → 29.063 | 55.467 → 66.507 |
| Standalone / Empty | 31.710 [31.693, 31.720] | 28.429 [27.842, 29.858] | 0.133 → 0.120 | 111.688 → 37.630 |
| Standalone / OneWhole | 31.855 [31.551, 32.234] | 28.787 [27.780, 29.050] | 1.728 → 2.020 | 44.154 → 35.890 |
| Standalone / EightWhole | 32.655 [32.572, 34.736] | 29.993 [29.894, 30.125] | 12.057 → 12.839 | 60.995 → 49.428 |
| Standalone / ThirtyTwoWhole | 41.168 [39.716, 43.620] | 37.385 [36.832, 38.448] | 46.850 → 52.159 | 83.263 → 86.875 |
| Standalone / PartialNewThenRepeat | 30.239 [29.867, 30.898] | 28.433 [28.364, 29.714] | 4.586 → 4.908 | 54.733 → 48.992 |
| Standalone / Overlap64 | 31.485 [31.460, 31.872] | 28.859 [28.835, 29.055] | 342.539 → 181.511 | 739.421 → 511.476 |
| Standalone / SameIdentity64 | 31.458 [31.362, 31.630] | 28.175 [27.817, 28.203] | 383.868 → 197.108 | 1250.710 → 883.026 |
| Standalone / Distributed184 | 31.896 [31.583, 32.357] | 29.107 [27.970, 29.373] | 1073.318 → 553.043 | 3533.005 → 2566.444 |
| Standalone / FastSweep90 | 19.297 [19.068, 19.391] | 27.796 [27.448, 28.267] | 394.377 → 186.729 | 792.784 → 516.010 |
| Standalone / FastSweep160 | 19.176 [19.088, 19.291] | 27.835 [27.583, 28.491] | 376.719 → 207.590 | 774.887 → 537.854 |
| Standalone / StationaryStop | 19.918 [19.765, 20.017] | 27.904 [27.347, 27.909] | 4.295 → 3.766 | 35.029 → 35.422 |
| Standalone / LongRepeatDistributed | 31.742 [31.412, 31.816] | 27.911 [27.589, 28.776] | 1080.001 → 607.273 | 3537.339 → 2538.341 |
| Standalone / ActualNewKnowledge | 30.264 [30.096, 30.336] | 28.645 [28.108, 29.296] | 6.077 → 7.293 | 46.207 → 52.158 |

性能结论明确为 FAIL：空场景和所有正式案例的 p95 都未达到 16.6 ms；PIE 32 Whole 每次仍有 123–126 ms 尖峰。快速扫视与停止案例的正式 after 比 before 更慢，不能选择早期 20 ms 烟测覆盖这些退化结果。批量 setup 约减半，但 184 条 distributed 的首帧仍超过 2.5 秒。

可比性边界：相同脚本路线按真实 delta 运动，原生捕获/反证的时序会改变背景记录数；Standalone OneWhole 的 after 资源最大数 2、before 1，不能声称各帧状态完全相同。64 压力样本的即时 65 条很快被合法反证降到 1，distributed 热态约 121/122，不能声称 64 条持续活跃压力已通过。等待前台的冷启动帧单独保留；before/after 激活延迟不同，不把等待时间伪装为可比启动性能。

FinalAttribution_Standalone01 为独立 Trace（不并入正式数据）：58.392 秒、完整协议、exit 0、severe 0、环境异常 0。Empty / NoGuidance / NoWorldLabels / NoUi / NoCoverageDraw / Restored 的 wall p95 为 29.498 / 28.635 / 28.102 / 26.879 / 25.101 / 27.004 ms。Empty GPU coverage 平均 0.243 ms，TSR 5.345 ms，LumenScreenProbeGather 3.485 ms，SourceUpdate CPU 5.193 ms。覆盖优化收益持续存在；剩余基础渲染和等待成本仍高，其增长原因尚未完全归因。现场 NVML 记录 GPU 97%、71°C、1920 MHz、169 W、P0；没有证据把退化归咎于温度或后台应用，也没有关闭用户其他程序。

Stabilization_FinalQualification01 最终 D3D12/SM6 视觉流程：906 帧、8632 个检查全部通过，九次独立资格会话 current color ratio 最低 1.0，无图像 oracle 失败；151.949 秒、完整协议/清理、exit 0、severe 0。Contracts、Episodes、Reobservation/WholeSessions 和长测继续。

长测驱动只在 ActualNewKnowledge 之后显式 Reset Room 03，恢复该案例实际移走的 source，再进入混合交互路线；记录 reset 前后资源，保持真实运动/可见范围。此变更发生在六组正式 after 结束之后，未改变 Matrix 或 C++ 二进制。先提交推送该检查点，再开始长测。

## 阶段 7：最终完整视觉回归

运行时代码保持最终构建不变。四个独立 D3D12/SM6 图形协议全部完成，清理标记完整、非调试器实际 exit 0、severe 0；图像原件已查看，协议完成与画面判据分别核验。

| Saved/ArchitectureAudit 运行目录 | 协议证据 | 独立画面/资源 oracle | 进程 wall 秒 |
| --- | --- | --- | ---: |
| Stabilization_FinalQualification01 | 906 帧、九次独立资格会话 | 8632 检查 PASS | 151.949 |
| Stabilization_FinalContracts01 | 178 帧、三次 PIE | Whole 首次离开 24/24 图像可见度 PASS | 71.858 |
| Stabilization_FinalEpisodes01 | 八轮观察和 cap 隐藏/恢复诊断 | 六组各 896 个内部样本，无缺口 | 42.211 |
| Stabilization_FinalWholeSessions01 | Reobservation + WholeSessions + NormalTurns，118 取证帧 | Reobservation 192、ConfirmedWholeCurrent 97、WholeSessions 332 检查全 PASS | 74.009 |

最后一组同时覆盖原连续四阶段观察、四次重复远近观察、四次重新取得 Whole 资格、相同小接触达标后保持彩色、旧完整历史不回退，以及真实相机墙体深度/隐藏墙负对照。图像 oracle 比较原始截图、实际材质/纹理绑定与独立解析内部样本，不能以原生测试成功替代。所用分析命令为 Scripts/AnalyzeWholeQualification.py、AnalyzeGrayWholeTransitions.py、AnalyzeGrayMemoryEpisodes.py、AnalyzeGrayReobservation.py、AnalyzeConfirmedWholeCurrent.py、AnalyzeWholeSessions.py，参数为表中对应目录。

这些视觉协议采用固定时间步，截图实际 2233×911；其用途是表面正确性和生命周期，不冒充正式 1080p 无截图性能矩阵。所有计时门槛仍以独立正式性能与真实长测为准。

## 阶段 8：完整模拟长测及普通 PIE 尺寸修正

`RunGrayObjectPolicyTests.ps1 -RunName Stabilization_FinalSoak01 -Tests Darkwell.PropLab.GrayHomeBaseline.FifteenMinuteInteractiveSoak` 完整完成 54,000 active、60 settle、600 idle；无 wall budget 中断。测试耗时 328.048 秒，进程 wall 358.431 秒、exit 0、severe 0，但测试本身 **FAIL**，脚本正确返回 1。失败均来自原性能门槛：active 共 1308 步 >33 ms、2 步 >100 ms、最长连续慢步 14；不能用正常退出覆盖测试失败。

每 3600 步的原始分布与资源记录在 log 和 soak-trends.json；active 最后一段 step p50/p95/p99/max 为 3.764/15.908/67.808/101.872 ms（该段分位数不能冒充完整 54,000 分位数）。Idle 600 步为 0.266/0.277/0.379/0.469 ms，0 次 >33/>100，零纹理上传、创建和历史扫描。模拟保持 9 个身份，资源峰值 records 4、textures 21、MIDs 36、caps 0、显示丢失检查 0，原记录/呈现资源边界断言通过。

**工作集异常未解决**：第 3600 步 4,770,422,784 字节，54000 步 11,194,589,184 字节，idle 结束 11,201,318,912 字节；同期存活 UObject 在周期 GC 后约 63,161–63,174，idle 63,057。不能由 UObject/纹理个数稳定推出内存稳定，也不能直接把进程工作集增长定性为灰色历史泄漏。该测试在同一个同步 Automation 调用里连续推进子系统，不运行普通引擎完整帧；引擎 Texture2D.cpp 的 UpdateTextureRegions 会把上传缓冲清理排到 RHI command lambda。队列是否积压尚无直接分配证据，故仅作为需验证的线索，不能当已证实根因。

普通默认 Editor `ProjectReference_PIE01` 实际尺寸为 1920×1082，被严格条件断言拒绝（complete false、exit 0、severe 1、27.596 秒），失败保留。新增 Editor 模块专用 SetPerformanceViewportSize，在普通 PIE 中固定实际 1920×1080，结束时恢复；不生成 Lab actor、不改普通玩法、不改变 Matrix。完整 Editor 构建 Stabilization_ReferenceViewportBuild01.log 成功，24.74 秒；UBT 因 adaptive unity 工作集同时重链 runtime DLL，runtime 源码没有新改动。此前六组正式 after 的原二进制/源码元数据继续保留，新增构建不冒充那些样本的二进制。

ProjectReference_PIE02 使用默认 Editor 工具集、普通 L_Prototype 原生玩法，实际 1920×1080/SP100、240 帧存活断言通过，complete/exit 0/severe 0，32.546 秒。p50/p95/p99/max 为 27.543/36.634/47.321/79.908 ms，66 帧 >33、0 帧 >100、最长 3 帧。原图显示 Health 36%、Stalker Hunting；计时后 GPU profile 的 TSR 事件确证实际 1920×1080。截图 showui 可包含窗口边框，不能把 PNG 外框尺寸当内部渲染尺寸。

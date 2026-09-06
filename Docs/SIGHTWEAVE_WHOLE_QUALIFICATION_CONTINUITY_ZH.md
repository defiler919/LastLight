# Whole 达标交接退灰修复

状态：**退灰修复及受保护行为的功能回归完成，可供人工复测。性能未验收，间歇 Editor 退出异常未关闭。** 2026-09-06 收尾只核对既有证据、分析已完成的性能数据并提交文档，没有新增运行时代码、重新设计或重跑长测试。

本轮从 `9a67c53f691ac4034e041c8ee872516ff7d69fc1` 出发，只修复获得整件资格时已显露表面回退的问题。每轮原跨度门槛、连续接触资格、HistoryMode、Partial、StationaryOnly、Never、合法墙体/视锥与正常相机深度规则不变。运行环境仍为 `D:\UE_5.8` 的 UE 5.8.2 CL56702186。

## 原始用户证据与定位边界

录像位于 `C:\Users\defiler\Videos\Captures\Darkwell - 虚幻编辑器 2026-09-05 22-52-19.mp4`，1920×1052，时长 12.377833 秒。逐帧时间戳确有 10.808467、10.836233、11.141767 秒：先局部彩色，再整件灰色，再恢复整件彩色。HUD 为 03 房间、SHA `9a67c53f691a`。

对应运行日志为 `Saved/Logs/WholeSession_UserRetestEditor.log`，施工前复制到 `Saved/ArchitectureAudit/WholeQualification_UserEvidence/user-editor.log`。录像中输出日志面板的 frame 1387 对应 `2026.09.05-14.52.29:694` UTC，coverage_revision 511、geometry_revision 33、records 2；随后 frame 1389 对应 `14.52.29:760`，coverage_revision 512、geometry_revision 35、records 1、source texture uploads 3，之后持续几帧上传。观察者没有平移，房间一直为 03。

**原日志没有逐帧跨度和局部/Whole 混合值，达标的中间帧也未必达到 hitch 日志阈值。** 因此原片段可对齐到局部→Whole/记录复用区间，但不能伪称原日志直接记下了那个视频帧的 100 cm。以下根因由同一对象、原配置、合法慢扫的带观测复现确定，不是凭录像猜测渐显或纹理。

## 修改前调用链与保护项

施工前已列出：合法局部采样 → RevealObservation 跨度确认 → TryResumeQualifiedWhole 历史复用 → AdvanceConfirmedWhole 显示值 → UpdateCurrentPartTextures 实际绑定 → 历史临时抑制/最终绘制。保护 01 首次历史交接、02 Partial 接缝与外部切口、03 StationaryOnly 隐藏移动/停止/再观察、05 Whole/Partial 与墙体、Never、反证、贡献互斥、历史资源身份和每轮门槛。

先增加按需单对象诊断，再运行未修复版本。诊断记录每个环节的局部值、Whole 累积值、源提交 CPU 像素、实际 MID 绑定的纹理/尺寸/ready、历史临时抑制和探针处历史贡献。它默认关闭，只有测试明确指定对象时记录；不作为产品规则输入。CPU 提交值不是 GPU 读回，真实绘制另由逐帧原图检查。

## 最早回退环节

`WholeQualification_Before` 使用原运行时行为加诊断，完整采集 906 张 2233×911 原图，01、03、05 各一次首次确认及两次有完整历史的重新确认，共九次；退出 0、severe 0、PIE 停止完成，169.159 秒。

以 03 的 `room3_session1_cross_078` 为例，同一帧确认跨度从未达标变为 100 cm：

| 环节 | 已合法局部 R / G | Whole 累积 R / G | 实际源提交 R / G |
| --- | --- | --- | --- |
| 帧开始 / 局部推进后 / 跨度确认后 | 1 / 1 | 1 / 1（旧会话表示） | 1 / 1 |
| 历史记录复用前 | 1 / 1 | 1 / 1 | 1 / 1 |
| ResumeStationaryKnowledge 后 | 1 / 0 | 1 / 0 | 1 / 1 |
| Whole 推进后 | 1 / 0 | 1 / 0.083333 | 1 / 1 |
| 源纹理提交后 | 1 / 0 | 1 / 0.083333 | 1 / 0.083333 |

最早错误是把“这轮已经在显示的局部”当成刚开始重新观察，在捕获复用后调用恢复函数清掉 LiveBlend。纹理切换忠实提交了已经回退的数值。历史临时抑制由 21186 个样本收回到 0，旧代理隐藏，记录 2→1；实际灰色来自 Current 的灰/彩混合，不是旧代理重复贡献造成。

首次确认是另一个入口：没有历史复用，局部 R/G 已为 1/1，但从未推进的 Whole 累积为 0/0。`AdvanceConfirmedWhole` 直接从零推进到 0.083333/0.083333，随后源提交值下降，既会退灰也会变得稀薄。二者均在真正绑定之前发生。

## 修复

1. Whole 由局部表示进入统一表示时，从**本轮当前合法局部**的 opacity/live 最大值承接统一对象显示进度。这样任何仍合法、已显露的局部都不会因显示范围扩大而减小；旧 Whole 累积和历史记录不参与授予本轮进度。
2. 当前局部已经在同帧推进过 DeltaSeconds，交接时不再重复推进。直接首次接触就达标时，仍按原 0.2 秒渐显；局部早已稳定显露时，整件在达标帧直接接上完整显示。这遵循整件共用一个值的现有合同，不保留永久局部分割。
3. 合格 Whole 的历史复用只切换捕获所有权和代理显示，不再调用会重置当前混合值的 ResumeStationaryKnowledge。Partial 的原恢复入口保持。
4. 源纹理池、历史临时遮罩、合法查询、证据、确认门槛和历史交接顺序没有修改；没有统一清零、延时、降门槛或透墙处理。纯扫掠且没有终点局部可承接的捕获仍使用原推进路径，不生成非法终点 Current。

## 连续帧验证

新增 `audit_whole_qualification.py`：在原房间固定合法观察者位置，缓慢扫入不足 100 cm 的部分观察，先停留使局部稳定，再每帧 0.1° 扫过门槛。保存达标前、每个跨越帧、达标帧、后续 24 帧、小接触 8 帧和退出 8 帧；每个对象共三轮，不在轮间 Reset。01 每轮 125 帧、03 每轮 124 帧、05 每轮 53 帧，总计 906 帧。

`AnalyzeWholeQualification.py` 固定检查原图柜门内部同一 44×50 像素区域。它独立检查颜色保持与 RGB 误差，并检查合法性、跨度、源绑定、提交值、单一历史贡献、记录数和资源身份。连续性使用游戏世界更新计数及 1/60 秒游戏时间；GFrameCounter 包含仅编辑器帧，不能单独当成游戏帧计数。未删除任何中间帧。

| 同一 oracle | 修复前 | 修复后 |
| --- | --- | --- |
| 总检查 | 8632 项，199 失败（99 显示值 + 100 真实图像） | **8632/8632** |
| 03 首次确认：原图最大 RGB MAE | 69.192 / 255 | 1.011 / 255 |
| 03 第一次再确认：原图最大 RGB MAE | 83.185 / 255 | 1.676 / 255 |
| 所有九轮原图最大 RGB MAE | 明显退灰/稀薄 | 2.387 / 255，低于 12 的 oracle 容差 |
| 达标帧已稳定源 R/G | 0.083333/0.083333 或 1/0.083333 | 九轮均 1/1 |
| 进程 | 退出 0 | 退出 0、severe 0、PIE 停止完成；166.437 秒 |

修复后证据目录 `Saved/ArchitectureAudit/WholeQualification_After`。人工按原始分辨率检查了 03 达标前一帧、达标帧、下一帧及整个过渡结束帧：已显露区域保持颜色，新范围接上整件显示，没有全体退灰或永久分割线。

新增原生 `Darkwell.ObjectMemory.WholeQualificationAppearance` 在 30/60/120/144 Hz 和 0.2 秒 hitch 检查首次慢扫承接、各 primitive 共用值、直接首次渐显不跳变/不双算时间、旧 Whole 表示不能跳过新局部进度。原 `WholeReobservation` 的普通非 Lab 宿主、多 primitive、历史/遮挡连续链增加达标帧上仍合法旧样本的 R/G 单调断言；其余产品断言保持。

定向 `WholeQualification_Focused2`：15/15，14 clean、1 warning，退出 0、severe 0，测试 12.763 秒、进程 35.992 秒。完整 `WholeQualification_FinalFunctional`：**142/142**，133 clean、9 warning，0 failed/not-run，退出 0、severe 0，测试 177.559 秒、进程 200.265 秒。使用上一轮完整 141 项 selector，自动纳入本轮新增一项；没有放宽原有正确性断言。

九个 warning 测试中的内容为既有地图 CleanupWorld 提示、编辑器联网探测超时、测试刻意触发的重复 StableID 拒绝和 64 条旧接口容量拒绝；没有测试失败。

## 受保护行为回归

以下旧驱动和旧 oracle 均未修改。所有图形进程串行使用真实 D3D12/SM6、原画质；图形报告与进程退出分别判定。

| 本轮证据目录（Saved/ArchitectureAudit 下） | 连续操作与结果 | 进程结果 |
| --- | --- | --- |
| WholeQualification_Contracts | 178 帧、3 次 PIE；Whole 首次离开 24/24 图像通过；03 隐藏移动→停止→新位置合法观察、StationaryOnly、Never、05 墙体、02 外部切口与 Reset 原断言通过 | 协议完成、PIE 停止、severe 0；日志关闭后退出 **0xC0000005**；88.759 秒 |
| WholeQualification_Episodes | 51 帧；Partial 多轮相反方向观察，6×896 个内部贡献样本无缺口；隐藏/恢复 cap 不改变内部表面 | 退出 0、severe 0；48.384 秒 |
| WholeQualification_ProtectedSessions | 118 帧；每轮跨度与短会话/历史复用 **332/332**，历史完整/再观察不回退 **192/192**，Whole 当前范围/合法接触/相机深度 **97/97** | 退出 0、severe 0；81.991 秒 |

人工查看原分辨率图像补充检查了 01/05 首次达标帧、03 首次离开完整灰影、02 cycle_4_history 无内部接缝与未观察外部切口、05 最终 Whole/Partial 历史。新显露区域在达标初帧可见短暂边缘重建痕迹，已显露区域保持颜色，过渡结束没有永久分割线。

## 资源对照

Before/After 同名 **906 帧×8 字段逐帧完全相等**：records、proxies、textures、mids、caps、fine_bytes、texture_creations、mid_creations。报告保存在 After/resource-comparison.json。每个三 primitive 对象首次局部/Whole 表示各分配 3 张纹理，累计 6 张，此后两轮不再创建；历史 epoch/proxy/texture/capture_set 身份在三轮离开时保持一致。

下表为第二、第三轮连续捕获期间的整个场景计数，包含前面房间留下的合法记忆，因此不能把累计场景数量当成单对象新增资源：

| 房间 | 记录范围 | 代理 | 纹理 / MID | 细网格字节 | 当帧新建纹理 / MID |
| --- | --- | --- | --- | --- | --- |
| 01 | 1–2 | 1 | 7 / 3 | 1,327,104 | 0 / 0 |
| 03 | 2–3 | 2 | 14 / 6 | 2,580,480 | 0 / 0 |
| 05 | 3–4 | 3 | 21 / 9 | 3,833,856 | 0 / 0 |

暂时多出的 1 条是既有规则要求的未确认本轮记录，达标复用后收回；没有新增无意义历史、代理或逐轮纹理。各房间 cap 均为 0。该修复只在局部→Whole 的一次交接扫描当前局部格子，不引入每帧新的历史扫描。

## 性能采样的实际结果

`WholeQualification_Timing` 在用户停止上一轮对话后继续完成，**不是中断采样**。源码为 `a4a17a412a35d2e9a0c7dd8917965d6e8c9323f5`；`source.patch` 仅含集成文档修改。`timing.json` 含 722 个逐帧样本，`timing-stages.json` 含 67 个阶段状态；2026-09-06 00:00:58 写完，00:01:07 日志正常关闭。`summary.json` 记录退出 0、severe 0、协议/PIE 停止完成，进程 63.051 秒。

收尾直接对已有数据运行 `AnalyzeGrayReobservationTiming.py`，生成 `timing-summary.json` 和 `timing-analysis.txt`；另保存全体样本统计 `timing-overall.json`。没有重新启动 UE。运行使用原 ReobservationTiming、D3D12/SM6、非固定时间、NoVSync、t.MaxFPS 0；未启用本轮单对象密集诊断或逐帧截图。以下耗时含原协议遥测成本，不能用系统 CPU 代替完整 wall frame：

| 指标 | 上轮 WholeSession_FinalTiming（0029e27） | 本次 WholeQualification_Timing（a4a17a4） |
| --- | --- | --- |
| 采样帧数 | 722 | 722 |
| 系统 CPU p50 / p95 / p99 / max，ms | 0.160 / 2.143 / 4.962 / 26.776 | 0.155 / 2.102 / 5.538 / 30.120 |
| 完整 wall frame p50 / p95 / p99 / max，ms | 13.648 / 23.950 / 26.085 / 47.196 | 31.791 / 44.743 / 47.487 / 77.994 |
| wall frame >16.6 ms / >33 ms | 169 / 3 | 712 / 285 |
| 进程退出 | 日志关闭后 0xC0000005 | 0 |

**本次完整帧耗时明显高于上次记录，性能预算仍未通过。** 这两份数据使用相同协议，但没有建立同布局的本次修改前对照；性能协议不固定游戏视口，现有报告也缺少可核验两次实际视口一致的记录。因此保留恶化的实测数字，同时将其原因记为未归因；不能据此证明本次代码导致恶化，也不能声称本次无性能回退或用已有性能失败掩盖差异。本次只收尾，不为归因补开性能改造或重跑完整审计。

372 个重复观察帧的资源与上轮完全一致：2 records、2 proxies、6 textures、4 MIDs、1 cap、2,744,320 fine_bytes；新建纹理/MID 均为 0。此项是既有性能路线的资源对照，不能替代上节逐帧慢扫门槛的双表示资源证据。此前批量历史初始化尖峰和长期运行性能验收仍未关闭，本轮没有复测它们。

## 阶段失败与验证边界

`WholeQualification_Focused` 新单元夹具在 Whole 已切换并释放局部掩码之后，又调用只适用于局部表示的 WriteWorldSnapshot，触发 BitArray 断言，退出 3。修改夹具使其跟随真实宿主分支顺序，不改运行时去吞掉断言，不删除正确性断言。修复后 Focused2 通过。这是本轮新增测试夹具错误，与此前日志关闭后的 Editor 退出异常不同。

Before 的首个 analyzer 用 GFrameCounter 验证游戏帧，遇到一帧纯编辑器 tick。核对相邻游戏时间和房间更新计数确实连续后，改用二者联合验证，并对 Before/After 同时重新分析；没有漏掉达标或过渡帧。Before 的 05 探针最初在未见一侧，After 诊断探针改至已见一侧；两轮观察路线、真实图像与独立像素区域完全相同。03/01 的完整因果探针未改变。

本轮 Contracts 在采证与 PIE 停止完成、日志关闭后退出 `0xC0000005`，进程协议明确失败。此前多个版本已有同样退出模式；本轮没有获取新的崩溃栈，不能断言每次都是同一个底层根因。Before、After、Episodes、ProtectedSessions、最终原生及 Timing 正常退出，也不能覆盖 Contracts 的失败。已有退出异常、本轮已修复的测试夹具错误、显示正确性和性能各自保留独立结论。

本轮没有改动 SightWeave 插件代码或插件公开接口，未重复独立插件打包；项目完整 Editor 编译日志：`WholeQualification_DiagnosticBuild.log`（52.29 秒）、`WholeQualification_FixBuild.log`（26.54 秒）、`WholeQualification_FinalBuild.log`（7.95 秒）、`WholeQualification_TestBuild.log`（7.84 秒），均成功，均不是 Live Coding。

**最后一次正式构建**为 `Scripts/BuildEditor.ps1`，目标 `DarkwellEditor Win64 Development`，日志 `Saved/Logs/WholeQualification_HandoffBuild.log`，结果 Succeeded，33.60 秒，DLL 最后写入 2026-09-05 23:57:37。保留引擎废弃 API 和非首选 MSVC 版本警告。完整原生与 After 的 source.patch 中五个 C++ 文件已逐一对照最终提交：唯一后续差异是把诊断注释中的“readback”改为“pipeline and binding”，澄清 CPU 提交值不是 GPU 读回；最终正式构建已包含该注释。运行逻辑和测试一致，收尾没有源码改动，故不重新构建或重跑功能测试。

## 复现

从仓库根目录执行，使用新的 RunName，不能覆盖现有证据：

```powershell
& Scripts/BuildEditor.ps1
& Scripts/RunGrayMemoryAudit.ps1 -RunName WholeQualification_Retest -Protocol Qualification
python Scripts/AnalyzeWholeQualification.py Saved/ArchitectureAudit/WholeQualification_Retest
& Scripts/RunGrayObjectPolicyTests.ps1 -RunName WholeQualification_RetestNative -Tests 'Darkwell.ObjectMemory.WholeReobservation+Darkwell.ObjectMemory.WholeQualificationAppearance+SightWeave.RevealPolicy'
```

原图、用户录像抽帧、samples.json、各环节状态、日志及失败报告保存在本机 Saved，生成目录不提交 Git。其它机器需重新采集。原 8632 项是逐帧图像/状态比较数，不是 8632 个独立 UE 测试。

### 人工复测

1. 在该分支打开 `Darkwell.uproject`，加载 `/Game/Maps/L_SightWeaveGrayPolicyLab` 后启动 PIE。通过大厅“03 移动物体”圆形控制台进入，面对控制台按 F；使用“重置当前房间”取得首次无记忆状态，选择整体对象，保持物体静止。
2. 站在能合法看到柜子一侧的位置，缓慢转向，使不足配置跨度的一部分先稳定彩色。继续慢扫跨过原 100 cm 门槛，全程录像；重点逐帧检查达标前已彩色的同一表面，不应在整件出现时退灰或消失。
3. 完全转开直到合法接触消失，核对第一帧完整灰影；再扫回来。先观察不足跨度的局部，再跨过同一门槛，检查旧灰影下的重新确认也不全体退灰。达标后减小到仍合法的小接触，应继续整件彩色；完全失联后再入，应重新累计。
4. 同样复测 01 首次/重复确认与 05 合法墙缝；相机被墙挡住的部分仍应有正常深度遮挡。核对 02 Partial 的内部接缝及外部切口、03 隐藏移动/停止、Never，不改它们的规则。复测后停止 PIE，保留录像与对应运行日志。

HUD 的运行 SHA 从当前 Git HEAD 读取，文档收尾提交之后会显示文档提交的 SHA；最终运行时代码仍是下列修复提交，不应把文档 SHA 当成又一次运行时修改。

## 提交与收尾状态

最终运行时修复：`a4a17a412a35d2e9a0c7dd8917965d6e8c9323f5`，`fix: preserve live appearance across Whole qualification`，已推送 `origin/codex/darkwell-prop-memory-gameplay-lab`。本次仅随后提交本专项文档和 `OBJECT_MEMORY_INTEGRATION.md` 的交接补充，文档提交记录可由 `git log -1 -- Docs/SIGHTWEAVE_WHOLE_QUALIFICATION_CONTINUITY_ZH.md` 查询。

收尾开始时本地 HEAD、上游和实时远端一致；暂存区为空，只剩上述两份可解释文档。进程检查没有遗留 UnrealEditor、Python、PowerShell 构建/测试或编译子进程，仅有本次短时检查命令；没有强杀进程或覆盖证据。`git lfs fsck` 通过，地图仍使用 LFS；Saved、Binaries、Intermediate、DerivedDataCache 由 Git 忽略。未 reset/clean、操作旧 stash 或移动 stable；两条 stable 远端仍为 `7534163b9c5718700b610e7677f47fbaa79cf977`、`404a5820739638f1097eaae0aa7fba19733298c3`。

本次不启动新测试或 PIE，电脑保持开启。最终文档提交、推送和工作树/远端核验结果随交付回复给出；本报告不将功能通过扩大为整体性能、退出或用户人工验收通过。

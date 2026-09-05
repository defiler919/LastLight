# Room05 再观察历史修复交接

状态：**PARTIAL — READY_FOR_USER_REOBSERVATION_MEMORY_REFRESH_RETEST**。
本专项功能自动验证通过，等待用户重新观察；完整帧性能、批量尖峰和间歇退出异常继续保留，不创建用户验收 stable。

起点 `bc96e98affefe354597d577f8e03ede85f86a2e0`，继续 `codex/darkwell-prop-memory-gameplay-lab`。
工作区最初干净，fetch 后本地/上游/远端相同，stash 为空，LFS fsck 正常。
两条 stable 仍为 `7534163b9c5718700b610e7677f47fbaa79cf977` 和 `404a5820739638f1097eaae0aa7fba19733298c3`。
引擎沿用 D:\UE_5.8，实际 5.8.2 CL 56702186，没有升级或修改资产。

## 用户复测事实

用户在已测试路线中：01 无问题；02 无内部接缝；03 无问题；05 左侧黄色长柜无问题；05 右侧蓝色柜存在旧残缺历史回退。
这些是路线级通过，不是全系统验收。运行时身份核对：蓝色 `Lab.V2.OcclusionWhole`，WholeObjectAfterSpan / StationaryOnly；黄色 `Lab.V2.OcclusionPartial`，SpatialPartial / StationaryOnly。

## 未修复证据

`Reobservation_BeforeFix` 普通宿主新测试失败（1 项，5 种步长，30/60/120/144 Hz 和 200 ms）。
独立已知长方体内部 28,288 样本中，完整捕获无缺失，但首次 H1 和完整观察后的 H2 各有 9,752 个 AA/提交像素缺失。
单独从完整观察开始的参考通过；连续路径与参考像素不等。

`Reobservation_Baseline03` D3D12/SM6、SP100、TSR4，原始游戏视口 2233×911，完成四步骤与四轮往返及独立参考。
蓝柜保持 (-450,7000,0)，玩家起点 (70,6000,92)，朝 115°；移开至 -90°；经墙缝走到 (70,6750,92)，朝 155°；再移开至 -90°。四步骤间没有 Reset、重新注册、策略/物体变化。
远处合法覆盖约 0.64，已确认；靠近为 1.0。H1 的捕获/记忆均 36,876，但零 AA 样本 13,073；H2 仍为相同 epoch、代理、纹理和 hash `11790327373081981827`。
独立直接完整参考零 AA 为 0，hash `6537765931154518915`。已打开四阶段、第二次退出 00/01 和参考原图。
前两次脚本采集因把纯文本诊断误当 JSON 而中断，未算通过；03 完成采集和 Stop PIE 后进程退出 `0xC0000005`，协议退出门失败，severe 日志 0。保留为上一轮已知退出问题的新复现。

## 最早失败位置

合法视野、确认、Whole 几何捕获均正确；`FDarkwellHistoryGridV2::Initialize(SealedMemory, CaptureMask)` 先从受墙遮挡的粗 Current 图像计算 FrozenAAEnvelope，再只用完整几何重写 InitialRemembered/Opacity/State，遗留错误零包络。
`BuildPresentation` 用 Opacity × FrozenAAEnvelope 输出，因此首次 H1 已永久残缺，并非完整历史被当前墙临时遮挡。
再观察获得完整 Live 后，复用检查发现几何捕获没有变化，合理保留 FineHistory，因而再次显示错误 H1。
实际可见代理已绑定对应纹理、Ready=1；不是错误代理选择、纹理上传遗漏或 GPU 延迟。没有反证/替代样本、对象运动或外观版本变化。
第三步确实进入了新的合法权威发布，合法覆盖 1.0，Confirmed=true、Moving=false、StationaryOnly capture eligible；没有遗漏观察者 revision。错误发生在首次封存后、细历史初始化时，早于纹理提交。此前二进制 capture/frozen mask 相等的检查没有覆盖 FrozenAAEnvelope，因此漏掉了实际灰色输出的缺失。
预备代理最终 Ready 和纹理范围正确，也没有选回另一份代理。Lab 注册普通生产场景，`bUseSpatialMemory=true` 同时让旧 Rememberable Presenter 的注册入口退出；不是两套 Presenter 重复接管。

## 修复与定向验证

Whole 几何捕获使用独立 `InitializeWholeCapture`，只共享栅格存储初始化。记忆、透明度和包络都来自合法封存的完整对象几何；物理网格负责实体轮廓。Partial 仍走原局部观察 AA。
修改没有增加 record、代理、纹理、revision 或每帧工作；相同状态继续复用已有细历史。反证后拒绝原捕获复用的规则未变，不重新初始化已擦除历史。没有新增长期知识持久化格式：此处是 PIE/运行时内存；重新 Play 即以修复代码开始。

标准正式构建通过，最终定向测试版本构建 `Reobservation_TurnMatrixBuild.log` 18.32 秒。
`Reobservation_FixedFocused` 同一版本 23/23 通过，21 clean、2 warning、0 severe，测试 29.963 秒，进程 50.569 秒。
包含普通宿主、新连续测试、Whole 边缘/墙/大步长、观察会话复用、隐藏捕获重建和独立接缝解析。
新增普通测试的 5 种步长均检查 28,288 个内部样本，H1、H2 首个退出帧、后续 4 轮均无缺失；280°/秒正常转头和瞬时转头均覆盖，最终完整纹理与独立参考逐像素相等。

`Reobservation_Fixed01` 四步骤和额外往返原始画面已打开检查；192/192 自动检查通过（同一采集内的比较数，不是 192 个独立自动化测试）。
H1、H2、全部重复历史和直接完整参考 hash 都为 `6537765931154518915`，原先 13,073 个零包络全部消失，代理与纹理身份始终复用。真实墙仍裁切远处 Live；H1 首次就已完整。
此图形进程完成采集并停止 PIE 后仍退出 `0xC0000005`，保留为失败退出，不能因图像通过称整个协议通过。

## 完整原生回归与测试夹具修正

完整运行时修复 SHA：`c7c02fc07f36880c6fa6c44eef0e77774e89deff`。
最终 C++（含测试清理）SHA：`e7b78942493371e679aabf122c54120ba8cb1bac`。

第一次 `Reobservation_FinalFunctional` 完整选择 141 项，140 通过、1 失败，severe 4、进程退出 0。失败来自 V2 元数据测试 raw LoadPackage 后未清理旧地图世界，其 WorldPartitionSubsystem 在 OrdinaryHost 的 GC 中仍处于初始化状态。
通过给这些无 engine context 的临时加载地图添加局部所有权和 `DestroyWorld(false)` 配对清理修正；已有用户/编辑器 world context 不清理，资产不保存或改写。此修正只在测试代码，不是 GUI 退出堆栈的修复。
`Reobservation_MapLifetime` 定向混合顺序复测 18/18、severe 0；正式构建 `Reobservation_TestLifetimeBuild.log` 成功，22.42 秒。

随后 **`Reobservation_FinalFunctional2` 在同一最终 C++ 版本上完整重跑 141/141 通过**，129 clean、12 带 warning、0 failed/not-run/severe，测试 164.558 秒，进程墙钟 186.266 秒，退出 0。这不是 140 项加补跑的合并结果。
warning 包括引擎网络探测超时、预期的重复身份/容量拒绝，以及两个元数据测试再次清理已清理地图的提示；没有放宽产品断言或屏蔽 ensure。
选择覆盖 ObjectMemory 普通入口、新四步回归、全部 ArchitectureAudit、GrayObjectPolicy、ConfirmedWholeViewEdge、MovingLiveContinuity、HistoryGridV2、SpatialHistory、FastSweep、SightWeave ObjectPolicy/RevealPolicy、两组 V2、AtoBtoCAndMultiCounts、RepeatedResetLifetime50 和 M6P1。
包括墙后对象不确认、不扩张世界覆盖、真实墙遮挡随观察者变化、Partial 内部无缺口且外部未知、隐藏网格/颜色变化、源销毁与同 ID 替换、已擦除内容重建不复活、Never、StationaryOnly、真实新状态可增长、Reset/PlayStop/资源释放。

`Reobservation_NormalTurns` 在 280°/秒正常转头下单独重复原四步骤，67 张原图、192/192 比较通过，退出 0、severe 0，109.632 秒进程墙钟。已打开 H1/H2、远处 Live 和第二次退出相邻原图；没有把转头途中离开对象后的等待算成首次交接证明，首次交接另用快速退出逐帧与 01 三轮回放核验。

SightWeave 插件源码和插件公共接口没有修改。本轮没有重新 BuildPlugin，上一轮严格独立打包及最小插件宿主 21/21 是历史结果。新增项目诊断/细历史辅助接口由 DARKWELL 正式构建和非 Lab 普通宿主测试覆盖；完整灰层仍是项目后端。

## 其它房间的真实回归

`Reobservation_PolicyRegression` 使用原有 Contracts 驱动，三次真实 Play/Stop，178 张 2233×911 原始游戏视口图像，D3D12/SM6、Screen Percentage 100、AA4/TSR。01 首次交接像素检查 24/24 通过，已打开第一轮退出 00/01；03 隐藏移动保留旧状态、隐藏停止不新增端点、重新合法观察后记录端点；04 Never 无历史；05 墙缝无遮挡和再次受墙遮挡的 Live 切分仍正确，黄色 Partial 仍保留局部内容。
原图同时检查了蓝柜墙遮挡的相邻阶段、03 端点历史、02 外部局部切口。全部驱动断言完成并停止 PIE，但进程退出 `0xC0000005`，75.484 秒、severe 0，退出协议仍失败。

`Reobservation_Room02` 使用原 Episodes 驱动，51 张原图，六个最终快照各 896 个独立已知实体内部样本全部无缺口。已打开 cycle2 部分历史、cycle4 完整历史、隐藏 cap 后的完整历史、再次进入 00、退出 00/01：内部无旧竖线，新增未观察端点保持合法渐显，外部切口保留。51 帧采集与 Stop PIE 完成，但进程退出 `0xC0000005`，48.249 秒、severe 0。

这些退出失败没有被正常退出的 NormalTurns 或 Timing 运行覆盖。新测试地图清理修正只解决 native 混合测试进程的 WorldPartition ensure，不解释或修复真实 GUI 的 Slate/退出堆栈。

## 本轮定向成本与资源

`Reobservation_Timing`：真实 D3D12/SM6，正常项目画质，不截图、不运行密集逐样本诊断；关闭固定步长、帧率上限和 VSync。计入每个阶段的第一帧与所有过渡帧，无阶段内预热删样本。采集 722 帧，测量墙钟 20.269 秒、游戏时间 20.220 秒，整个编辑器进程 51.593 秒；退出 0、severe 0。不是十五分钟测试或完整系统性能验收。

| 阶段 | 帧数 | 系统 CPU p95 / max ms | 完整墙钟帧 p95 / max ms |
| --- | ---: | ---: | ---: |
| 首次受限观察 | 33 | 4.900 / 28.991 | 41.948 / 59.617 |
| 首次封存 | 28 | 1.268 / 30.078 | 31.425 / 65.891 |
| 背对对象走近 | 150 | 0.848 / 1.005 | 37.954 / 41.527 |
| 再次完整观察 | 33 | 0.353 / 0.700 | 32.401 / 36.044 |
| 再次封存 | 28 | 0.979 / 1.808 | 32.093 / 34.662 |
| 四轮远近重复合计 | 372 | 4.518 / 10.478 | 36.288 / 42.643 |

再次封存没有历史纹理上传或 cap 重建；没有新增每帧 Whole 历史重建或秒级封存延迟。蓝柜在整个重复过程中始终使用同一历史 epoch、proxy、texture；包括黄色对象在内，常驻计数始终是 record 2、proxy 2、texture 6、MID 4、cap 1、细历史 2,744,320 字节。

**常驻计数稳定不等于零分配。** Current 在受墙遮挡的空间纹理与无遮挡 1×1 纹理之间切换时，每次重建蓝柜三个源纹理；四轮有 24 次源纹理创建、0 次 MID 创建。它发生在模式切换帧，并非每帧创建；历史代理/纹理没有更换。修复前 Baseline03 和修复后 Fixed01 的 near_full_entry_00 都创建 3 个源纹理，Current 累积创建数都从 3 到 6，且此函数没有本轮代码改动。这项既有分配成本未在本轮消除，也不能被“同状态复用”口号掩盖。

重复段工作集从 4,375,822,336 到 4,393,410,560 字节（约 +17.6 MB），不能宣称进程内存零增长。全部 722 帧完整墙钟 p50/p95/p99/max 为 26.039/37.447/41.484/65.891 ms，193 帧 >33 ms、0 帧 >100 ms；完整帧预算仍不合格。没有用原生系统耗时替代完整帧门槛。此处有修复后的全过渡测量与修复前后资源对照，但没有同协议的修复前完整计时，不宣称精确性能提升百分比。批量历史初始化和长时系统性能未重测，上一轮阻塞继续保留。

## 命令和本机证据入口

从仓库根目录运行，RunName 必须唯一，既有结果不会覆盖：

```powershell
.\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot "D:\UE_5.8"
.\Scripts\RunGrayObjectPolicyTests.ps1 -RunName <名称> -Tests 'Darkwell.ObjectMemory+Darkwell.PropLab.ConfirmedWholeViewEdge+Darkwell.PropLab.ArchitectureAudit'
.\Scripts\RunGrayMemoryAudit.ps1 -RunName <名称> -Protocol Reobservation
.\Scripts\RunGrayMemoryAudit.ps1 -RunName <名称> -Protocol Reobservation -NormalTurns
python Scripts/AnalyzeGrayReobservation.py Saved/ArchitectureAudit/<名称>
.\Scripts\RunGrayMemoryAudit.ps1 -RunName <名称> -Protocol Contracts
python Scripts/AnalyzeGrayWholeTransitions.py Saved/ArchitectureAudit/<名称>
.\Scripts\RunGrayMemoryAudit.ps1 -RunName <名称> -Protocol Episodes
python Scripts/AnalyzeGrayMemoryEpisodes.py Saved/ArchitectureAudit/<名称>
.\Scripts\RunGrayMemoryAudit.ps1 -RunName <名称> -Protocol ReobservationTiming
python Scripts/AnalyzeGrayReobservationTiming.py Saved/ArchitectureAudit/<名称>
```

完整 141 项的精确 selector、测试状态和输出在 `Saved/GrayObjectPolicy/Reobservation_FinalFunctional2/Reobservation_FinalFunctional2.summary.json` 及同目录 Report/index.json。不是使用默认会包含全系统长性能测试的宽泛入口。
以上图形目录均位于 `Saved/ArchitectureAudit`，含 source.txt/source.patch、driver.py、editor.log、summary.json；图像运行另有 samples.json，性能运行有 timing.json、timing-stages.json 和 timing-summary.json。本机 Saved 原图/日志/运行数据不提交 Git，其它电脑需要重新采集。
最快查看修复前后：Baseline03 与 Fixed01 中的 `stage1_far_partial_live.png`、`stage2_H1.png`、`stage3_near_full_live.png`、`stage4_H2.png`，以及 `second_exit_00.png`/`01.png`。固定柜体内侧图像区域对完整参考的平均 RGB 误差由 H2 的 11.288 降到修复运行所有 38 张历史帧最高 0.211；同时存在独立几何内部断言，不能只靠同一缓存的 hash 自证。

## 最短用户复测与交付边界

打开 `/Game/Maps/L_SightWeaveGrayPolicyLab`，Play，在大厅 F 进入 05。站在南侧远处，从墙缝观察右上蓝柜，确认后转开；**第一次灰影就应完整**。不 Reset，穿过开口走近，看见完整蓝柜后再转开；灰影仍完整，继续远近往返也不回缩。墙继续裁切当前 Live，黄色柜子保持局部策略。随后快速对照 01 首次退出、02 分段补全、03 隐藏移动。

本轮没有更改地图/模型/材质资产、引擎、视野参数、对象策略或 stable 引用，没有恢复旧 stash、引入黑色层或新的玩法框架。修复是项目公共运行时入口，非 Lab 特判。
最终交付提交在本文所属 Git 提交和最终答复中标明；上方分别记录了原始起点、运行时修复和最终 C++ 验证 SHA。所有有效检查点立即推送到同一推荐分支。交付收尾核验本地/上游/远端一致、LFS 正常、工作区干净，并关闭测试/采证进程后重新打开 Lab，PIE 保持停止；电脑保持开启。

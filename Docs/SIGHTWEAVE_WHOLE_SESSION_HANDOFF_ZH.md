# WholeObject 连续观察资格修改与回归

状态：**本轮规则和受保护行为的自动功能回归完成，可供用户复测。** 完整系统性能与间歇 Editor 退出问题仍未关闭；不创建用户验收 stable。

本轮唯一产品变化：每次新的连续合法观察，重新满足对象原有的配置跨度门槛。过去的确认和完整历史不能跳过本轮门槛。本轮达标后，小块持续合法接触仍显示整件；真正失去接触后结束资格。历史按 HistoryMode 保留。未改变视锥、墙体查询、世界探索、邻居确认或相机深度。

起点 `bfe4e9ef73b2f9caf058d4d74cd4b68555b55226`，分支 `codex/darkwell-prop-memory-gameplay-lab`。实际安装 UE 5.8.2 CL 56702186，未升级引擎。未修改二进制资产，未触动 stable 或 stash，不创建用户验收 stable。

## 修改前确认的调用链与保护范围

| 调用链 | 本轮风险与保护 |
| --- | --- |
| Fog revision-matched coverage → CurrentLive 当前合法采样 / 物理遮挡扫掠 → RevealObservation.Observe | 保持原配置跨度、连续物理 footprint、墙体/视锥合法性；无效发布不视为空位或真正失联。 |
| UpdateTracked → AdvanceConfirmedWhole / WritePartRasters → 源 MID / 源纹理 | 新轮未达标只显示合法局部；达标后的完整 Current 不按墙体/视锥逐部分裁切，保留正常相机深度。 |
| 合法离开 / 隐藏运动 → FreezeCurrentForHiddenMotion → Stamp / FullGeometryMask → InitializeWholeCapture | 先交接捕获，再结束资格；保留首次离开帧、完整灰影、几何 AA，不能回退旧残缺状态。 |
| BeginCurrentObservation / ResumeUncontradictedObservation / Abandon | 未确认候选与长期记录分离；短看不能误删旧记录；只根据已记录的姿态/内容/反证决定是否复用。 |
| UpdateHistoricalContributionExclusion → AdvanceFineHistory → UpdateRecordTexture / Cap | 临时绘制互斥与永久证据分开；合法空位、其它反证继续有效，Partial 接缝/封口不改。 |
| FCurrentPresentation / FRecordVisual / ReleaseSourcePresentation / Reset / EndPlay | 资格结束不销毁资源；旧代理/历史纹理保持身份；局部与整件源纹理切换复用。 |

以上调用链及保护项已在施工前向用户列出。保护项还包括 StationaryOnly、Never、隐藏内容变化隔离、普通对象入口、贡献互斥、重复 Play/Stop 和 Reset 生命周期。

## 实现边界

- `FSightWeaveRevealObservation::EndSession` 只结束本轮资格及跨度进度，保留 footprint 与配置跨度。宿主在有效失联时先封存最后的合格观察，再调用它；无效覆盖不会结束本轮。支持的纯扫掠观察也先封存再结束资格，不显示非法终点 Current。
- 未达标的 Whole 使用独立未封存 Current 记录。`TransientCurrentSuppression` 仅作用于实际历史纹理 A 通道，不传给 FineHistory 的永久证据；离开后恢复原历史提交。原本的 Superseded 和 VerifiedEmpty 仍保持单调。
- 达标后通过 `TryResumeQualifiedWhole` 检查已封存姿态、内容及反证，再复用原记录、代理和纹理。不同姿态/内容或已被反证的记录不能走该入口。原 Partial 复用链保留。
- 源表现缓存一个备用纹理槽，最多保留局部、整件两种表现；切换时重新提交当前像素。缓存存在不代表资格，也不提供历史证据。
- Whole 捕获包含与细部几何相交的边界 footprint。该 footprint 的中心为空不能证明整格为空；仅对已捕获的 Whole 边界补查实际几何相交，并将捕获域加入帧内占据缓存键。合法覆盖查询不变，Partial 的中心采样不变。新的合格 Whole 在已确认、相同姿态和各 primitive 几何相同的情况下，完整替代旧 Whole footprint，避免反证后重建留下细把手边缘残片。
- 新增测试专用视口尺寸入口，固定视觉回放为原 oracle 的 2233×911；测试退出前恢复自动尺寸。真实墙钟性能协议不调用它，维持修改前后的同机原视口条件。

## 连续回归与证据

运行时及连续回归提交 `0029e274cbef74139524ac07d2ac79fa7de4fa63` 已推送。最终 C++ 为 Build8；以下最终原生、图像、独立插件/宿主和性能对照均使用这一版本。阶段失败保留在 Saved，不将定向补跑与完整回归混写。

| 最终证据 | 结果 |
| --- | --- |
| `Saved/Logs/WholeSession_Build8.log`，完整 DarkwellEditor Win64 Development | 成功，13.08 秒；不是 Live Coding。 |
| `Saved/GrayObjectPolicy/WholeSession_FinalFunctional2` | 141/141，132 clean、9 warning、0 failed、0 not-run；退出 0、severe 0；测试 168.528 秒、进程 190.276 秒。 |
| `WholeSession_Focused4` | 16/16，15 clean、1 warning；退出 0、severe 0。普通单体及三 primitive（主体、门、细把手）在 30/60/120/144 Hz 和 0.2 秒 hitch 连续回归。 |
| `Saved/ArchitectureAudit/WholeSession_Visual3` | 118 张原始 2233×911 图，D3D12/SM6；流程及 PIE 停止完成，退出 0、severe 0，进程 68.425 秒。 |
| 同一 Visual3 的三个独立 oracle | 新会话 332/332；原历史 192/192；当前 Whole / 相机深度 97/97。原产品图像断言保持。 |
| `Saved/Logs/WholeSession_StandalonePlugin.log` | UAT BuildPlugin StrictIncludes，Editor Development / UnrealGame Development / Shipping 全通过；NoPCH / NoSharedPCH / DisableUnity，退出 0，185.024 秒。 |
| `Saved/ArchitectureAudit/WholeSession_MinimalHost` | 仅加载上述打包插件的独立空宿主，无 Darkwell 模块；21/21 clean，0 warning、0 failed，退出 0，进程 31.199 秒。包含新会话跨度、公开配置、代理生命周期与生成夹具完整转换。 |
| `Saved/ArchitectureAudit/WholeSession_Contracts` | 178 张原图、三次 PIE 生命周期；脚本内原断言全部通过；24/24 首次 Whole 离开帧图像检查通过；退出 0、severe 0，64.190 秒。包含 StationaryOnly 隐藏移动/停止/再观察、Never、合法墙缝、Whole/Partial 与 Reset 释放。 |
| `Saved/ArchitectureAudit/WholeSession_Episodes` | 51 张原图；6/6 独立 Partial 表面检查，每组 896 个内部样本无缺口；完整后局部再观察与 cap 隔离保持正确。流程及 PIE 停止完成、severe 0，37.996 秒；但日志关闭后退出 `0xC0000005`，进程协议判定失败，不能写作全通过。 |

Visual3 四轮均从配置 100 cm 出发：短看合法覆盖约 0.129902、跨度 82.5 cm，只显示当前合法局部并保留其余灰影；连续移动视角达到 105 cm 后显示整件；退回同一小块合法覆盖仍显示整件；失联第一帧跨度归零且完整灰影交接。下一轮重新经历 82.5 → 105，旧记录不会赋予资格。原 H1 epoch 1、代理、纹理、36876 个捕获样本及捕获 hash `6537765931154518915` 保持一致。

原生连续测试还覆盖：不达标退出不生成新 Whole 历史、不覆盖旧记忆；合格再观察复用；无效发布保留会话；隐藏移位保持旧姿态；合法空位产生反证；隐藏返回和短看不能复活旧证据；重新合格生成新捕获并清退旧残片；Always / StationaryOnly / Never 与运动状态组合；墙后无非法接触或邻居确认；实际提交 A 通道的贡献互斥与预热后零纹理创建。仅更新旧测试中永久确认的预期；原尺寸、样本等价、完整表面与资源身份断言未删除或放宽。

人工按原始分辨率检查了短看/合格退出相邻帧与相机墙深度对照；确认灰影完整、首帧无空帧，真实相机墙仍遮住整件对象。固定视口仅用于可复现图像，真实性能协议保持原始非固定时间设置。

另外人工检查 Contracts 的 Whole 第一张离开帧、隐藏运动停止后的旧位置记忆与重新合法观察后的终点记忆；检查 Episodes 的 `cycle_4_history`、`cycle_7_history`、`diagnostic_caps_hidden` 原始图，内部表面完整且无接缝。原图、采样及退出码均保存在对应目录。

最终原生 9 个 warning 测试涉及：地图只读加载夹具的 CleanupWorld 提示、引擎网络连通性探测超时、重复 StableID 拒绝和容量到限 fail-closed。无 correctness failure；其中重复 ID 和容量警告属于相应负向测试预期。

已定位并修复的阶段问题：

1. `WholeSession_Focused1`：14 项中 13 项通过；新临时遮罩未接入贡献诊断。补齐诊断，并增加实际提交纹理 A 通道的独立互斥检查，原断言保留。
2. `WholeSession_Functional1`：139/141，退出 0，无 severe。一个真实回退来自旧复用入口借用了新 Current 姿态，造成旧历史尺寸变化；Whole 改为统一验证已封存姿态。另一个是合成批量夹具依赖上一轮永久确认，现由明确的合成完整观察满足每个夹具会话的同一门槛，样本等价断言不变。
3. `WholeSession_Focused2`：16/16，14 clean、2 warning，退出 0、无 severe，测试 15.105 秒；含 Whole 连续再观察、合法空位/隐藏返回、各 HistoryMode、插件跨度及上述两项完整样本等价检查。
4. `WholeSession_Visual1`：118 张原图、流程完整、退出 0、无 severe；视口变为 1616×831，三个固定视口 oracle 主动拒绝判定。未放宽断言，增加测试捕获尺寸后重跑。
5. `WholeSession_FinalFunctional`：141/141，但这是细把手扩展回归之前的中间版本；最终证据使用带 `2` 的完整重跑，不以旧运行名称冒充最终版本。
6. `WholeSession_Visual2`：当前/深度 97/97；原历史 192 项中 29 项失败。真实回退是三处细把手捕获边界被中心采样误判为空，导致旧记录与资源无法复用。修复实际 footprint 占据后重跑全部图像断言。该次流程与 PIE 停止完成，日志无 severe，但进程在日志关闭后退出 `0xC0000005`，不记作进程通过。
7. `WholeSession_Focused3`：15/16。新增多 primitive 用例暴露反证后重建遗留细边残片，修复相同几何 Whole 完整替代；同时新增的空位计数限定为 `InitialRemembered > 0`，避免把从未捕获的 padding 当成记忆损失。真实合法空位及不可复活断言保留。修复后 Focused4、完整 FinalFunctional2 和 Visual3 均通过。

## 资源与性能对照

基线 `WholeSession_BaselineTiming` 在任何修改前的干净 `bfe4e9e` 运行；修改后为 `WholeSession_FinalTiming`。两次串行、同机、相同 ReobservationTiming 路线/画质/遥测，D3D12/SM6、非固定时间、NoVSync、t.MaxFPS 0，不截图、不做密集逐样本图像诊断，没有并行 UE 或编译负载。性能协议不使用新增固定视口入口；沿用编辑器布局，未为基线额外记录独立视口尺寸元数据。包括所有过渡帧，没有在阶段内删掉首帧或尖峰。

两次均为 722 帧：实际测量 10.928 → 10.978 秒，游戏时间 10.908 → 10.959 秒。整体编辑器进程 89.184 → 42.672 秒包含启动/退出差异，**不能据此声称运行时提速**。

| 阶段 | 修改前系统 CPU p95 / max ms | 修改后系统 CPU p95 / max ms | 修改前完整帧 p95 / max ms | 修改后完整帧 p95 / max ms |
| --- | --- | --- | --- | --- |
| 首次局部观察 | 2.555 / 34.336 | 2.059 / 23.476 | 21.090 / 53.222 | 21.836 / 42.728 |
| 首次交接 | 1.455 / 28.064 | 1.278 / 26.776 | 15.245 / 49.852 | 14.542 / 47.196 |
| 再次完整观察，33 帧 | 0.378 / 0.749 | 0.346 / 4.118 | 17.685 / 24.218 | 16.663 / 17.191 |
| 再次交接 | 0.982 / 1.757 | 0.995 / 1.753 | 17.891 / 23.388 | 16.747 / 22.700 |
| 四轮重复，372 帧 | 2.233 / 5.456 | 2.310 / 5.762 | 22.505 / 26.725 | 21.841 / 26.175 |
| 全部 722 帧 | 2.137 / 34.336 | 2.143 / 26.776 | 23.938 / 53.222 | 23.950 / 47.196 |

新规则的可见成本：再次进入第 382 帧须重新证明跨度，CurrentReveal 从 434.4 到 3795.5 us，合法覆盖查询 7 → 11537（其中 sweep_queries 11530），该帧系统 CPU 从 0.749 到 4.118 ms。没有取消遮挡证明来消除此成本；重复段 p95 也从 2.233 到 2.310 ms。单次配对测量不支持精确提速/退化百分比或长时稳定性结论。

完整帧 16.6 ms 预算仍不合格：超预算帧 160 → 169，超过 33 ms 均为 3 帧。已有批量初始化尖峰及长时性能验收本轮没有重跑，不宣称关闭。

| 资源范围（全部重复帧） | 修改前 | 修改后 |
| --- | --- | --- |
| records / proxies / textures / MIDs / caps | 2 / 2 / 6 / 4 / 1，均不增长 | 2 / 2 / 6 / 4 / 1，均不增长 |
| FineHistory 字节 | 2,744,320 | 2,744,320 |
| 重复段纹理 / MID 新建 | 0 / 0 | 0 / 0 |
| 全 722 帧纹理 / MID 新建（含初次及独立参考 Reset） | 10 / 7 | 10 / 7 |
| 重复段进程 working set，首 / 尾 / 峰值字节 | 4,536,905,728 / 4,550,131,712 / 4,550,131,712 | 4,391,006,208 / 4,380,692,480 / 4,395,642,880 |

这条原配对路线每次初次接触就足够达标，没有进入局部纹理表示。新增短会话路线 Visual3 另外验证双表示缓存：三 primitive 的局部源纹理固定为 Texture2D_6/7/8，Whole 固定为 Texture2D_10/11/12；预热后该对象累计创建数恒为 6，四轮不再新建。整个房间纹理数恒为 9（比单表示多 3 张局部缓存），proxy 2、MID 4、cap 1、FineHistory 字节仍相同；短看时多一个未封存候选 record，离开或达标即收回，不增加历史记录。H1 的 epoch / proxy / texture 身份在所有轮次保持。资源复用不等于全进程零分配或内存零增长。

原始 `timing.json`、`timing-stages.json`、`timing-summary.json` 和 `before-after-comparison.json` 在相应 Saved 目录。基线退出 0；修改后采样及 Stop PIE 完成、severe 0，但日志关闭后退出 `0xC0000005`，性能进程协议为 **失败**，上面的已完成采样不能覆盖退出失败。

## 退出与既有未关闭项

本轮 Visual2、Episodes、FinalTiming 均在采集完成和 PIE 停止之后、日志关闭后返回 `0xC0000005`。Visual3、Contracts、完整原生和独立宿主正常退出。失败运行没有重命名或被正常退出覆盖。

前轮 `WholeCurrent_NormalTurns`、`WholeCurrent_Contracts` 和更早 `Reobservation_Room02` 已记录同样模式，见 [前轮交接](SIGHTWEAVE_CONFIRMED_WHOLE_CURRENT_HANDOFF_ZH.md) 与 [再观察交接](SIGHTWEAVE_REOBSERVATION_MEMORY_REFRESH_HANDOFF_ZH.md)。此前调试栈定位到退出时 Core / Slate 文本布局和控件销毁过程；本轮未取得新的逐次崩溃栈，因此只报告相同退出模式，不能据此断言每次崩溃具有完全相同根因，也不宣称修复。

新增产品回归（错误消耗历史、细部边界错误空位、反证后残片）已在最终完整测试和图像测试上修复验证；已有完整帧性能、批量历史初始化尖峰、长期运行性能验收和间歇退出问题继续单列，不互相掩盖。

## 复现与提交边界

在 `D:\UE_pro\Darkwell` 运行。RunName 必须换成新的名字，脚本拒绝覆盖证据。

```powershell
& Scripts/BuildEditor.ps1
$wholeSelector = (Get-Content Saved/GrayObjectPolicy/WholeSession_FinalFunctional2/WholeSession_FinalFunctional2.summary.json -Raw | ConvertFrom-Json).selector
& Scripts/RunGrayObjectPolicyTests.ps1 -RunName WholeSession_RetestNative -Tests $wholeSelector
& Scripts/RunGrayMemoryAudit.ps1 -RunName WholeSession_RetestVisual -Protocol Reobservation -WholeSessions
python Scripts/AnalyzeWholeSessions.py Saved/ArchitectureAudit/WholeSession_RetestVisual
python Scripts/AnalyzeGrayReobservation.py Saved/ArchitectureAudit/WholeSession_RetestVisual
python Scripts/AnalyzeConfirmedWholeCurrent.py Saved/ArchitectureAudit/WholeSession_RetestVisual
& Scripts/RunGrayMemoryAudit.ps1 -RunName WholeSession_RetestContracts -Protocol Contracts
python Scripts/AnalyzeGrayWholeTransitions.py Saved/ArchitectureAudit/WholeSession_RetestContracts
& Scripts/RunGrayMemoryAudit.ps1 -RunName WholeSession_RetestEpisodes -Protocol Episodes
python Scripts/AnalyzeGrayMemoryEpisodes.py Saved/ArchitectureAudit/WholeSession_RetestEpisodes
& Scripts/RunGrayMemoryAudit.ps1 -RunName WholeSession_RetestTiming -Protocol ReobservationTiming
python Scripts/AnalyzeGrayReobservationTiming.py Saved/ArchitectureAudit/WholeSession_RetestTiming
python Scripts/CompareWholeSessionTiming.py Saved/ArchitectureAudit/WholeSession_BaselineTiming Saved/ArchitectureAudit/WholeSession_RetestTiming
```

插件验证命令：`RunUAT.bat BuildPlugin -Plugin=<repo>/Plugins/SightWeave/SightWeave.uplugin -Package=<new output> -TargetPlatforms=Win64 -StrictIncludes`。独立宿主通过 `AdditionalPluginDirectories` 只加载新打包插件，以 NullRHI 运行 selector `SightWeave.ObjectPolicy+SightWeave.RevealPolicy+SightWeave.M4P1.Proxy+SightWeave.M4P1.Lab.GeneratedFixture.FullTransition`；完整命令与报告在 `WholeSession_MinimalHost/editor.log` 和 `Report/index.json`。

完整原生 selector 保存于 summary.json，覆盖 ObjectMemory、合法覆盖、ArchitectureAudit、GrayObjectPolicy、WholeViewEdge、MovingLive、HistoryGridV2、SpatialHistory、FastSweep、Lab、重复 Reset50、运行时多对象与 M6P1；不是只运行新的 100 cm 用例。Saved 原图、日志、Trace、打包文件及生成目录不提交 Git，其它机器需重采集。

Git diff 已审阅，`git diff --check` 与 `git lfs fsck` 通过；工作区无二进制资产修改。stable 远端引用保持 `7534163b9c5718700b610e7677f47fbaa79cf977` 和 `404a5820739638f1097eaae0aa7fba19733298c3`，stash 为空；未创建或移动 stable。

交付时重新打开 `/Game/Maps/L_SightWeaveGrayPolicyLab`，用户编辑器进程 31872；日志 `Saved/Logs/WholeSession_UserRetestEditor.log` 证实地图加载完成，Unreal MCP `IsPIERunning` 返回 false。没有遗留自动测试 UE 进程，电脑保持开启。

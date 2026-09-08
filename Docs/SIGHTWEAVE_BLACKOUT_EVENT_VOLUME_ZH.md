# Blackout Event Volume（2026-09-08，基线与 gameplay 修复）

## 人工反馈后的修复（2026-09-08）

稳定基线 `c4a6e83c44c3a61e0c8e7413ef378cee2258bf94` / `stable/sightweave-blackout-event-volume-20260908` 保持不动。下文原始交接记录的是该基线；第一次真实人工 gameplay 随后发现事件边界 hitch 与原视野边缘黑缝。本次只修这两个问题，不开始新切片。

详细证据、配置区别及命令见 [gameplay 修复证据](Evidence/SIGHTWEAVE_BLACKOUT_GAMEPLAY_FIX_20260908/README.md)。所有本次路线均为自动验证与代理检查的真实 D3D12 画面，**不是用户的新一轮人工 gameplay 验收**；仍需用户在实际关卡重测。

### 黑缝根因与修复

不是固定 AABB 四边的 epsilon/取整差，也不是 GPU 纹理未刷新。跨区 SpatialPartial 在 Begin/End 时原来会强制 `FreezeCurrentForHiddenMotion`，随后 `ForgetKnowledgePreservingLive()` 清掉整个 Local 的已知 D/capture mask（包括目标外），再用零 delta 重建 Current。旧历史按 footprint overlap 交出贡献时，新的 Current 未完整接住原边缘的 bilinear/AA 支持，留下沿**切换当时视野**的黑线。因此进出均能制造一条线，后续新扫过区域却正常。

现在同 pose/content/geometry、仍合法 Current 的静态 Partial 把已有数据和资源转入单调递增的新 write epoch，不为一次区域开关制造历史捕获；真正离开观察、移动或 revision 改变仍走原封存路径。区域清理只按原半开 AABB 样本中心谓词删除目标内事实，保留目标外 Local 知识。独立 Block 保留原先的 pre-block 捕获语义，不能把复合 Clear+Block 的优化直接套上去；没有 Current 的解封仍失效旧 LocalEpoch，防止旧 Clear epoch resume。

历史/Current 交接只在已有事实证明的完整滤波内部消除人为 AA 分界，使用已有 appearance/remembered mass，不强制 alpha；Current G 和二值历史 A gate 不扩大。单次事务先完成原 Clear/Block authority，再发布最终纹理和 cap，同调用返回前完成，没有额外首显帧。Whole 原子规则、Partial 精度、Unknown、F 覆盖、cap 与 teardown 语义不变。

### Hitch 根因与修复

真实主胶囊一次进入/退出各只有一次 Begin/End，F 路径也在同一 blackout core 卡住。基线细分计时：首次进入 transient exclusion 847.49 ms、historical ownership 782.87 ms；退出 ownership 532.68 ms。重复全量封存/历史交接、占用查询、纹理/cap 重建是热点，资源重建仅约 5 ms，不是主要 GPU fence/readback 问题。

同姿态 Current 转移消除上述不必要历史；Clear+Block 合并显示发布，按目标样本限定 dirty/clear，修正 Partial transient 的重复失效条件，并复用精确 physical snapshot、AABB 轴成员缓存与纯 CPU 栅格工作。并行任务在本调用内 join，没有异步旧结果或跨帧状态窗口。未加入新性能系统；`DARKWELL_BLACKOUT_TIMING=1` 的计时只在非 Shipping 启用，三个纹理 enqueue 有独立 scope。

同公司机器、同 VolumeDemo 路线首个 Begin：2522.99 → 25.10 ms；End：998.50 → 9.34 ms。原生 D3D12 20 次进入/20 次退出，3000 个完整帧的原始 CSV 和统计在证据目录；不把带 SceneCapture/Flush 的自动测试帧当性能数据，不跨机器毫秒 A/B。

最终原生重跑 Begin 最大 24.38 ms、End 最大 11.77 ms；进入/离开窗口最大完整帧 45.40 / 25.53 ms，3000帧最大45.40 ms。20/20主胶囊 callback 与20/20事件严格对应，所有帧状态错误为0。两轮原生运行结束均为4 records，驻留 cells/masks 完全一致。

### 回归与后续

- 完整 `DarkwellEditor Win64 Development` Build 成功，最终计时 scope 构建 15.54 秒。
- 冻结功能回归 156/156，通过并保留全部 142 个基线测试。
- D3D12 BlackRegion 合同 23/23 + VolumeDemo 1/1，原 9/9 全部逐名覆盖；额外 repeated transition 1/1，20 次真实胶囊进出，0 warning。
- seam 探针五阶段连续已知位置分别为 192188 / 165308 / 165308 / 165308 / 199233，缺失全部 0；37° Partial、Whole、四边/角点、重复 Clear/Block、停用后不复活与 reobserve 通过。历史-only 边界诊断不作为验收 gate。
- 全 SightWeave 默认项目 D3D12 是 **312/314**；两个既有 M4P1 非抖动 CustomDepth 像素基线失败，原始夹具下独立 **2/2** 通过。保持断言和 TAA/TSR，不修改项目配置；这不是默认项目单进程全绿。既有失败见 `SIGHTWEAVE_DARKWELL_VISUAL_RESCUE_REPORT.md` §11.5–11.6，本次保留失败与隔离重跑报告。
- 下一步仍是用户实际关卡人工复验两个 gameplay 缺陷；不继续开发新功能。

---

## 原始 Event Volume 基线交接

起点 `aa201c20b3c68ca3fad748e2c034b51d2795c0ae`。公司仓库 D:\UE_projects\LastLight，UE 5.8.2。仅增加真实关卡 overlap 来源，不修改 Trigger、区域、知识、渲染、形状或优先级规则。

## 接口与范围

新增可放置 C++ `ADarkwellBlackoutEventVolume`，拥有 QueryOnly 的 UBoxComponent `EventBounds` 和现有 `UDarkwellBlackRegionEventAdapter` 子组件 `EventAdapter`。编辑器调整 EventBounds/Actor Transform，并在 EventAdapter.Target 指向现有固定 AABB Trigger。

事件 Box 是玩家参与范围；黑区仍由 Target 原位置/HalfExtentXY 定义，两者完全独立。Volume 从不读取/改写黑区范围，也不调用 ConfigureRegion/Clear/Block/Knowledge。

## Begin / End 来源与共存

只接受 ADarkwellCharacter 主 Capsule 的原生 OnComponentBeginOverlap/EndOverlap，附属武器/身体部件不计数。单玩家、一个参与者；没有引用计数或优先级。重复 overlap 更新不重复开始；离开最后的主胶囊 overlap 时 EndEvent。正常成功后 Volume 不 Tick。初始化时 floor authority 可能未就绪，仅对待完成的首次进入暂时重试，离开/死亡/销毁会取消。

事件开始仅经 Adapter.BeginEvent；结束仅经 Adapter.EndEvent。F 可在事件期间手动开/关，重复 overlap 不改回；离开事件范围时结束当前事件。原测试事件命令仍独立保留，不用它驱动 Volume。HUD 显示 Volume Event Started/Idle 与 Target Active/Inactive，二者不混淆。

## 生命周期

玩家增加原生 OnDied 委托，在原 HandleDeath 完成死亡处理后同调用广播。Volume 订阅它以及玩家 OnEndPlay；死亡/销毁/离开世界立即结束事件并移除委托、清除弱参与者、停止待开始重试。尸体重新 overlap 不开始。Volume Destroyed/EndPlay 同样幂等结束，Adapter 和 Trigger 自身清理继续兜底；不恢复已 Clear 的灰色。

world teardown 保留上一片修复：释放 Block/owner/modifier，但不再派发无用的 Scene 显示重建，不产生旧 SpawnActor warning。

## 人工流程

运行 `Scripts/LaunchBlackRegionLab.ps1`，无需控制台命令。初始玩家 (-200,-130)，在事件框外。青色线框是 Lab 范围指示，不是 Knowledge、黑雾或 VFX；未知场景本体仍按原规则显示。

事件框中心 (10,-30,90)，半尺寸 (150,140,140)，XY [-140,160] × [-170,110]。固定黑区仍为 XY [-160,130] × [30,140]。进入以角色胶囊与框的实际接触判断，不是中心点判断。

1. 保持在青色框外观察橙色 Whole、蓝色 37° Partial，转开形成灰色。
2. 用 WASD 走进青色框，Volume Event 自动 Started，Target Active，已知黑区自动 Clear+Block。
3. 在框内观察黑区，Live 正常；转开后黑区保持 Unknown，框外记忆保留。
4. 走出青色框，Event Idle/Target Inactive。保持没有合法观察，旧灰不会恢复。
5. 再次合法观察并转开，才有新 Memory；可反复进出。
6. 在框内靠近绿色开关（150 cm）面向它按 F，验证手动覆盖；重复 overlap 不强行改回。

## 验证

证据在 Docs/Evidence/SIGHTWEAVE_BLACKOUT_VOLUME_20260908。自动 VolumeDemo 使用真实 ADarkwellCharacter 胶囊移动和引擎 overlap，覆盖进出、重复更新、F、死亡、尸体重入、销毁玩家、销毁 Volume、已在框内时生成 Volume、world EndPlay。没有直接调用 Enter/Leave 或用适配器调用假装 overlap。

保留调查失败：旧 SceneCapture 世界仅手动初始化各 Actor，不具备真正的 world BeginPlay/InitializeActorsForPlay。物理 overlap 已成立，但 AActor::ProcessEvent 因 World.AreActorsInitialized 为 false 丢弃动态委托；单独设置 BegunPlay 或推进一帧不够。最终使用 InitializeActorsForPlay、world BegunPlay 和物理首帧，并配套 World.EndPlay。所有失败 Saved 记录保留，没有删除场景或放宽断言。

通用 RunBlackRegionTriggerTests.ps1 把新 Volume 路线放在独立的 `${RunName}_Volume` 有界运行中，保留此前全部测试，不通过加长单次超时容纳更多场景。

下一最小切片建议：把现有 Volume + Adapter + 固定 Trigger 组合摆进一个实际关卡区域，验证关卡动线；不增加黑层核心规则。

## 最终结果

- `Scripts/BuildEditor.ps1`：完整 DarkwellEditor Win64 Development 目标构建成功，非 Live Coding。
- `Scripts/RunUnknownPartialCutTests.ps1 -RunName BlackVolumeFinal -Tests 'Darkwell.BlackRegion.VolumeDemo+Darkwell.BlackRegion.CleanLab+Darkwell.BlackRegion.Contract+Darkwell.BlackRegion.CurrentPartialProbe+Darkwell.UnknownRegion+Darkwell.UnknownPartial'`：D3D12/SM6，9/9 通过，0 failed / not-run / severe，165.12 秒。唯一测试 warning 是 google.com/generate_204 HTTP 超时；没有 SpawnActor warning。
- VolumeBlackLab 的 00–07 图序列确认初始 Unknown、原灰、进入 Clear、Blocked Live、离开 Live 黑、退出首帧及等待仍黑、重观察重建。Whole / Partial / 37° 和原 F 回归通过。
- 另以 LaunchBlackRegionLab 启动实际游戏窗口，保存 Manual/01_outside、02_entered、03_exited。桌面工具短按未产生持续 WASD 移动，故用引擎 BugItGo 定位实际 Pawn，进入后用 Walk 恢复其自动开启 Ghost 时关闭的碰撞，观察真实 overlap 启动；退出定位触发 overlap 结束。此补充证据不是人工 WASD 全流程，自动 VolumeDemo 则始终保持胶囊碰撞并以 SetActorLocation 实际进出。
- 实际窗口确认 Started/Active 自动清灰，退出 Idle/Inactive 不恢复旧灰，正常退出；完整日志存 logs.zip。人工操作步骤仍为上面的 WASD 进出，无需命令。

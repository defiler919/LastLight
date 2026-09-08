# Blackout Event Volume（2026-09-08，完成）

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

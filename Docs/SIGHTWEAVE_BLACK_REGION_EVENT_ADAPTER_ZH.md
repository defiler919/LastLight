# Black Region：玩法事件适配器（2026-09-08，完成）

起点 `b4ed5f175f590a23f6c372dca803dab7ab37044a`，公司 `D:\UE_projects\LastLight`，实际 UE 5.8.2。仅实现通用事件边沿到现有固定 AABB Trigger 的适配，不引入事件总线、Monster、移动黑区、shape、SaveGame、SuppressLiveVision 或性能专项。

## 接口 / 事件语义

`UDarkwellBlackRegionEventAdapter` 是无 Tick 的 C++ ActorComponent，可通过编辑器添加到事件拥有者。`Target` 在实例中指向已有 `ADarkwellBlackRegionTrigger`，蓝图只读；C++ 事件或蓝图事件绑定可调用：

- `bool BeginEvent()`：事件开始，调用 Target.Activate；成功后才记录 Started。未开始播放、销毁/teardown、目标无效/跨世界或 Trigger 拒绝时返回 false，可在条件就绪后重试。
- `void EndEvent()`：仅在 Started 时调用本次开始所绑定 Trigger.Deactivate，然后回到 Idle。没有开始过的 End 以及重复 End 无副作用。
- `bool IsEventStarted()`：查询事件状态，**不是**区域 Active 状态。使用原生 `Darkwell.BlackRegion.Event.Idle/Started` tags；区域状态仍只由 Trigger 拥有。

成功开始时保存弱目标句柄，End 不会误关后来重新配置的其他 Target。目标销毁后可安全结束，组件不延长目标生命周期。重复 Begin 不再调用 Activate，不再次 Clear，即使 F 已手动停用也不会重新激活。失败的 Begin 不吞掉下一次合法开始。

适配器没有 ConfigureRegion、Clear、Block 或 Knowledge 算法。Trigger 原同调用先 Block 后 Clear、样本精度、Whole 原子边界、Partial 37°、cap 和首显合同保持不变。结束只解除 Block；旧灰不复活，合法 Live 保留，新的合法观察才能重建 Memory。

## 与 F 共存

这是明确的边沿命令语义，不是持续占用/优先级系统：事件开始打开；F 在事件期间可开/关；重复开始不覆盖 F；事件结束关闭。事件已结束后 F 再次打开，重复 End 不会关闭这次 F 激活。没有每帧强制覆盖、引用计数或多事件聚合。本片每个演示事件绑定一个现有 Trigger；并发事件仲裁不在本片内。

HUD 同时显示 Trigger Active/Inactive 和 TEST EVENT Started/Idle，避免把手动覆盖后的两个状态混为一谈。F 的原距离、朝向、遮挡、提示和开关接口保持原样。

## 干净 Lab 的确定性测试事件

`Scripts/LaunchBlackRegionLab.ps1` 仍启动干净 `/Game/Maps/L_BlackRegionLab`。fixture 拥有默认 Idle 的 `LabBlackoutEvent` 组件，开始播放只绑定目标，不自动开始事件，也没有压力路线。

1. WASD/鼠标探索 Whole 与 37° Partial，转开形成灰色。
2. 在任意位置打开游戏控制台（~），执行 `Darkwell.BlackRegionLab event_begin`。这是可重复 C++ Lab blackout 测试事件的开始通知，经适配器激活，完全不要求靠近 F 开关。
3. 关闭控制台；框内灰色清成 Unknown。重复 event_begin 不再次 Clear。
4. 观察黑区：Live 正常；转开/离开：仍是 Unknown。
5. `Darkwell.BlackRegionLab event_end`：只解除 Block，保持不观察时旧灰不恢复；重复结束无效。
6. 重新合法观察再转开，形成新 Memory。`event_status` 查询事件和 Trigger 两个状态。
7. 可在事件 Started 期间靠近控制台按 F，验证手动覆盖；再次 event_begin 不强行改回，event_end 关闭。

## 生命周期与退出 warning

EndPlay 和 OnComponentDestroyed 都幂等 EndEvent；Idle 组件销毁不会关闭独立的 F 激活。Trigger 原 Destroy/EndPlay 和 subsystem token 清理继续生效。

已定位上一轮 Active 退出 warning 为我们的生命周期路径：ReleaseGameplayControl -> ReleaseBlockRegistration -> MemoryScene.SetMemoryWriteBlock(false) -> BEFORE_SAMPLE_BLOCK_CHANGE 封存 / Live 重查询 -> SpawnActor；此时 UWorld 已 BeginTearingDown，世界拒绝创建显示 Actor。旧实际窗口日志包含两次该 SpawnActor warning，并非无关外部噪声。

修复只在 `World->bIsTearingDown` 时跳过 MemoryScene 回调和 Publish，仍注销 runtime modifier、清除 Block/owner、更新 authority revision。正常运行时的 Deactivate（包括普通 Actor/Component 销毁）仍走原封存和显示路径，不延迟解除，不篡改知识。即将销毁的 Scene 无需重建显示。

## 验证 / 后续

完整构建、自动与 D3D12 证据见 `Docs/Evidence/SIGHTWEAVE_BLACK_REGION_EVENT_20260908/README.md`。新 `Darkwell.BlackRegion.EventDemo` 复用同一 clean fixture 的真实帧序列，独立输出 EventBlackLab 截图；原 F CleanLab 和 PartialProbe 仍运行。

下一最小切片建议：将此适配器绑定一个具体关卡事件的原生开始/结束通知，保持单一固定框；先做一个实际事件来源，不扩展并发仲裁或新知识规则。

最终完整 Build 成功；BlackEventFirst 22/22 通过（21 clean、1 外网 HTTP 超时 warning、0 fail）。真实窗口 event_begin/event_end、F 覆盖、双状态 HUD 通过。Active 退出 SpawnActor warning 从旧日志 2 次降为 0，正常退出；精确日志与 DLL 哈希已归档。

最终检查补上 Lab HUD/命令/EndPlay 对显式移除事件组件的 IsValid 防护。再次完整构建成功，BlackEventAccepted 重跑两个直接相关 D3D12 测试通过；最终截图、源码 patch 和构建报告一并归档。

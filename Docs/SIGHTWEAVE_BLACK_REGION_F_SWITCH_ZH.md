# Black Region：真实 F 键控制台（2026-09-08，完成）

起点 `b1b6ba3f570d3fe190762eea9b6fc579c1ada9fa`。本片仅接入固定 AABB Trigger 的 F 交互；不修改知识、栅格、渲染、cap、Whole/Partial 或区域算法。

## 接口与输入

新增原生可放置 `ADarkwellBlackRegionSwitch`，实现现有 `IDarkwellInteractable`。编辑器指定 `Target`（BlackRegionTrigger）和 `InteractionDistance`（默认 150 cm）；控制台无 Tick，也不维护第二份 Active 状态。现有 Character 映射 `F -> InteractAction`，绑定 `ETriggerEvent::Started`，再调用现有 `UDarkwellInteractionComponent::TryInteract()`。因此一次按下切换一次，按住不会每帧反复 Clear。

统一交互组件继续检查前方 60° 半角、碰撞查询、视线遮挡及默认 300 cm 上限；控制台额外限制自身中心的 XY 距离 150 cm，直接调用 Interact 也不能绕过该距离。超距、背对、目标已销毁或自身未 BeginPlay/正在销毁时不能交互。

原 HUD 显示 `[F] Black region INACTIVE - Activate ...` 或 `[F] Black region ACTIVE - Deactivate ...`；顶部常驻实际区域状态，底部说明靠近绿色控制台并面向它。状态来自 Target->IsActive，不会与调试 console 命令失步。失败显示 Trigger 原失败原因，不假报 Active。

## 生命周期和知识语义

Inactive 时仅调用 `Target->Activate()`；Active 时仅调用 `Target->Deactivate()`。没有复制 ConfigureRegion、ClearMemory、BlockMemoryWrites。Trigger 仍负责同一调用内先 Block 再 Clear、幂等、独占 owner 和固定框。重复 F 是有意的启停切换，重复 Activate/Deactivate 的幂等性仍由 Trigger 保证。

控制台 Destroyed/EndPlay 幂等调用目标 Deactivate；Trigger 自身 Destroyed/EndPlay 和 region subsystem teardown 保留原兜底。销毁目标不会留下可用交互；销毁控制台也不会留下无人可关的 Block。解除只释放 Block，不撤销已 Clear 的事实，不恢复旧灰。再次合法观察才能产生新 Memory；Live 保持原合法视野/照明规则。

## 干净 Lab / 人工步骤

运行 `Scripts/LaunchBlackRegionLab.ps1`。地图仍是 `/Game/Maps/L_BlackRegionLab`，D3D12/SM6。C++ fixture 增加一个绿色控制台，位置 `(-230,-30,40)`，位于黑区外；原 Whole、37° Partial、固定 AABB 和起点不变。控制台作为第四个静态记忆源使用既有 Whole policy，初始也为 Unknown，不预填知识。没有 Moving/Multi 路线、自动激活或新形状。

1. 进入 Lab，用 WASD/鼠标探索，观察橙色 Whole 和蓝色 37° Partial，再转开形成灰色。
2. 走到左侧绿色控制台附近（150 cm 内），面向它，看到 `[F] ... INACTIVE` 后按 F。
3. 灰色在固定黑区内被清除；Whole 完整消失，Partial 仅框内被切除。顶部和提示变成 ACTIVE。
4. 观察/进入黑区，Live 正常；离开或转开后框内保持 Unknown。
5. 回控制台按 F 停用。保持不观察目标，旧灰不会自动回来。
6. 再次观察后转开，新 Memory 才形成。远离控制台或背对它按 F 无效。

## 验证

完整 Editor Build 成功；最终 `BlackSwitchAccepted` 为 21/21 通过（20 clean、1 外网 HTTP 超时 warning、0 fail）。实际游戏窗口 F 激活/停用、提示更新、背对拒绝和停用无旧灰均验证通过；正常退出完成 world cleanup。

结果及证据见 `Docs/Evidence/SIGHTWEAVE_BLACK_REGION_F_SWITCH_20260908/README.md`。自动 CleanLab 经同一交互组件启停，并验证超距（含直接调用）、背对、聚焦提示、状态切换、控制台/目标销毁释放。原完整定向集合覆盖 Whole/Partial/Unknown/37°、Trigger、Partial fence 与 CurrentGrid 生命周期。

下一最小黑色层切片建议：一个明确的玩法事件适配器驱动现有固定 AABB Trigger；不引入新形状、SuppressLiveVision 或独立知识逻辑。本轮不实施。

最终 DLL 的实际窗口再次验证 F 启停后，在 Active 状态正常退出。原解除 Block 的封存路径在 teardown 中触发一次被世界拒绝的 SpawnActor 非致命警告；日志正常到达 LogExit，没有断言或崩溃，完整日志保留在证据 zip。

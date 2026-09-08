# 固定 AABB 黑区玩法触发器（2026-09-08，完成）

最新运行时崩溃修复、独立干净 Lab 入口与验证见 [运行时稳定性交接](SIGHTWEAVE_BLACK_REGION_RUNTIME_STABILITY_ZH.md)。本文件下方保留前一切片记录；旧 Moving Lab 人工入口已被替换。

**本切片完成：可放置 C++ Actor、显式启停接口、Lab 演示入口、生命周期清理、完整 Editor Build 和 12 项真实 D3D12 定向回归通过。** 公司仓库 `D:\UE_projects\LastLight`，分支 `codex/darkwell-prop-memory-gameplay-lab`，起点 `e39351e3481f7a7819697386d1d5d0cf43bd4a70`。实际引擎 `D:\UE_5.8` 为 5.8.2 CL56702186。运行时、测试、交接和证据同本提交，最终 SHA 由本文件所属提交确定。

## Actor 与接口

`ADarkwellBlackRegionTrigger` 位于 `Source/Darkwell/{Public,Private}/VisionPresentation`，可在编辑器放置，也可由 C++ Spawn；可派生 Blueprint 绑定关卡事件，核心逻辑全部留在 C++。默认 **Inactive**，不自动激活，不进行 Tick，不以碰撞自动切换状态。

| 项目 | 语义 |
|---|---|
| Actor Location XY | 首次成功激活前的世界 AABB 中心 |
| `HalfExtentXY` | 编辑器配置的世界厘米半尺寸，每轴最大 320 cm；不乘 Actor Scale |
| `BoundsGuide` | 无碰撞的编辑器线框，世界轴对齐；Z 高度仅作指示，不是新知识维度 |
| `bool Activate()` | 申请独占控制权，复用 ConfigureRegion，执行 Block + Clear；成功返回 true |
| `void Deactivate()` | 仅释放自己持有的 Block/控制权，重复调用无副作用 |
| `IsActive()` / `GetState()` | 活动状态；原生 tags 为 `Darkwell.BlackRegion.Active` / `.Inactive` |
| `GetLastFailure()` | 未就绪、无效/不同框、Whole 横切或控制权冲突等失败反馈 |
| `GetFixedBounds()` | C++ 查询实际固定框；首次成功后锁定，不跟随运行时 Actor 移动 |

状态与失败反馈为 Transient。编辑器搜索 `Darkwell Black Region Trigger` / `DarkwellBlackRegionTrigger` 放入已有 SightWeave 玩家/floor authority 的关卡，调整位置和 HalfExtentXY 即可。旋转、缩放不会把框变成 OBB。运行时只有已初始化且处于 BeginPlay/Play 的 Actor 可以激活；EndPlay 后不能重新抢占 Block。

本版严格保留原区域子系统的范围：**每个世界一个固定 XY AABB**，最大 640×640 cm，原 2.5 cm 网格及 `[Min, Max)` 样本中心规则不变。整个世界生命周期内不能换框；销毁 Actor 不重置区域知识网格，否则会让已 Clear 的地面恢复默认 Remembered。第二个 Trigger 不能抢走正在使用的 Block，也不能解除别人的 Block；停用后，同一框可由另一个 Trigger 接管。切换不同框需要新世界。

## 启停与清理

激活使用原 `UDarkwellMemoryRegionSubsystem::ConfigureRegion / SetBlockMemoryWrites / ClearMemory`，不复制知识算法。在同一调用内先 Block、再 Clear，防止 Clear 的合法 Live 重查询在随后启用 Block 的封存步骤中重新留下记忆。成功后才发布 Active；失败释放本次取得的控制权/Block。已 Active 时再次 Activate 不 Clear、不增加 authority revision。

激活期间仍按原合法 Vision/照明显示 Live，但阻止区域内新 Memory；离开 Live 后保持 Unknown。停用只取消 Block，不补回地面 bits、旧 epoch 或样本。离开状态下停用及空闲没有复活；新合法观察才重建。如果停用时仍处于合法 Live，之后的合法观察可按原规则建立新知识，没有额外强制延迟。

`Destroyed` 和 `EndPlay` 都幂等释放所有权。子系统 `Deinitialize` 再作兜底。清理使用原已注册 token，即使当前 floor scope 已变化，也能释放原 modifier；已随 runtime teardown 移除的 token 按已释放处理。控制权比较不依赖 Actor 在销毁过程中仍满足 `IsValid`。只有 Block 与控制权被释放，已清除的知识和固定域仍保留。

Whole 横切继续由原子规则拒绝且保留旧事实；完整包含时清 Whole。SpatialPartial 继续逐 coarse/fine 样本判断，区域外样本字段不变。没有修改上一片灰层 B 过滤修复、FrozenAAEnvelope、cap 或 0 额外首显合同。

## 人工测试

推荐从 PowerShell 启动独立交互游戏窗口：

```powershell
& Scripts/LaunchBlackRegionLab.ps1
```

它打开 `/Game/Maps/L_ProjectFogPropGameplayLab?PropLabOriginal?InWorldControls`，D3D12/SM6，1280×720，不运行自动路线、不自动激活。也可在已有 PIE/游戏控制台输入 `Darkwell.BlackRegionLab open` 进入此 Lab。

1. 打开控制台（`~`），输入 `Darkwell.BlackRegionLab setup partial`。会生成 **Inactive** 的原生触发器，目标是 37° 旋转的柜体；线框仅为 Lab 边界提示，不是黑雾特效。Setup 本身不 Configure/Clear 区域。
2. 关闭控制台，用原有 WASD/鼠标移动、观察 `ROTATE` 柜体，再转开视线，建立并看到已有灰色 Memory。
3. 输入 `Darkwell.BlackRegionLab activate`。再输入一次可检查幂等；`Darkwell.BlackRegionLab status` 查看状态。
4. 走近/观察柜体：区域内 Live 正常。离开或转开：框内保持 Unknown，框外已有灰保留。
5. 保持没有合法观察，输入 `Darkwell.BlackRegionLab deactivate`。旧灰不会恢复；重复停用没有额外效果。
6. 再走近/观察并离开，新的 Memory 才能建立。

验证 Whole 时先 `Darkwell.BlackRegionLab open` 重开世界，再 `Darkwell.BlackRegionLab setup whole`。此时框完整包含柜体，Whole 不会被中间切开。不要在同一已配置世界用新的 setup 试图换框；命令会拒绝而非偷偷重置历史。

Lab 命令是 Development 演示入口，非 Shipping 接口；产品关卡使用 Actor 的 C++/BlueprintCallable 方法。现有输入和地图资产均未改写。Lab 控制台入口已自动执行验证；本报告的视觉证据来自测试世界 SceneCapture，不声称人工实走或 PIE 屏幕验证。

## 验证与证据

```powershell
& Scripts/BuildEditor.ps1
& Scripts/RunBlackRegionTriggerTests.ps1 -RunName BlackTriggerAccepted
```

完整 `DarkwellEditor Win64 Development` Build **Succeeded**，日志 `Saved/GrayObjectPolicy/BlackTriggerAcceptedBuild.log`。最终 DLL SHA-256：`5554090a0bb546593d6292369a6ed566160505911d03fd703e32a2e818d25dc4`；之后未修改 C++。

最终 **12/12 通过，11 clean + 1 warning，0 failed / not-run / severe，exit 0**。唯一 warning 是 UE 的 Google `generate_204` HTTP 连通性探测超时。复用原有有界前台握手和电源 guard，未放宽时限或前台要求。

新增最小覆盖：Whole、SpatialPartial 两项合同测试，LabEntry，以及触发器驱动的 TemporalSurface。覆盖默认未激活、无效范围/Whole 横切无副作用、重复启停 authority revision 不变、竞争者和非所有者不能解除 Block、Live 独立、离开/停用不复活、新观察重建、重新激活清除、Destroy、RemovedFromWorld EndPlay、真实 `World.EndPlay`、子系统清理及 EndPlay 后拒绝重新激活。原 UnknownRegion、UnknownPartial（含 37° 条纹断言和旧直接接口时序流程）、照明边界、A0、Whole/Partial cap 回归一起通过。

新增 D3D12 流程以 227 个真实引擎帧、768×768、持久 view state、时序 AA 开启（项目 AA=4）和固定曝光运行。已核对正常 Live、离开 Block、停用首帧、空闲与重新观察：没有新增竖纹/seam，没有旧灰复活。cap 和灰层都开启。它是测试世界 D3D12/SM6 渲染，不是性能测量。

- [触发器 D3D12 阶段图](Evidence/SIGHTWEAVE_BLACK_REGION_TRIGGER_20260908.png)
- [完整测试摘要、源码/截图哈希和过程记录](Evidence/SIGHTWEAVE_BLACK_REGION_TRIGGER_20260908.json)
- 原始图、日志与报告：`Saved/GrayObjectPolicy/BlackTriggerAccepted`

过程失败保留：前两次编译分别修复了 TObjectPtr 循环声明和测试缺失头文件；`BlackTriggerFirst` 的生命周期夹具缺少 PostInitializeComponents，现增加已初始化/BeginPlay 断言并走真实 World.EndPlay；框断言改为精确比较实际作者参数生成的中心＋半尺寸结果及其锁定值，没有降低精度。`BlackTriggerFinal` 曾 12/12 通过，复核后又添加 Transient 和 EndPlay 后激活守卫，最终重新 Build 并以 `BlackTriggerAccepted` 12/12 验收。

## 下一最小黑色层切片

建议把此 Actor 接到**一处现有 F 键交互开关**，增加明确的开启/关闭提示，仍调用相同 Activate/Deactivate，验证离开/返回开关和重复操作。这样把已验证的控制台演示接成一处真实关卡交互，不扩大知识或形状逻辑。本轮没有实现这个下一片。

未增加其他 shape、Monster Adapter、SuppressLiveVision、SaveGame、黑雾动画/VFX 或性能专项。灰色 checkpoint `eeeec6506d1fecfd2d05bd08095b8230287a4d4f`、stable/tag 不动；A1/P1/B0 默认仍为 0，cold184 未改未重测，**INITIALIZATION 仍 FAIL**。

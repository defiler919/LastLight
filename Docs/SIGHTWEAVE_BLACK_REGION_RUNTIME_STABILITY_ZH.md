# Black Region：运行时崩溃与干净人工入口（2026-09-08，完成）

最新 F 键交互入口见 [Black Region F 开关](SIGHTWEAVE_BLACK_REGION_F_SWITCH_ZH.md)。

后续人工发现的部分显现/封存栅栏已单独修正，最新视觉交接见 [Partial 栅栏修复](SIGHTWEAVE_PARTIAL_REVEAL_FENCE_ZH.md)。下文保留上一轮运行时稳定性验收，其截图未覆盖本次窄视角场景。

起点 `f80d9d1f7802cd5c2e9f82a21bed4f8c7b3468f7`，公司仓库 `D:\UE_projects\LastLight`，实际引擎 `D:\UE_5.8` 5.8.2。暂停 F 键施工，本片仅关闭运行时故障并替换人工入口。最终验收结果见本文件末节；证据不作跨机器毫秒 A/B。

## 根因与可复现证据

`CurrentLiveGrid` 有两种表示：dense 的 Local cells / Coverage / 两个 mask 必须同尺寸；Whole 确认后使用单个 Whole appearance，保留 Local 几何，但有意释放两个 dense mask。后者本身不是内存损坏，也不是 `PrepareCurrentRaster` 随意改变局部采样分辨率。

真正错误在 capture 事务：常规 `bTransformChanged` 的位置容差为 **0.25 cm**，而 Whole 原子封存要求 `SnapshotTransform` 与 `CurrentLive.LastLegalPose` 在 **1e-6** 内一致。小位移可能跳过 record pose 更新，`AdvanceConfirmedWhole` 却以实际 pose 写入 raster / LastLegalPose，随后 stamp 只更新 revision，没有同步 pose。另外 swept-only 分支复用 current slot 时也曾漏掉 pose 提交。

失配后的链条是：Whole compact → `WHOLE_FREEZE_REJECTED atomic=0` → 拒绝函数返回但 current slot 仍存活 → `EndSession` 结束确认 → fallback `WriteWorldSnapshot` → `Sample` 用 Local 网格计算索引，读到已释放的 bit mask。用户崩溃日志确实在断言前记录了 Whole atomic rejection。

本机保留三组证据：

- 原版完全无输入静置 150 秒未崩。不能声称重现了用户当时的确切手工操作；用户也无法确认操作序列。
- `GridCrashBefore` 在原 runtime 上确定性重现 `Sample → WriteWorldSnapshot → BitArray`，256 cells / 16×16，两个 mask 均为 0，进程 exit 3。
- `TinyPoseCounterfactual` 只撤回精确 pose 提交一行，实际驱动同一个 Lab 柜体移动 **0.1 cm**、轻微转视角、离开：record X=-300.000，grid/actual X=-299.900；原子封存拒绝，测试失败，合法 Whole 观察丢失。保留了拒绝后的清理，因此这组反证没有再崩溃；它专门证明“仅防崩”不够。恢复精确提交后同一回归通过。

## 修复与生命周期合同

`StampConfirmedWholeCapture` 统一提交生成 raster 的精确 `LastLegalPose` 与 capture revisions；sweep 分支在推进/封存前也提交已证明的当前 pose。**没有放宽 1e-6 精度，没有 clamp mask 索引，没有修改 Whole span 或 sample Clear/Block 判定。** 无效 Whole candidate 被拒绝后释放 current slot，不能以 compact 状态落入 dense fallback；已有独立历史记录不在此删除路径中。

`Advance` 仅在明确的 compact→dense 转换中重建 mask；普通 dense 状态尺寸不符立即断言，不再静默重建。`WriteWorldSnapshot` / observation mask / stationary resume 入口声明表示状态和尺寸前置条件。`ResetGeometry` 同时清除 fully-observed envelope 和 transient 标志，避免新几何继承旧的完整观察状态。world raster / Local raster 始终分离，resume 保持原尺寸，Clear/Block 不改变 Local 几何尺寸。

新增回归：compact/dense 生命周期、ResetGeometry 后 Unknown、拒绝封存释放 current、0.1 cm 精确 pose 事务。原 CPU knowledge、Whole 原子规则、SpatialPartial 样本边界、cap、37° B 过滤及 0 额外首显合同保持原路径。

## “越跑越卡”与增长

崩溃不是 bit 数组越长越大导致的：出错时是 mask **被释放为 0**。原旧 Lab 300 秒无输入运行完成 40,458 次更新，records 一直为 3、Local cells 为 17,214、geometry resets 为 9、episodes 为 3。手电耗尽后发生一次 Live→Memory：mask bits 从 22,744 增至 34,428（退出 Whole compact），fine samples 从 0 至 99,200，之后稳定。这是一次有界状态转换，不是持续增长。

不能据此把用户感觉的卡顿解释成同一个数组故障。旧 Moving Lab 包含多物体、可动历史、占用与 cap 重建；合法观察到的新 pose 会产生实际历史成本。生产 `BeginCurrentObservation` **不使用旧 API 的 64-record 上限**，不能宣称所有运动压力路线都有固定历史上限。本片没有通过裁掉合法历史来限制内存，也没有作性能专项。静置观测与实际运动/多历史压力必须分开。

开发诊断 `-DarkwellMemorySoakSeconds=N` 每 10 秒输出 identities、records、Local cells、Local mask bits、record cells/mask bits、fine samples、resets、episodes、进程内存，并在截止时正常退出。未传参时没有定时扫描/截图/退出行为。数据只支持该段有界运行，不是无限时长稳定性证明。

## 新人工入口

`/Game/Maps/L_BlackRegionLab` 使用独立 `ADarkwellCleanBlackRegionLab`，不生成 `ADarkwellMovingPropLabRoom` 或家具压力地图。仅三个静态知识源：一个 600×400 cm 地面、一个 Whole 方块、一个 37° SpatialPartial 长方块。地面也从 Unknown 样本开始，不继承旧地图 RememberedFromStart；初始 record 数为 0。玩家合法 Live（包括身体周围）仍即时显示，因此“全黑”指未观察区域，不强制压黑合法 Live。

固定 Trigger 范围 X=[-160,130]、Y=[30,140]，完整包含 Whole，穿过 Partial。默认 Inactive，直接使用同一 Activate/Deactivate，不需要 setup。无自动移动、Multi、F 开关、SaveGame、黑雾效果或新 shape。物体注册、知识、历史、材质仍复用原 Scene / Region / Trigger。

```powershell
& Scripts/LaunchBlackRegionLab.ps1
```

WASD 移动、鼠标观察。先观察两个方块，转开看灰；`~` 控制台输入 `Darkwell.BlackRegionLab activate`，观察/离开，确认 Whole 全部 Unknown、Partial 仅框内 Unknown；保持离开状态输入 `Darkwell.BlackRegionLab deactivate`，旧灰不恢复；再合法观察才恢复记忆。`status` 查看状态，重复启停幂等；`open` 重开干净世界。地面也参与相同 Clear/Block，所以区域边界在已观察地面上可见。

旧场景仍保留，定向压力复现使用原地图 URL，不删除失败环境：

```powershell
& Scripts/RunBlackRegionSoak.ps1 -RunName OldLabCheck -OldLab -Seconds 300
& Scripts/RunBlackRegionSoak.ps1 -RunName CleanLabCheck -Seconds 300
```

Map 由 `Content/Python/create_clean_black_region_lab.py` 通过 Unreal Editor API 创建并以 Git LFS 提交；脚本拒绝覆盖已有地图。退出/Destroy/World teardown 继续调用原 Trigger 清理。

## 验收记录

完整 `Scripts/BuildEditor.ps1`：DarkwellEditor Win64 Development **Succeeded**，使用实际 UE 5.8.2。存在既有编译器非 preferred 与 Engine Character.h 弃用提示；不是 Live Coding。最终 `Scripts/RunBlackRegionTriggerTests.ps1 -RunName BlackRuntimeFinal`：**19/19 clean，0 warning、0 failure、0 not-run，进程 exit 0**；RTX 4060，D3D12/SM6。

本次完整覆盖三项 CurrentGrid 生命周期、BlackRegion Whole/Partial 与 Trigger 清理、Unknown 样本区域、37° 真实帧 Clear/Block、解除首帧、重新观察、Whole 紧凑路径、Partial cap、pose bounds 换维和 topology reset。新 Clean Lab 额外用 300 个真实引擎帧逐阶段 capture，保留 temporal AA/view state，等待 shader 编译和渲染完成。正常场景保留全部地面、道具与 cap；没有靠隐藏部分组件作验收。

新 Lab 暴露并修复另一项实际视觉问题：地面灰 history 与道具灰 history 都是透明材质，默认按代理中心距离排序，巨大地面可能盖住高于它的灰道具。CPU retained samples 与几何 bounds 均正确。最终让这个固定、始终位于道具下方的地面使用 authored `TranslucencySortPriority=-1`，并把该渲染元数据随 observed primitive 捕获、传到 history proxy（默认 0 行为不变，非零进入 content revision）。未改透明度、采样精度、AA 或 cap。

已保存排序修复前正常场景与最终正常场景的 Clear / Deactivate 截图。修复后 37° 灰面连续；Whole Clear 全无，Partial 框内 Unknown、框外保持；Block 期间 Live 正常，离开仍保持切口；停用首帧与静置均不恢复旧灰，重新合法观察才重建。旧 37° 回归画面也无新竖纹或额外 seam。场景可见的采样 footprint 阶梯边缘属于原分辨率边界，本片没有降低精度或关 AA 消除它。

可移交证据：`Docs/Evidence/SIGHTWEAVE_BLACK_REGION_RUNTIME_20260908/`，包含原版崩溃摘录、0.1 cm 反证、完整测试条目、D3D12 阶段图、构建日志及最终 DLL SHA256。调查中 `CleanFloorIsolation` 曾因临时测试诊断遍历 null owner 崩溃，已删除该临时隐藏地面诊断；正式 Clear / Deactivate 失败场景原样保留并通过。这与产品 mask 崩溃不同。原始 Saved 证据仍留在本机，未覆盖或删除。

最终二进制 `OldFinal300`：300 秒，37,800 次更新，exit 0。records=3、Local cells=17,214、resets=9、episodes=3 全程不变；启动后 mask bits=22,744，手电状态变化后=26,588；fine samples 首次稳定=66,560，约 220 秒后=99,200，之后稳定。内存启动后有资源驻留增量，末段稳定约 4.99 GB，没有持续加速增长。统计原文见 `OldFinal300/storage.log`。
最终二进制 `CleanFinal300`：300 秒，43,489 次更新，exit 0。开始只有玩家周围地面的一个 current record；窗口取得输入后，既有鼠标朝向带来合法观察，10 秒时为 3 records / 40,512 Local cells / 79,488 mask bits，之后 records、cells、resets=3、episodes=3 均不再增长。约 220 秒手电耗尽后 compact Whole 转回 dense，mask bits=81,024，fine samples 从 0 一次变为 63,168；到 300 秒稳定，末段约 5.01–5.02 GB。没有循环路线、重复 pose 或持续历史生成。`CleanFinal300/GameViewport.png` 是真实游戏窗口，已人工检查标题、控制提示、合法 Live 与黑色 Unknown 背景；观察前全 Unknown 则见 `FinalD3D12/CleanBlackLab/00_unknown.png`。

**本运行时修复与干净人工 Lab 切片完成。** 完整构建、19 项定向自动回归、真实 D3D12 阶段视觉、旧/新各 300 秒长运行均通过。没有观察到新的竖纹、额外 seam、已 Clear 的旧灰复活或崩溃。此结论限定于已记录的测试，不把未能还原的用户操作序列或任意时长/运动负载声称为已验证。

下一最小切片仍是一处 F 键开关；只能在本运行时和人工视觉验收完成后继续。本片不改变全局 INITIALIZATION 仍 FAIL 的旧 checkpoint 结论。

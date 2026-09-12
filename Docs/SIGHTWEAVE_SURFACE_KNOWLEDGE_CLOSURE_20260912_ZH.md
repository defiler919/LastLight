# Surface Knowledge V1 性能与合同复核（2026-09-12）

**仍为 WIP，不把自动化执行结束当作产品合同通过。** 本轮从实际仓库恢复上下文，继续 `codex/wip-surface-knowledge-v1-20260910`；起始 HEAD 为 `8047a5d58ecd40e0b0909fe106f056c829ed30a6`。最终状态以提交本文的 commit、下列最终证据和远端分支为准。

## 恢复事实与范围

- 初始工作区 clean，fetch 后本地/远端 ahead/behind 为 0/0；Git LFS fsck 通过。没有移动 stable tag、合并原工作分支或修改 Unreal 二进制资产。
- 实际引擎 `D:\UE_5.8` 为 **5.8.2**，与 AGENTS.md 标称 5.8.1 有差别。使用 `Scripts/BuildEditor.ps1` 构建完整 `DarkwellEditor Win64 Development` 目标；非 Live Coding，非 clean rebuild。
- 本机 Ryzen 9 3900X / RTX 2070 SUPER / 32GB RAM。不同时间的历史 18.979ms 不替代本轮同机基线。
- 保持固定 Box、0.625cm 支撑网格、23 个默认 Apartment Surface domain；10,624,516 cells / 45,231,904 atlas bytes。未迁移平面地板，未增加移动 Surface epoch、复杂多边形、洞口、跨层传播或新玩法。

## 根因与实现

原路径每个细分区域重复构建世界角点、运行源/合法光检查、遍历遮挡几何并做凸 beam SAT。长墙上的二维边界会放大为数万次求证。初始分阶段测量中，dirty P95 41.661ms，观察约 40.975ms，Whole 识别约 0.407ms、像素发布约 0.207ms；根因主要在 CPU 求证，不是 GPU 权威或 atlas 容量。

1. **Runtime 准备几何证据。** 新的 `SightWeaveSurfaceProof.cpp` 从同一不可变 SurfaceScene 构造凸阴影内/外包络。正向证据使用扩大实体的外包络；负向证据使用缩小实体的内包络。它们之间保持回退带。退化/接触情况仍调用原 beam/点查询。
2. **保留原世界空间接触余量。** 仅给投影轮廓增加余量不等价于旧 SAT；真实游戏差分曾发现少量多出的 Known。现在先在世界几何上扩张 Box/有限墙包络，再投影；没有用更宽松的证明改变 Unknown 边界。
3. **Runtime 支撑列批处理。** 新的 `SightWeaveSurfaceRaster.cpp` 对严格垂直的固定面求出完整支撑单元的许可区间。Vision 与同一合法 Light 的区间相交，再合并不同合法对。一个真正遮挡点即可否定该单元的“完整支撑通过”，并不声称整格内每一点都不可见。未决带合并后调用原 region proof；非垂直面与 hard suppression 使用原回退路径。
4. **同一帧、同一 owner 的紧凑缓存。** 角点缓存只保留 source-restricted 可见性与贡献 light handles；完整点查询仍保留原归因结果。Nominal 与 Surface 点谓词由 Hard 和准备路径共用，避免两个权威实现。参考/优化模式切换会清空证据缓存，差分不会借用另一条路径的结果。
5. **有限、同步并行。** 大面的支撑列最多分四批，只读取捕获的不可变 Runtime 数据。任务内的几何、区间、法线和容器独立；join 后才执行游戏线程上的精确回退与 Knowledge 写入。没有跨帧结果、预算截断或延迟观察。像素打包也只并行读取 CPU bits，RHI 发布仍在 join 之后。
6. **位集与 dirty 更新。** 合并相邻支撑区间；连续 word pass 更新 Live / Known / Hidden，保留相邻 bits 和尾部。只上传实际发生变化的各面 dirty rectangle。缓存与批处理不改变 0.625cm 支撑网格。

并行候选曾因 `Box.Resolve` 的输出法线被多个任务共享而出现 Apartment 差分失败。最终把该临时变量放入每个任务；失败批次 `surface_closure_runtime_candidate` 保留，最终 Runtime 和真实游戏全量差分均在修正之后运行。更早 `pair_parallel` / `pair_shared` 的性能不作为最终交付数据。

`SightWeave.Surface.PreparedProofs=0` 保留参考 Observe / region / bit 写入路径，供差分和诊断。它不是关闭 Surface 功能。`SightWeave.Surface.Profile=1` 的计时包含诊断开销，不用于性能验收。

## 语义与生命周期修正

- Whole recognition 与每个 Surface Known 继续独立。Whole 的连续支撑跨度不再跨越 Unknown 缺口，也不跨行连接端点；新增真实遮挡反例。阈值 0 只在首次合法接触后识别，小对象阈值按可达到尺寸约束；Partial/Static 使用明确的非 Whole sentinel。
- Clear 清除知识并使下一次合法观察可重写；Block 保留事实、隐藏灰色并阻止新知识写入，Live 仍由 Runtime 决定。Scope 不匹配时 P4 fail closed，旧事实不迁移到新 scope。
- 已销毁组件释放 domain、Runtime receiver、atlas 和材质引用；解除注册恢复源材质。132 次同 ID 注册/销毁验证不会耗尽 128 个并发域配额。
- Observer Pose 沿宿主提供路径动态取得；新增几何/批处理没有写死眼高。GPU/P4 仅消费 CPU atlas，不计算合法观察或授予 Knowledge。
- 域 archive 仍未接入项目 SaveGame / 跨关卡 stable-scope orchestration；沿用原 V1 边界，不能宣称完整游戏存档已经实现。

## 旧回归中发现的问题

为区分新增回归与原 WIP 问题，本轮把源码恢复为未修改的 `8047a5d`、完整构建，并独立运行三项失败测试：**三项在原 HEAD 上同样失败**。之后恢复优化代码继续工作。证据 `surface_closure_8047_legacy_repro` 的 source.json 指向该 HEAD；该次 `git diff --binary` 无输出，runner 没有创建 source.patch（不是归档遗漏）。

- 原 map 夹具用 `FindWorldInPackage`，可能取到同包里已退休的 transient fixture 而不是实际加载的 map，导致后续 GC 触发 Landscape subsystem ensure。改为精确查找命名 map world，且只清理本夹具拥有、已初始化且无 engine context 的 world。
- UnknownPartial / UnknownRegion 及历史纹理读回在渲染线程初始化前捕获了空 TextureRHI。将同步放到捕获引用之前，保留并补强有效 RHI、尺寸、逐像素 CPU/GPU 一致性断言。
- Torch/Lantern 夹具把所谓远端目标放在 150cm，实际处于 900cm 灯笼范围内。现在用 1100cm 的目标验证 900/1250cm 区别；保留三种表现模式下的 HardLive、敌人和 HUD 一致性断言。
- Occupancy 原复用测试使用三 primitive 柜体，而生产缓存只支持单 primitive。保留复合体的全部逐样本/dirty/revision 断言，并增加正式注册的单 Cube 夹具验证复用；没有扩大生产缓存资格。
- **快扫合同仍阻塞。** `WholeObjectFastSlowConfirmationEquivalent` 要求从 0° 一帧转到 160°、两个端点都未见物体时产生灰色知识。当前 `DarkwellHistoricalVisibilitySweep.cpp` 明确只接受真实发布的 Runtime 快照，拒绝 adapter 插值朝向授予知识。让该旧断言通过需要明确并实现 Runtime 连续轨迹观察合同，不能恢复非权威插值或扩大 Known。本轮保留原断言与失败证据。
- 大批 legacy D3D12 同进程测试曾耗尽约 39GB commit；失败日志保留。分批/单独进程结果与中断的大批次分别记录，不能把未执行部分计为通过。

## 最终验证与性能

最终完整目标构建 `Scripts/BuildEditor.ps1`：**Succeeded，15.24s**，见 `SurfaceClosureFinalBuild.txt`。编译器仍有既有 `FImageUtils::CompressImageArray` API 弃用警告。最后一处 C++ 修改仅修正 occupancy 夹具，之后未改生产实现。

- `surface_closure_runtime_final`：D3D12/SM6 **77 项，76 clean + 1 warning，0 failed / notRun / severe**。其中一个 NullRHI 专用断言在 D3D12 下跳过，不能以此证明 NullRHI。warning 为故意重复注册夹具时的拒绝日志。
- `surface_closure_null_verified`：随后独立 NullRHI **1 clean passed**，实际执行 `NullRHIFailClosedNoAllocation`，补齐上项 RHI 分支。
- 其中 Surface Knowledge 9 项覆盖逐面事实、归档、Whole/Block/Clear、Object/Static/P4、生命周期、性能和两类差分；shadow oracle 41,525 clear / 69,850 blocked 检查，24 帧细胞差分 95,616 格；Apartment 冷启动加 20 次更新逐位比较 **223,114,836 格**。
- `surface_closure_fixture_verified`：D3D12 **2 clean passed**，包括修正后的复合/单 Cube occupancy（31 组比较）与无差分开销的 Apartment fixture。
- `surface_closure_visual_verified`：旧 BlackRegion / FogVisual / UnknownPartial / UnknownRegion / GrayPolicyLabV2 / ObjectMemory / CurrentGrid / ConfirmedWholeViewEdge，D3D12 **63 项全部通过（62 clean + 1 warning），0 failed / notRun / severe**，213.38s wall。此前空 RHI / ensure 失败没有在此最终批次重现。
- 该视觉 warning 项 `UnknownRegion.Whole` 含 HTTP 探测超时和 RHI **reserved virtual size 258GB 超过 256GB budget** 日志；它不是实际物理显存用量，也不能忽略为整批资源稳定性通过。此前更大的同进程批次确有 commit 内存耗尽，长批次资源压力仍是风险。
- `surface_closure_rules_verified`：旧 Whole/Partial/History/Spatial/Stale/Scope/Runtime/Unknown 共 **64 项，62 passed（含 2 warning），2 failed**。occupancy 夹具随后由上一批修正并通过；剩下的 `WholeObjectFastSlowConfirmationEquivalent` 保留失败，原始 HEAD 重现也是失败。没有将整个 64 项批次改记为成功。
- `surface_closure_manual_isolated`：7 项 ManualSwitch 全部通过（3 项 HTTP 探测超时 warning）；同批第 8 项是修正前 occupancy 失败。单独执行避开此前 monolithic legacy 批次内存耗尽，不能把中断后未跑的移动/长稳压测算作通过。
- `surface_closure_game_parity_final`：真实 Apartment `-game -d3d12 -sm6`，预热和路线共 **613 次更新 / 6,512,828,308 格** Known、Live、Hidden 比较，**0 mismatch**。这组含完整参考计算，计时不用于性能结论。

最终固定路线两组使用相同 DLL SHA256 `08B263404D023F11E18CAC4CAF9570EF98095ABD0998CEC2EC2997246488280D`，每组 240 帧 / 237 dirty 帧；姿态逐帧 **0 mismatch**。前台 1280×720、100% screen percentage、关闭 VSync 和帧率上限。无 Profile/Parity 开销。分位数按 nearest-rank，详见同目录 `summary.json`。

| 指标 | 参考路径 `pair_reference_final` | 优化 `pair_final` |
|---|---:|---:|
| Surface dirty P50 | 31.937ms | 8.218ms |
| Surface dirty P95 | **106.398ms** | **17.405ms** |
| Surface dirty max | 139.915ms | 20.974ms |
| Observe P95 | 104.491ms | 16.313ms |
| Publish P95 | 2.059ms | 2.065ms |
| Engine Game Thread P95 | 116.401ms | 34.707ms |
| wall frame P95 | 116.298ms | 35.917ms |
| max authority corner queries | 61,325 | 6,376 |
| max upload bytes | 11,222,016 | 11,222,016 |

Surface dirty P95 降低 **83.6%（约 6.1 倍）**；相同 23 域 / 10,624,516 格 / 45,231,904 atlas bytes，没有降低精度或删合法采样。参考开关仅切换证明/位集路径，仍使用本轮发布打包和 Whole 修正，因此比较主要隔离 CPU 求证成本。

小型 Apartment fixture 的本轮初始同机 P95 **41.661ms** → 最终 **17.789ms**，最大角点查询 13,158 → 4,287，max upload 442,836 bytes 不变。初始值启用了分阶段诊断，不能把该比值视为严格无诊断 A/B；严格性能比较以上述 PairRoute 为准。旧交接 18.979ms 属于不同运行时间/环境，保留作历史值，不冒充本轮基线。

最终 fixture cold **36.555ms**，静止 **33.498μs / 0 queries / 0 uploads**。正常输入 30 秒路线 `surface_closure_native_final` 完成 **1,379 帧 / 1,307 moving frames / 1,445.17cm / 2,024.96°**，Surface dirty P95 **13.821ms**、max **17.136ms**，Engine Game Thread P95 **31.453ms**。该路线受实际碰撞与帧率影响，与早期参考 A 的物理路径不同，不计算两者加速比。

**性能有显著改善，但整帧 60fps 预算没有通过，Surface Knowledge V1 仍不能正式验收。** 主要剩余阻塞是旧快扫与 Runtime 发布合同不一致，以及压力路线的 CPU 尾部预算。没有进行完整人工 Apartment gameplay 验收。

原始记录在 `Saved/GrayObjectPolicy`、`Saved/StaticKnowledge`；版本化选择集位于 [Evidence](Evidence/SURFACE_KNOWLEDGE_CLOSURE_20260912/)。每次运行保留 selector/command、source.patch、source.json、日志、JSON 和必要截图。参考与优化的游戏比较要求 step / x / y / yaw / source_x / source_y 逐帧相同。

复现入口：

```powershell
./Scripts/BuildEditor.ps1
./Scripts/RunGrayObjectPolicyTests.ps1 -RunName <unique> -Rendering -Tests '<selector>'
./Scripts/RunApartmentSightWeaveBenchmark.ps1 -RunName <unique> -Pair PairRoute -SurfaceReference
./Scripts/RunApartmentSightWeaveBenchmark.ps1 -RunName <unique> -Pair PairRoute
./Scripts/RunApartmentSightWeaveBenchmark.ps1 -RunName <unique> -Pair PairRoute -SurfaceParity
./Scripts/SummarizeSurfaceKnowledgeBenchmark.ps1 -InputDirectory <optimized> -ReferenceDirectory <reference>
```

带 `-SurfaceApartmentParity` 或 `-SurfaceParity` 的运行只用于一致性证明，计时含参考算法和缓存干扰。PairRoute 是固定姿态压力回放，不能冒充真实 WASD gameplay；默认基准脚本另有真实输入 30 秒路线。

## 人工 gameplay 复核准备

在 `L_SightWeaveApartmentLab` 用正常玩家输入检查：绕固定柜体逐面观察、低视点未见顶面、升高后才获得顶面知识、Whole 识别不填满背面；再验证 Clear 后转回重观、Block 内保留旧事实但不新增、Blackout 内外切换、离场和重入。保留 0.625cm 未决边缘。自动化截图和逐位比对不代替这次人工体验验收。

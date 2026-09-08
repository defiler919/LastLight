# Blackout gameplay seam / hitch 修复证据

公司机器，`D:\UE_projects\LastLight`，UE 5.8.2，Win64 Development Editor / D3D12 SM6。起点及稳定 tag 均为 `c4a6e83c44c3a61e0c8e7413ef378cee2258bf94`；稳定 tag 不移动。原始日志的 UTC 时间与本地 Asia/Shanghai 相差 8 小时。

## 结论及证据范围

人工报告的“进入或退出各留下当时视野边缘的一条黑线”已在干净 Black Region Lab 自动路线复现，最终相同路线与 CPU→提交纹理探针不再出现该内部黑缝。20 次真实主胶囊进出没有重复 Begin/End，也没有原先秒级 hitch。所有证据为自动运行或代理检查真实 D3D12 图像，**不能替代用户实际关卡人工 gameplay 复验**。

默认项目全 SightWeave 仍有两个仓库已有的 M4P1 夹具配置失败，详见测试表；不声称默认配置全部通过。它们在原非抖动 CustomDepth 夹具下 2/2 通过，TAA/TSR 始终开启。没有为了黑区修复改变 AA、reveal bounds、region bounds 或采样精度。

## 黑缝的可复现因果链

1. 基线 `BlackoutHitchBefore` 在已探索后真实胶囊进入固定事件框，再转向、退出、转向、重观察。`Before/04_blocked_live.png` 的地面内部能看到原视野轮廓黑线，线并不沿 Target 的矩形边界。
2. 原路径在区域切换上强制封存仍然 Current 的同姿态 Partial，随后全 Local `ForgetKnowledgePreservingLive()`，丢失目标外已经证明的 D/capture mask。新旧 epoch 的 ownership 交接和各自 AA 包络不再覆盖同一内部滤波支持。
3. 诊断中间版本 `BlackoutDirectTrial` 的 frame_151 / 181 各有 984 个连续已知位置缺失，frame_211 / 221 各 602。示例 world `(277.125,-172.875)`：Local D=0、capture mask=0、appearance=1；旧 fine `InitialRemembered=1`，但已被标为 Superseded、gate=0。该点在 Target 外，足以排除“矩形边界要加 epsilon”的解释。CSV 连同当时 source.patch 保留在 zip；这是诊断中间版本，不能冒称纯 c4 基线。
4. 同 pose/content/geometry 且仍合法 Current 的静态 Partial 现在转移原 record/资源到新 epoch，不虚构一次历史捕获；区域内仍由原 Clear 中心谓词清理，区域外 Local 事实保留。若不满足条件则正常封存。独立 Block 不能使用 Clear+Block 的 capture 省略。
5. 对已有历史的交接，用完整已知滤波支持证明 Current 替代；已知内部的 AA 使用原 appearance 或记忆质量，未知边界、A gate、Current G 不扩张。Clear/Block 改变和最终 texture/cap 同调用发布，不以延迟帧换正确性。

最终 `BlackoutFinalContracts_Volume` 探针在 1 cm 间隔、偏移 .125 cm 的地面采样，用周围 3×3、2.5 cm 间隔的合法知识证明连续内部，然后按材质的 bilinear RGB / nearest A 检查提交纹理：

| 阶段 | 连续已知位置数 | alpha < .99 缺失 |
|---|---:|---:|
| frame_151（进入后转向） | 192188 | 0 |
| frame_181 | 165308 | 0 |
| frame_211（退出） | 165308 | 0 |
| frame_221（退出后转向） | 165308 | 0 |
| final（合法重观察） | 199233 | 0 |

`.125` 和 `.99` 只用于诊断采样/断言，不进入 authority 或材质。`region_edges_history_only.csv` 只看历史，Current 接管时可能无有效点，**明确不是 PASS gate**。Final Ground 转移后无多 epoch，旧 batch-oracle 的 compared=0 也不算非空性能/正确性证明；真正的 seam gate 上表始终非空。此前带历史的 scalar/batch oracle 中间运行保留原报告。

## Hitch 测量与修改

`BLACKOUT_TIMING` 使用同调用嵌套计时，root 为真实 overlap Enter / Leave 或 F Interact；inclusive 与 exclusive 分开，不能把嵌套时间相加。原始数据在 `callback_profiles.json` 和 zip。

| 同一 VolumeDemo 路线首个事件 | 基线 ms | 修复 ms |
|---|---:|---:|
| BeginEvent | 2522.992201 | 25.097899 |
| EndEvent | 998.501301 | 9.340100 |
| Enter callback | 2523.038402 | 见 callback_profiles.json |
| Leave callback | 998.543002 | 9.388600 |

基线首个进/出分别一个 callback、一个 Begin/End。生命周期测试后段主动测试 duplicate overlap、死亡、销毁和 F，因此不能把该段多个操作同一 engine frame 的调用累加误判为一次真实移动重复回调。

第二次基线细分 `BlackoutDetailBefore`：

| 热点（inclusive ms） | Enter | Leave |
|---|---:|---:|
| transient contribution exclusion | 847.489 | — |
| historical ownership exclusion | 782.871 | 532.675 |
| occupied sample 查询 | 206.928 | 120.771 |
| cap CPU build | 118.755 | 86.207 |
| texture submission（含准备） | 80.681 | 65.102 |

首个基线进入 history texture create 总计 5.149 ms，其中 UpdateResource 2.517 ms；不是秒级热点。该 core 没有 FlushRenderingCommands、GPU fence wait、readback、forced GC；测试本身的 Render/Flush 明确在性能路线之外。同步资源创建存在但占比小，资源复用后普通同姿态 region 转移不再重建历史资源。最终独立 `CurrentTextureEnqueue` / `HistoryTextureEnqueue` / `RegionTextureEnqueue` scope 只计入队调用，不冒充 GPU 执行耗时。

F 对照走同一 Trigger core：基线 Interact 停用 1084.971 ms / 激活 4524.169 ms；修复 7.22 / 15.25 ms。F 前后场景状态与 overlap 首次状态不同，不是严格逐帧 A/B，也不将其当纯 frame time；足以证明慢点在共同 core，而非 overlap 通知层。

修改包括：

- 同姿态 Current 数据/资源转移到新 write epoch，保留实际封存条件，避免同一次切换创建随后立即被新 Current 覆盖的历史。
- 复合 Clear+Block 的 authority 不变，只合并中间显示发布，最终同调用一次刷新；不排队到未来帧。
- 区域清理与 dirty 仅访问保守 ROI，再由原 half-open center 谓词决定成员；axis cache 与逐样本 scalar oracle 完全一致。
- 复用精确 physical snapshot；修正 Partial transient 错误失效；空历史需 CPU 证明无 residual 后才退休，保留有贡献的 cap。
- 大块纯 CPU raster/AA/pixel 工作并行并在调用内 join；packed bit 写块按 word 对齐，point-query UObject fallback 仍串行。没有降低样本密度或跨帧资源生存风险。

## 原生窗口 20 次性能验收

`BlackoutGameTransferTwenty`：1280×720 D3D12/SM6 native game，60 fps 上限；300 warmup 帧后，20 次完整进出、3000 帧。每次进入/离开调用真实 Player.SetActorLocation，胶囊碰撞保持开启，未直接调用 EventAdapter；没有 SceneCapture、readback 或 Flush。每次边缘取包含该帧的 5 帧窗口，全程必须前台。

| 项目 | 进入 | 离开 |
|---|---:|---:|
| transitions / callbacks / Begin或End | 20 / 20 / 20 | 20 / 20 / 20 |
| Event p50 ms | 20.645201 | 11.058703 |
| Event p95 ms | 25.703602 | 13.080701 |
| Event max ms | 27.546700 | 13.703298 |
| 边缘窗口最大完整帧 ms | 48.400499 | 26.627898 |

3000 帧 p50=16.843602、p95=25.943398、p99=35.928600、max=51.278800 ms；非切换窗口 max 同样 51.278800 ms。没有进入/退出独有的异常秒级长帧。复用现有 SightWeave complete-step 100 ms 硬上限，**不是承诺整游戏满足 16.6 ms 帧预算**。全帧 Active/Started 状态错误=0，边缘错误=0。结束 identities=4、records=4、local_cells=40736、local_mask_bits=79488、fine_samples=66752、record_cells=42572、record_mask_bits=681152；单调 epoch/episode 是序号，不是驻留历史增长。

追加独立 enqueue scopes 后的最终重跑 `BlackoutGameFinalTwenty` 也通过，见 `native_final_20_transitions.json` / `native_final_3000_frames.csv`，核心算法未变：Begin p50/p95/max 为 20.178098 / 23.463000 / 24.379101 ms；End 为 10.926001 / 11.580102 / 11.771098 ms。进/出边缘窗口最大完整帧 45.398097 / 25.528301 ms；全部3000帧 max=45.398097 ms、p99=35.137001 ms。依然各20 callback/20事件、0状态错误，结束驻留 cells/masks/records 与上一轮完全相同。Current/History/Region 纹理 enqueue 每次事件合计最大分别为 .062898 / .032302 / .020899 ms；未见同步等待热点。72.57s 为完整启动/运行/正常退出 wall time。

失败中间版本未删除：GameOutsideTwenty / GameRowsTwenty / GameBoundedTwenty 虽完成20次但超过100ms，均不计 PASS；更早 retention/fine-owner 方案仍有秒级增长，已放弃。失去前台的早期运行作废。不能把这些中间版本当纯 c4 的20次基线；纯 c4 测量为上面的同场景 callback，未取得可用的原生20次完整帧基线，因此不虚构 before 最大完整帧。

## 自动回归和构建

| 运行 | 配置 | 结果 |
|---|---|---|
| BlackoutFinalEditorBuild | 完整 Editor Development 目标 | 成功 7.98s |
| BlackoutFinalTimingBuild | 最终 enqueue 计时 scope，完整 Editor Development 目标 | 成功 15.54s |
| BlackoutFinalFunctional | NullRHI，冻结功能集 | 156/156，144 clean、12 warnings，无失败；142基线全保留 |
| BlackoutFinalContracts | 当前项目 D3D12/SM6 | 23/23，0 warning |
| BlackoutFinalContracts_Volume | 当前项目 D3D12/SM6 | 1/1，0 warning |
| BlackoutFinalRepeated | 当前项目 D3D12/SM6，20次真实胶囊切换 | 1/1，0 warning，106.77s；不是性能测试 |
| BlackoutFinalSightWeave | 当前项目全 SightWeave D3D12/SM6 | 312/314（309 clean、3 warnings、2既有夹具失败） |
| BlackoutVisualIsolation | 默认项目独立重跑上述2项 | 0/2，保留失败 |
| BlackoutLegacyFrozenFixture | 原 M4P1 CustomDepth 夹具，TAA/TSR开启 | 2/2，0 warning |

旧 9 项逐名对应 `baseline_9_coverage.json`，全通过。新增 AxisMembership 对四边/角点、精确中心边界、fractional bounds、重建尺寸比较 scalar oracle；SampleCut 保留原 A/B/C，并增加内部矩形的 A/B/C 与37°/reobserve；CurrentRegionTransfer 40次转移精确比较知识和历史、保持 record 数和 preparation lifetime，真实观察退出仍封存。

两项默认项目失败是 `SightWeave.M4P1.Visual.Camera34Observability` 与 `ContinuousTransition`，与历史交接 `Docs/SIGHTWEAVE_DARKWELL_VISUAL_RESCUE_REPORT.md` §11.5–11.6 的 jittered CustomDepth 兼容性记录相符，不运行 DARKWELL 黑区路径。原始夹具通过启动参数 `r.CustomDepthTemporalAAJitter 0` 恢复，**不是关闭 TAA/TSR**；脚本只允许对这两项指定该开关，不能用于黑区测试。没有放宽像素断言、修改 plugin、删除失败或改 DefaultEngine.ini。需读取表中的配置区别，不能宣传为默认项目314/314。

完整套件中的显式 capture/soak/ETW 专用用例在没有其专用参数时按既有方式报告说明，本次不宣称进行了新的长时 ETW/soak。功能 warning 包括 duplicate-ID/capacity 阴性测试、HTTP 超时，以及未修改的 `L_SightWeaveGrayPolicyLab` 夹具的 CleanupWorld 重复清理提示；不声称全项目零 warning。黑区最终合同、Volume、Repeated 和 native 无新 SpawnActor/lifecycle warning。Build 只有既有 UE 废弃 API/非首选 toolchain 提示。

通用两阶段 runner 的首次第二段被尚未退出的 Unreal 子工作进程冲突保护拒绝；等待退出后独立运行 Volume 成功，未禁用保护或并行运行 Unreal。

## 重现命令与文件

```powershell
Scripts/BuildEditor.ps1
Scripts/RunGrayFunctionalRegression.ps1 -RunName <unique>
Scripts/RunBlackRegionTriggerTests.ps1 -RunName <unique>
Scripts/RunUnknownPartialCutTests.ps1 -RunName <unique> -Tests 'Darkwell.BlackRegion.TransitionProbe' -WorkloadTimeoutSeconds 900
Scripts/RunBlackoutGameplayTransitions.ps1 -RunName <unique>
Scripts/RunUnknownPartialCutTests.ps1 -RunName <unique> -Tests 'SightWeave+Darkwell.SightWeave' -WorkloadTimeoutSeconds 900
Scripts/RunUnknownPartialCutTests.ps1 -RunName <unique> -Tests 'SightWeave.M4P1.Visual.Camera34Observability+SightWeave.M4P1.Visual.ContinuousTransition' -LegacyM4P1Fixture
```

人工仍用 `Scripts/LaunchBlackRegionLab.ps1`：框外探索成灰 → 主胶囊进入青框 → 灰清为黑 → 框内合法 Live/转开保持黑 → 离开框无旧灰恢复 → 转动/合法重观察才建立新灰，观察原视野边缘、四边/角点与37°物体；再测 F 覆盖及正常退出。未自动替用户做此轮人工关卡验收。

`Before/` 与 `After/` 是未经编辑的原 D3D12 PNG；`callback_profiles.json` 是原始嵌套 scope 数据；native CSV 是完整帧；`logs_reports_and_source.zip` 保存各运行报告、日志、诊断 CSV、source.patch 与 Build。source.json 的 HEAD 是基线，未提交工作通过 patch 标识，Timing.h 单独留档。PNG 走 Git LFS；Saved、二进制构建产物和资产不提交。

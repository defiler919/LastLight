# Static Environment Knowledge 验证结果

公司 i5-13500 / RTX 4060 / Windows 11 / UE 5.8.1。源码起点 `b1fb4ee6ac644fe4dd28779b07e0a1635c148bd1`；最终源码为包含本报告的提交。稳定 tag 未变。架构与限制见 `../../SIGHTWEAVE_STATIC_ENVIRONMENT_KNOWLEDGE_ZH.md`。

## 最终 A/B

同一最终功能构建的 Editor Development 二进制；D3D12 SM6、1280×720、100% SP、原 TSR（AA=4）、无 dynamic resolution、VSync=0、MaxFPS=0；原 488×408 P4、16 occluder segments。30 秒预热后测量 30 秒原生 WASD＋鼠标转向。没有降低速度、碰撞、2.5 cm cell 或 4×4 fine support，没有扩展视野/光照。

| Run | GT mean / P95 / max ms | RT mean ms | RHI mean ms | GPU mean ms | OM mean ms | Static mean ms |
|---|---:|---:|---:|---:|---:|---:|
| ReferenceVerified：43 OM | 241.95 / 583.67 / 1360.34 | 9.02 | 4.08 | 28.40 | 235.31 | 0 |
| StaticVerified：29 Static＋14 OM | 22.56 / 56.82 / 136.50 | 6.25 | 3.36 | 5.61 | 14.88 | 3.82 |

GT 均值下降约 90.7%。125 帧 / 124 moving frames / 6481.59 cm / 1965.82°，对照 1329 帧 / 1178 moving frames / 3902.50 cm / 2109.02°。所有测量帧均前台、1280×720。该导航会保留真实角色碰撞并绕开阻挡；帧率会影响最终路径，**不是逐帧相同的确定性空间回放**，不把百分比当作无误差的通用产品保证。GT/RT/RHI/GPU 是流水线指标，不能相加成 frame time。

前一构建（仅尚未加入 P4 inactive 材质关闭保护，正常 Active 算法相同）的独立结果全部保留：ReferenceFinal GT 264.01 ms；StaticFinal 31.50 ms，P95 84.75 ms；StaticRepeat 26.69 ms，P95 121.12 ms。新路径三次均值 22.6～31.5 ms，约 32～44 fps，**尚不是稳定 60 fps**。不隐去长帧或挑最快一组宣告性能完成。

## 静态工作量和内存

StaticVerified 每帧：

- candidate tiles 224：小公寓的整个已声明范围在当前 view draw AABB 内；只做 tile 哈希/证明，未遍历全部建筑 fine samples。
- touched tiles mean 10.21 / P95 16 / max 25。
- tested fine samples mean 4293 / P95 6548 / max 11617；原 Coverage oracle queries mean 4886 / P95 7886 / max 15211。
- written fine samples mean 73.39；tile uploads mean 0.215 / P95 2 / max 8。
- Static update mean 3.82 / P95 6.39 / max 10.72 ms。所有合法新 bit 当帧写入，Clear 当次事务清除，未增加状态延迟。
- 224 declared tiles；最终 199 resident tiles，bit payload 407552 bytes（398 KiB），不含 TMap/TSet/MID 等元数据。每 resident tile 2048 bytes。
- 共享 atlas 16 MiB＋page table 64 KiB；29 个 MID，无建筑 per-object current texture、历史 proxy 或 cap。其它两次 resident tiles 为 146/215，随实际探索区域不同。
- 剩余 OM：14 identities / 14 records、17735 local cells、292992 fine samples。整个 Unreal 进程 private bytes 约 5.80 GB，不能当成静态 subsystem 内存。

## 剩余瓶颈和下一步

StaticVerified 剩余 OM 14.88 ms，UpdateTracked 14.62 ms；Coverage 10.60 ms、CurrentReveal 4.94 ms、historical 9.63 ms。这些是**嵌套计时，不可相加**。OM 单帧最高 125.14 ms，Coverage 最高 119.06 ms，仍解释主要长帧；其它新路径跑次 OM 平均 16.7～23.3 ms。

下一性能切片应单独定位这些动态对象的 Coverage/current/history dirty 依赖与重复扫描，建立不改变 authority 的增量更新证据，再考虑 60 fps gate。静态本身约 4 ms 仍可进一步缓存已证明子区域，但本轮没有扩展到剩余 OM history index/current texture/cap 重构。GPU 当前约 5.6～6.9 ms，60 fps 路线可信但**未证明达标**。大地图还需要显式 GPU residency/多 scope streaming，不能把本片的 bounded atlas 当作已完成全地图流式系统。

## 功能验证

- `Scripts/BuildEditor.ps1`：最终 DarkwellEditor Win64 Development **Succeeded**（EditorBuild.log）。非 Live Coding。
- `Scripts/RunUnknownPartialCutTests.ps1 -RunName StaticKnowledgeVerified -WorkloadTimeoutSeconds 900 -Tests 'Darkwell.SightWeave.StaticKnowledge+Darkwell.BlackRegion.VolumeDemo+Darkwell.BlackRegion.CleanLab+Darkwell.BlackRegion.Contract+Darkwell.BlackRegion.CurrentPartialProbe+Darkwell.UnknownRegion+Darkwell.UnknownPartial+Darkwell.SightWeave.Closure.VisionIlluminationBoundary'`：真实 D3D12 **14/14**，13 clean＋1 warning，0 failed/not-run/severe。覆盖原 Blackout Volume 9 项选择器、新 3 项静态 CPU oracle 以及 Vision/Illumination 闭合测试。
- 唯一 test warning：UnknownRegion.Whole 期间外部 `google.com/generate_204` HTTP 3 秒超时。未出现新 SpawnActor/lifecycle warning、assert、GPU crash；不为清零外部 warning 改系统。
- `verify_static_knowledge_apartment.py` / `PIEFinal`：真实 D3D12 PIE，关闭门 probe Unknown，打开后 doorway Known，solid-wall probe Unknown；静态卧室 Unknown → observe → Remembered → Clear＋Block → Blocked Live 不写 → 离开黑 → release 不恢复 → reobserve；20 次重复 Trigger Activate/Deactivate 和 Active teardown 均完成。
- `PIE/results.json` 给出与截图同阶段的 CPU 查询和状态；图像确认灰色清除、Blocked Live 保留、解除不恢复。首次图名 `initial_unknown` 指未探索卧室仍 Unknown，出生区已经过正常合法首帧观察，不声称整张截图应全黑。
- 截图核对仅为自动 PIE 证据审查，**不是用户人工 gameplay PASS**。原 ObjectMemory/现有 AA 的细边、颗粒和动态物体表现未在本轮重构；仍需要用户在公寓中手动 WASD/F 复验。

## 证据及复跑

`benchmark-summary.json` 保留五次正式运行的全部分位数、storage、渲染配置；`runtime-evidence.zip` 包含 frames.jsonl、日志、命令、completion、storage 及最终 PIE 日志。`binary-material-sha256.json` 标识测量/测试模块和材质。ReferenceVerified 与 StaticVerified 之间没有 C++/材质或配置修改。提交自查后仅删除 benchmark 一个空行的尾随空格并再次完整 Build（Succeeded），没有任何逻辑变化；该重链接 hash 和日志单列于 `post-format-build.json` / `EditorBuildWhitespace.log`，不冒充与测量模块字节相同。版本化 build 日志去除行尾空格，Saved 原日志保留。编译出现 Engine Character.h 自身的 GetMovementBase 弃用 warning，未修改引擎。

开发过程中的 PIE1（Python subsystem 获取接口）、PIE2（角色朝向被原 sticky aim 覆盖）未通过；已使用 Lab 原生 getter/原 AimAtWorldPoint 修正测试驱动。PIE3 截图与下一阶段异步串帧，未作为最终截图证据；PIE4 和最终 PIE 均等待截图完成才改变 pose/事件。原始尝试留在公司 Saved，未删除失败场景，也未为通过测试改变合法观察规则。

人工入口保持 `Scripts/LaunchApartmentSightWeaveLab.ps1`，没有默认自动路线。自动 benchmark 用 `Scripts/RunApartmentSightWeaveBenchmark.ps1 -RunName <新名称> [-Reference]`；离线归纳用 `Content/Python/summarize_static_knowledge_benchmark.py <run目录> ...`。

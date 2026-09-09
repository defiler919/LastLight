# Runtime Hard EffectiveLive 下游收口（2026-09-09）

基线 `190e67985bdc038a1256228a0baa3829dacb1a84`，公司机器，分支
`codex/darkwell-prop-memory-gameplay-lab`。稳定 tag
`stable/sightweave-pre-formal-vision-illumination-20260909` 保持指向
`04b01b02cf180ae340795fa369b9c1a367c8ce12`，没有移动。

本切片统一**现有 Runtime 语义**，不宣称最终高柜顶面、楼板/洞口传播等产品能力已经完成。
高度带分层经过用户明确授权。没有修改地图，没有降低 2.5cm authority 或原有 fine support 密度。

## 权威与消费

- Runtime 新增 `FSightWeaveHardCoverageSet`：持有不可变发布帧，以 owner/floor/revision 和 Runtime
  已有高度谓词建立分区。闭区间端点独立处理，谓词完全相同的高度区间共享 prepared plane。
- 点查询最终调用原来的 `QueryEffectiveLiveValidated`。区域证明只在结果不会变化时跳过点查询；
  几何谓词仍调用 Runtime 原来的 nominal/polygon evaluator。Source 兼容光源关系、Bypass、遮挡、
  Floor/Height、Suppression 的结果没有另写成 DARKWELL 的规则。
- Object Current/Whole legal contact 使用 primitive 原有局部采样平面的世界 Z；历史 coverage
  使用捕获 primitive 的同一平面，避免 Actor pivot Z 与 primitive Z 混用。跨多个平面的投影历史，
  空证据必须同时证明全部贡献平面，不能用亮的低层清除未观察的高层。
- Object admission 的 Actor-pivot 粗覆盖不能否决不同高度平面上已经合法的 primitive。
  同高度谓词的普通 Apartment 对象继续走原来的快速 admission；只有谓词不同才查询局部支撑。
- Static 的 sparse XY store 按高度分层。已有知识相同且合法谓词相同的层共享存储/页面；
  发生分歧时先 copy-on-write，再观察。Clear 和 Block 随存储保留；拆层、解除 Block 均不恢复旧灰。
- CPU Coverage 生产入口现在返回 hard 0/1；原 `.99` 不再判定软 AA 值是否可以写 Memory。
  原五点支撑、Whole 确认、Partial capture、Clear/Block 仍保留。
- P4 消费同一 prepared authority 的高度 atlas，按世界 Z 选择层。4×4 面积采样仅生成表现值；
  不参与 Knowledge 写入。扫描线只跳过已证明恒定区间，保留全部原定面积采样位置。
  兼容 XY RenderTarget 逐 texel 拷贝，表面继续双线性采样；TAA/TSR、Mip 和材质 Whole 分支保留。
- 旧解析 Coverage 保留给明确的纯数学/冻结兼容 fixture；当前 Adapter 的发布快照总是携带 Hard authority。
  插值 Adapter 视锥不再自行创造 Knowledge：正式路径只接受真实 Runtime 发布帧的同步支撑。
  连续时间轨迹证明应由 Runtime 将来发布，不能由下游猜测。

## 差分与回归证据

证据目录：`Docs/Evidence/SIGHTWEAVE_HARD_AUTHORITY_20260909`。

- 原 26 探针保留，并从 characterization 改为现有语义的 acceptance。
  Hard、Static fine support、Object local support 和 GPU texel 是不同采样定义，报告不混为一个点。
- `light_height_mismatch`：Hard 拒绝的低层不再被 CPU/Object/Static/P4 接受；高层仍可合法观察。
- `light_edge` 450.25/451：Hard 支撑已经合法时，两类 store 不再因软 Coverage `<.99` 漏写。
- 新增持续存储跨高度拆层、Clear、Block、解除、重观察；新增 Actor Z=92 / primitive Z=240
  的 Current 与历史高度偏移回归；新增多平面投影历史的空证据保护。
- Runtime 独立测试覆盖兼容/不兼容光、Bypass、墙、不同高度、全部高度端点、启停/Revision、
  Suppression、区域证明及 World teardown 后首次查询另一高度。
- GPU oracle 不再以旧解析 shader 为权威。保留旧图最大差分记录；对变化像素及未变化抽样像素，
  独立执行 Runtime 4×4 点查询。最后一次对照 10,182 像素，最大误差 0。
- D3D12 自动验收批次：Authority 24/24；Blackout/Unknown 10/10；Presentation 6/6；
  高度偏移补充 3/3。原 Volume 主胶囊 overlap、F 共存、Whole、37° Partial、cap、Clear/Block、
  退出不恢复和重新观察均包含在相关批次内。单个 HTTP 连通性检查超时不属于 gameplay warning；
  没有新 SpawnActor/lifecycle 错误。
- 高度偏移修正后，Blackout/Unknown 与 Presentation 的组合批次再次验证，
  `HardUnifiedReleaseContracts` **16/16、0 warning / failed / not-run / severe**。
- 材质只通过 Unreal Editor Python 重新生成四个 `.uasset`，没有普通文件系统写资产或修改 `.umap`。
- Build：`Scripts/BuildEditor.ps1`，完整 `DarkwellEditor Win64 Development`，非 Live Coding。

这些是自动测试、真实 D3D12 readback/SceneCapture 和自动输入 benchmark，**不是用户人工 gameplay 验收**。

## 性能与失败记录

协议：现有 Apartment native benchmark，D3D12/SM6，1280×720，100% screen percentage，
AA=4，动态分辨率关闭，正常 Character WASD 与鼠标转向；30 秒预热、30 秒测量。
`performance.json` 提供 average/P95/P99/max、GT/RT/RHI/GPU 和 subsystem 数据。

同机既有优化后 `HistNative2` GT 平均 16.70ms。统一后的 `HardUnifiedNativeFinal` 为 16.78ms；
最终高度偏移修正后的 `HardUnifiedReleaseNative` 共 1806 帧，GT **16.61 / 22.72 / 29.84 / 33.33ms**
（平均/P95/P99/max），GPU 平均 **5.17ms**。Static 平均 3.07ms，ObjectMemory 4.25ms，
Hard 表现发布 4.72ms。旧基线 GT 为 16.70 / 21.94 / 25.62 / 35.04ms：平均和最大值没有退化，
本次 P99 高约 4.22ms，不能用平均数隐藏这一点。
这是同机同协议对照，实际角色轨迹随碰撞/帧率而变，
不是逐姿态严格 A/B，更不是跨机器毫秒比较。没有声称稳定锁定 60fps。

保留未通过的迭代，不以删场景或放宽阈值取得 PASS：

1. r1 发布平均约 49.44ms、r2 约 13.46ms、r3 约 7.86ms，均没有作为可交付结果。
2. 首次广泛功能运行 r2 出现约 10.85GB texture residency、异常长帧和无效前台证据。
   新增纹理在替换/teardown 时明确释放；最终有界批次不再出现该异常。早期结果不计作通过。
3. r3 独立 GPU oracle 最大误差 0.003906：已做面积 AA 的 atlas 再次双线性拷贝导致误差。
   改为等分辨率 texel 拷贝，没有放宽 `.002` 验收阈值或关闭表面 AA。
4. 性能根因是边界区过多重复硬查询、无收益的唯一点缓存写入、无关光源边界使盲区证明失败，
   以及等价高度层重复观察。扫描线、逐 Source 三值区域证明、候选支撑包围盒、共享层消除了这些重复工作。

## 剩余边界

- Runtime 仍使用已有 FloorId 和高度带合同；跨层楼板/洞口传播、高柜顶面/法线规则未新增。
- ObjectMemory 保留现有 upright primitive 的二维 footprint 和投影历史模型，未变成任意三维表面/体素知识系统。
- P4 当前高度 atlas 和 Static GPU residency 有明确容量上限，失败时显式关闭表现并保留 CPU 知识；
  大规模高度区间压缩、页面流送/淘汰和连续高度移动的分区整理尚不是本切片。
- 下游仍有纯表现纹理生成和投影历史处理成本；GT 平均接近原基线，不等于正式游戏全场景 60fps 保证。
- 需要用户继续人工试玩；本轮没有修门、扩展光照传播或开展新的玩法切片。

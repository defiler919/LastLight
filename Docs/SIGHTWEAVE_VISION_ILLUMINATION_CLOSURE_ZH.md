# Vision / Legal Illumination 职责收尾（2026-09-07，家里）

本轮完成职责边界修正及定向验证，可创建 `checkpoint/sightweave-gray-performance-closure-20260907`，指向包含本报告的最终提交。性能专项维持结束，不开始黑色层；A1/P1/B0默认仍0，INITIALIZATION仍FAIL，未移动既有stable/tag。

## 起点与原因

家里 `D:\UE_pro\Darkwell`，分支 `codex/darkwell-prop-memory-gameplay-lab`。起始本地/远端HEAD均为 `ca0e12f812af4d38249f7827d6996f9a723580b9`，working tree clean。读取AGENTS、最新B0交接及Adapter/Loadout/FogVisual与所需测试；实际引擎目录 `D:\UE_5.8` 的Build.version为5.8.2。

火把耐久耗尽只改变Loadout照明状态，**没有停用底层Cone Vision Source，也没有把它的2200cm range缩短**。因此“无任何合法照明时EffectiveLive只剩近身圆”本来是正确表现。

但存在三项真实职责混用：

1. 表现快照把 `ConeRangeCentimeters` 设为 `min(Vision.Range, Torch.Range)`，并把 `bConeLegallyLive` 直接设为 `Cone.active && Torch.active`，表现路径没有独立光照空间门控。
2. 玩家Cone仅接受`Darkwell.Visible.Torch`，灯笼原来只是非权威渲染光，环境合法光无法以自己的能力标识接替。
3. Cone半角会随瞄准进度52→35度变化，不符合本轮固定角度产品合同。

## 修正

- SightWeave玩家几何固定 **最大范围2200cm、半角52°（全角104°）**；删除瞄准进度对角度的修改。火把、电量、灯笼、环境亮度均不修改/禁用Cone。
- 永久Body Vision仍为半径120cm的radial source，维持`BypassLegalIllumination`，与手持光源状态无关。
- 现有手持Illumination handle按真实Loadout状态更新自身激活、能力及照明半径：火把1250cm；灯笼基础合法照明900cm，有燃料时可用，耗尽时无光。没有重做聚光/闪光玩法或耐久算法。
- Cone兼容Torch、Lantern、Environment三种明确合法能力。环境光必须在SightWeave注册为有效、同scope且兼容的Illumination Source（环境能力名`Darkwell.Visible.Environment`）；普通PointLight/SpotLight的画面亮度不自动授予知识。
- 表现快照从runtime已发布的Cone兼容光源索引取得活跃合法光源。几何range/angle独立保留，通过空间覆盖求交：`Live = max(BodyBypass, min(OccludedCone, Union(LegalLights)))`。每个光源都按自身位置、范围、方向及现有墙段遮挡计算，不能因为远处有一盏灯而点亮整个Cone。
- CPU覆盖、uniform区域证明、历史旋转扫掠与GPU LiveCoverage同步接入。灯光集合改变会拒绝无法证明的旧旋转连续性；不改变灰色知识、身份、Whole/Partial和cap规则。
- GPU使用很小的float描述纹理承载合法光源列表，复用现有连续覆盖材质；材质通过Unreal Editor Python定向重建，仅修改 `M_DarkwellFogCoverage.uasset`，经Git LFS存储。没有用文件系统工具重写Unreal资产。
- CPU-only数学fixture保留原有无显式光源输入模式；真实Adapter始终启用独立合法光门控。现有字段`bConeLegallyLive`为兼容名称，Adapter现在仅用它传递Cone几何激活，照明由独立列表决定。

## 四种场景及D3D12结果

测试位置：玩家(-650,0)，Body探针(-590,0)，近目标(550,0)、远目标(950,0)；两目标均在固定Cone几何内。环境合法光中心(750,0)、半径300cm；另查(350,0)不被此光照亮。火把从0.001真实耐久经`TickComponent(1s)`耗尽，不只修改表现开关。

| 场景 | Body bypass | 近目标EffectiveLive | 远目标EffectiveLive | Cone几何 |
|---|---|---|---|---|
| 火把亮＋黑暗环境 | 有效 | 有效 | 无效，超出火把照明 | active / 2200cm / 半角52° |
| 火把耗尽＋环境合法光 | 有效 | 有效 | 有效，由环境光提供 | 完全不变 |
| 火把耗尽＋完全无合法光 | 有效 | 无效 | 无效 | 完全不变，PureVision仍覆盖远点 |
| 火把重新点亮 | 有效 | 恢复 | 无效，仍超出火把照明 | 完全不变 |

四场景的runtime EffectiveLive、CPU analytic coverage、真实D3D12 R16F纹理读回均满足表格。额外验证了灯笼基础光可满足500cm处的Cone、燃料耗尽仅停止照明，几何保持不变。

D3D12视觉证据为**实际LiveCoverage纹理整图读回**，不是NullRHI数值冒充截图；原始PNG在 `Saved/IlluminationClosure/01_TorchOn_Dark.png` 至 `04_TorchRestored.png`。环境光图显示与近身圆分离的合法照亮区域，无光图只剩近身圆。火把恢复前后PNG一致。没有宣称进行了完整PIE游玩视觉全集。

最终增加旧火把全纹理oracle：将同位置radial火把的旧1250cm解析覆盖与新独立光门控逐像素比较，**最大R16F误差0.000488**（门槛0.002）。交集在各自遮挡之后取min，避免同位置Cone和灯光把遮挡边缘透明度乘两遍。

## 验证与限制

- 完整Editor构建：`Scripts/BuildEditor.ps1 -EngineRoot D:\UE_5.8`，最终 `Saved/IlluminationClosure/Build04.log`：**Succeeded**。
- 官方Editor Python材质更新：`Content/Python/update_legal_illumination_coverage.py`，`Saved/IlluminationClosure/MaterialFinal.log`成功。
- 最终D3D12/SM6 `VisionLightClosureFinal`：**9/9 clean PASS，warning0、severe0、exit0**。包含新边界测试、原VerticalSliceAuthority、A0 PresentationResidency、CanonicalCoverage三项、旋转连续性保护、快慢扫掠等价、RememberedProp.RuntimeAtoB。
- 首轮新增测试夹具缺少要求的敌人persistent ID，导致请求未实际激活后空纹理读回崩溃；已修正夹具并添加激活/纹理检查。原 `VisionLightClosure01` 失败日志保留，不计为通过。中间通过结果之后，最终材质边缘修正已重新运行全部上述9项。
- 没有跑性能矩阵、cold184、长测或扩展A1/P1/B1。当前灰色合同的定向回归通过，不把本次功能收尾改写成全系统性能PASS或完整环境照明编辑工作流验收。
- [可随Git同步的验证摘要和文件hash](Evidence/SIGHTWEAVE_VISION_ILLUMINATION_CLOSURE_20260907.json)。本地Saved证据不要求跨机器存在；checkpoint和报告记录完成状态。

复核命令（从家里仓库执行，使用新的RunName）：

```powershell
& Scripts/RunGrayObjectPolicyTests.ps1 -RunName NEW_VisionLight -Rendering -Tests 'Darkwell.SightWeave.Closure.VisionIlluminationBoundary+Darkwell.SightWeave.M6P1.Integration.VerticalSliceAuthority+Darkwell.ObjectMemory.PresentationResidency+Darkwell.FogVisual.CanonicalCoverage+Darkwell.PropLab.MovingRules.FastSweep.AuthorityRevisionAndReactivationGuards+Darkwell.PropLab.MovingRules.FastSweep.SlowFastSweepEquivalent+Darkwell.FogVisual.RememberedProp.RuntimeAtoB'
```

最终提交SHA从 `git rev-parse checkpoint/sightweave-gray-performance-closure-20260907^{commit}` 取得，与最终交接一致；不在自身提交文件中填写自引用SHA。

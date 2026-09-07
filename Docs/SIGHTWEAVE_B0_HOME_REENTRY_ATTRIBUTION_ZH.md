# SightWeave B0：家里机器固定 16 条同步重入归因（2026-09-07）

结论：**没有达到 25–30% 的投入门槛，建议结束 SightWeave 性能专项，进入后续游戏系统开发。** 保留 A1 为可选内存策略，HistoryResidency/P1/B0Probe 默认均为 0。没有开始 B1、atlas、custom scene proxy 或完整 shared renderer。INITIALIZATION 仍 FAIL。

运行时提交：`dba941fb7217b52739273f3985b38e064d1e1f7d`，已推送开发分支。测量时先构建未提交的相同源码，environment.json 因此记录起始 HEAD 1673142 和 source.patch；随后提交的 C++ 与被测源码相同，没有混用 DLL。最终文档 SHA 由本报告所在提交和最终交接给出，不在文件内制造自引用 hash。

## 1. 环境与证据边界

家里仓库 `D:\UE_pro\Darkwell`，引擎 `D:\UE_5.8`；Ryzen 9 3900X / RTX 2070 SUPER / 32 GiB。磁盘 Build.version 实际为 **UE 5.8.2、CL56702186**，与原指导文件 5.8.1 不同；没有改引擎版本或借此改历史结论。真实 D3D12/SM6，Standalone 1920×1080、screen percentage 100、AA4、VSync/固定步长关闭；foreground 全测量窗口有效，独立进程，不截图的性能对照全部开启相同 trace。

六条有效性能运行的 Darkwell DLL SHA256 完全一致：
`985C7EC3BF768935733F8683A5D4331661486067CA0EF5A8384A67E0C1055C40`。
完整机器信息、每 record 分段、每批总账、engine counters、文件 SHA256 和定向验证结果已存入 [可移植证据摘要](Evidence/SIGHTWEAVE_B0_HOME_20260907.json)。原始 frames、日志、trace 和截图位于家里 `Saved/Stabilization/B0_Home_*`，不进入 Git；以后缺少这些本地文件不否定本报告已完成的工作。

公司 A1 的 26.728/29.043 ms、Residency GT 13.994 ms 仅为**公司历史基线**，其运行时仍 a8d12dd。本报告的所有收益比例使用家里同 DLL 数据，不拿公司毫秒数作严格 A/B 分母。

## 2. 固定压力合同及总账方法

沿用原 `OldHistory64FewDemand`：N64，32 confirmed Whole + 32 Partial，窄需求 K16；6 秒自然老化，原 340 帧相机路线未改。距离、预取、1 秒保留、5 秒 capture pin、LRU、CPU knowledge、独立 pose、Whole/Partial/cap 语义均未修改。原 synthetic cold184 输入和 gate 未动，本轮没有运行或优化 cold184。

每个真实重入批次严格 **16 records = 8 Whole + 8 Partial，16 uploads + 8 cap mesh submissions**；原型每批延后到本调用末注册 16 mesh + 8 cap，共 24 个组件。不是延后一帧，不跨帧积累，不用预建对象或放宽驻留合同。全部有效运行 failures/missing=0、CPU evidence hash 相同且前后不变：`16192475078042990979`。

家里路线耗时使 `multiple_reentry` 的保留期自然到期，因此每次实际有 **4 个 16 条批次**（boundary、turn180、teleport、multiple_reentry），共 64 次重建；公司记录只有前三批共48。保留第四批，不缩短路线以复刻公司计数。每模式 3 次、12 个批次；按相同四阶段比较。

`DarkwellB0Probe` 用 GT 嵌套互斥桶计时：子 scope 从父桶扣除，逐 record 求和与批总和校验。cap CPU 包含同帧 worker join 的 elapsed time；这些是路径占用 GT 的墙钟时间，**不是经 ETW 校正的纯处理器执行周期**，可能包含调度/抢占。只测固定16，不假装已用多K回归识别通用截距。

## 3. 16 条重入成本分解

下表使用基线 mode1 的12个批次算术平均，因而可以闭合总账（四舍五入有微小误差）。原型 mode2 使用同样计时。

| 互斥阶段 | 基线 ms / 16条 | 原型 ms / 16条 |
|---|---:|---:|
| pixels生成、复制/抑制处理及新纹理清零 | 1.679 | 2.069 |
| Float16分配与转换 | 1.338 | 1.704 |
| texture + cap signatures | 2.813 | 2.809 |
| cap CPU cells、topology、合并及必要诊断准备 | 3.673 | 5.278 |
| 每record表现对象创建/配置 | 2.477 | 2.401 |
| GT提交，原型包含末尾flush | 2.184 | 1.927 |
| record内其他检查/准备 | 0.081 | 0.058 |
| record外扫描、循环和归因记录开销 | 0.316 | 0.314 |
| **整批驻留处理（不含末尾JSON封装）** | **14.560** | **16.562** |

原型平均CPU受到真实离群帧影响，全部保留；不能将此均值差断言为批量接口导致更多CPU算法工作。中位批次为14.353→14.471 ms，实际重入完整帧中位27.482→27.470 ms，约0.012 ms差异不构成收益证据。`Residency.FrameMs`还包含末尾JSON封装；完整 wall frame包含Python采样、引擎和实际渲染等待，不能把14.56当完整帧。

### CPU不可由本B原型消除的工作

基线CPU准备合计 **9.502 ms**，范围 **9.063–9.900 ms**。16条相同纹理尺寸192×96，共294,912 texels，仍从最新已提交CPU状态重生成、签名和Float16转换；8条Partial仍计算cap，不使用旧渲染对象作为知识来源。已有 PartBounds/PartGeometry 经A0释放保留，此fixture不重建它们；需求球的 bounds/CapQuads 扫描发生在record外。record内资产路径检查、资源存在性和发布标志等约0.081 ms。

这里的“必须”限定为**保留本轮CPU准备合同、仅更换表现提交后端**。9.502 ms不是任何未来renderer的数学下界：例如改变缓存/签名策略、数据布局或把工作移到GPU可能改变准备成本，但那不是本轮已证明的B收益；初始清零也包含在pixels桶内。没有把authority/Evidence的推进算法重写成更便宜的替代品。

### 每record独立对象及配置

| 桶 | 基线批次平均 ms |
|---|---:|
| proxy actor + root构造/配置（root注册另记） | 0.782 |
| static mesh component构造/配置 | 0.408 |
| texture UObject/平台数据/配置（UpdateResource另记） | 0.339 |
| MID创建（参数及绑定另记） | 0.556 |
| cap component构造/配置/材质加载绑定（注册另记） | 0.391 |
| 合计 | 2.477 |

这些是对象**构造路径**，不是剥离分配器、资源大小和配置后的纯UObject分配成本。cap桶含其SetMaterial/asset load；proxy的SetMaterial/参数发布另列下表，因此cap的0.391 ms是“纯cap对象成本”的上界，而不是声称已分离出cap绑定的精确毫秒数。这个保守归类不影响对象+提交的整体预算。

### GT / render提交

| 桶 | 基线批次平均 ms | 原型批次平均 ms |
|---|---:|---:|
| 注册（原型含同帧flush） | 0.738 | 0.401 |
| proxy材质加载、参数和SetMaterial | 0.271 | 0.251 |
| texture UpdateResource + UpdateTextureRegions入队 | 0.371 | 0.490 |
| cap SetMesh及其visibility | 0.725 | 0.710 |
| 最终SpatialReady/visibility发布与标志 | 0.079 | 0.075 |
| 合计 | 2.184 | 1.927 |

这些是GT API及产生render work的成本，**不是GPU上传耗时**。texture/cap数据量没变，不能认为共享后端可以把这些字节和几何处理完全清零。

`ExportB0Trace.ps1`导出第一对trace，明确ThreadId，64个GT `Darkwell_B0_Reentry`与四批对应；单批reentry scope之和基线13.808–15.096 ms，与互斥桶总账相符，批次扫描和记录字符串在scope之外。原型另有同帧 `Darkwell_B0_BatchRegister`。

为了观察异步提交，在每批首scope前5ms至末scope后40ms的宽窗口查询RenderThread：基线 `UpdateAllPrimitiveSceneInfos` 累计1.450–1.632 ms，原型1.551–3.187 ms。**这是约58–75ms多帧窗口内的inclusive scope观察，含正常场景工作，不是该批专属RT费用，不与GT相加，也不由此声称完整RT上界。** 没有观察到足以把本原型抬到25–30%门槛的额外串行render收益。第一轮43.664ms峰值时CPU准备为24.405ms、注册flush0.335ms；trace同样显示峰值在重入GT准备窗口。未做OS调度归因，不凭scope名称断言离群的具体系统原因。

### 固定成本与线性成本的边界

按当前N64/K16模型，总账是“固定N64需求扫描 + 16条各自CPU/对象/提交 + 一次flush + 记录开销”。基线平均每record CPU约0.594ms、对象约0.155ms、GT提交约0.136ms；这是8W/8P混合平均，不假设Whole和Partial一样贵。只有Partial产生cap。

基线record外约0.316ms，包含扫描和16条计时结果封装，**不能全部叫作可由B消除的固定成本**。原型flush平均0.336ms、中位0.328ms，也仍遍历24个组件，不是与批大小无关的常数。记录了每record样本和已知操作计数；没有为了拟合漂亮直线修改固定16合同。不能由单一K推断完整atlas的固定开销或任意N外推。

## 4. 唯一小型B实验及实测结果

B0Probe=0关闭；1只归因、原始注册；2仅在自动驻留重入调用中收集组件，资源、材质、CPU cap、可见标志准备后，调用UE `FRegisterComponentContext`统一注册并Process，返回前完成。该接口内部仍逐组件AddPrimitive，**没有把24个SceneProxy或16个texture/MID变成1个**。它验证的是减少空cap注册后重建、合并注册阶段的窄方向，不能冒称已实现shared renderer。

所有对象仍独立，epoch/pose/CPU authority和生命周期不变；不建atlas，不池化、不修改A1需求策略、不更改默认值。唯一实验没有在失败后扩成第二套renderer。

| 同DLL运行顺序 | B0模式 | 家里老历史路线最大完整帧 ms |
|---|---:|---:|
| B0_Home_A1 | 1 | 29.752 |
| B0_Home_B1（名字中的B1仅表示样本序号） | 2 | 43.664 |
| B0_Home_B2Bounded | 2 | 28.886 |
| B0_Home_A2 | 1 | 28.612 |
| B0_Home_A3 | 1 | 28.468 |
| B0_Home_B3 | 2 | 35.783 |

基线峰值中位28.612，原型35.783ms。注册阶段均值节省约 **0.337ms**（中位约0.325ms），整体提交均值节省0.256ms；**整帧改善不成立**，不能报注册降低约46%就宣称整帧降低46%。全部启动/pin创建帧保留在frames/performance文件中，不用老历史路线峰值覆盖初始化失败。

## 5. 收益上限及是否投资B

A. 当前每record对象+提交的**全部可疑预算**平均 `2.477 + 2.184 = 4.661ms`，批次范围约4.28–5.15ms。纯对象、注册、proxy绑定、最终可见发布合计约3.57ms；其余texture/cap提交约1.10ms还携带不可凭空消失的数据工作。不能承诺全部4.66ms都可消除。

B. 同合同CPU准备约9.50ms，并未因批量注册下降；其他检查及扫描/采样约0.40ms。缓存bounds保留，无额外CPU知识降精度。

C. 把4.661ms全部变成零、假设节省一比一反映在完整帧，是**本已测GT预算的乐观上界**：对平均重入完整帧27.592ms约16.9%，对路线峰值中位28.612ms约16.3%。连其他约0.40ms也不合理地全部免掉，仍仅约17.7–18.3%。真实B仍需至少一个对象、数据传输、cap/独立pose描述，收益应低于这个预算；已测小型原型只有约0.3ms注册改善、无稳定整帧收益。没有测完整B，不能给出虚假的确定“最佳atlas帧数”或宣称这个GT预算是任意未来GPU架构的绝对上限。

D. 家里25–30%目标要求峰值中位至少减少约7.15–8.58ms。当前对象/提交可疑预算及唯一原型均没有可信证据支持这一幅度，也没有新增RT关键路径收益证据填补差额。**按用户的证据门槛止损：停止SightWeave性能专项，保留A1默认关闭作为可选内存策略，进入后续游戏系统开发。不给B1生产切片，不建议继续追局部微项。** 这是投资决策失败，不是声称所有可能renderer已被数学排除。

## 6. 验证、失败记录与真实验收状态

- 完整 `DarkwellEditor Win64 Development`：`Scripts/BuildEditor.ps1 -EngineRoot D:\UE_5.8`，最终 `Saved/Stabilization/B0_HomeBuild/Build02.log` **Succeeded**。首构建的头文件顺序提示及UE_JOIN弃用提示已在最终构建前修正；引擎自身deprecated警告仍保留。
- 真实D3D12定向 `A1ConservativeDemand + PresentationResidency + OrdinaryHost`，B0Probe=2：**3/3 clean PASS，warning0、severe0**。A1新增“本调用返回时mesh/cap已注册”检查；A0覆盖离线CPU继续推进、纹理读回/cap一致及失效/生命周期；没有扩大完整矩阵。
- 两次独立PIE视觉 `B0_Home_VisualA/B`，各18图，完整36图完成、severe0；快速转身/瞬移首样本可见历史，无观察到新增空白帧。Whole/Partial独立pose和cap外观维持，结构性注册检查同调用完成。世界区域比较非bit-exact（AA及时间相关画面），平均绝对RGB差0.047–0.332/255；该指标仅辅助，不能代替任意相机/任意资产的零延迟证明。**本定向路线0额外首显帧，不宣称泛化验收。** 截图性能不混入六条Standalone样本。
- `B0_Home_B2/B2Retry/B2Fixed` 在前台握手阶段原子替换文件遭WinError5，正式测量未开始；全部保留并判无效。runner读取改为允许FileShare.Delete，Python原子发布再加最多8次、间隔5ms的有界PermissionError重试，仍失败则显式失败；仅startup/失败IPC，不在有效测量中等待或重抢前台。6项foreground单元测试通过。Bounded后的性能两模式仍同DLL、同driver、同渲染条件，启动IPC修复不计为性能收益。
- **INITIALIZATION / BATCH HITCHES仍FAIL；FRAME PERFORMANCE仍FAIL，长期资源/架构审计不升级。** 冷184合同保留，公司Saved不在家里不代表旧工作未完成。没有冷184新改善声明、完整矩阵、十分钟长测、Shipping、任意多视图或完整B验收。
- stable仍为 `404a5820739638f1097eaae0aa7fba19733298c3` / `7534163b9c5718700b610e7677f47fbaa79cf977`；Docs/AI未改。只推送开发分支。

## 7. 复核入口

以下命令从 `D:\UE_pro\Darkwell` 执行，重新运行必须使用新的RunName，不能覆盖现有Saved。

```powershell
& Scripts/BuildEditor.ps1 -EngineRoot D:\UE_5.8
& Scripts/RunGrayPerformanceBaseline.ps1 -RunName NEW_A -Mode Standalone -Protocol A1 -HistoryResidencyMode 1 -B0Mode 1 -NoAuthoringToolsets -Trace
& Scripts/RunGrayPerformanceBaseline.ps1 -RunName NEW_B -Mode Standalone -Protocol A1 -HistoryResidencyMode 1 -B0Mode 2 -NoAuthoringToolsets -Trace
& Scripts/RunGrayObjectPolicyTests.ps1 -RunName NEW_TEST -Rendering -Tests 'Darkwell.ObjectMemory.A1ConservativeDemand+Darkwell.ObjectMemory.PresentationResidency+Darkwell.ObjectMemory.OrdinaryHost' -ExtraEditorArgs '-DPCvars=r.Darkwell.ObjectMemory.B0Probe=2'
python Scripts/AnalyzeB0Reentry.py Saved/Stabilization/NEW_A Saved/Stabilization/NEW_B --output Saved/Stabilization/NEW_summary.json
& Scripts/ExportB0Trace.ps1 -RunName NEW_A
python -m unittest discover -s Scripts/Tests -p test_gray_benchmark_foreground.py
```

分析器校验同DLL、CPU摘要、每批16/8、upload/cap增量、逐record互斥总账及0missing/failures。原始引擎RT/RHI/GPU counters有延迟和重叠，仅保存在证据摘要供交叉检查，不相加制造收益。

# 第二阶段：固定原始几何的空间显露

**PARTIAL — READY_FOR_USER_FIXED_GEOMETRY_SPATIAL_REVEAL_RETEST**

基线：`babd391e1fb6074056ada11d15bc111747deb329`。分支：`codex/darkwell-prop-memory-gameplay-lab`。开始时 HEAD、upstream 一致，工作区干净。本阶段只有一个提交（本文件所属提交，最终 SHA 见交接消息）。`0032533` 没有恢复、读取或复用。

用户已经人工确认第一阶段隐藏投影；**本阶段用户人工 PIE 尚待执行**。这不是最终 Mode 2，不开始黑色剖面阶段。

## 实现和范围

只对活动手动房间的 `Lab.ManualStale.Cabinet` 绑定 `M_ManualFixedReveal`。它复制现有表面的原光照图，仅新增 OpacityMask：

```text
UV = (原始表面 AbsoluteWorldPosition.xy - FogWorldMin.xy) * FogWorldInvExtent.xy
局部年龄 = Raw >= 0.99 ? min(1, 上帧局部年龄 + DeltaSeconds / 0.20) : 0
局部 alpha = 启用 && 已绑定 && Raw >= 0.99 ? 局部年龄 : 0
主通道 = DitherTemporalAA(局部 alpha)
同一网格的阴影通道 = 1 (ShadowPassSwitch)
Mode 0/1 = 完整遮罩 1，原来的整件显示判定不变
```

世界坐标排除 shader offsets。没有 World Position Offset 连接，没有顶点位移、缩放裁切、动态 Bounds、切片或辅助几何。`M_ManualFixedRevealRamp` 使用现有两张固定大小的软覆盖 RT，逐位置立即累计；新实例/切换模式会清掉视觉历史，不能继承已观察空地的渐变值。没有等待整件覆盖门限，也没有整个柜子的统一透明渐入。材质实例只在 BeginPlay 的原绑定位置创建，模式变化不替换 MID 指针，参数变化不更改 AppearanceRevision。

Mode 2 的三个原组件需要提交绘制，才可能显示首次接触的窄区域。Adapter 仅在这个手动对象上让材质控制实际像素；`SourceLive`、`Observe`、碰撞、StableID、快照捕获/验证/清除仍使用原判定。渲染组件 `IsVisible=true` 不代表柜子像素已经可见，未覆盖区域由材质裁掉。原生生成帧仍沿用 `babd391` 的隐藏初始化。

`babd391` 的三件真实部件 `CastShadow` / `CastHiddenShadow` 不修改；阴影分支不受显露遮罩限制，所以始终使用同一完整原柜子投影。ABSENT 销毁实际 Actor，真实阴影源归零。灰色记忆代理仍不投影。没有第二套投影几何。

运行时原生类本来有 **12 个 StaticMeshComponent 槽位，其中 3 个用于此柜子、9 个未用且隐藏**。这 12 个原槽位全部保留，没有新增 PrimitiveComponent。不是将“12 个槽位”误报为只存在 3 个 UObject；当前柜子的有效真实部件和投影源始终各 3。既有记忆代理和实验室其他隐藏布局保留。

未修改：Mode 0/1 行为、正式地图（含 `/Game/Maps/L_Prototype`）、任何 `.umap`、SightWeave 公共合同、正式记忆规则、`Darkwell.uproject`。没有封口，没有边缘抗锯齿专项，没有复杂网格/薄结构/大量家具性能结论。点状时域渐变仅用于约 0.20 秒局部柔化。

## 运行时代码和实际文件

运行时 C++ 共 **新增 41 行、删除 3 行**（净增 38；包括声明、注释和空行，不计测试/脚本/资产）：

| 文件 | 本阶段改动 |
| --- | --- |
| `Source/Darkwell/Private/VisionPresentation/DarkwellPropGameplayLab.cpp` | +31 / -2；绑定专用材质、局部视觉历史 |
| `Source/Darkwell/Private/VisionPresentation/DarkwellRememberedPropSubsystem.cpp` | +5 / -1；仅手动 Mode 2 提交原组件供材质裁切 |
| `Source/Darkwell/Public/VisionPresentation/DarkwellPropGameplayLab.h` | +5 / -0；绑定状态及方法声明 |
| `Source/Darkwell/Private/Tests/DarkwellSightWeaveAdapterTests.cpp` | 新增几何/材质回归，旧隐藏投影测试区分提交状态与像素可见性 |
| `Content/Darkwell/Vision/PropLab/M_ManualFixedReveal.uasset` | 固定原几何的遮罩材质，LFS |
| `Content/Darkwell/Vision/PropLab/M_ManualFixedRevealRamp.uasset` | 只处理局部视觉年龄的 RT 材质，LFS |
| `Content/Python/create_manual_fixed_reveal_materials.py` | 官方 Editor API 创建上述两个资产；不保存地图 |
| `Content/Python/verify_fixed_reveal_shader.py` | GPU 材质读回，只有瞬态离屏材质/RT，无测试几何 |
| `Content/Python/verify_manual_fixed_reveal.py` | 有限真实 PIE 驱动、逐 tick 几何检查 |
| `Docs/DARKWELL_FIXED_GEOMETRY_REVEAL_RETEST.md` | 本交接报告 |

## 构建与自动化

标准命令：`Scripts/BuildEditor.ps1`，`DarkwellEditor Win64 Development`，不使用 Live Coding。

- Build01：失败，新代码的 `TObjectPtr` 自动类型推导和 Expression 指针访问错误；修正后保留完整日志。
- Build02：成功。
- Build03：增强回归后成功，最终 C++ 构建。编辑器 DLL 占用造成链接重试 16 秒，关闭编辑器后正常完成。
- 合计 **3 次标准构建：2 成功、1 失败**。

```powershell
& 'D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UE_pro/Darkwell/Darkwell.uproject' -unattended -nullrhi -nosound '-ExecCmds=Automation RunTests Darkwell.PropLab.ManualSwitch+Darkwell.FogVisual.RememberedProp+Darkwell.PropLab.Scope+Darkwell.FogVisual.GrayUnlit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/UE_pro/Darkwell/Saved/AutomationReports/FixedReveal02' '-abslog=D:/UE_pro/Darkwell/Saved/PropGameplayLab/FixedReveal/Automation02.log'
```

Automation01 和最终 Automation02 各 **9/9 通过、0 失败、0 带警告测试、0 未运行**，两轮共 18 项执行。最终 9 项：GrayUnlit.MaterialContract；RememberedProp.AtoBPolicy / RuntimeAtoB；ManualSwitch.ErasureAndModeChanges / FixedRevealGeometry / FixedRevealMaterialContract / HiddenShadowSameGeometry / IsolationAndTenCycles；Scope.DefaultAndNoWorld。

`FixedRevealGeometry` 用原合法覆盖系统扫描 **481 个角度**。原 Actor、三个部件 Relative/World Transform、Local/World Bounds、八个固定角点、网格资产、全部 LOD0 顶点的世界坐标及 MID 身份均严格比较（容差 0）。扫描经过 0%、窄条、约 25/50/75%（100 个表面样本，区间 ±2%）和 100%。组件指针/数量和三个隐藏投影源不变。负控制仅修改捕获的测试数据，确认薄片缩放、Bounds 重建、移动顶点、增加部件都会失败，**没有缩放真实测试柜子**。

`FixedRevealMaterialContract` 检查实际资产是 Masked、WPO 为空、世界位置坐标连接实际存在、Raw 与局部年龄用同一 UV、Raw 是合法覆盖纹理、Soft 是逐位置纹理、主通道连接局部 alpha 而非整件标量、阴影连接常数 1。旧隐藏投影测试继续覆盖 Mode 0/1/2 各两次存在性循环及快照不受投影影响。

## D3D12 / SM6 真实 PIE 和实际画面

成功轮次 **GPU07**：RTX 2070 SUPER，D3D12 / PCD3D_SM6，正常 TSR (`r.AntiAliasingMethod=4`)，`r.ScreenPercentage=100`，原生 **1920×1080**。45 张 PNG 全部核对为该尺寸，没有重采样冒充原生分辨率。本阶段按要求未跑 1440p 矩阵。

- **2 次完整消失—验证空位—重生—重新观察循环**；两次空位置均 100% 验证、残影 opacity=0，再背向返回观察隐藏阴影。
- **3 次六秒慢扫**：左→右、右→左、斜向。右→左是在第一轮重生已经观察后，再离开视野进行，保留原记忆表现。
- **2095 次几何检查**：每个 Slate tick 检查，同时关键状态会额外采样；这个数不是独立 GPU 帧数。全部 12 个原槽位的数值 Transform、Bounds、角点均不变；相同 Actor 内的组件路径不变，有效真实投影源 3，ABSENT 为 0，代理不投影。只允许新实例第一帧在 Ready=0 时尚未提交主通道，下一次检查必须提交。
- **54 个 GPU 材质用例，每个 64 像素，共 3456 个读回比较**。从实际资产提取显露表达式、使用实际 Ramp 材质，在瞬态离屏夹具中检查双向 0%、3.125% 窄条、25/50/75/100%，每步四个 0.05 秒子步；新位置立即从 .25 开始而非等待，0.20 秒到 1，已覆盖位置不会随新位置统一淡入。另测非法 Raw=.5、未绑定、Mode0/1 完整旁路。未对家具绑定测试材质，没有生成几何。

实际打开并检查以下接触图和原图，不只统计文件数：

```text
Saved/PropGameplayLab/FixedReveal/PIE_20260831_221239/
  left_to_right_contact.png   （0005–0016；相邻帧 0009–0012）
  right_to_left_contact.png   （0017–0028；相邻帧 0021–0024）
  diagonal_contact.png        （0033–0044；相邻帧 0037–0040）
  shadow_pairs.png            （0003/0004、0031/0032）
  0002_c0_empty_verified00000.png
  checks.json / geometry.json / review.json
```

左扫时顶面左边和门面底边固定，右裁切边逐步右移；反向时右边固定、左裁切边逐步左移。斜向时顶面和门面按同一世界覆盖显露，仍保持原柜子透视。没有观察到缩小后的完整柜子、压成竖片、整件突然弹出、第二套柜子或重复投影。图中开放切面和点状柔化原样保留；黑色区域是原真实阴影，不是本阶段新增封口。

补充原像素测量：左扫接触图的绿色像素从 0→8659→23395→…→54421；其左边界保持 ROI x=54。右扫 0→8575→23447→…→54436，右边界保持 x=385。逐色阈值与最终图比较最多有 29 个边缘颜色/TSR 差异像素，未删除；这种颜色分类不是权威遮罩真值，不以它声称零误差，也不将其作为放宽通过门限的依据。几何严格比较和 GPU alpha 读回是独立证据。

两轮固定接收区域 `(1008,520)-(1032,546)`（624 像素）中 RGB 各通道 <65 的像素：ABSENT 各为 **0**，未显露的 PRESENT 各为 **600**。相邻显露帧中继续使用原三个真实阴影源，没有切换几何；灯光随瞄准旋转而自然变化。阴影只能在现有 Lit 接收区域看见，灰色 Emissive 记忆地面不强行接收阴影。

异步截图在后续渲染帧消费请求，状态标签可能与图像相差一帧；不声称遥测和像素同帧锁步。玩家和 Controller 的输入 tick 暂停以控制慢扫，世界、组件、灯光、覆盖和渲染继续运行。它是受控真实 GPU PIE，不替代用户人工输入；不作性能/帧率验收。

## 失败和警告全部保留

本阶段全部日志在 `Saved/PropGameplayLab/FixedReveal/`，不覆盖上一阶段。GPU 共 7 次启动尝试，只有 GPU07 完成所需流程：

1. GPU01：两份新资产的默认黑纹理为 sRGB，却声明线性 sampler，编译失败、UE 使用默认材质。改用既有线性占位纹理，Ready=0 保持未绑定时关闭。离屏读回另不支持 R16f，测试改用可读回的 RGBA16f（运行时 R16f 不变）。此轮不能算显露成功。
2. GPU02：54 个 shader 用例通过，重置后的几何序列化比较失败；旧驱动将 Rotator 对象表示作为值，已改为 pitch/yaw/roll 数值并增加前后数据保存。失败清理又反复访问失效的 Controller，造成大量重复错误；原日志全部保留，后续修正清理入口和失败后审计。该轮没有作为几何通过证据。
3. GPU03：与自动化命令行编辑器争用 MCP 8000 端口，未启动 PIE；随后串行启动。
4. GPU04：审计先于 Mode2 初始化，错误地要求原 Mode0 隐藏组件已经提交；驱动改为先 reset/mode2，完成初始化再审计。
5. GPU05：shader 54 用例通过，生成第一帧的原隐藏初始化触发提交状态断言；补上只针对新实例第一帧、Ready=0 的明确检查，所有几何断言保持严格。
6. GPU06：上述第一帧检查误用仅支持 MaterialInstanceConstant 的 Editor API 读取 MID，Python 类型错误；改用 MID 自身公开 getter。shader 54 用例仍通过。
7. GPU07：54 shader 用例及两循环/三慢扫通过，实际打开 45 张序列中的上述图组确认。仍保留原引擎/工具警告。

| 日志 | Warning 行 | 启动 Condition failed 行 | 其他 Error 行 |
| --- | ---: | ---: | ---: |
| Assets01 / Assets02 | 6 / 6 | 0 / 0 | 0 / 0 |
| Build01 / Build02 / Build03 | 6 / 4 / 4 | 0 / 0 / 0 | 14 / 0 / 0 |
| Automation01 / Automation02 | 3 / 3 | 13 / 13 | 0 / 0 |
| GPU01 | 10 | 13 | 2 |
| GPU02 | 7 | 13 | 6136 |
| GPU03 | 7 | 13 | 1 |
| GPU04 | 8 | 13 | 3 |
| GPU05 | 7 | 13 | 3 |
| GPU06 | 7 | 13 | 5 |
| GPU07 | 8 | 13 | 1 |

此表计日志行，不去重；资产 commandlet 的汇总会重复警告。GPU02 的其他 Error 含失败清理反复输出和嵌套 Python 异常行，不是 6136 次独立几何失败。Build03 另有编辑器占用提示。MSVC 非首选版本、引擎头文件弃用、启动 13 条 Condition failed、NavMesh/Crowd、MCP EULA/旧 session、MotionVector 线程标记、Shot 查询性能警告均没有屏蔽。GPU07 唯一其他 Error 为重启后的 MCP session 过期并自动重连；没有新材质编译错误或本阶段失败标记。

## 人工复测与停止条件

打开 `/Game/Maps/L_ProjectFogPropGameplayLab`，PIE 未运行时点击 Play：

```text
Darkwell.PropLab stalemanual reset
Darkwell.PropLab stalemanual mode 2
Darkwell.PropLab stalemanual teleport top
Darkwell.PropLab stalemanual teleport bottom
```

先在 top 看完整原柜子，再去 bottom 走入圆盘使其 ABSENT。离开圆盘，沿右侧通道手动返回上房，完整验证空位（100%，快照 EMPTY，正常地面）。回圆盘，先退出再踩入使 PRESENT。背向柜子手动返回，**此后不要使用 teleport top**，因为它会自动朝向柜子。先看隐藏真实阴影（建议柜子屏幕左下侧，接近 `(4840,330,92)`，朝向 270°），再缓慢转动视野。

分别左→右、右→左、斜向扫过，关注固定顶面、门面、把手位置：只能增加被覆盖的原表面，不能出现缩小的完整柜子或整件弹出。观察阴影延续，再完整重复第二轮。开放切面和边缘点纹不作为本阶段封口/抗锯齿完成的证据。

提交后立即推送当前分支，只提交上述 10 个文件，两份二进制由 LFS 管理；不提交截图、录像、Saved、Binaries、Intermediate、DDC 或生成报告。最终 Git/LFS 检查及 SHA 见交接消息。结束停在实验地图、PIE 未运行、可点击 Play。**等待用户人工确认；不得开始黑色剖面，不得标记 Mode 2 完成。**

# Historical Coverage：最终结果

公司 i5-13500 / RTX 4060，UE 5.8.1 Development Editor，D3D12 SM6、1280×720、100% SP、原 TSR（AA=4）、VSync=0、无动态分辨率/固定时间步。P4 488×408、16 条原遮挡线段，29 Static＋14 ObjectMemory。基线 501a18c，最终源码为包含此报告的提交。没有修改地图、材质、门、视野范围或 authority 精度。

## 固定位置/移动配对

`HistBeforeGray` / `HistBeforeUnknown` 使用仅新增 benchmark harness 的原算法构建；After 使用最终修复构建，各 run 的 source.json 保存 DLL hash。相同 6 处合法观察准备，然后厨房 `x=-330±25 cm, y=150 cm` 横移；Gray 朝 20°，反向少历史方向朝 200°。三扇门在准备阶段调用原接口打开，所有真实遮挡保持。

每组 240 个测量步骤。四组两两比较，**step、玩家 XY、发布的 body-source XY 均完全相同**（summary.json 的 paired_pose_equality 全 true），不存在因帧率改变真实导航路径而产生的配对差异。未知方向指少历史/朝外侧方向，不声称整个视野完全没有 Gray。

这是按步骤的 pose replay，**不是同 wall-clock 横移速度、不是完整世界状态逐字节快照回放**。不同运行耗时会影响 Torch 剩余电量和时域显示；各次测量中合法光源持续有效。相同的合法准备序列不强制写入知识，切换朝向后 Current/History 分布可自然不同。结果用于隔离当前 coverage 求解成本；另用原生输入测试实际游戏表现。截图审查和自动回归不称作人工 gameplay PASS。

| 配对 | GT mean / P95 / P99 / max ms | Historical mean / P95 / P99 / max ms | Coverage queries mean |
|---|---:|---:|---:|
| Before Gray | 125.93 / 143.07 / 166.65 / 213.87 | 100.56 / 114.71 / 135.06 / 177.42 | 830348 |
| Before Unknown | 13.48 / 16.24 / 17.17 / 18.03 | 1.02 / 1.17 / 1.32 / 1.57 | 8965 |
| After Gray | 15.39 / 16.85 / 21.83 / 24.16 | 1.08 / 1.42 / 1.73 / 1.81 | 5576 |
| After Unknown | 15.79 / 18.13 / 19.49 / 20.31 | 1.26 / 1.53 / 1.81 / 1.90 | 5666 |

Gray ObjectMemory 平均 115.40 → 4.98 ms；Coverage 109.49 → 1.23 ms。查询计数是原 point-oracle 请求，不含新增的整块证明内部射线判断；整块证明的 CPU 成本已包含在实测时间内。Unknown 组整帧均值没有变快，不能宣称所有方向全面提速；需要关闭的 Gray/Unknown 数量级差异已消失。

## 两次真实原生移动

原有 waypoint harness，通过 GameViewport InputKey → EnhancedInput → CharacterMovement 注入 WASD；鼠标按原瞄准逻辑转向。速度、碰撞保持。30 秒预热后，分别测量 30 秒，所有测量帧前台、原生分辨率。

| Run | GT mean / P95 / P99 / max ms | Historical mean / P95 / P99 / max ms | Coverage max ms |
|---|---:|---:|---:|
| HistNative1 | 16.72 / 22.43 / 26.25 / 30.00 | 1.12 / 1.85 / 3.65 / 11.05 | 3.82 |
| HistNative2 | 16.70 / 21.94 / 25.62 / 35.04 | 1.15 / 1.88 / 3.55 / 10.54 | 3.23 |

1794/1798 原始测量行，各排除第一条 reset 后空 OM telemetry 行；有效 1793/1797。moving frames 1577/1563，实际位移 1146.77/1076.80 cm，转向 2027.72/2028.81°。真实碰撞导航不保证与旧 benchmark 逐帧同路径，故不能把旧 22～31 ms 与这两组直接称作严格位置 A/B。

四个修复后 workload 中 Historical ≥50 ms 次数均为 **0**。原生 OM 平均 5.06～5.08 ms，CurrentReveal 3.64～3.68 ms；Static 平均 5.77～5.82 ms，GPU 平均约 5.22～5.24 ms。各 scope 存在嵌套，GT/RT/GPU 是流水线，不可累加。具体 RT/RHI、P99 与其它 counters 全部保留在 summary.json。

本片达到“这些可重复退化场景不再产生 50～100 ms Historical/Coverage 长帧”。仍不代表任意大地图/任意 epoch 数都已有上界，也不代表整帧稳定 60 fps。整帧 P99/max 尚超过 16.67 ms；剩余主要预算为 Static、Current 与其余引擎/场景工作。没有为了达到平均数而隐藏长帧、删掉路线或关闭 AA。

## 验证与代码范围

- `Scripts/BuildEditor.ps1`：完整 DarkwellEditor Win64 Development Succeeded，EditorBuild.log。初始 harness 构建被用户运行中的 Live Coding 阻挡；用户关闭游戏后重建成功，未删除源码或测试绕过。
- 最终 `RunUnknownPartialCutTests.ps1 -RunName HistoryProofFinal -WorkloadTimeoutSeconds 900`，selector 见 automation-summary.json：19/19，18 clean＋1 外部 HTTP 连通性超时 warning，0 failure/not-run/severe；不是 SpawnActor/lifecycle warning。
- Canonical uniform / Current point-grid / common shadow / 原五点 raster oracle 精确比较；0°/37°/90°/179°、同线段证明、不同墙角点不能封闭窄缝、body 未遮挡不能只凭 cone 清零。
- 原 Whole/Partial/37°/Unknown/Clear/Block、Blackout Volume/CleanLab、Static Knowledge、Vision/Illumination 定向回归通过。没有更改这些规则。
- 运行时代码改动集中于 `DarkwellCoverageRaster.cpp`：补充共同阴影零覆盖证明；五点最小值到零时精确提前退出。原索引/历史存储/ownership/cap/current textures 未重构。

`before-gray.png` / `after-gray.png` 是同一最终 pose 的真实 D3D12 截图，现有门和视觉表现未修；不能把画面存在的原问题称为已验收消失。

`runtime-evidence.zip` 保存六次 workload 的完整逐帧 JSON、命令、日志、storage、hash 和截图；`final-binary.json` 对应 After / Native / 最终回归构建。复算使用 `Content/Python/summarize_history_coverage.py`，P95/P99 为 nearest-rank。人工试玩入口保持 `Scripts/LaunchApartmentSightWeaveLab.ps1`。

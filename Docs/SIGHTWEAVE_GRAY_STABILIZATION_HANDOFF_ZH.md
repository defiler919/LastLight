# 灰色层功能检查点与稳定化施工

2026-09-06 开始，状态：施工中；性能、首次/批量尖峰、长期资源和 Editor 退出均未验收。不得据此创建发布 stable 或开始黑色层。

## 起点和人工验收边界

- 实际起点：`a6e4324839acb9b3be7e7c5ea14c0bb4d994735b`，本地/上游/实时远端一致，工作树及暂存区为空，stash 列表为空，`git diff --check` 和 `git lfs fsck` 通过。
- 用户在本任务明确确认该交接状态的 Whole 达标交接退灰测试通过，并表示当前人工路线灰色层行为可接受。最终运行时是 `a4a17a412a35d2e9a0c7dd8917965d6e8c9323f5`；a6e4324 为文档交接，不能说它又修改了运行时。
- 验收覆盖用户的 Whole 局部→整件达标连续显示人工路线及其目前使用过的灰色层交互；不是所有几何的穷尽证明。上一轮自动化 142/142、连续帧 oracle 8632/8632 是既有证据，不能作为本轮重跑结果。
- 两条远端 stable 固定为 `stable/sightweave-gray-core-20260903` → `7534163b9c5718700b610e7677f47fbaa79cf977`、`stable/moving-history-grid-v2-20260902` → `404a5820739638f1097eaae0aa7fba19733298c3`，本轮不移动。
- 非发布 tag `checkpoint/gray-functional-accepted-20260906`：启动时本地及远端不存在，拟指向 a6e4324；注释为“灰色层功能人工验收检查点；性能和退出稳定性仍开放；不是发布 Stable。”
- 实际引擎 `D:\UE_5.8`：Build.version 为 **5.8.2 CL56702186**。AGENTS 的 5.8.1 是环境说明差异，实验按实际二进制记录。

## 冻结的外部规则

1. Whole 每次连续合法观察都用对象自己的 MinimumObservedSpanCm（默认约 100 cm）重新确认；旧历史/缓存不授予资格，不跨轮累计。真正失联才结束；无效 Coverage、revision mismatch、未就绪不等于失联。达标后仍有合法接触就保持整件。
2. 局部→Whole 达标交接必须承接已显露表面：首次与再确认均不得退灰、消失、重新点阵、空白帧或整体闪烁。
3. 合格观察先完成正确历史交接再结束资格；首次离开不空白、后续不回退残缺历史。未达标不能生成完整 Whole 历史、覆盖或删除旧合法知识。
4. 视锥/距离/墙体控制合法接触和跨度；Whole 达标后的统一展示不再按玩家到墙边界逐块裁切，相机深度遮挡保留；不得穿墙发现、扩张地面探索或确认其它/完全不可见对象。
5. SpatialPartial 看到多少显示多少，允许的历史累计局部；补全无内部旧接缝，未观察区域不出现，合法外切口和深灰 cap 保留。
6. StationaryOnly 移动中 Live 但不生成运动历史；隐藏停止不自动记终点，必须重新合法观察。Never 不留灰影；Always 保持兼容语义。
7. StableID、真实 Transform、内部销毁、隐藏换模型/颜色不等于合法反证。Superseded 不伪装 VerifiedEmpty；合法擦除不复活，重建不读取未见隐藏状态。
8. 相同未反证状态允许复用捕获/proxy/texture/geometry snapshot，不无意义逐轮增长；真实新增未解决知识允许增长，不为常量内存丢弃。

## 施工与验收协议

先建立退出最小复现和受控性能入口。所有样本独立命名并保留失败；图像、功能、退出分开判定。关键同条件前后实验串行重复至少三次，记录实际游戏视口、质量、运行方式、前后台和硬件。冷启动及初始化帧单独保留，不藏进预热。

完整帧目标 p95 ≤16.6 ms、p99 ≤33 ms；普通交互不得可重复 >100 ms 或持续慢帧。Editor PIE 与 Standalone 各自报告；模拟 54,000+600 与至少十分钟真实 D3D12 wall-clock 各自报告。若空场景失败先归因基础成本。

禁止强杀/立即 ExitProcess/忽略退出码伪装稳定，禁止修改系统配置、降低画质、改变规则或牺牲合法知识。所有有效阶段明确暂存、commit、push 后核对 local/upstream/remote。生成证据保留 Saved，不提交资产或生成目录。

## 本轮结果（持续更新）

| 维度 | 状态 | 本轮证据 |
| --- | --- | --- |
| FUNCTIONAL REGRESSION | PARTIAL | 人工功能检查点已记录，待本轮受控完整回归 |
| EXIT STABILITY | PARTIAL | 既有间歇 0xC0000005 开放；旧 Slate 栈不是已确证根因 |
| FRAME PERFORMANCE | PARTIAL | 历史 23.950→44.743 ms 不具严格同视口对照，待新基线 |
| INITIALIZATION / BATCH HITCHES | PARTIAL | 待冷/热与真实有效历史数量复测 |
| LONG-RUN RESOURCES | PARTIAL | 待本轮模拟长测和十分钟真实运行 |

本检查点只新增文档，不改 C++、构建配置、插件或资产，因此无需在该文档阶段重新 BuildPlugin；正式运行前核验/构建 Editor。


## 阶段 1：执行器完成与退出所有权

原 `RunGrayMemoryAudit` 图形驱动把 `set_keep_python_script_alive(True)` 保留到退出，却直接发送 `CLOSE_SLATE_MAINFRAME`。实际引擎源码 `EditorPythonExecuter.cpp` 显示：正常 keep-alive 结束会先 DestroyNotification 再排入 QUIT_EDITOR；提前请求引擎退出会让执行器 Tick 跳过该分支，Notification 到 Python 插件 OnShutdownModule 才 SetComplete。`LaunchEngineLoop.cpp` 的顺序是初始 Core ticker Reset → GEngine PreExit → Slate Shutdown → AppPreExit / 模块 Shutdown。Slate async notification 的 UpdateNotification 会把强持有 OwningNotification 的委托排回 ticker。

这解释了与旧 dump 相符的迟到通知→文本析构链。进一步反汇编核对 Core 的队列/函数对象析构与 Slate 捕获参数的析构位置，不能把旧 nearest-export 名称（例如 HazardPointer）当精确函数名。**目前还没有新的失败 dump 把原通知标题或生产者直接读出，不能扩大为每个 0xC0000005 都已证明同根因。** 新造的晚通知实验未稳定触发 AV，其正常结果也保留。

修复已有 audit/gray performance 驱动：原回调注销和 PIE 停止流程完成后，设置 keep-alive=False，把完成/关闭交还执行器；不强杀、不延迟很多秒、不忽略退出码、不清空全局 ticker，不改玩法。受控 `-LegacyDirectClose` 能在同一版本/同一路线上恢复旧关闭方式作为负对照，默认关闭。

| 本轮样本 | 调试器 | 真实退出码 | 内容 |
| --- | --- | --- | --- |
| ExitBefore_Main01 | 无 | 0 | 无 Python，真实主窗口 Alt+F4，未进 PIE |
| ExitBefore_Python0/1/2_01 | 无 | 各 0 | 旧直接关闭；0/1/2 次 PIE 最小路径 |
| Stabilization_ExitBefore_Contracts01 | 无 | **0xC0000005** | 178 帧、3 PIE 完成、severe 0，退出失败 |
| Stabilization_ExitBefore_ContractsDebug01 | 退出前附加 | 0 | 同旧 Contracts；不能覆盖非调试器失败 |
| Stabilization_ExitAfter_Contracts_013849 / _014027 | 无 | 各 0 | 修复后原 Contracts，59.919 / 58.935 秒 |
| Stabilization_ExitAfter_Episodes_013950 / _014127 | 无 | 各 0 | 修复后原 Episodes，35.915 / 43.173 秒 |

运行目录均在 Saved/ArchitectureAudit；最小路径在 Saved/Stabilization。新造通知探针 ExitNotification_Before01、Before02、LateDebug01 均退出 0（最后一项调试器从启动附加），不算已重现根因的证明。退出验收继续开放，需追加最终正常主窗口、多 PIE 和失败路线覆盖。

阶段 1 只提交驱动所有权修复与此处进度，场景/资源算法不变。性能元数据与 Editor 专用诊断入口仍在下一阶段施工；已有烟测包含脚本变量错误、Standalone 内置 ToolsetRegistry PythonTestRunner 初始化错误和后台 PIE，均明确不是正式基线。单独禁用 ToolsetRegistry 会被依赖重新启用，不能声称该开关已生效。当前机器 CPU Ryzen 9 3900X、GPU RTX 2070 SUPER、驱动 32.0.16.1088、RAM 34,305,445,888 字节。用户已退出 SpaceCraft，后续正式实验串行独占 UE/构建负载。

# DARKWELL AI 公共事实快照

仅保存已确认事实、当前有效状态与权威索引；维护方法见 [职位上下文入口](README.md)。

## 核验基点

- Repository: `defiler919/LastLight`
- Development Branch: `codex/darkwell-prop-memory-gameplay-lab`
- Verified Commit: `21cbd519c09c451f9cd1fda339bcc6ec6b2109dd`
- Verified Date: 2026-09-07

此 SHA 是写入本快照前已核验的仓库基点，不是本文件提交的自引用；不承诺此后的 HEAD 自动有效。

## 项目基础

- DARKWELL：3D 顶视角，Windows / Win64，单人离线；C++ 持有核心玩法，Blueprint/资产承担表现、绑定与调参。依据：[DECISIONS](../DECISIONS.md)。
- 仓库声明 UE 5.8.1，默认引擎根目录 `D:\UE_5.8`，可用 `DARKWELL_UE_ROOT` 覆盖；见 [AGENTS](../../AGENTS.md)。最近已提交性能证据记录 UE 5.8.2 CL56702186，属于实验环境记录，不代表仓库声明已升级或本次重新检测了引擎。
- 视野插件为 SightWeave，实际模块含 `SightWeaveRender`、`SightWeaveRuntime`、`SightWeaveEditor`、`SightWeaveTests`；以 [插件描述](../../Plugins/SightWeave/SightWeave.uplugin) 为准。

## 冻结规则摘要

依据：[灰色层交接](../SIGHTWEAVE_GRAY_STABILIZATION_HANDOFF_ZH.md) 的“起点和人工验收边界”“冻结的外部规则”，精确定义不由本摘要替代。

- 战争迷雾为 Black / Gray / Current 三态；CPU 持有玩家知识，视觉不能授予知识。历史依赖合法空间证据，StableID、隐藏真实状态和 Superseded 均不能冒充 VerifiedEmpty；合法擦除不能复活。
- Reveal / History 独立。DARKWELL 使用 `WholeObjectAfterSpan / 100 cm / StationaryOnly`，兼容 `SpatialPartial / Always / Never`；每轮连续合法观察重新满足对象配置跨度，旧历史/缓存不能授予资格，无效 coverage 不算失联。
- Whole 达标后持续合法接触显示整件，保留正常相机深度；不扩张世界探索、不穿墙授予知识。局部到整件不退灰，首次离开先正确交接历史再结束资格；未达标不覆盖旧知识。
- Partial 保留合法局部累计、完整内部表面与外切口 cap；StationaryOnly 不记运动路径，隐藏停止须重新合法观察才记终点；Never 不留历史。
- 相同未反证状态可复用资源；真实新增知识允许增长。每对象最多 64 条封存历史加一个未封存 Current 预留；不自动淘汰，只有完全 VerifiedEmpty 历史可释放。容量合同见 [DECISIONS](../DECISIONS.md)。
- 用户人工接受覆盖当时 Whole 达标连续显示及已使用灰色交互路线，不是穷尽验收或发布 stable。性能施工保持上述产品语义冻结。

## 当前有效状态

- 项目顾问职位上下文机制 V1 已完成实际对齐验收；依据为用户于 2026-09-07 本次归档请求中的明确确认，验收范围见 [顾问快照](PROJECT_ADVISOR_STATE_ZH.md#当前推进位置)。此结论不改变 SightWeave 的验收状态。
- 仍处于灰色层性能收敛，整体 `PARTIAL — GRAY_STABILIZATION_BLOCKED`；本阶段不开始黑色层、不移动 stable。
- Occupancy 同帧生产切片已完成，最终运行时 `f12c6c1`；最新阶段证据见性能审计第 20 节。184 压力两对同二进制 A/B 的最大整帧中位 587.833→543.815 ms；仅支持该切片收益。
- INITIALIZATION / BATCH HITCHES：FAIL（最终峰值约 535–552 ms）；FRAME PERFORMANCE：FAIL；LONG-RUN RESOURCES：PARTIAL；ARCHITECTURE AUDIT：PARTIAL。局部优化未替代全系统验收。
- 最新定向功能 4/4、必要 Contracts / Episodes 视觉通过；上一阶段完整 148/148 属于旧版本证据，不能宣称当前版本完整套件已重跑。EXIT STABILITY 在已验证路径范围内 PASS，不能从旧交接复活已修复 blocker，也不能推广到所有退出路径。
- 历史采用空间候选 / dirty region，非候选可休眠；当前无跨帧队列，seal 外首次 proxy / texture / resource 创建切片尚未施工。全地图分块、流式资源及增量空间索引没有已确定的最终方案。

## 权威索引（按需读对应章节）

- [AGENTS](../../AGENTS.md)：仓库、构建与资产安全规则。
- [DECISIONS](../DECISIONS.md)：长期决定；Deferred / candidate 条目不是最终确认，旧状态须核对后续明确替代证据。
- [灰色层交接](../SIGHTWEAVE_GRAY_STABILIZATION_HANDOFF_ZH.md)：顶部 Occupancy 最新状态；“冻结的外部规则”及人工接受范围；旧阶段不当作现状。
- [性能架构审计](../SIGHTWEAVE_PERFORMANCE_ARCHITECTURE_AUDIT_ZH.md)：第 20 节最新切片与证据限制；第 15 节全局分项基线，后续章节按明确范围更新。
- [Whole 会话](../SIGHTWEAVE_WHOLE_SESSION_HANDOFF_ZH.md) / [Whole 达标交接修复](../SIGHTWEAVE_WHOLE_QUALIFICATION_CONTINUITY_ZH.md)：会话合同与不退灰交接；退出/性能状态以更新交接为准。
- [普通 Actor 接入](../OBJECT_MEMORY_INTEGRATION.md)：当前宿主与插件接入边界。

详细日志、图像、Trace 在文档引用的本机 `Saved/`，不随 Git 交付；以上证据摘要来自已提交报告，本次没有重跑或重新核验全部原始证据。

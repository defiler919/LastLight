# DARKWELL AI 公共事实层

> 用途：供项目内不同 AI 职位共享“已经确认的当前项目事实”。
> 本文件是高密度当前快照，不是聊天记录、变更日志或完整设计文档。

## 元数据

- State Version: 1
- Project: `DARKWELL`
- Repository: `defiler919/LastLight`
- Development Branch: `codex/darkwell-prop-memory-gameplay-lab`
- Verified Project Commit: `d7511db9ed7e1c776076a57ea74b6a5bb60ae0fc`
- Verified Date: 2026-09-07

`Verified Project Commit` 表示本文件中的项目事实已经核对到该项目提交。AI 状态文件自身的提交不要求写回自己的 SHA；对齐时如果 HEAD 仅比该提交多 `Docs/AI/*` 状态维护提交，不视为项目事实过期。

## 公共层维护规则

1. **只保存已确认事实。** “考虑、倾向、候选、可能、希望、待讨论”等内容不得进入公共层。
2. 职位层中的候选内容，只有在用户明确确认，或已有明确的正式代码/已接受决策文档证据时，才能晋升到公共层。
3. 当前聊天只能提供候选信息；发生冲突时，以当前 Git、代码和权威文档为准。
4. 本文件只保留“当前有效状态”。旧状态、讨论过程和被替代方案交给 Git 历史或专项文档，不在末尾持续追加流水账。
5. 详细规则只写摘要并链接权威文档，不复制大段审计、日志和测试证据。
6. 原则上保持在 **250 行以内**；接近上限时先压缩、删除已失效或已由权威文档承载的信息，再增加新内容。
7. 未显式指定职位的新聊天窗口默认 **无职位**。职位只有在用户明确任命后才激活。
8. 未来可以增加编剧等职位；职位专属的待定内容留在职位层，不自动污染公共层。

## 已确认项目基础

- Engine: Unreal Engine 5.8.1；Windows desktop / Win64。
- Runtime: 单人、离线。
- 实现原则：核心玩法规则由 C++ 持有；Blueprint/资产主要承担表现、绑定和调参。
- Perspective: 3D 顶视角。
- 当前主要视野插件/系统身份：`SightWeave`；模块为 `SightWeaveRuntime`、`SightWeaveEditor`、`SightWeaveTests`。
- 主要开发仓库规则见根目录 `AGENTS.md`；长期技术/设计决定见 `Docs/DECISIONS.md`。

## 已确认 SightWeave / 灰色层产品事实摘要

- 战争迷雾为三态：未探索 Black、已探索 Gray、当前合法可见 Current。
- 玩家知识由 CPU 权威数据持有；视觉层不能反向授予玩家知识。
- `SpatialEvidenceOnly`：历史解析依赖合法空间证据；StableID 是身份，不等于玩家知识，不能用身份全局清历史。
- Reveal 与 History 为独立策略。DARKWELL 当前核心组合包含 `WholeObjectAfterSpan` / 100 cm 与 `StationaryOnly`；`SpatialPartial`、`Always`、`Never` 等兼容规则保留。
- Whole 的每个新的连续合法观察 session 独立满足配置 span；旧确认、旧灰记忆或缓存不能跳过本次门槛。
- Whole 一旦在当前连续 session 资格成立，继续合法接触时保持整物体 Current；普通相机深度遮挡仍保留，但不会借此扩张世界探索、邻近地面或穿墙知识。
- `StationaryOnly`：运动中不产生新的路径/中间姿态历史；隐藏停止不能自动制造最终记忆，必须静止后重新被合法观察。
- 历史容量与释放、完整 Whole 规则、重复观察连续性等精确定义以专项权威文档为准，不以本摘要替代。

## 当前已确认技术状态摘要

- 灰色层核心功能行为已完成用户手工接受并保持冻结；后续性能施工不得顺手重设计产品语义。
- 当前仍处于灰色层性能收敛阶段；黑色层工作尚未开始。
- 历史运行时使用空间候选/dirty region 机制，远距离非候选历史可以休眠，不要求每帧全世界扫描。
- Large World / 全地图灰色记忆的最终 chunk、流式表现资源和增量空间索引方案 **尚未确定**；这是未来独立 scalability audit 的议题，不得把候选尺寸或方案写成公共事实。

## 权威资料索引

- 仓库/构建/资产安全：`AGENTS.md`
- 已确认长期技术与设计决定：`Docs/DECISIONS.md`
- 灰色层当前功能与稳定化交接：`Docs/SIGHTWEAVE_GRAY_STABILIZATION_HANDOFF_ZH.md`
- 性能架构与证据：`Docs/SIGHTWEAVE_PERFORMANCE_ARCHITECTURE_AUDIT_ZH.md`
- 普通 Actor 接入：`Docs/OBJECT_MEMORY_INTEGRATION.md`
- Whole 连续观察：`Docs/SIGHTWEAVE_WHOLE_SESSION_HANDOFF_ZH.md`
- Whole 资格连续性：`Docs/SIGHTWEAVE_WHOLE_QUALIFICATION_CONTINUITY_ZH.md`

## 明确不属于公共事实的内容

以下内容除非以后被明确确认，否则不得从职位讨论直接写入本文件：

- 尚在讨论中的故事、世界观、敌人、玩法设想；
- 某职位认为“比较好”的候选方案；
- 尚未验证的性能推测；
- Large World 的具体 chunk 尺寸、流式策略或跨帧实现方案；
- 临时测试、失败实验和聊天中的中间判断。

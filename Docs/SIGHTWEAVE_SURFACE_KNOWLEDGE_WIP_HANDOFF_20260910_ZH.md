# Surface Knowledge V1 — WIP 交接（2026-09-10）

> 2026-09-12 续工结果见 [性能与合同复核](SIGHTWEAVE_SURFACE_KNOWLEDGE_CLOSURE_20260912_ZH.md)。保留本文历史事实；新一轮完成了保守 Runtime 批处理、最终游戏逐格差分和性能测量，但因旧快扫合同与整帧预算仍阻塞，Surface Knowledge V1 继续为 WIP。以新报告及其 Evidence 为最新状态。

**未完成、未合并回工作分支。用户因周额度要求停止扩张并优先备份。**

- 恢复分支：`codex/wip-surface-knowledge-v1-20260910`。
- 起点／原工作分支：`codex/darkwell-prop-memory-gameplay-lab`，`0ebf08e5b57b9477256ddc4e6c73edecb7c2cf05`。该工作分支未推进。
- stable tag 未移动：`stable/sightweave-hard-effective-live-unified-20260910`；annotated tag object `f53d832b3a035bf4413109e3619f1687c27715f2`，peeled commit `3447970d799a09080e5bdb91255ed07b4a881bbd`。
- 本文与施工代码一同提交；恢复时以本 WIP 分支的 Git commit 为准，不把文中的起点 SHA 当施工结果。

## 已写入的模型和路径

1. `DarkwellSurfaceKnowledge.{h,cpp}`：固定 Box 六面独立 CPU Known / Live / Hidden 位集；局部网格最大间距 0.625cm。键区分 owner、floor、authored domain ID、content version、face、cell，同 XY 不合并。Known 不从 Whole 或二维 footprint 扩散。只接受 Runtime 整域证据；最细单元仍无法证明时保持 Unknown，不能用五点通过填满未采样内部。
2. `SightWeaveSurface.cpp` / `SightWeaveWorldSubsystem`：新增 `TrySurfaceRegion`，沿原 Hard EffectiveLive 链裁决。四角同 source / common legal light 的合法证据，加凸 beam SAT 无遮挡证明；否定侧包括背面、范围／高度／方向界、suppression、单一凸实体的整片阴影证明。4096 项 source-restricted 角点缓存绑定 published frame + owner；旧点查询仍保留。这里的证明数学和边界退化处理仍值得继续独立审查。
3. `DarkwellSurfaceKnowledgeSubsystem`：显式固定 Engine Cube 域，捕获刚体姿态与完整运行时 memory scope；登记 Runtime receiver，更新 CPU，上传每面 dirty rectangle。六面可转置打包，RGB 分别为 Live / Known / presentation suppression。默认上限 128 域、总 atlas 32M texels；容量或固定几何声明违约显式 fail closed。没有移动物体历史代理。
4. ObjectMemory：`bUseFixedSurfaceKnowledge` + `SurfaceContentVersion` 为显式入口，只接受单 Cube、StationaryOnly。`HasSurfaceKnowledge` 与 `IsSurfaceObjectRecognized` 分开；Whole recognition 不填充其他面。Reset 撤销本 Scene 的固定域并恢复源材质。
5. Static：新增 `RegisterImmutableSurfaceBox` / `HasSurfaceKnowledge`；与 Object 共用同一 CPU 存储服务，注册归属分开。旧 XY / height 查询仍只代表 legacy store，不能用于推断新 Surface。
6. Clear / Block：沿既有 Object / Static 区域入口转发；区域是 XY column，按各面样本的实际世界位置处理。Clear 清 Known，下一次合法观察可重写；Block 保留已有事实、阻止新写入并遮住灰色，不阻止 Live。Runtime 已发布 memory modifier packet 的 Block / Suppress 也参与记忆写入／显示处理。未完成所有旧历史事务回归。
7. P4：新 `M_SurfaceKnowledgeV1` 仅寻址 CPU atlas，不含眼点、光照合法性或遮挡判定。Live 使用 Lit BaseColor，Known 使用灰色 Emissive；未知面黑色。LWC 中先减 ObjectPositionWS 再转 float。资产由 `Content/Python/create_surface_knowledge_material.py` 经 Unreal Python 正式生成；未修改旧 `.uasset/.umap`。
8. Archive：CPU 提供事务式 Save/Load，校验版本、owner/floor、ID、精确几何／姿态；只保存 Known。服务层 SaveDomain/RestoreDomain 另保存 Whole recognition。**尚未接入项目 SaveGame / persistence provider 和跨关卡 stable-scope orchestration**，不能宣称完整游戏存档已完成；authored IDs 仍需宿主保证命名空间稳定。

## 默认迁移边界

Apartment 墙体和固定家具使用 Surface（23 域）；门、控制台及其他 legacy 对象保留旧路径。平面地面保留原二维 Static 路径；`bObserveFloorSurfaces=true` 是显式的全地面迁移／压力选项，尚不适合作为默认。新增 Surface API 可以表示地面，但不等于全场景替换完成。

没有实现移动 Surface epoch／空证据擦除、复杂多边形、楼板洞口或跨层传播、翻滚／动画。默认墙体与家具迁移本身也仍待整场景玩法验收。

## 已有验证；不能扩大结论

- 最后完整目标构建：`Scripts/BuildEditor.ps1`，`DarkwellEditor Win64 Development` **Succeeded**，19.09s（增量完整目标，非 clean rebuild / Live Coding）。实际引擎为 `D:\UE_5.8` 的 5.8.2，文档标称 5.8.1；工具链及既有引擎警告未处理。
- 最后 D3D12/SM6 批次 `surface_knowledge_final_runtime`：**39 passed = 38 clean + 1 warning，0 failed / notRun**。包含 6 个 Surface Knowledge 测试、Surface Runtime / Static / M2 Query / Vision×Illumination 差分与 M6P1。确切 selector 和事件见归档 JSON。
- P4 有真实网格 scene capture、纯色正对照、逐 texel GPU/CPU atlas 比对；覆盖正面 Live→灰色、未知顶面黑色、升高后顶面灰色、Block 隐藏但保留事实、scope 失效黑色。它是自动化 fixture，不是完整 Apartment 人工走查。
- **原定旧 Whole/Partial/UnknownRegion/Blackout 全套回归尚未在最终代码上完成，不宣称全部冻结合同已验收。** 用户停止要求到达时，已启动的 39 项批次自然结束；未再启动测试。

最终批次性能（诊断值，非全游戏帧率／预算通过）：

| 场景 | 结果 |
|---|---|
| 单柜 167,936 cells，观察点连续变化 | CPU P95 39.201μs / P99 45.400μs；100 帧 400 次 authority corner query |
| 单柜完整遮挡 | 24.602μs；6 个否定证明、0 次精确角点查询 |
| 默认 Apartment 23 域 | cold 51.908ms；dirty P95 18.979ms，max 20.886ms；max 13,158 queries / 442,836 upload bytes |
| 默认 Apartment 静止 | 215.899μs；0 queries / uploads；atlas 45,231,904 bytes |
| 全地面 39 域（较早压力批次） | 优化后仍约 59ms dirty P95；atlas 71,773,984 bytes，因此未作为默认 |

**23 域更新本身仍可能消耗整个 60fps CPU 预算，性能验收未通过。** 当前按帧失效后仍遍历已登记域；复杂边界和多光会放大细分成本，尚无跨帧区域证书复用／大面积边界专用准备。不得用降精度或显示层扩大 Known 来解决。

## 失败记录与下一步

全部现有构建日志、材质生成日志、历次 Surface 测试日志／JSON／source.patch、最后捕获已复制到 `Docs/Evidence/SURFACE_KNOWLEDGE_WIP_20260910/`，原 Saved 记录没有删除。中间测试 source.patch 不包含当时未跟踪文件的完整快照，不应误认为每个中间批次均可独立重建。

保留的主要失败：首次编译的数值收缩／archive API、后续 TObjectPtr range-loop 编译错误；GPU 01–05 黑屏失败；最早全 Apartment 约 2.5s 更新的性能失败。黑屏正对照同样失败，原因是测试没有像正式 P4 Adapter 一样抑制旧 composite；修正呈现选择后通过。没有移除正对照或降低断言。凸阴影证明与角点缓存将压力耗时降低，但没有消除性能风险。

**下次第一步：fetch 并切到这个 WIP 分支，按 AGENTS.md 核对 HEAD／工作区，读取本文及 final_runtime JSON；先审查 region proof 与 scope/clear/block/lifecycle 边界，再确定性能收敛方案。不要从原工作分支误判代码丢失，也不要立即扩大迁移。**

之后补齐旧 Whole/Partial/UnknownRegion/Blackout D3D12 回归与 Apartment 实际玩法／最终帧预算测量；确认固定对象生命周期、存档宿主边界和保守 Unknown 边缘可接受后，才决定是否 cherry-pick / 合并回工作分支。不要移动 stable tag。

复现已有批次可用 `Scripts/RunGrayObjectPolicyTests.ps1 -Rendering`，selector 直接取证据 `surface_knowledge_final_runtime.summary.json`，使用新的 RunName 避免覆盖。本 WIP 不代表 Surface Knowledge V1 完成。

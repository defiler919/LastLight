# SightWeave Post-M4P2 路线图审计

## 1. 审计状态与日期

- 审计状态：**COMPLETED**（文档审计完成；本文件的提交、推送和最终 Git/LFS 闭合由本次任务最终报告记录）。
- 审计日期：2026-08-28（Asia/Shanghai）。
- 审计类型：只读仓库/源码审计加单一文档产出。
- 本次没有启动 Unreal Editor，没有构建、自动化、Cook、Package 或功能实现，也没有重跑任何 M4P2 验证。

本文使用四种证据标签，避免把建议伪装成已有要求：

- **[权威]**：现有 requirements、architecture、migration、contract、handoff 或 final validation 明确写出的要求或状态。
- **[源码事实]**：冻结基线中的源码、模块依赖、资源历史或具体符号证明的当前状态。
- **[建议]**：本次 Agent 根据权威要求和源码缺口提出的排序或边界。
- **[需决策]**：现有权威文档明确未冻结，或不应由实现者自行默认的产品/合同选择。

## 2. 分支与冻结基线

- 已完成分支：`codex/m4p2-sightweave-packaging-performance-closure`。
- 冻结基线：`a37580f4d1b65304626075c2df6675f0bc1dc56b`。
- 审计分支：`codex/post-m4p2-sightweave-roadmap-audit`，从上述冻结基线创建。
- 建分支前确认 M4P2 local、upstream、remote 均等于冻结基线，工作树为空，Git LFS 没有待提交或待推送对象。
- 建分支前远端不存在 `refs/heads/codex/post-m4p2-sightweave-roadmap-audit`。
- `Darkwell.uproject` 在本机没有差异；本次仍将它视为禁止暂存、改写或提交的文件。

M4P2 的完成事实来自 `Docs/SIGHTWEAVE_M4P2_EXECUTION_REPORT.md:5` 和 `Docs/SIGHTWEAVE_M4P2_FINAL_VALIDATION.md:5`。本审计不重新解释或重验其产品、打包、性能和验证结果。

## 3. 审计范围与方法

完整阅读了指定的 M4P2 四份文档、M4P1 Handoff/Final Validation/Subject Memory Contract、M3.5 Handoff/Final Validation/Memory Contract，以及现行 requirements、architecture、migration、existing-system audit、baseline validation、decisions、progress、visibility、项目和插件 README。另用 `rg --files` 与 `rg -n` 检查了 `M4P3`、`M5`、`next milestone`、`follow-up`、`deferred`、`excluded`、`TODO`、`roadmap`、`adapter`、`integration`、`persistence`、`save`、`production`、`L_Prototype` 和 `SightWeave`。

关键的源码否定性检查如下：

- `rg -n "SightWeave" Source/Darkwell Config Darkwell.uproject` 返回零匹配。
- `rg -n "FSightWeaveSaveSnapshot|CaptureMemorySnapshot|RestoreMemorySnapshot|..." Plugins/SightWeave/Source/SightWeaveRuntime Plugins/SightWeave/Source/SightWeaveRender` 返回零匹配。
- `rg -n -i "M4P3" Docs Source Plugins Config`（排除生成目录）返回零匹配。
- `SightWeaveEditor` 中不存在 `RegisterComponentVisualizer`；当前模块启动只注册设置并驱动 Lab，见 `Plugins/SightWeave/Source/SightWeaveEditor/Private/SightWeaveEditorModule.cpp:348`、`:351`、`:378`。

否定性结果均同时用相邻正向符号验证边界，避免仅凭命名推断。

## 4. 已冻结完成的 M1–M4P2 能力摘要

| 阶段 | 已冻结完成的能力 | 仓库证据 |
| --- | --- | --- |
| M1 | `SightWeaveRuntime` / `SightWeaveEditor` / `SightWeaveTests`、强类型 handles、floor/height/source/compatibility/reveal 基础类型、`USightWeaveWorldSubsystem` 生命周期和独立 Lab | `Docs/SIGHTWEAVE_M1_HANDOFF.md:5`, `:23-35`, `:45-54` |
| M2 | 显式 2.5D floor、vision、legal illumination、occluder、reference solver、spatial index、immutable snapshot、exact point/bounds/sample/batch query、source-specific compatibility、bypass 和 hard suppression | `Docs/SIGHTWEAVE_M2_HANDOFF.md:5`, `:14-20`, `:38` |
| M2P 系列 | CPU hot-path、prepared event index、batch、dynamic update、归因和性能/回归基础；M4P2 最终以未削弱阈值闭合 Prepared4096 与 Batch512 | `Docs/SIGHTWEAVE_M4P2_FINAL_VALIDATION.md:55-67` |
| M3.0–M3.4 | 冻结并实现 CPU-authoritative polygon 到 GPU sparse R8 hard mask、persistent residency、post-tonemap hard composite 和 inward-only feather；GPU 始终只是 presentation derivative | `Docs/SIGHTWEAVE_M3P0_HANDOFF.md:11-15`, `Docs/SIGHTWEAVE_M3P2_HANDOFF.md:58-68`, `Docs/SIGHTWEAVE_M3P3_HANDOFF.md:13-29`, `Docs/SIGHTWEAVE_M3P4_HANDOFF.md:39-48` |
| M3.5 | CPU packed HardMemory、memory modifiers、persistent GPU mirror、explicit static-environment neutral presentation、clear/block/suppress、Coarse 25 cm production precision、Lab 和生命周期 | `Docs/SIGHTWEAVE_M3P5_FINAL_VALIDATION.md:81-87`, `Docs/SIGHTWEAVE_M3P5_MEMORY_CONTRACT.md:13-20`, `:171-185` |
| M4P1 | 五类 subject policy、falling-edge Last-Seen immutable descriptor、Custom provider fail-closed boundary、render-only static-mesh proxy、clear/suppress/reacquire/identity lifecycle和人工 Camera 0–4 验收 | `Docs/SIGHTWEAVE_M4P1_FINAL_VALIDATION.md:47-65`, `:186`, `Docs/SIGHTWEAVE_M4P1_SUBJECT_MEMORY_CONTRACT.md:19-43` |
| M4P2 | BuildPlugin、clean host、Editor/Game/Shipping、Shipping isolation、Cooked/Staged D3D12/SM6 smoke、完整回归、资源生命周期和冻结性能门闭合 | `Docs/SIGHTWEAVE_M4P2_EXECUTION_REPORT.md:5`, `:50-72`, `:74-102`, `:130-157` |

这些能力是下一里程碑必须复用和冻结的既有合同，不得在插件中以新 adapter 名义重写第二套 floor/source/query/memory/render/subject core。

## 5. 路线图在 M4P2 后是否已有明确命名的下一里程碑

结论：**有旧路线图中明确命名的后续阶段，但没有一份状态同步、可无歧义执行的 Post-M4P2 “下一里程碑”定义。**

1. **[权威]** `Docs/VISION_SYSTEM_MIGRATION_PLAN.md:214` 命名了 `M5 — Fab-quality authoring, compatibility, and performance gate`，`:235` 命名了 `M6 — DARKWELL adapter integration without authority overlap`，`:290` 与 `:330` 继续命名 M7/M8。
2. **[权威但陈旧]** 同一文件 `:384-386` 仍宣称立即下一步是 M0，然后才是 M1；这与 M1–M4P2 已完成事实冲突。
3. **[源码/搜索事实]** 仓库中没有 `M4P3` 字符串，也没有 Post-M4P2 roadmap 文档。
4. **[权威]** 原 M4 同时包含 subject policies、persistence 和 reveal override（`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:178-211`）；但 M4P1 明确仍将正式 gameplay integration、persistence 和 reveal override 推迟（`Docs/SIGHTWEAVE_M4P1_FINAL_VALIDATION.md:186`），M4P2 又明确排除它们（`Docs/SIGHTWEAVE_M4P2_PACKAGING_PERFORMANCE_PLAN.md:15`）。

因此不能机械地宣布“下一步就是 M5”：M4 的 persistence/reveal 部分尚未完成，而 M4P2 已提前覆盖了 M5 中大量 packaging/performance 工作。仓库需要本审计给出建议，而不是假装旧路线图已经精确描述当前状态。

## 6. M4P2 后仍未完成的真实事项及权威来源

| 未完成事项 | 分类 | 权威来源与当前事实 |
| --- | --- | --- |
| 版本化、确定性 plugin snapshot、capture、验证和 atomic restore | **[权威要求；源码未实现]** | `Docs/VISION_SYSTEM_REQUIREMENTS.md:199-209`; `Docs/VISION_SYSTEM_MIGRATION_PLAN.md:178-212`; M3.5 只测算压缩体积而未实现 persistence，见 `Docs/SIGHTWEAVE_M3P5_MEMORY_CONTRACT.md:145-150`；当前 `FSightWeaveMemoryAuthority` 只有 write/clear/modifier/query/publish packet，见 `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveMemory.h:195-237` |
| Subject Reveal Override 的最终移动主体 presentation、expiry/revocation callback 与隔离矩阵 | **[权威要求；基础 registry 已有但产品 lane 未闭合]** | `Docs/VISION_SYSTEM_REQUIREMENTS.md:169-171`, `:223`, `:245`; `Docs/VISION_SYSTEM_MIGRATION_PLAN.md:190-205`; 当前 specification/handle API 在 `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveTypes.h:338-365` 和 `SightWeaveWorldSubsystem.h:226-236`，实现只存、更新、移除记录并 publish snapshot，见 `Plugins/SightWeave/Source/SightWeaveRuntime/Private/SightWeaveWorldSubsystem.cpp:1130-1180` |
| 正式 WorldSubsystem subject lifecycle / host adapter 组合 | **[源码事实]** | `FSightWeaveSubjectMemoryAuthority` 是独立 plain authority（`Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveSubjectMemory.h:270-298`）；非测试/非 Lab 组合不存在，当前使用点是 `FSightWeaveM4P1LabFixture::Authority`（`Plugins/SightWeave/Source/SightWeaveEditor/Private/SightWeaveM4P1LabFixture.h:52`） |
| Fab-quality visualizers、conversion preview、undo、validation、完整示例和用户文档 | **[权威 M5；当前只有基础 authoring actors/Lab]** | `Docs/VISION_SYSTEM_MIGRATION_PLAN.md:214-233`; plugin README 只把 Editor 描述为 extension point（`Plugins/SightWeave/README.md:28-31`）；当前 Editor module 是 settings/Lab controller（`Plugins/SightWeave/Source/SightWeaveEditor/Private/SightWeaveEditorModule.cpp:348-381`） |
| DARKWELL project-owned Adapter、observe-only/single-authority switch、专用 integration map | **[权威 M6；源码未接入]** | `Docs/VISION_SYSTEM_MIGRATION_PLAN.md:235-288`; `Source/Darkwell/Darkwell.Build.cs:11-23` 没有 SightWeave module dependency，且 `Source/Darkwell` 对 `SightWeave` 零匹配 |
| DARKWELL legal lights、remote sources、player body-circle、dynamic occluders、subjects、HUD/interaction、monster blackout、damage-source reveal 接线 | **[权威 M6；源码未接入]** | `Docs/VISION_SYSTEM_MIGRATION_PLAN.md:253-280`; 当前 legacy 仍遍历 `ULocalLightComponent`（`Source/Darkwell/Private/Gameplay/DarkwellVisibilityComponent.cpp:222-283`）并通过旧 cell query 驱动 subjects（`:522-573`, `:640-652`） |
| `/Game/Maps/L_Prototype` production acceptance | **[权威 M7；尚未开始]** | M7 明确要求 M6 批准之后才可启用，见 `Docs/VISION_SYSTEM_MIGRATION_PLAN.md:290-328`; M3.5/M4P1 都明确未修改该地图（`Docs/SIGHTWEAVE_M3P5_FINAL_VALIDATION.md:78`, `Docs/SIGHTWEAVE_M4P1_FINAL_VALIDATION.md:118`），M4P2 继续排除它（`Docs/SIGHTWEAVE_M4P2_PACKAGING_PERFORMANCE_PLAN.md:15`） |
| legacy fog 删除 | **[权威 M8；明确未授权]** | `Docs/VISION_SYSTEM_MIGRATION_PLAN.md:330-360` |

## 7. 当前插件与 DARKWELL 的集成边界

### 7.1 SightWeave 插件实际具备的通用边界

- **[源码事实]** `USightWeaveWorldSubsystem` 已存在并拥有 floor、vision source、illumination source 和 query 注册/生命周期 API：`Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveWorldSubsystem.h:125-179`。
- **[源码事实]** 可生成 floor/vision/legal-light/occluder 组件，见 `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveComponents.h:12-77`, `:76-125`。
- **[源码事实]** HardMemory scope 内含 world generation、Knowledge Owner、Floor、origin/plane、precision 和 canonical profiles，见 `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveMemory.h:60-73`。
- **[源码事实]** Memory region 已支持 circle、axis-aligned box、rotated box、polygon 和 height/floor scope，见 `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveMemory.h:44-99`；room 可以在 host/editor 层解析为 polygon，但当前没有独立 `RoomId`/room-volume persistence 合同。
- **[源码事实]** BlockMemoryWrites / SuppressMemoryPresentation authorable component 已存在，见 `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveComponents.h:252-290`；hard live suppression 仍是独立 API。
- **[源码事实]** StaticEnvironment 需要显式 immutable classification，见 `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveComponents.h:199-225`。
- **[源码事实]** Last-Seen CPU authority、Custom provider 和 render-only proxy 已存在，但 World/host orchestration 尚未形成正式 production component/adapter。

### 7.2 DARKWELL 当前实际使用 SightWeave 的程度

结论：**插件会作为项目插件被发现/加载，但 DARKWELL gameplay、HUD、save 和 `L_Prototype` 对它的生产使用程度为 0。**

- `Plugins/SightWeave/SightWeave.uplugin:8-31` 将插件默认启用并声明 Runtime/Render/Editor/Tests 模块；这是模块可用性，不是产品接线。
- `Darkwell.uproject:13-90` 没有显式 SightWeave 项，`Source/Darkwell/Darkwell.Build.cs:11-23` 没有 `SightWeaveRuntime`/`SightWeaveRender` 依赖，`Source/Darkwell` 对 `SightWeave` 零匹配。
- 玩家仍构造 `UDarkwellVisibilityComponent`：`Source/Darkwell/Private/Player/DarkwellCharacter.cpp:128-133`。
- HUD 仍加载 `/Game/UI/Fog/M_FogMemoryComposite` 并用旧 visibility query 过滤 threat rows：`Source/Darkwell/Private/UI/DarkwellHUD.cpp:102-113`, `:224-233`。
- legacy visibility 仍以 10 Hz cell refresh、ECC_Visibility traces、自动 local-light 扫描和 `IDarkwellFogSubject` actor-origin callback 为权威：`Source/Darkwell/Private/Gameplay/DarkwellVisibilityComponent.cpp:138-181`, `:222-283`, `:522-573`, `:640-652`。
- save/load 仍只捕获/恢复 legacy explored cells 和 presentation cells：`Source/Darkwell/Private/Save/DarkwellSaveSubsystem.cpp:220-231`, `:385-391`; 字段在 `Source/Darkwell/Public/Save/DarkwellSaveGame.h:64-74`。

### 7.3 正式接入状态矩阵

| 接入项 | 插件通用能力 | DARKWELL 正式接入 |
| --- | --- | --- |
| World Subsystem | 有：`USightWeaveWorldSubsystem` | **无**：没有 DARKWELL dependency、adapter 或 authority mode |
| Floor / room / floor transition | Floor registry 有；region polygon 可表达 room footprint | **无**：没有 DARKWELL FloorId 映射、room/floor lifecycle 或 transition adapter |
| Legal illumination | 显式 illumination source component/query 有 | **无**：DARKWELL 仍自动扫描普通 `ULocalLightComponent` |
| Remote vision/light | source active/update API 有 | **无**：没有 security camera/remote item adapter |
| Region lock-black/lock-gray/clear | Hard suppression、ClearMemory、BlockMemoryWrites、SuppressMemoryPresentation core 有 | **无**：没有 monster/room/area gameplay wiring |
| Subject / enemy display | policy/Last-Seen authority与 proxy core 有 | **无**：Stalker/Warden/拾取物/设施仍实现 `IDarkwellFogSubject` |
| Damage-source reveal | specification/handle registry 有 | **无**：没有 DARKWELL damage qualification、attacker resolution、moving reveal presentation |
| Plugin save/load | **无正式 capture/restore schema/API** | **无**：DARKWELL save 仅存 legacy fog fields |

## 8. `/Game/Maps/L_Prototype` 缺少的 SightWeave 生产接入

`Content/Maps/L_Prototype.umap` 的 LFS pointer/blob 最后由提交 `ca35caf9f2f5a9f2d5e52f0f9a414cdb15eb306e`（`feat: build playable DARKWELL prototype`）修改，当前 blob 为 `a108fd9e8deb6fcc1d386e4d83850eedccfc4e74`；SightWeave 里程碑没有修改它。默认 Game/Editor map 仍是该地图，见 `Config/DefaultEngine.ini:3-6`。

依据 M6/M7 的权威 integration map 和 production acceptance 边界（`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:235-251`, `:290-320`），`L_Prototype` 当前缺少：

1. stable single active Floor definition、floor origin/plane 和 height mapping；
2. 墙、房间轮廓、门等显式 SightWeave occluders，以及 door transform/update adapter；
3. player illumination-gated directional source和永久 body-circle bypass source；
4. torch、lantern、facility/pickup lights 的 explicit legal-illumination profile，而不是 ordinary UE light 自动发现；
5. static-environment eligibility/neutral attributes；
6. enemies、pickups、door/container/machine/exit 的 subject policy、persistent identity 与 snapshot provider；
7. HUD、interaction、threat rows 的统一 hard query route；
8. monster blackout、room/area clear/block/suppress 的 region wiring；
9. attacker-on-owner-hit damage reveal adapter；
10. one-setting `Legacy` / `SightWeaveObserveOnly` / `SightWeave` authority switch，以及切换时旧 blendable/hidden/proxy 状态复位；
11. SightWeave snapshot 在 DARKWELL next save schema 中的嵌入和 no-v6-fog-import 行为；
12. production PIE、save/load、floor transition、mission 和性能验收。

这些是“缺游戏层接线”，不是要求在 SightWeave core 中重写 solver、atlas、HardMemory 或 Last-Seen。

## 9. 已由插件完成、不得在游戏层或插件内部重做的功能

下一里程碑必须复用以下公共合同：

- explicit 2.5D vision/legal-illumination polygon authority与 source-compatible hard query；
- generation-safe handles、world/floor/owner/profile exact identity、immutable revisions；
- CPU packed HardMemory 和 GPU derived mirror；
- ClearMemory、BlockMemoryWrites、SuppressMemoryPresentation、SuppressLiveVision 的既有语义；
- sparse atlas、page table、fail-black、hard composite、inward-only feather；
- explicit StaticEnvironment neutral memory；
- five subject policies、falling-edge immutable Last-Seen descriptor、basic opaque static-mesh proxy、Custom provider fail-closed route；
- M4P2 已闭合的 packaging/Shipping/lifecycle/performance gates。

特别禁止：用 DARKWELL adapter 新建第二套 cell grid、第二套 light compatibility、第二套 memory store、第二套 proxy state machine，或让 GPU/Scene Color/普通 UE light成为 gameplay/save authority。CPU/GPU authority边界见 `Docs/VISION_SYSTEM_ARCHITECTURE.md:352-376`，旧/新 coexistence 冲突见 `Docs/VISION_EXISTING_SYSTEM_AUDIT.md:253-266`。

## 10. 候选里程碑比较

### 候选 A：M4P3 — SightWeave Deterministic Persistence and Atomic Restore Closure

> **[建议命名]** 仓库没有既有 `M4P3` 名称；此名称用于补齐原 M4 明确要求但 M4P1/M4P2 未实现的 persistence 部分。

- **仓库证据**：`REQ-SAVE-001..005`（`Docs/VISION_SYSTEM_REQUIREMENTS.md:199-209`）；原 M4 的 deterministic snapshot/compression/atomic restore（`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:178-212`）；integration 前 standalone save/restore gate（`Docs/VISION_SYSTEM_REQUIREMENTS.md:253`）。
- **玩家价值**：在未来切到 SightWeave 后，探索记忆、明确持久化的区域状态和 Last-Seen subject 能随 continuation save/load 保留；当前阶段价值是解除 M6/M7 的硬前置，不立即改变 `L_Prototype` 玩家体验。
- **架构依赖**：复用 `FSightWeaveMemoryAuthority` packed tiles、exact scope、`FSightWeaveSubjectMemoryAuthority` descriptor；需要先冻结 provider payload、floor mapping、modifier persistence 与 limits。
- **插件公共合同**：**会改变**。需要新增 plain versioned snapshot structs、capture/validate/restore results 和 atomic restore API；不能改写 M3.5/M4P1 frozen semantics。
- **产品地图/资产**：**不需要**。可用现有 Lab runtime fixture 与 native tests；不改 `.uasset`、`.umap`、`Darkwell.uproject`。
- **自动化**：可覆盖 deterministic byte/field ordering、round trip、compression、corrupt/future/oversized/duplicate/missing-floor/missing-provider、no-partial-apply、teardown/world generation、GPU derived rebuild、reveal/transient source exclusion。
- **人工验收**：不应设为 completion gate；没有产品 UI/视觉变化。可选一次现有 Lab restore visual smoke，但 native + real-world automated lifecycle 足以覆盖该里程碑。
- **主要风险**：错误序列化 GPU/active source/transient reveal；partial restore 污染已有 authority；provider payload 无界；world/floor identity 在另一 session 被误当 runtime identity；公共 schema 过早僵化。
- **预计规模**：中等，约 5–8 个可靠提交；Runtime public/private + Tests + docs，不含 host save slot。
- **为什么现在做**：它是权威 M4 缺口，也是 M5 “save restore from documented steps”和 M6/M7 save integration 的前置。先完成它可保持插件独立、范围单一，并避免在 DARKWELL adapter 中临时发明私有序列化。

### 候选 B：M5 — Fab-quality Authoring / Compatibility Remainder

- **仓库证据**：完整 M5 在 `Docs/VISION_SYSTEM_MIGRATION_PLAN.md:214-233`；当前 Editor 只有 settings/Lab controller，缺正式 visualizer/conversion/validation。
- **玩家价值**：主要是开发者/Fab 可用性，降低配置 floor、occluder、light、modifier、subject 的错误率；短期不改变 DARKWELL 玩家体验。
- **架构依赖**：需要稳定的 persistence/reveal/public component contracts，避免工具为随后改变的 schema/API author 数据。
- **插件公共合同**：Runtime 变化应小；主要扩展 Editor contract、validation diagnostics 和 authoring data ownership。
- **产品地图/资产**：会修改插件示例 `.umap`/可能新增 editor-authored assets，必须通过 Unreal Editor/API；不应修改 `L_Prototype`。
- **自动化**：validator、undo、idempotent bake/conversion、stale-data、duplicate floor、unsupported material 和 map dependency tests；BuildPlugin/clean host仍需保持。
- **人工验收**：需要用户在 Editor 中完成 authoring workflow 和 visualizer 可用性检查。
- **主要风险**：工具范围膨胀、资产迁移/引用风险、在 persistence/reveal 尚未冻结前固化错误 UX。
- **预计规模**：大，可能 8–12 个可靠提交并涉及二进制资产。
- **为什么推迟**：M4P2 已闭合 M5 的 packaging/performance 大块，但 M5 exit criteria明确要求 save restore；应先补 persistence，再把剩余 authoring 作为单独里程碑。

### 候选 C：M6 — DARKWELL Adapter Integration without Authority Overlap

- **仓库证据**：`Docs/VISION_SYSTEM_MIGRATION_PLAN.md:235-288` 明确命名、定义专用 integration map、三种互斥 authority mode、adapter map 和接入顺序。
- **玩家价值**：最高且直接；可在真实 DARKWELL 规则中比较并最终切换 SightWeave。
- **架构依赖**：plugin persistence、正式 subject/reveal orchestration、M5 authoring/diagnostics；还依赖 authority switch、floor/occluder/light/subject ID mapping 设计。
- **插件公共合同**：原则上 Adapter 应只消费公共 API；但当前 SubjectMemory 不是 WorldSubsystem production lifecycle，可能暴露一个需要先在插件内闭合的 public seam。
- **产品地图/资产**：**会涉及**新的 `/Game/Maps/L_VisionIntegration.umap` 或等价 integration map；权威路线图明确禁止算法调试阶段直接改 `L_Prototype`。可能需要产品-owned assets，但不应先改 `L_Prototype`。
- **自动化**：observe-only parity、authority exclusivity、HUD/interaction/enemy/pickup/door/light/blackout/reveal/save adapter tests；完整 Darkwell + SightWeave regression。
- **人工验收**：需要用户在专用 integration map 中做 real PIE；M7 才在 `L_Prototype` 做生产人工验收。
- **主要风险**：双重 blendable、双 subject hiding、双 memory writer、save divergence、一次任务塞入过多游戏规则。
- **预计规模**：很大，必须拆分 M6 子阶段，不能和 persistence 或 M7 production acceptance 合并。
- **为什么推迟**：`REQ-ACCEPT-001` 和 migration invariant 要求 save/restore 等 Lab gate 在 integration 前通过；当前 persistence 缺失，直接 M6 会迫使 Adapter 私有实现未冻结合同。

## 11. 唯一推荐的下一里程碑

**[建议] 推荐：`M4P3 — SightWeave Deterministic Persistence and Atomic Restore Closure`。**

一句话目标：**为现有 CPU HardMemory 与 Last-Seen descriptor 提供版本化、确定性、有界、全量验证后原子应用的独立插件 snapshot capture/restore 合同，不接入 DARKWELL save slot。**

选择它的主因：

1. 它直接补齐 `REQ-SAVE-001..005` 和原 M4 明确范围，而不是发明新产品功能。
2. M4P1/M4P2 都明确没有完成 persistence。
3. `REQ-ACCEPT-001` 要求 save/restore 在 DARKWELL integration 前通过。
4. 当前 packed memory 和 Last-Seen core 已具备，缺的是 durable schema/API，不应在 M6 Adapter 内临时重做。
5. 它可以保持纯 C++/文档范围，不触碰 `L_Prototype`、二进制资产或 `Darkwell.uproject`，风险和回滚边界清晰。

## 12. 推荐里程碑的明确任务边界

应包含：

1. 新增 neutral、plain、versioned `FSightWeaveSaveSnapshot`（最终名称可在合同审查中确认）。
2. 明确 schema version、format GUID/fingerprint、hard limits、deterministic ordering 与 canonical comparison。
3. 导出选定 Knowledge Owner/floor scope 的 packed HardMemory tiles。
4. 导出可持久化 Last-Seen records，以 host-provided stable persistent Subject ID 为键。
5. 根据用户已批准的规则处理 persistent modifier mutations；未批准前不得实现默认。
6. capture 不读取 GPU，不保存 live polygons、active source state、RHI resources、derived caches、temporary reveal overrides 或 transient fades。
7. restore 先在临时 plain data 中完成 version/limit/duplicate/floor/provider/settings/checksum 验证，全部通过后一次性替换 CPU authority。
8. restore 后 publish 新 revision，令 GPU memory/static/proxy derivatives 从 CPU authority 重建；不得反向读取 GPU。
9. 明确 corrupt/incompatible/oversized/future/missing floor/missing provider 的 result enum、diagnostics 和 fallback。
10. 加入 focused native automation、real-world lifecycle、NullRHI/D3D12 回归、BuildPlugin/clean-host 验证和合同/validation/handoff 文档。

## 13. 明确非目标

- 不接入 `UDarkwellSaveGame`、`UDarkwellSaveSubsystem`、slot、autosave、level reload 或 menu。
- 不迁移 legacy v6 fog cells；无有效 SightWeave snapshot 时未来 SightWeave memory 从空开始。
- 不实现 DARKWELL Adapter、authority switch、HUD/interaction/enemy/door/light/mission wiring。
- 不修改 `/Game/Maps/L_Prototype`，也不创建实际 M6 integration map。
- 不扩展 skeletal/VFX/light/audio/material semantic snapshot。
- 不实现 damage qualification、attacker resolution 或 reveal visual policy。
- 不修改 GPU authority、hard query、memory precision、feather、static environment 或 Last-Seen presentation semantics。
- 不重做 M4P2 Build/Cook/Package/performance 工作；只运行与 persistence 变更风险相称的 future implementation gates。
- 不删除 legacy fog。

## 14. 必须冻结的既有合同

- M2 CPU explicit-geometry/source-compatible query authority。
- M3 GPU 仅 presentation derivative、fail-black、no gameplay/save readback。
- M3.4 hard point gate先于 inward feather。
- M3.5 HardMemory CPU packed tile为唯一 authority；Coarse 25 cm production memory precision；clear/block/suppress ordering与 no-change zero-work。
- M4P1 policy/falling-edge/descriptor/exact scope/generation/proxy isolation。
- Subject Reveal Override 不进入 knowledge state、memory 或 save。
- M4P2 public behavior、Shipping boundary与性能阈值。
- no-v6-fog-import、single player、single active presented floor。

## 15. 预计修改的模块和文件范围

计划范围（建议，不是本审计已执行的修改）：

- `Plugins/SightWeave/Source/SightWeaveRuntime/Public/`：新增 persistence types/API header；必要时给 Memory/Subject authority 增加受控 export/import plain-data seam。
- `Plugins/SightWeave/Source/SightWeaveRuntime/Private/`：deterministic capture、validation、compression、atomic restore、diagnostics。
- `Plugins/SightWeave/Source/SightWeaveTests/Private/`：新增 M4P3 persistence/restore/lifecycle tests。
- `Docs/`：M4P3 persistence contract、final validation、handoff。
- `SightWeaveWorldSubsystem.h/.cpp`：仅在需要 host-facing orchestration 时增加 API；不得借机重构 M2/M3 hot path。

正常情况下不应修改：

- `Source/Darkwell/**`、`Source/Darkwell/Darkwell.Build.cs`；
- `Plugins/SightWeave/Source/SightWeaveRender/**`（除非 restore-derived rebuild 暴露真实、单独审查的生命周期缺口）；
- Shaders、Config、`.uplugin`；
- `Darkwell.uproject`；
- 任何 `.uasset`/`.umap`。

## 16. 是否涉及 Unreal 二进制资产

推荐 M4P3：**不涉及**。现有 Lab 与 runtime-generated fixtures 足够承载测试；应以 native plain-data tests 和既有 real-world lifecycle harness 完成。若实现者发现必须改 `.uasset`/`.umap` 才能验证 schema，属于范围扩张，必须停止并请求用户批准。

后续 M5 authoring 与 M6 integration map 会涉及 Unreal 资产；M7 才可能修改 `L_Prototype` 或其获批 successor，并且必须通过 Unreal Editor/官方 API。

## 17. 分阶段实施方案

1. **合同冻结**：记录 schema ownership、included/excluded fields、stable IDs、limits、modifier/provider policy、atomicity、failure enums 和 migration rule；先解决第 23 节用户决策。
2. **Plain snapshot capture**：从 CPU authority 导出 canonical sorted floors/tiles/subject records；证明 repeated capture byte/field identical。
3. **Validation/compression**：有界编码、checksum/size/count checks、future/corrupt/duplicate/provider/floor/settings rejection。
4. **Atomic restore**：临时解析验证，成功后一次 publish；失败保持原 authority、revisions和 derived presentation不变。
5. **Lifecycle/derived rebuild**：world restart、floor remap、teardown、GPU mirror/proxy rebuild，不恢复 live/source/reveal transient state。
6. **Focused verification**：NullRHI、D3D12/SM6、M3.5/M4P1相关回归、BuildPlugin/clean host。
7. **文档与 Git 闭合**：final validation、handoff、diff、generated-dir、LFS、local/upstream/remote equality。

## 18. 建议 Git 分支名

`codex/m4p3-sightweave-persistence-restore-closure`

该分支只在用户明确批准实施提示词和第 23 节决策后，从本审计最终推送 SHA 创建；本次审计不得创建它。

## 19. 建议可靠提交检查点

1. `docs: define SightWeave M4P3 persistence contract`
2. `feat: capture deterministic SightWeave snapshots`
3. `feat: validate and restore SightWeave snapshots atomically`
4. `feat: restore SightWeave subject persistence records`（仅含已批准 provider/persistent-ID 边界）
5. `test: validate SightWeave persistence failures and lifecycle`
6. `test: close SightWeave persistence packaging boundaries`
7. `docs: record SightWeave M4P3 final validation`

每个 checkpoint 必须非空、与风险相称地验证后再提交；不得用临时提交掩盖未冻结合同。

## 20. 自动化与构建矩阵

这是未来 M4P3 的建议矩阵，不是本次审计执行记录：

| Gate | 覆盖范围 |
| --- | --- |
| Pure/native NullRHI | canonical ordering、round trip、compression、all limits、corrupt/future/oversized、duplicate ID、missing floor/provider、no partial apply、no GPU/source/reveal serialization |
| World lifecycle NullRHI | capture -> teardown/restart -> floor mapping -> restore -> query；old-world/generation rejection |
| D3D12/SM6 focused | restore 后 memory mirror/static eligibility/Last-Seen proxy由 CPU snapshot重建；无 stale resource、RDG/RHI error |
| M3.5 focused regression | HardMemory bytes、clear/block/suppress、selected precision、no-change、derived mirror contract不变 |
| M4P1 focused regression | descriptor identity/revision、clear/suppress/reacquire、proxy isolation不变；transient reveal不进 snapshot |
| Full SightWeave final | NullRHI + D3D12/SM6，精确 discovered/performed/result；不重设 M4P2 阈值 |
| DARKWELL regression | 至少 full `Darkwell` NullRHI，证明插件 public change未破坏 host；DARKWELL source不应有 diff |
| Editor build | `DarkwellEditor Win64 Development`，满足 `AGENTS.md` 对 C++ 变更的最终构建要求 |
| BuildPlugin / clean host | Editor Development、Game Development、Game Shipping；Shipping不含 Tests/Editor/readback implementation |
| Cook/Package | 默认不要求；若没有 asset/config/descriptor 或 staged-file-IO变更，不重复 M4P2 Cook/Package。若实现范围改变则需用户重新授权 |

所有 generated reports/logs/Binaries/Intermediate/Saved/DDC 保持 ignored/uncommitted。

## 21. 用户人工 PIE 验收需求

推荐 M4P3：**不需要用户人工 PIE 作为 COMPLETED gate**。

原因：插件不拥有 save slot、menu、level loading 或 DARKWELL product flow；该里程碑的正确性是 deterministic plain data、atomic failure和 lifecycle，自动化可客观覆盖。D3D12 real-world automated restore smoke 可以验证 derived presentation。若用户愿意，可做一次现有 Lab 的可选视觉确认，但不得替代自动化，也不得因无人工 PIE 把 M4P3 降为 PARTIAL。

M6 需要专用 integration-map 人工 PIE；M7 需要 `L_Prototype`/successor 的完整人工 production acceptance，见 `Docs/VISION_SYSTEM_MIGRATION_PLAN.md:302-320`。

## 22. COMPLETED / PARTIAL / BLOCKED 判定条件

### COMPLETED

- 第 23 节决策已由用户明确批准并写入冻结合同；
- versioned snapshot、deterministic capture、bounded validation/compression、atomic restore和 subject records 全部实现；
- 所有 negative cases 明确失败且不部分修改 CPU authority；
- live polygons、active source state、GPU resources、derived caches、reveal overrides和 transient fades未被序列化；
- restore 后 authoritative queries、memory/proxy derived rebuild、world/floor/generation isolation正确；
- 第 20 节必需 gates 通过，public/Shipping boundary 和 frozen M3.5/M4P1合同不变；
- 没有 DARKWELL、asset、map、uproject 或 generated-dir diff；
- 文档、Git/LFS 和 remote closure 完成。

### PARTIAL

- plain HardMemory capture/restore 已完成，但 subject records、approved provider route、atomic negative cases、D3D12 derived rebuild 或 required packaging/clean-host gate有一项未闭合；
- 任何 provisional schema 仍未获得产品决策；
- 不得把“核心可用”或单一 happy-path round trip写成 COMPLETED。

### BLOCKED

- 用户未批准必须的 modifier/provider/limit decisions，且不同选择会改变 durable schema；
- 发现 frozen M3.5/M4P1 public contract无法承载 atomic restore，需要超出 M4P3 的架构变更；
- required build/test 无法运行且已穷尽安全诊断，或需要未经授权的资产/产品接入修改。

## 23. 风险、回滚边界与仍需用户决定的问题

### 风险与回滚

- 新 schema 在 DARKWELL 尚未采用前可以整体回滚，不影响现有 v6 save；不得提交 converter 或改写旧 save。
- atomic restore 的回滚单位是完整 CPU snapshot application；失败不得清空当前 memory 后再返回 error。
- 公共 schema 一旦被 M6 host save采用就成为 durable contract，因此 M4P3 必须先冻结 limits/versioning/migration。
- 若 Custom provider 缺失，必须按批准策略 fail whole restore、drop only that subject record或显式 partial result；实现者不能自行选择。
- 不得把 runtime world serial 持久化为跨 session identity；restore 需要 host stable floor/subject mapping。

### 实施前必须由用户决定

1. **[需决策] persistent modifier policy**：V1 snapshot 是否只保存 resulting HardMemory bits + subject records，还是保存某类 durable clear/mutation；active `BlockMemoryWrites`/`SuppressMemoryPresentation` 是否一律由 host load 后重新激活。Architecture 明确把 modifier persistence/fade details列为未冻结 gate：`Docs/VISION_SYSTEM_ARCHITECTURE.md:707-723`。
2. **[需决策] Custom provider persistence**：M4P3 是否只支持 built-in opaque static-mesh Last-Seen record，还是同时定义 bounded versioned provider payload；missing provider 的 atomic restore policy是什么。
3. **[需决策] hard limits/reference target**：snapshot 最大字节、floor/tile/subject/provider payload count/size和压缩算法/版本。Requirements 的 `<8 MiB` 仍是 provisional target，见 `Docs/VISION_SYSTEM_REQUIREMENTS.md:263-274`。

Agent 建议但不替用户决定：第一版不序列化 active modifiers，只保存已经形成的 HardMemory 与 Last-Seen records；所有 active blocker/suppressor由 host重新注册。Custom provider payload应为显式版本、严格单项/总量上限、provider缺失时默认拒绝整个 snapshot以维护 atomic语义。硬上限应在合同提交前由用户批准。

## 24. 下一份“实施提示词”的完整骨架

下面骨架只供下一任务使用；填完用户决策并明确授权后才可执行：

```text
继续 DARKWELL / SightWeave，但只实施：
M4P3 — SightWeave Deterministic Persistence and Atomic Restore Closure。

仓库：D:\UE_pro\Darkwell
引擎：D:\UE_5.8
审计分支：codex/post-m4p2-sightweave-roadmap-audit
审计冻结 SHA：<AUDIT_FINAL_SHA>
实施分支：codex/m4p3-sightweave-persistence-restore-closure

先读取并遵守 AGENTS.md，然后核对 local/upstream/remote、工作树、LFS。
若除允许保留的 Darkwell.uproject EngineAssociation 差异外存在未知修改，停止；
不得 reset/clean/restore/delete。

必须完整阅读：
- Docs/SIGHTWEAVE_POST_M4P2_ROADMAP_AUDIT.md
- Docs/SIGHTWEAVE_M4P2_EXECUTION_REPORT.md
- Docs/SIGHTWEAVE_M4P2_FINAL_VALIDATION.md
- Docs/SIGHTWEAVE_M4P2_HANDOFF.md
- Docs/SIGHTWEAVE_M4P1_SUBJECT_MEMORY_CONTRACT.md
- Docs/SIGHTWEAVE_M3P5_MEMORY_CONTRACT.md
- Docs/VISION_SYSTEM_REQUIREMENTS.md 的 Save/persistence 部分
- Docs/VISION_SYSTEM_ARCHITECTURE.md 的 Save schema/versioning 与 remaining gates
- Docs/VISION_SYSTEM_MIGRATION_PLAN.md 的 M4/M5/M6 边界

用户已批准的 durable decisions：
1. Persistent modifier policy：<填写>
2. Custom provider payload 与 missing-provider policy：<填写>
3. Snapshot hard limits、压缩和版本策略：<填写>

一句话目标：
为现有 CPU HardMemory 与 Last-Seen descriptor 提供版本化、确定性、有界、
全量验证后原子应用的独立插件 snapshot capture/restore 合同；不接入 DARKWELL save slot。

必须实现：
- plain versioned snapshot structures；
- deterministic canonical capture；
- approved compression/checksum/limits；
- floor/subject/provider stable-ID mapping contract；
- validate-before-apply atomic restore；
- explicit result enums/diagnostics；
- restore revision publication and derived GPU/proxy rebuild；
- corrupt/future/oversized/duplicate/missing-floor/missing-provider/no-partial tests；
- exclusion of live/source/GPU/cache/reveal/transient state；
- contract、final validation、handoff docs。

必须冻结：
- M2 CPU authority；
- M3 GPU presentation-only/fail-black；
- M3.5 packed HardMemory、Coarse 25 cm、modifier ordering；
- M4P1 policy/descriptor/proxy semantics；
- M4P2 Shipping/performance gates；
- no-v6-fog-import。

明确非目标：
- Source/Darkwell integration、UDarkwellSaveGame/Subsystem、slot/menu/level load；
- Adapter/authority switch；
- damage-source gameplay trigger/reveal visual policy；
- L_Prototype 或 integration map；
- .uasset/.umap/Darkwell.uproject；
- Shader/Config/uplugin；
- skeletal/VFX/light/audio/material snapshot expansion；
- legacy deletion；
- 未授权 Cook/Package 重跑。

建议可靠 checkpoints：
1. docs: define SightWeave M4P3 persistence contract
2. feat: capture deterministic SightWeave snapshots
3. feat: validate and restore SightWeave snapshots atomically
4. feat: restore SightWeave subject persistence records
5. test: validate SightWeave persistence failures and lifecycle
6. test: close SightWeave persistence packaging boundaries
7. docs: record SightWeave M4P3 final validation

验证矩阵：
- focused NullRHI persistence/negative/lifecycle；
- focused D3D12/SM6 derived rebuild；
- M3.5 and M4P1 focused regressions；
- final full SightWeave NullRHI + D3D12/SM6；
- full Darkwell NullRHI；
- DarkwellEditor Win64 Development；
- BuildPlugin + clean-host Editor/Game Development/Game Shipping；
- severe-log、diff、generated-dir、Shipping isolation、Git/LFS closure。

人工 PIE：不是 M4P3 completion gate；可以做可选 Lab visual smoke。

若任何实现需要修改 Source/Darkwell、Darkwell.Build.cs、Darkwell.uproject、
.uasset、.umap、Shader、Config、uplugin 或重新定义 frozen contracts，立即停止并报告。

最终报告必须给出 COMPLETED/PARTIAL/BLOCKED、分支、baseline/final SHA、
提交/推送、schema/limits、自动化/构建结果、未运行项、diff/asset/uproj/LFS状态、
恢复命令。完成后停止，不开始 M5/M6。
```

## 25. 最终路线图排序建议

在用户批准第 23 节决策的前提下，建议的单线顺序是：

```text
M4P3 persistence/atomic restore
    -> M5 remaining authoring/validation/documentation closure
    -> M6 dedicated DARKWELL integration map + Adapter + authority switch
    -> M7 L_Prototype/approved successor production acceptance
    -> separately approved M8 legacy deletion
```

这不是对旧路线图的改写授权，而是本次审计基于仓库现状提出的排序建议。每个大型方向仍必须由独立任务、独立分支和明确授权执行。

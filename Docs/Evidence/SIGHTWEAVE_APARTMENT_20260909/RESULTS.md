# Apartment Lab 验证证据（2026-09-09，公司）

起点/远端核对：`87f9a96e5c99f3ffe1c6fa274c41b853dfe56112`，branch `codex/darkwell-prop-memory-gameplay-lab`。这是自动验证与证据采集，**不是人工 gameplay/视觉验收 PASS**。

## Build

`Scripts/BuildEditor.ps1`：DarkwellEditor Win64 Development 目标成功，非 Live Coding。最终构建 7.25 秒；日志 `EditorBuild.log`。保留引擎 GetMovementBase 弃用警告和工具链版本提示；没有项目编译错误。

## 自动测试

最终命令：

```powershell
Scripts/RunUnknownPartialCutTests.ps1 -RunName ApartmentValidated -Tests 'Darkwell.Apartment+Darkwell.BlackRegion.Contract+Darkwell.UnknownRegion+Darkwell.UnknownPartial+Darkwell.SightWeave.Closure.VisionIlluminationBoundary'
```

D3D12/SM6：**9/9 通过**，0 failed、0 not-run、0 severe；8 clean + 1 warning，warning 为 google.com/generate_204 HTTP 超时，非 SpawnActor/lifecycle。详见 `automation-summary.json`、`automation-report.json`。

新增 `Darkwell.Apartment.LayoutDoorsAuthority`：初始零历史、3 扇实际原生门、16 条遮挡线、SightWeave/P4 active；每扇门重复两次关闭→实际门铰动画打开→关闭，CPU query 与 P4 一致、重关立即遮挡；长墙阻挡；客厅环境光独立于随身光、不能扩大视锥；暗卧室远于 awareness 时需要合法光；既有 Trigger Activate/Deactivate 重复幂等。

已有合同覆盖 Unknown Whole / SpatialPartial、样本 Clear/Block、37° 和时间边界。本轮没有运行全历史插件矩阵，不把 9 项定向回归称为全部 SightWeave 测试。

调查中保留的失败：

- ApartmentInitial：卧室初稿 650 cm 超过现有 BlackRegion 最大 640 cm，Activate 被正确拒绝；将房间隔墙和目标范围共同设计为 640 cm，没有扩大上限或改 Knowledge。
- ApartmentFinal：新照明断言未过，因为合成测试世界只有 Pawn，没有 Controller，夹具 GetPlayerPawn 返回空而未注册环境光。补上 Controller/Possess 后 ApartmentValidated 全部通过，未放宽断言。

## 真实 D3D12 PIE

`Content/Python/verify_apartment_sightweave_lab.py` 在真实 Editor PIE 启动 `/Game/Maps/L_SightWeaveApartmentLab`，最终输出 `Saved/ApartmentPIEFinal`。正常 EndPIE/Editor quit；`PIE-complete.txt` 保存边界说明。

实际路径日志：P4 488×408、2.5 cm/texel、cachedSegments=16、oldSightWeaveVisual=Suppressed。每个采样阶段旧 HUD `FogTexture=None`、`FogCompositeMID=None`，角色 Legacy Visibility tick=false，存在 ObjectMemoryScene 和 3 个 DarkwellDoor。日志和完整 Actor/Component/材质绑定清单在 `runtime-evidence.zip`。

截图序列（1280×720）：

1. `01_entry_closed.png`：玄关初始观察，关门阻挡门后。
2. `02_living_observed.png`：客厅合法观察。
3. `03_bedroom_observed.png`：卧室建立观察。
4. `04_room_remembered.png`：离开后保留灰色。
5. `05_blackout.png`：Trigger Active，卧室已观察内容 Clear。
6. `06_blocked_live.png`：Blocked 区域仍有合法 Live。
7. `07_blocked_away.png`：离开合法观察仍 Unknown。
8. `08_deactivated_no_restore.png`：Inactive，不恢复旧灰。
9. `09_reobserve.png`：新合法观察恢复当前显示。

脚本使用玩家定位及现有 Trigger API；为了固定截图朝向暂时停止 Controller cursor steering，结束前恢复。它没有用 WASD 完整走通路线或键盘按 F，不能替代人工 F/门洞通行/视觉检查。C++ 门测试使用真实 Door.Interact 与门铰 Tick，但也不冒称人工按键验收。

墙体/历史边缘存在可见阶梯、时域颗粒；本轮只保留记录，未改变材质、AA、ownership、cap、合法采样精度。BlackRegion 沿墙轴线定义，跨边界的 SpatialPartial 墙体仍按原样本规则保留区域外部分，不强制整墙 Clear。不存在“所有边缘已无瑕疵”的验收结论。

最终 PIE 没有新的 SpawnActor、assert、ensure 或 lifecycle warning。诊断 `obj dump` 会触发 console lookup 提示，另有既有 LabRoute 查找提示；引擎启动自测的 Condition failed 日志在进入 PIE 前，不能误算为本轮 gameplay assert。

## 资产与旧地图

新 umap 只通过官方 Unreal Editor Python 创建/保存，Git LFS 跟踪。L_Prototype.umap 与起点完全一致；因冻结性能 Reference 仍依赖它而保留。默认地图/启动地图/README 日常入口切至公寓，Reference 回放增加显式 Legacy opt-in。`legacy-references.txt` 为引用审计索引，历史文档不改写。

stable/sightweave-blackout-event-volume-20260908 远端解引用仍为 `c4a6e83c44c3a61e0c8e7413ef378cee2258bf94`，未移动/覆盖。

下一步：用户人工试玩公寓，验收实际 F 交互、门洞通行、墙角/家具边缘和 Blackout 流程。本轮不继续 AA 或其他功能。

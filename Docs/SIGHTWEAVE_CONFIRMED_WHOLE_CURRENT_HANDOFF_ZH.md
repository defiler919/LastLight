# Confirmed Whole 当前显示行为修正

状态：**READY_FOR_USER_CONFIRMED_WHOLE_CURRENT_RETEST**。本轮产品行为及自动功能验证完成，等待用户复测；既有整体性能和间歇退出阻塞继续保留。

本轮从 `713fb290314443101c2cf2aa9a5e5560e27e2484` 继续同一开发分支。按用户本轮产品决定，取代上一轮“墙体逐部分裁切已确认 Current”的行为。

显示行为及原生测试提交：`7f646a033cbe43d75b6d8adcd8b56890e7d0e345`；最终 C++ 提交 `0eb5a902a81f796c4c63630635decc5f489b1dd2`，均已推送。后者增加一处按需诊断修正：细把手的保守几何栅格与五点贡献采样有 8 个外边缘样本差异，即使源纹理已统一，也会误报 VIEW_EDGE。现在统一源纹理不报告空间切线，两种原始采样统计仍保留，没有篡改为同一个掩码。

## 产品规则与实现

视锥、距离和实体墙的视线遮挡仍决定合法接触和配置跨度确认。未确认时继续局部显示；完全没有合法接触不能确认。

确认后，只要仍有合法接触，所有注册的对象部件使用同一个对象级显示值。视野边界和玩家到墙体的视线边界不再逐部分切掉对象；保留既有整体渐显。失去全部合法接触后关闭 Current，按 HistoryMode 封存完整几何：Always 可记录，StationaryOnly 仍要求合格静止观察，Never 不留灰影。

`ADarkwellObjectMemoryScene::UpdateTracked` 在原有合法查询、跨度确认和 revision 校验之后调用 `FDarkwellCurrentLiveGrid::AdvanceConfirmedWhole`。该函数由原来的完整显示通路收敛而来；删除旧的 Whole 墙体显示栅格和第二次逐点门控，避免两种 Current 语义并存。逐帧端点没有合法接触、但旋转区间有合法观察的路径仍只能封存合格历史，不能显示视野外 Current。

原始 `CurrentLegalCoverage` 原样保留；没有修改 Fog/SightWeave 的遮挡查询、发布源、世界探索或其它对象确认。Whole 历史仍使用完整注册几何和 `InitializeWholeCapture`，兼容的再观察仍复用原记录、代理和纹理。SpatialPartial 的局部栅格、历史、封口及反证处理不变。

实际源网格继续绑定原来的 Masked 材质及其正常深度测试；没有修改材质资产、深度开关或添加透墙绘制。05 房间说明同步改为蓝柜确认后完整显示、黄柜维持局部策略。旧交接保留为历史记录，当前语义以本文、`OBJECT_MEMORY_INTEGRATION.md` 和 `DECISIONS.md` 为准。

## 验证边界

普通宿主 `Darkwell.ObjectMemory.WholeReobservation` 增加确认首帧的实际提交纹理检查、完全被墙挡住的前后状态、被挡住的独立邻居、世界覆盖不扩张、Masked/深度材质绑定，以及 Always / StationaryOnly / Never × 静止/移动矩阵。继续覆盖 30/60/120/144 Hz、200 ms、大幅及正常转头、首次与重复历史、独立完整观察参考。

修复前 `WholeCurrent_BeforeFix` 失败，5 种步长的首次确认及稳定 Current 各有 12,072 个缺失样本；合法接触和历史断言通过。初次测试构建的 const/弱指针类型错误已修正，日志保留。

首次完整运行 `WholeCurrent_Functional` 138/141 通过、3 失败、0 severe、进程退出 0。两处是旧产品语义断言（旧房间文案与墙后局部 Current）；另一处是新增模式矩阵只改 authoring 字段而没有重新注册 Policy，仍使用旧的已解析策略。按组件现有契约修正测试设置，更新旧行为断言；未修改插件策略解析或放宽非法接触断言。

标准构建 `Saved/Logs/WholeCurrent_FinalBuild.log` 成功，21.37 秒。随后在同一版本上完整重跑 `WholeCurrent_FinalFunctional`：**141/141 通过**（131 clean、10 warning、0 failed/not-run/severe），测试 165.487 秒、进程 186.189 秒、退出 0。不是将第一次结果与补跑合并。覆盖普通宿主、整个灰对象策略及 Whole 边缘测试、世界覆盖、Partial/反证/封口、隐藏移动、Never、重观察复用、销毁替换、Reset 与 50 次生命周期等。

包含薄部件诊断修正的最终构建 `WholeCurrent_DiagnosticBuild.log` 成功，14.08 秒；**同一最终 C++ 再完整重跑 `WholeCurrent_FinalFunctional2`，141/141 通过**（132 clean、9 warning、0 failed/not-run/severe），测试 165.674 秒、进程 186.685 秒、退出 0。warning 为原有预期拒绝、元数据地图重复清理和引擎连接探测等；没有屏蔽产品断言。

前两次图形启动 `WholeCurrent_Visual` / `Visual2` 在初始截图前因脚本调用非反射 C++ getter / 不可读取的私有 transient 属性退出。改为通过已公开的 actor/mesh/material/texture Python API 检查真实绑定；这两次均不计图形通过，完整失败日志保留。第一次进程另有既有的退出 `0xC0000005`，第二次进程退出 0。

`WholeCurrent_Visual3` 完整采集 70 张 2233×911 原图，D3D12/SM6、SP100、AA4，Stop PIE 完成、退出 0、0 severe，55.975 秒。原历史分析器 192/192 通过。已打开当前、H1、第二次退出及相机深度三张对照原图。新 Current 分析器初次未通过：把源 actor 的非记忆装饰网格默认纹理也纳入 uniform 检查、把保守边缘采样差异当成缺失，并错误要求玩家/火把位置变化后 RGB 光照仍相等。采集改为按真实 SourceBindings 逐一定位原始部件；几何统计与独立直接完整参考比较，原图用旧缺失区的蓝色前景验证（修复前 0%，Visual3 为 95.9%，余下变化含光照/边缘），相机深度仍以墙恢复前后的原图差分验证。保留失败结果，不将 Visual3 的初版 Current 分析称为通过。

`WholeCurrent_NormalTurns` 在最终 C++ 上以 280°/秒转头，完整采集 70 张原图。最终 Current 分析 **73/73**、历史分析 **192/192** 通过；这些是采集内比较数，不是独立 UE 测试项数。覆盖每张已确认 Current 的三部件真实源纹理、合法接触、完整贡献、原图旧缺失区、独立完整参考、H1/H2 首次与重复退出，以及相机遮挡/仅隐藏墙渲染/恢复墙的正对照。已打开关键原图。进程 61.112 秒、0 severe、完成采集与 Stop PIE 后退出 `0xC0000005`；画面通过，退出协议仍失败，未将其藏在正常退出的 Visual3 结果里。

`WholeCurrent_Contracts` 完成三次真实 Play/Stop、178 张原图和全部脚本断言；01 首次退出图像检查 **24/24** 通过。已打开 01 首帧灰影、05 受限接触的完整蓝柜、03 重新观察后的端点灰影及 02 局部外切口原图；黄色 Partial、StationaryOnly、Never、Reset 的原有路径通过。进程 74.942 秒、0 severe，采集及 Stop PIE 完成后仍退出 `0xC0000005`。本次未重跑独立的 Room02 Episodes 长序列、插件独立打包或压力测试，不借用之前结果宣称本轮重新验证。

资源对照：旧 `Reobservation_Fixed01` 从墙缝走近时蓝柜源纹理累计创建数 3→6，四轮后绑定已更换为 Texture2D_36/37/38；新 Visual3 三张源纹理保持 Texture2D_7/8/9，最终 NormalTurns 保持 Texture2D_10/11/12，确认后的源纹理均为 1×1，远近往返和四轮重复不再因遮挡模式切换重建。历史分析同时验证 epoch/proxy/texture 复用。这个结果不等于全进程零分配、内存零增长或性能验收，本轮未重跑完整帧计时和长时压力协议。

## 复现命令与证据

```powershell
.\Scripts\BuildEditor.ps1 -Configuration Development -EngineRoot 'D:\UE_5.8'
.\Scripts\RunGrayObjectPolicyTests.ps1 -RunName <唯一名称> -Tests <完整 selector>
.\Scripts\RunGrayMemoryAudit.ps1 -RunName <唯一名称> -Protocol Reobservation
python Scripts/AnalyzeConfirmedWholeCurrent.py Saved/ArchitectureAudit/<名称>
python Scripts/AnalyzeGrayReobservation.py Saved/ArchitectureAudit/<名称>
.\Scripts\RunGrayMemoryAudit.ps1 -RunName <唯一名称> -Protocol Reobservation -NormalTurns
.\Scripts\RunGrayMemoryAudit.ps1 -RunName <唯一名称> -Protocol Contracts
python Scripts/AnalyzeGrayWholeTransitions.py Saved/ArchitectureAudit/<名称>
```

完整 selector 沿用 `Saved/GrayObjectPolicy/Reobservation_FinalFunctional2/Reobservation_FinalFunctional2.summary.json`。新日志、每次运行源代码差异和原始截图均保存在独立命名的 Saved 目录，不覆盖之前证据。

相机深度实验在连续四步及独立参考完成后进行：玩家固定在墙北侧合法观察，镜头移到墙南侧、正对蓝柜；依次拍摄墙正常、仅临时隐藏墙渲染作正对照、恢复墙渲染。实体遮挡段不变，墙渲染在 finally 中恢复，未保存任何资产。

## 用户复测

Play 后在大厅 F 进入 05，从墙缝观察右侧蓝柜，达到确认跨度时当前应整件显示。保持少量合法接触时不得重新切成一角；转开后灰影完整；走近、远离、重复转头也应一致。黄色柜仍只保留合法观察过的局部。完全被墙挡住且从未合法观察的对象不得出现或确认；相机前的墙仍正常遮住对象。

已有完整帧性能、批量尖峰和间歇 Editor 退出异常属于独立未关闭项。本轮不建立最终灰层 stable，不宣称完整系统性能或用户验收完成。沿用实际 UE 5.8.2 CL56702186（D:\UE_5.8），没有升级引擎或修改插件、地图、材质等二进制资产。

交付继续 `codex/darkwell-prop-memory-gameplay-lab`。两条 stable 引用与空 stash 未动，LFS fsck 正常。最终交付提交在本文所属 Git 提交及答复中标明；测试进程退出后重新打开 Lab，PIE 保持停止，电脑保持开启。

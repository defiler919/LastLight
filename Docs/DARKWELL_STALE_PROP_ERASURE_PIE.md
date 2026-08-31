# DARKWELL 灰色记忆残影重新验证实验

基线：`b3bafd34ddb1c2b0431f287dc5b154bdeb05f4df`。
分支：`codex/darkwell-prop-memory-gameplay-lab`。
只使用 `/Game/Maps/L_ProjectFogPropGameplayLab`。本实验不选择最终 Mode，不决策 Policy 1，不改变正式地图或 SightWeave 插件。

## 人工 PIE 顺序

1. 打开上述 Lab，Play。控制台命令必须在活动 Lab PIE World 中执行。
2. 先执行 `Darkwell.PropLab reset`，等待地图重新进入 PIE。
3. 依次运行下面三条命令，每条等约 36 秒自动结束，再执行下一条：

```text
Darkwell.PropLab stale 0
Darkwell.PropLab stale 1
Darkwell.PropLab stale 2
```

默认均为 C：长柜从画面左向右扫描。不能在一轮中途改变 Mode/Policy/工具/敌人或路径；重新运行 `stale` 会重新创建相同初始实验。`reset` 随时返回干净 Lab、满生命、满 Torch、固定武器状态。`help` 显示命令。

其他案例只加一个字母，例如 `Darkwell.PropLab stale 2 F`。每个案例都要以相同字母运行 0、1、2。

| 案例 | 事件与检查 |
|---|---|
| A | 60×50×45 cm 箱子，视野外销毁，扫描旧位置 |
| B | 82×76×190 cm 冰箱，视野外移到 B，扫描 A |
| C | 1400×110×90 cm 单一长柜，从画面左向右扫描 |
| D | 同 C 几何与位置，反向扫描 |
| E | 同 C，中点暂停时做两次 ±4° 往返 |
| F | 1000×110×90 cm 长柜，复用 Lab 现有半高墙遮挡；未观察区域应保留 |

所有移动目标的 B 均为 `(3000,3000,0)`，在本次路线有效视野范围之外。原 Lab 家具在实验期间隐藏并临时关闭碰撞，结束后恢复；实验目标和残影均为临时对象，不修改地图资产。无默认敌人，不显示 HUNTING。实验每帧固定 Torch 100、热量 0、装弹 2；每轮开始和结束恢复满生命。

## 确定性时序

- 0–6 秒：真实观察初始物体；三种 Mode 使用相同的整体物体显示，先建立真实记忆。
- 6–8 秒：视野朝外，完整灰色记忆保留。
- 8 秒：冻结已观察到的 A 快照，视野外销毁或移动真实物体。
- 8–10 秒：继续朝外，不能清除残影。
- 10–20 秒：固定玩家位置和相机，Torch 恒速横扫至中点。
- 20–22 秒：暂停；E 增加小幅往返。
- 22–32 秒：继续恒速扫过剩余区域。
- 32–36 秒：再次朝外，检查已清除区域不恢复；F 未验证部分仍在。
- 36 秒：记录结果，销毁本轮临时实验对象并恢复稳定 Lab，显示本轮结果。重置产生的新一轮对象不是旧残影恢复。

F 初次观察从半高墙开放一侧进行，6 秒后切换到遮挡侧；三种 Mode 的切换位置、时刻完全相同。扫描阶段相机没有平移。

## 独立空位证据

`FDarkwellEmptyVerification` 把冻结快照的世界 XY 占用包围盒分成不大于 10 cm 的格子，每格检查四角和中心。五点都必须达到 **连续合法 LiveCoverage ≥ .99**，且该格没有任何碰撞启用的真实实验家具占用，连续确认 **.10 秒**后永久记录为空。单帧计入最多 1/30 秒，卡顿不能一次提供全部确认时间；确认前失去覆盖会重置该格的短确认计时。

LiveCoverage 来自现有 DARKWELL Fog 的合法光和遮挡判断，不是材质像素；没有修改插件权威。确认后的格子只增不减，离开视野不会重现。实物身份、显示门限 `.50/.25`、StableID 已移动或销毁的内部知识都不能提供空位证据。不同 StableID 的家具占据同一格也会阻止该格确认。

这是供用户比较的 **2.5D、有限采样、占用足迹**合同，不是完整三维体积可见性证明。一个格子确认后会擦除该格对应的垂直表面列；10 cm 采样会形成可见小台阶。采样范围、确认时间和“全部格子”标准均为本轮明确的实验参数，不代表最终玩法参数。F 的保留区域用于直接揭示当前遮挡合同的影响。

## 三种 Mode 共用证据

| Mode | 灰色残影的消除合同 |
|---|---|
| 0 | 所有格子均确认，即已验证比例 **100%**，才整件一次消失。只看到边缘不能整件清除。 |
| 1 | 确认的格子立即擦除，未确认区域保持灰色；因此允许用户评估“半个柜子残影”的体验。 |
| 2 | 与 1 完全相同的确认区域和时刻，每格确认后约 **.20 秒**短渐隐，随后永久消失。不会随着视野离开而恢复。 |

本轮局部渐隐与上一轮实时材质的柔化是两种不同实验。当前 `PropPresentationMode` 数字在专用 `stale` 路线内选择残影消除方式；初次观察的普通家具显示强制相同，避免混入上一轮对照。Policy 始终锁定 0。原有 `.50/.25` 仍只负责正常真实物体是否 Live，不能清除冻结的 A。

灰色残影采用新增的 Lab 专用不受光透明材质，沿世界坐标取累计空位掩码，保留固定几何明暗。无 CustomStencil、屏幕合成、长时间画面历史、阴影或动态光照输入。Mode 2 的短暂透明只表达已确认的记忆消除。

HUD 显示 Mode、Policy、案例/时间、阶段、StableID、已验证比例、物体级 empty 是否成立、残影是否存在、原因、ENEMY 0。`VerifiedEmpty` 与 Mode 2 残影可能共存最多约 .20 秒：权威已经确认，视觉渐隐尚未结束。

## 验证记录

**PARTIAL — READY_FOR_USER_STALE_PROP_ERASURE_PIE**。从 `f667527` 及保留工作树恢复后的 C 单路线重复验证、三模式同轨对照和真实 Editor PIE 已通过。完整记录以 `Docs/DARKWELL_STALE_PROP_ERASURE_HANDOFF.md` 顶部的恢复记录为准，下方历史失败继续保留。

- `StaleResumeProbe01` / `StaleResumeRepeat01`：Mode 0 / C 两次 1080 帧元数据和格子 hash 完全一致；随后 C 三模式各 1080 帧的时钟、实际朝向、相机、事件和空位证据一致。D3D12/SM6、1080p、TSR；不是重新批量运行 C/F 六条。
- 实际打开并排图（25% / 50% / 75%）、相邻连续帧和原始帧。Mode 0 保留完整残影至约 23 秒全部格子确认，Mode 1 局部擦除，Mode 2 同一区域约 .20 秒渐隐；离开视野不恢复，B 无泄漏。
- `StaleResumeBuild01` 标准 Editor Development 构建成功（up to date / 0 actions）；`StaleResume01/index.json` 21 项通过、0 失败，1 项预期警告。
- `StaleResumePIE03` 真实 Editor PIE：三模式完成、固定相机与 route/world 时钟、禁止敌人和 Policy 1、每轮恢复家具、受伤/切暗后的 reset 全部通过。该可变帧率 PIE 仅验证生命周期，不宣称逐帧结果等于固定 30 fps 采集。
- 最新采集和最终 PIE severe=0；引擎启动 Condition failed、Experimental Toolsets、NavMesh/Crowd、CVar/渲染线程和工具链警告仍保留，不能报告日志零错误。A/B/D/E/F 和 1440p 本次未重采，旧失败 C/F 不被追认。

自动证据用于发现实现错误，不能替代用户对实际动态 PIE 的最终体验判断。最终 Mode / Policy 1 尚未决定。

失败记录保留在忽略的 `Saved/PropGameplayLab`：

- `StaleBuild01`：首次编译使用 `auto*` 遍历 `TObjectPtr` 导致类型推导失败，已改为显式组件指针；本轮新增浮点截断警告已修正。
- `StaleMaterial01`：默认纹理与 Linear Grayscale 采样类型不符；已重新建立专用灰色材质。`StaleMaterial02` 在加载旧失败资产时仍记录一次旧错误，其后重建保存，最终运行必须无材质编译失败。
- C 盘约剩 1.6 GB，默认 Zen 返回 HTTP 507。后续验证使用进程参数 `-ZenDataPath=D:/UE_projects/LastLight/DerivedDataCache/StaleLabZen`；不修改全局缓存设置、不删除旧数据。
- `StaleSmoke01`：旧普通 SurfaceSweep 分支可能影响初次灰色记忆阶段，已隔离专用实验的身份显示；该轮不作为最终公平对照。
- `StaleAutomation02`：新增测试把观察原点（主体中心 Z=45）误当作 Actor 地面原点 Z=0；改为比较实际观察快照原点，随后 21 项通过（20 无警告、1 原有预期重复 Fixture 警告）。
- `StaleFinal01`：逐帧 hash 对照发现实际朝向受鼠标输入干扰，不能作为公平证据。专用路线现占用内部 route 14，复用已有 PlayerController 的 Lab 路线输入保护；不修改正式控制器，也不重写命令 World 解析。重新采集后才可交付。
- `StaleResumePIE01` / `02`：新检查脚本先把 `ghost=0` 误识别为时间，后在实际 9.995548 秒 / HUD 10.00 的边界误判朝向。分别修正字段边界及 HUD 舍入区间；2075 帧离线回放通过后 PIE03 完整通过。原生 .01° 全精度朝向诊断未放宽，运行时代码未因此修改。

复现验证工具：`Scripts/RunStalePropLab.ps1`，`Scripts/ReviewStalePropLab.py`。截图、视频、报告和缓存全部留在忽略目录，不提交。

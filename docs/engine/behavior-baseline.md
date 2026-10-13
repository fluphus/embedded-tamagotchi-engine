# 行为基线（原始依据及 P4 对照）

依据为基线提交 `448dbe158595c1db9f1d2fc37caa15810c53de1c` 的代码；
可用 `git show <提交>:<路径>` 复核。以下 P1 推导和 P0 源码调查保留为历史依据；
“保留”不代表硬件运行验收。P4 扩展验证见文末及 acceptance.md。

## P1 预期从旧实现推导

| 场景 | 旧实现依据 | 预期与测试 |
| --- | --- | --- |
| 正常 poke | seriousness.cpp::onInteract，game_config.h 的 POKE_SERIOUSNESS_DELTA=15 | SR 50→35；更新 last_interact_time 与 last_poke_effect_time，暂停至 now+1800，余数不清零 |
| 普通投喂互动 | onInteract 的 INTERACT_FEED 分支始终扣 SERIOUSNESS_INTERACT_DELTA=15 | 35→20，不检查 poke 冷却，不启动/延长暂停；完整投喂的食物/连携效果在调用之前，不能把本切片的 -15 当作完整投喂净变化 |
| 冷却重复 poke | `last_poke_effect_time==0 || now-last>=1800`；暂停写入在判定之前 | +0/+1/+1799 秒重复不再扣 SR；每次都更新互动时间和暂停截止，生效时间不变 |
| 冷却边界 | 同上，`>=` | +1800 恰可再次扣 15；+1801 同样可扣；依据上次生效时间而非上次尝试 |
| 暂停边界 | onIdleTick 使用 `now < idle_paused_until` | 截止前 1 秒不累计余数；截止当秒可累计，旧余数=4 时立即 +1 并归零；到期后同样可累计 |
| 时间调用约定 | tick 每调用一次累计一个分钟；batch 从 `currentTime-minutes*60` 开始，每轮先加 60 | tick 不自行测 elapsed；测试 +1801 的额外 tick 仍算一个分钟，不能当作“真实一秒涨一分钟”新玩法 |
| 长推进 | onIdleBatch 保留逐分钟判断暂停 | T 时 poke 后从 T 模拟至 T+3600：分钟 30..60 共 31 个有效 tick，35→41、余数 1；再推 1440 分钟封顶 100 |
| 数值边界 | clamp 限制 [0,100]，每 5 分钟 +1，未喂满 +10 | 测试起始 SR 0/1/14/15/16/99/100、扣减封底、idle/日结封顶；不承诺对任意损坏 int16 状态的纠错 |
| 分级 | getTier：>32、>65、>=100 | 0..32 LOW，33..65 MID，66..99 HIGH，100 MAX；逐一断言边界 |
| 白狮计时 | updateRhongoTimer 仅白阵营成体且未终态；100 开始，持续 172800 秒触发 | T 开始，T+172799 未触发，T+172800 进入白狮且只发一次触发诊断 |
| 白狮安全跌落 | SR <80 才清除 timer | feed 从 95→80 保留 timer，从 94→79 清零；验证 Reset 日志及日结 before/after 值 |
| 终态 | idle/interaction 两种终态都提前返回；applyMissedFeedPenalty 仅排除白狮 | 两终态均不响应 poke/tick/batch；直接调用惩罚时白狮不变、黑狮 +10，原有差异保留 |

## P0 调查：其他玩法与流程的源码基线

- `feeding.cpp`：每日最多 3 次、间隔 1800 秒；抽 4 选 3。时间窗按小时为 [8,9)、[12,13)、[18,19)。
  幼体连携影响 HP，成体根据阵营影响 SR；幼体喜欢食物依投喂后 HP≥50 判白线。
  特殊食物在 applySpecialFood 内判麻婆（10%），累计 3 次黑狮；Oda 麻婆计数锁 1。
- `evolution.cpp`：年龄≥3 日毕业，HP≥50 白线，否则黑线；成体按 SR 分级切形态，白娱乐形态随机后锁定。
  Nobu 不自然毕业，吃一次麻婆成长 Oda；Nobu/Oda 路线提前返回普通进化检查。
  `main.cpp::doReset` 和菜单销毁路径含概率：通常 10‰，prevAgeDays==5 为 250‰，轮次 329→330 强制 Nobu。
- `main.cpp::doDayEnd`：清暂停截止，批量补 1440 分钟，终态时跳过普通日结；否则窗口奖惩、未喂满严肃值惩罚、年龄与毕业/进化、重置每日投喂并保存。
  `skipTime`：按完整日循环 doDayEnd/advanceDays，再补剩余分钟；剩余分钟路径可从上次生效 poke 推导恢复暂停。
- `setup`、设时回调/串口：首次新宠确认时间后重建时间基线；深睡使用 RTC 推进及离线补偿，断电复位等待用户设时。
  模拟时钟是 epoch 秒 + millis 已过秒数 + 手动偏移；`SIM_MINUTES_PER_REAL_SEC=0`，当前不自动加速。
- 图鉴按 FORM_COUNT=12 存 bool 数组，进化/初始化等入口保存解锁；队列有等待输入节点与页面停留时长。
  演出依赖 PetState/特殊形态，不能仅移动目录就称通用化。
- `save_manager.cpp`：三个物理槽，默认活动 0/1；通常覆写活动槽较旧序号，读回验证成功才推进序号，验证失败粘滞重试。
  load 优先尝试较新活动槽，失败回退；仅一个有效头时会重建活动槽对后尝试读取。
- `main.cpp`/`save_manager.cpp`：完整导入含图鉴，恢复宠物与图鉴；仅宠物导入进入串门，冻结主人槽，其余两槽轮转。
  恢复主人时对齐冻结期间时间戳、重置当天投喂；串门时拒绝再次导入与相关改时操作。
  `SaveManager/save_manager.py` 依据 READY 校验导入长度，每块 16 字节等 RECV 确认，最后 COMMIT。

## 原始 v9 与协议边界

- SAVE_DATA_VERSION=9；SaveHeader、PetState、GalleryData、DeviceState 均直接按内存布局处理。
  NVS `s0/1/2_hdr`、`s0/1/2_dat`、槽图鉴及独立 gallery/dev_state/slot_cfg 需要分别保留。
- checksum 不是标准 CRC：从 uint16 0xFFFF 起，对每字节异或、左循环移位 3、再加该字节（16 位截断）。
  校验覆盖 sizeof(PetState) 原始字节，包括编译器填充；不能用主机 sizeof 代替 ESP32 布局。
- 完整导出为 SaveHeader+PetState+GalleryData；宠物导出为前两者，皆 memcpy。
  `SAVE_EXPORT_BEGIN/DATA/END`、`SAVE_IMPORT_BEGIN/DATA/COMMIT/ABORT`、READY/RECV、槽状态协议保留。
- loadSlot 检查版本、data_size、读取长度和 checksum；importSlotRaw 还有语义合法性检查。
  当前格式没有内容包身份或独立稳定 ID 边界；当时的新格式及迁移规划属于 P2；当前结果见文末。
- P1 阶段 check_legacy_boundary.py 只证明 9 个关键文件与基线字节一致；**没有旧 v9 样本验证，不宣称旧档兼容已经验证**。

## P0/P1 历史调查：文档差异与原有异常

| 项目 | 代码事实/影响 | 状态 |
| --- | --- | --- |
| README.md/README_zh.md 宣称 TFT_eSPI | display_config 宏也沿用该名，renderer 实际 include Adafruit_GFX/Adafruit_SSD1351；platformio.ini 依赖 Adafruit | 记录，未改文档或后端名称 |
| README 宣称显示及串口占位均可编译运行 | 配置是硬编码 #define，并非命令行可可靠切换；P1 阶段仅尝试默认后端编译，无硬件运行证据 | 占位构建/运行未执行 |
| epoch=0 同时是“从未”哨兵 | 在 0 秒 poke 后，1 秒再次 poke 会扣 SR（生效时间仍被视作从未） | 主机特征测试保留 |
| 时间倒退 | poke/白狮计时使用无符号相减；now<last 时可绕过冷却，batch 起点也可能下溢 | poke 倒退特征测试保留；溢出范围的游戏政策待规划 |
| 白狮 SR 80..99 | 计时不清零，但只在 SR≥100 时检查持续时长是否触发 | 代码审查记录，未设计新规则 |
| 黑狮日结惩罚不对称 | applyMissedFeedPenalty 只保护白狮；当前 doDayEnd 外层通常已保护两终态 | 函数级特征测试保留 |
| RHONGO_RESET 枚举 | 定义为本 tick 重置，但返回值调用 getRhongoState，实际重置后得到 INACTIVE | 原有未实现语义，未改 |
| 漏分钟/超长补偿风险 | checkNewMinute 只比较分钟字段而非已过分钟数；批量为 O(minutes)，日结主动清暂停 | 代码风险记录，未做整机重放验证 |
| 坏档及主人恢复失败 | setup 所有加载失败可启动新宠；doReset 冻结主人读取失败有强制结束串门/重置访客分支 | 原有数据保护风险；P2 需明确拒绝/回退政策，P1 阶段未改 |
| v9 填充及写入原子性 | SaveHeader 未整体清零、结构体裸写；头与数据分别写入，故有掉电中间状态 | P1 阶段未改存档，不把源码保护当作故障注入验证 |

P1 阶段新增缺陷：当前验证未发现。该结论仅限主机严肃值测试及已执行的构建/静态对照范围。


## P2–P4 实现与行为对照

原规则迁至 content/artoria，公共机制提取至 engine/physiology；pet 入口是真实生产 facade。
run_compatibility.py 从上述 Git 对象提取原始规则，在主机提供只包含日志/随机声明的最小 shim，再与当前生产实现比较35条固定输入输出轨迹。
比较包括所有 PetState 字段及随机数调用次数，当前35/35完全相等；历史实现的 shim 仅用在对照侧，当前主机测试无需 Arduino 环境。
范围包括幼体食物/连携、窗口、成体阵营/分级、日结、poke/冷却/批量推进、白娱乐锁定、白狮、麻婆黑狮、Nobu/Oda。
这提供关键规则回归证据，不等价于对所有可能状态穷举。

run_tests.py 保留P1的7例91断言。run_engine_tests.py 又验证两包完整生命周期、日结/离线计划/首次初始化、冻结时间对齐、序号和有界队列、动画不结算规则。
run_storage_tests.py 调用同一生产 SaveManager 及注入的 KeyValue，验证真实轮转和失败分支。
资源重新生成后的 Artoria 214序列与现有生成文件逐字节一致（唯一外层编译选择 guard 已包含在比较中）。
现有相同像素资源、原形态0..11和食物0..7的身份保留；图鉴及查资源不再依赖固定12规模。

保留的已知语义/异常：epoch=0有效poke哨兵、时间倒退无符号差、日结先清暂停补1440分钟、导入时间对齐将未来暂停clamp至localNow、黑终态直接遗漏投喂惩罚与白终态不对称。
没有把这些改成新玩法。分钟推进仍O(minutes)，动画节点进入顺序沿用原实现。

明确的必要修复：原串口占位后端缺 AnimState 声明导致编译失败，增加显示类型include；有独立原始构建失败日志。
按任务书的数据保护要求，启动全坏/未知数据不再可能静默覆盖；串门主人恢复失败保留主人档与串门状态，停止原先销毁主人槽的失败回退。
未知16位食物ID现在明确拒绝，避免新契约经8位适配截断成有效旧食物；合法原始ID行为不变。
其他运行限制与未实测项见 acceptance.md，不将编译结果表述为实机运行通过。

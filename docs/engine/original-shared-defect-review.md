# 近几轮引擎修正与原版游戏的同源问题审查

> 后续维修已完成：本报告保留修复前的审查结论。当前原版状态、437条应用断言/11条Python协议断言及完整补丁见[原版最新交付报告](/home/omni/.codex/worktrees/original-save-repairs/esp32-artoria-tamagotchi/reports/original-repair/README.md)。下面的缺陷复现脚本及日志是历史证据，不用于验收已修复的当前原版；当前入口为原版工作树中的 `python tests/host/run_all.py`。

日期：2026-10-12。审查对象：

- 原始 Git 基线 `448dbe158595c1db9f1d2fc37caa15810c53de1c`。
- 原版维修工作树 `/home/omni/.codex/worktrees/original-save-repairs/esp32-artoria-tamagotchi`，分支 `codex/original-save-repairs`。这是保留 v9、没有拆分引擎的修补版，不是当前主目录。
- 当前引擎补充验收及 Moonberry 定向修复。审查不更改任何生产文件、不操作设备。

结论：**部分是原有问题，上一批存档维修已回移；最后一轮补充验收的若干公共修复尚未回移。Sprout 专属问题不适用于原版。** 不能把引擎最近的9 PASS当作原版已经覆盖同一范围。

## 原版维修工作树仍存在的问题

下列位置均指原版维修工作树；路径和行号可直接按上方目录定位。严重度是本次审查的优先级建议，不是实机故障率。

| 项目 | 优先级 | 来源及位置 | 本轮证据 | 影响及回移状态 |
|---|---|---|---|---|
| 三键销毁组合松开产生新的普通 PRESS | P1 | 原有；`src/input/button_driver.cpp:97`、`:216`，`src/input/input_manager.cpp:151` | **生产驱动复现**：真实GPIO三键按住触发、resetCombo、松键后输出3个PRESS；没有使用injectEvent。 | 三键确认菜单会接收这批新事件；L/M分别是移动和确认，可能使确认失去另一次按键的保护。当前探针验证驱动事件，未运行原版完整菜单完成销毁。原版未回移release抑制。 |
| 设时最终保存失败仍宣布正常运行 | P1 | 原有；`src/main.cpp:326`–`:370`；原始main同一路径在`:488`输出成功 | **源码确认**：先清waiting标志、修改时间/RAM；persistDeviceState结果丢弃；save失败仅不markSaved，随后仍switchContext/showSystemReady/输出entering normal operation。 | 原版已有导入/主人恢复guard不能保护这条独立设时路径；还包括首次设时期间图鉴写入。不能保证失败或中断后时间/RAM/存档一致。本轮未对原版此回调逐写点注入故障；引擎对应修复尚未回移。 |
| NVS打开失败被当作新游戏 | P1 | 原有；`src/core/save_manager.cpp:52`、`:698`；维修版`src/core/save_workflows.h:7`；原始main`:1798`、`:1811` | **生产启动服务复现**：已有有效主人档，init返回NVS错误1，loadStartup仍返回NewGame。 | 无法读取不应等价于空设备；普通启动继续新游戏/设时路径。当前未初始化管理器会拒绝写入，因此本探针没有证明已覆盖旧档，问题是错误启动状态及成功提示资格。引擎initialized检查尚未回移。 |
| Python导出缺失数据仍写备份、报成功 | P1 | 原有；`SaveManager/save_manager.py:198`（cmd_export） | **实际工具函数复现**：模拟串口BEGIN/END都声明84字节、DATA只有1字节AA；cmd_export写1字节文件并输出“已导出”。 | 不完整导出可能覆盖用户指定的已有备份文件。真实Python函数参与，未复制导出实现；UART为模拟，非设备实测。原版未回移长度核对及失败保留原文件。 |
| 单调毫秒累计一圈后日历倒退 | P2 | 原有；`src/core/time_manager.cpp:47` | **生产TimeManager复现**：每次增加100000000ms并调用now，第43步epoch从1708267200倒退到1704072232。 | 固定millis锚点累计达到2^32ms（约49.7日）后elapsed回绕；并非每次硬件计数越零都立即失败。可能影响日结、冷却和保存调度。引擎滚动锚点修复尚未回移。 |
| 页面hold/toast跨millis回绕提前到期 | P2 | 原有；`src/display/DisplayManager.cpp:139`、`:169` | **源码确认**：仍用nowMs >= deadline；例如now=UINT32_MAX−100、deadline=now+1000回绕成899，立即满足到期条件。 | 停留/提示会在边界处过早结束。仅源码和边界算术确认，本轮未运行原版DisplayManager集成。引擎有符号差值比较尚未回移。 |
| 存档sequence耗尽回绕，重启读旧档 | P2 | 原有；`src/core/save_manager.cpp:88`、`:145`、`:179`；基线相同加一逻辑在`:81` | **生产SaveManager/启动复现**：槽0主人HP50/seq UINT32_MAX；新HP73保存返回OK，槽1写seq0，重启加载HP50。 | 最新成功保存没有被选为最新。实际生产save/load，不是只测编码；只是主机故障夹具，不是旧设备档证明。引擎耗尽拒绝策略尚未回移。 |
| Python把超时半行当完整协议行 | P2 | 原有；`SaveManager/save_manager.py:55` | **实际_read_line复现**：两次readline分别返回SAVE_EXPORT_BE及GIN...，工具当作两行，不拼回BEGIN。 | 真实UART readline在超时可返回半行，造成标记/HEX/回执丢失。引擎工具缓冲半行修复尚未回移。 |
| 串口半行/导入会话无超时，超长行截断后执行；无偏移重试重复追加 | P2 | 原有；`src/main.cpp:880`–`:953`、`:1390`；`SaveManager/save_manager.py:287` | **源码确认**：行满后丢后缀但保留前缀执行；无接收/会话超时；DATA直接received+=wrote，Python丢ACK后重发同一分包。 | 会话/半行可长驻、后续输入拼入；丢ACK重试会追加重复字节、破坏长度或校验而失败。并不据此声称损坏宠物必被接受：COMMIT仍有长度/校验与生产guard。引擎协议加强尚未回移；本轮未完整跑原版UART协议矩阵。 |
| 年龄上界/离线上限缺少保护 | P2 | 原有；`src/core/game_state.h:118`、`src/main.cpp:603`、`:632`、`:732` | **源码确认**：age_days为uint16且日结直接++；skipTime按全部offlineDays逐日循环，无引擎366日/年龄容量预检。 | 年龄65535继续日结回绕0；超长离线执行耗时没有明确支持上限。本轮未实测原版最大离线耗时、看门狗或完整年龄轨迹，不把性能风险说成已经实机复现。新引擎限制属于容量政策，回移时需明确同样的支持上限。 |

TimeManager、ButtonDriver、DisplayManager、Python工具与原始基线逐文件`git diff --exit-code`结果0：这些遗漏不属于引擎拆分引入。main/save_manager已经做过前批原版维修，以上另对原Git对应逻辑核对，不能用整文件相同来证明。

## 已回移的原有问题

以下依据当前原版生产接线和已保留的原版回归报告；**本轮没有重跑其1684断言或两后端构建**。

| 引擎审计问题 | 原始是否存在 | 原版维修工作树现状 |
|---|---|---|
| 唯一有效访客分支把冻结主人槽重加活动对 | 是 | load/回退/写目标均排除冻结主人；已有真实生产回归。 |
| 仅gallery/device附属数据被当新游戏、未知/坏槽当空槽覆盖 | 是（未知包概念为引擎新增，原版同源问题是未知版本/坏记录） | 已保护原始键、启动恢复锁、逐槽筛选，并拒绝覆盖v10未知记录。 |
| 完整导入/仅宠物串门中途失败假成功、失败后仍危险写入 | 是 | 原版COMMIT调用save_workflow::commitImport，候选RAM、repair_guard与逐步错误处理已接入。 |
| 两个结束串门入口忽略持久化失败 | 是 | reset/销毁共用restoreOwner；原主人保持冻结，候选主人写另一槽，完成后提交；有29个导入/恢复中断点历史证据。 |
| 主人不可读的破坏性fallback及错误文案 | 是 | 已保留数据并提示owner_unavailable/需要恢复。 |
| 擦除失败仍提示Erased | 是 | 已检查返回值；原版历史回归覆盖擦除失败、导出主人及明确恢复闭环。 |
| 串口占位后端声明依赖导致无法编译 | 是 | 已修公共include和宏覆盖；历史两后端构建通过。 |

原版没有引擎的owner_restore独立副本，恢复流程保存原主人普通槽，既有SAVE_EXPORT可取回；因此不能把“仅剩owner_restore”的新引擎用例生搬到原版，也不能据此说原版恢复资料完全不可取回。普通v9轮转仍是多键写入，历史报告已明确不提供所有普通保存的跨键事务。

## 原版没有对应的问题

- Sprout Moonberry健康属性导致三选连携不可达：原版只有Artoria，食物表有足够健康食物，根本没有Moonberry/Seed/Bloom内容；不需回移本次内容决定。
- Seed40/Bloom47图鉴按数组位置强转Form：原版枚举连续0..11、固定图鉴范围对应，当前原版内容下不是这个缺陷；是引擎支持非连续稳定ID后的适配遗漏。
- Sprout积水uint16饱和、第二包扩展版本/内容身份/生成表容量：原版没有该状态和契约；其中与原版共享的sequence、年龄容量已经单列，不能归为全部不适用。
- 注入时钟、公共服务接口、演出契约拆分本身是架构迁移，不是一项待回移的原版故障修复。队列满拒绝原版已存在，新增测试/溢出标志不等于原来存在越界。

## 本轮可复查证据

```sh
python docs/engine/evidence/original-shared-review.py \
  --original /home/omni/.codex/worktrees/original-save-repairs/esp32-artoria-tamagotchi
```

退出0，[日志](evidence/original-shared-review.log)。脚本0的含义是**成功复现六项问题**，不是原版故障已经修复；六项为时间倒退、松键PRESS、sequence回绕、NVS打开失败启动、Python半行和导出短数据假成功。

脚本编译原版实际TimeManager/ButtonDriver/SaveManager/启动服务，外围只替换GPIO/millis/存储/串口；存档探针复用原版既有测试Store和seed，不复制生产判断。存档结构匹配既有目标夹具检查的ABI，仅用于运行这次sequence/startup测试，不能冒充真实设备旧档。Python探针调用原版实际_read_line/cmd_export。全部构建和输出文件位于自动删除的临时目录。

前批已修内容及历史测试见原版工作树的`reports/original-repair/README.md`及其evidence；本报告的源码确认项没有冒称通过原版运行测试。原版生产源码、引擎生产源码、既有补丁文件均未修改，也未重生成旧交付补丁。未检查真实UART/屏幕/掉电、动态资源或设备长运行。

建议下一轮原版维修优先处理销毁确认、设时成功条件、存储不可用启动和主机备份完整性，再处理回绕/协议/容量边界；需要在原版生产调用链补回归后才更新原版交付补丁，不应直接复制新引擎或v10格式。

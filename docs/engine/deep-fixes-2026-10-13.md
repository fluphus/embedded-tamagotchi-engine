# D1–D6 定向修复与回归（2026-10-13）

范围：主目录引擎工作区，Artoria / Sprout 两包。原审查见 [deep-acceptance-2026-10-13.md](deep-acceptance-2026-10-13.md)。本轮修复六个已确认问题，不增加玩法、不重新设计引擎、不修改独立原版维修工作树。未连接、复位、刷写、导入或擦除真实设备；板上仍是上一轮固件，新固件硬件验证未执行。本报告是执行证据，最终集中验收仍由规划模型负责。

## 修复与生产路径

| 问题 | 状态 | 修复与证据 |
|---|---|---|
| D1 时间锚点 | PASS（主机） | 原子宠物记录的 `savedAt` 为该快照的恢复基准；SaveManager 在成功加载/提交后更新此值，普通复位、断电设时及串口 `load` 采用同一基准，保留秒。独立设备时钟即使落后，也不会重复补偿。原分钟/跨日处理提取为 `processElapsedTime()`，在输入/命令保存前执行；日结规则本身未改。离线补偿、`d`、运行中的 `SET_TIME` 在现有持久保护标记内完成，最终提交保存与时间对齐的快照；中断后进入 Recovery。见 `clockProgress`、`timeOperationCuts`。 |
| D2 普通销毁 | PASS（主机） | 串口 `reset` 与真实菜单按钮共同调用 `app::resetPetChecked`。先写保护标记并保留旧宠物到 `owner_restore`；旧槽冻结，图鉴/设备态/新宠物写入及读回成功，完成标记落盘后才提交 RAM 和成功演出。任何中间失败不继续正常写入；重启只加载完整旧/新结果或 Recovery。见 `resetFaults`。 |
| D3 休眠门槛 | PASS（软件控制流） | `enterDeepSleep` 检查设备态写入与读回。宠物写入仍允许在粘滞目标上最多3次尝试；已提交宠物的读回验证失败立即终止，不轮转另一槽掩盖故障。`verifyLatestSave` 只校验本次成功写入的实际槽/sequence，不能以相同时间/宠物的旧槽替代。验证/设备态失败锁住后续自动写入，所有必要步骤成功才设置 RTC 标记并调用睡眠入口。见 `sleepGate`。 |
| D4 设时编辑 | PASS（主机按钮闭环） | 确认回调返回 `Accepted/Retry/Recovery`。超容量拒绝保持编辑状态，支持返回修改日期后再次确认；存储失败显示恢复提示，不能当作完成进入正常游戏。见 `setupButtons`。 |
| D5 图鉴持久化 | PASS（主机） | SaveManager 保留一个容量固定的待写图鉴副本。短暂失败后的再次解锁或宠物保存会先重试；未成功不能提交缺失图鉴的新宠物记录。成功宠物快照包含当前形态，不依赖稍后的演出回调才能补齐；全局图鉴写入读回验证。失败不显示“图鉴全部解锁成功”，手动保存不再忽略第二笔图鉴写入结果。见 `galleryRetry`。 |
| D6 串口推进 | PASS（主机） | 1..10080分钟按引擎声明的1440分钟上限分批执行；改变任何状态前校验范围与 epoch 容量。10081等超限值明确拒绝，不再静默截断；调试命令仍只执行原分钟规则，不新增逐日结算。见 `advanceContract`。 |

主要代码：`src/core/save_manager.{h,cpp}`、`src/app/{startup.h,import_workflow.cpp,workflows.cpp,ui_binding.cpp,console.cpp}`、`src/input/menu_controller.{h,cpp}`、`src/platform/arduino/lifecycle.cpp`、`src/main.cpp`。引擎只增加同一批量上限常量，没有新增角色分支或平台依赖。

## 定向回归覆盖

正式入口为 `tests/host/run_deep_regression.py` / `deep_regression_test.cpp`，已纳入 `run_all.py`。沿用集成测试的存储/GPIO外围适配器，另外替换 ESP 休眠接口为调用计数；编译真实规则、菜单、时间、应用工作流、串口命令和生命周期源文件。没有复制这些模块的实现作为测试目标。

- D1：正常宠物/访客各连续720次菜单互动、约27小时模拟推进，自动保存始终被互动延后；正常重启、实际串口加载及重复断电设时均不倒退已存进度、不重复结算年龄。原冻结主人字节不变。自动保存的 stage/宠物提交/设备态三个写点逐一中断；最新槽损坏时按回退快照自身时间恢复。正常/访客离线设时、`d`、运行中 `SET_TIME` 的每个实际持久写点中断均受保护。
- D2：串口/物理按钮状态机两个入口，各注入首保护标记、主人备份、图鉴、设备态、stage、最终宠物、完成标记失败；再逐一中断全部7个成功写点。失败时 RAM 仍是旧HP79/旧rounds，没有成功日志，旧记录原字节保留。首标记未写成功的重启加载完整旧状态；其他未完成写点进入 Recovery；完成标记后加载HP50/rounds+1。成功后再次保存、重启仍为完整新结果。
- D2人工恢复：失败重启后，实际 `SAVE_EXPORT_OWNER`（备份写失败时使用 `SAVE_EXPORT <旧槽>`）导出并检查旧HP79，再经 `SAVE_RECOVERY_ERASE CONFIRM` / `SAVE_IMPORT_BEGIN/DATA/COMMIT` 恢复，重启加载旧宠物。普通启动不擦除或自动完成未结束操作。
- D3：持续stage写失败、最终宠物写失败、stage读回失败、提交记录读回失败、设备态写/读回失败，以及“旧槽宠物与时间完全相同”的验证陷阱均不进入睡眠；验证/设备态失败后保存资格关闭。成功路径只调用一次睡眠入口。
- D4：真实按钮确认超容量的2026日期被拒绝，再返回年份、修正为2024、重新确认成功；存储失败有恢复提示且无设时成功日志。
- D5：单次图鉴失败后显式解锁重试与隐式宠物保存重试均覆盖；持续失败时不写不完整宠物记录。重置到初始形态、重启后，全局图鉴、槽快照和实际串口导出均保留先前形态。
- D6：1440/1441/10080/10081，比较实际命令的时间与生产逐分钟规则结果；支持范围一起推进，超限一起拒绝，年龄不被调试命令额外增加。

原审查 `evidence/deep-review/run.py` 与 `probes.cpp` 保留作为历史缺陷复现材料：其中退出0表示复现到缺陷，修复后不应再期待它退出0。正确行为由上述正式回归接替，未删除审查证据、未修改旧存档布局基线。

## 命令、退出码与版本

所有本轮最终证据位于 `evidence/deep-fixes/`：`host-exits.json`、`sanitized-exits.json`、`build-exits.json` 记录命令和实际退出码；同名日志记录输出。`source-sha256.json` 记录最终源文件摘要，`build-results.json` 记录镜像摘要和静态资源。后续报告文案不属于源文件冻结清单。

```sh
python tests/host/run_deep_regression.py
HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' \
  python tests/host/run_deep_regression.py
python tests/host/run_storage_tests.py
python tests/host/run_audit_tests.py
python tests/host/run_protocol_tests.py
python tests/host/run_integration_tests.py --require-all
python tests/host/run_engine_tests.py
python tests/host/run_compatibility.py
python tests/host/check_architecture.py
# integration与audit同样执行上述HOST_TEST_FLAGS配置
/tmp/artoria-deep-fixes-venv/bin/pio run -j 4 \
  -e esp32-s3-devkitc-1 -e artoria-serial -e sprout-ssd1351 -e sprout-serial
```

上一轮 `/tmp` PlatformIO环境已不存在，首次构建调用未能启动；本轮新建 `/tmp/artoria-deep-fixes-venv` 并安装PlatformIO6.2.0后完成构建，工具安装日志为`build-tool-setup.log`。本机临时环境没有加入交付源码。构建使用现有已下载工具链/依赖及项目构建缓存，本轮没有重复独立临时目录干净构建。

工具版本：Arduino framework4.20017.260907+sha.dcc1105b，Xtensa8.4.0+2021r2-patch5，esptool4.11.0，Adafruit GFX1.12.6、BusIO1.17.4、SSD1351 1.3.3；完整版本见构建日志。

## 最终结果与构建资源

| 检查 | 两包结果 | 退出码 |
|---|---|---:|
| D1–D6正式定向回归 | Artoria2525 / Sprout2525断言 | 0 |
| D1–D6 ASan + UBSan | 同上；未报告内存/未定义行为错误 | 0 |
| 存储 | Artoria69 / Sprout67断言 | 0 |
| 既有故障/中断审计 | 两包各11组全部通过，包含导入/主人恢复/未知数据保护 | 0 |
| 实际Python工具 + 生产协议 | 两包导入、导出、重复分包/提交、超时和人工恢复通过 | 0 |
| 输入/时间集成 | Artoria213 / Sprout359断言；普通及ASan/UBSan通过 | 0 |
| 既有审计 ASan + UBSan | 两包各11组全部通过 | 0 |
| 引擎规则 | Artoria720 / Sprout715断言 | 0 |
| 原始玩法对照 | 35条确定性轨迹一致，含全部宠物字段及RNG计数 | 0 |
| 引擎依赖边界 | 10个标准C++头文件检查及独立最小策略执行通过 | 0 |
| 四环境固件构建 | 两包×SSD1351/串口后端，全部SUCCESS | 0 |
| Python语法 / git diff --check | 受影响runner语法及空白检查通过 | 0 |

仅本轮最终源码生成上述证据。定向回归最后补强了D5实际导出的历史图鉴断言，以及D3失败后的写入资格断言，故最终计数为2525，早期2483计数不是交付结果。

| 环境 | Flash bytes | 静态RAM bytes |
|---|---:|---:|
| esp32-s3-devkitc-1 | 3745649 | 22520 |
| artoria-serial | 1683881 | 22288 |
| sprout-ssd1351 | 417141 | 22520 |
| sprout-serial | 393649 | 22288 |

这些数据来自本轮最终固件构建，不代表本轮实测堆、栈或长期运行资源。

## 兼容性及未执行项

v9字段、枚举、ABI夹具、校验和新旧存档编码没有修改。现有两包存储、导入/主人恢复故障及中断矩阵、实际Python工具协议套件全部复验；原始Artoria规则35条确定性轨迹仍相同。没有用主机结构体转储新增旧档样本，没有动真实用户存档。

明确保留旧分钟检测、离线不足一分钟/分钟余数、整天日结补1440分钟等玩法语义；不把这些既有语义包装为本轮已修复。新恢复基准消除的是已存快照与独立旧锚点的脱节。主机写入中断模型仍依赖存储契约“单键原子替换”，不是物理断电试验。

新源码实机刷写、真实RTC/深睡唤醒/掉电、实体屏幕及按键、运行堆栈与老化均未执行。此前 `hardware-acceptance.md` 的实测属于修改前版本，不证明本轮新固件已通过实机。独立原版维修工作树未回移本轮改动。六项主机回归通过不等于整个引擎最终验收通过。

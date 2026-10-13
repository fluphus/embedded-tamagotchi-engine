# 最终补充验收（2026-10-12）

> 此页保留该次离线验收历史。2026-10-13 用户已接入并授权测试开发板，新增实机证据见 [hardware-acceptance.md](hardware-acceptance.md)；下文“未操作设备”仅指原轮次。

对象是当前 `/home/omni/esp32-artoria-tamagotchi` 的引擎交付：Artoria、Sprout 两包。独立原版维修工作树未在本轮修改，不能把本轮新增修复说成已回移到原版。

结论：**9 项 PASS，0 项 FAIL，4 项 BLOCKED**。规划已明确批准 Moonberry(ID42) 的 healthy 改为 true；重新生成内容表后，A7 的 Sprout 连携及两种特殊食物已通过真实输入闭环回归。其余项目沿用前轮证据，本轮未重跑无关存储故障矩阵。没有连接、烧录、擦除或导入真实设备，不宣称集中验收或实机验收通过。

## 总表

所有测试路径以仓库根为基准；完整输出在 [evidence](evidence/) 中，文件名前缀 `supplement-`。下文函数均调用交付的生产实现。

| 编号 | 状态 | 测试位置 / 命令 | 实际结果 | 限制 |
|---|---|---|---|---|
| A1 串门槽位排列 | PASS | `audit_test.cpp::permutations`；`python tests/host/run_audit_tests.py --case 7` | 两包各 276 断言。主人 0/1/2 × 首个访客取两种槽位，共每包六种排列；实际进入串门、六轮重启/保存、结束及三轮保存/重启。冻结字节不变，结束后始终 HP50。 | 冻结主人损坏的既有分支另由 case 2 覆盖；不等价于设备掉电实测。 |
| A2 导入中断重启 | PASS | `audit_test.cpp::importInterruptions`，case 8；原有 `restoreOwner`，case 6 | 两包完整/仅宠物导入各在实际 11 次成功持久写入后中断，**每包 22 个中断点**。前十点均 Recovery、禁止后续自动/图鉴写；完成标记落盘后加载完整 HP81 和正确串门标志。主人恢复原有七个中断点继续通过。 | 在 KeyValue 单键原子替换契约下模拟进程中断；真实写入期间断电属于 B12。 |
| A3 存储服务故障 | PASS | `audit_test.cpp::serviceFaults`，case 9；case 3/5；协议擦除失败测试 | 保存服务每包55断言，另加实际首次设时回调六种故障（最终集成 Artoria184 / Sprout180断言）：打开失败走 Recovery；读取失败不当新游戏；容量不足、首次 guard 写失败、主人副本读回不一致、擦除失败均无成功提示，后续写被禁止。已有 pending 的故障重启仍 Recovery；首次 guard 根本未写入时原记录未变，恢复服务后重启只加载原主人（原为空设备时仍为空设备）。实际设时在打开失败/恢复锁时拒绝执行；设备态、最终存档或完成标记失败不会提示进入正常运行，重启保持 Recovery。 | `contains/length` 接口不提供独立错误枚举；故障模型为 open 拒绝、已存在键读取失败、容量拒写、读回不一致和 clear 拒绝。不能外推真实 NVS 所有错误码/损坏模式。故障注入覆盖启动、导入、主人恢复、首次设时和恢复擦除，普通销毁/日结每个写点的故障组合未逐一注入。 |
| A4 人工恢复闭环 | PASS | `run_protocol_tests.py`；生产 `saveProtocol`、`exportOwnerBackup`、`commitImport`、`loadStartup` | 实际 Python 工具先导出有效主人，明确执行恢复擦除，再通过实际完整导入恢复 HP50 并重启。覆盖完整导入失败、主人最终写失败、只剩 `owner_restore`；仅副本启动保持 Recovery，完整及仅宠物来源副本均可导出并作为完整主人备份恢复，擦除失败不解锁。 | 明确擦除前必须将备份保存到主机；没有自动清理或自动回滚。损坏的未知副本不会被伪装成有效备份。 |
| A5 串口/Python 联调 | PASS | `python tests/host/run_protocol_tests.py`；实际 `SaveManager/save_manager.py` 的 `cmd_import/cmd_export/_read_line` | 两包经过生产命令组装、HEX接收、完整提交、启动加载。双向分片、丢 ACK 重发、重复偏移包、立即及中途断连、全部 ACK 超时、非法 HEX/偏移、超长整行、半行超时、未完成提交、ABORT、重复 COMMIT 均验证；拒绝路径持久写次数不增加。丢导出数据行时拒绝覆盖备份文件。 | 模拟字节传输和 NVS；没有验证实际 UART 驱动、USB CDC、线缆、波特率或缓冲丢包率。 |
| A6 时间/调度集成 | PASS（保留旧语义限制） | `integration_test.cpp::calendar/rollover/visitTime`；`python tests/host/run_integration_tests.py` | 实际 TimeManager、首次设时回调、doDayEnd、skipTime、两个主人恢复入口及 DisplayManager。跨午夜/月/年、2024 闰日及2023非闰日各只触发一次日结；正常重启及设时离线补偿、串门一天的冻结记录/主人时间对齐通过。计数器近回绕及跨完整周期后日历单调、动画和自动保存正确；poke 冷却不随毫秒回绕提前失效，页面 hold 不提前到期。 | 保留分钟分量检测：精确跳 60 分钟 `checkNewMinute()==false`；日结清暂停后补 1440 分钟；离线按整日/剩余分钟，设备时间锚点截到分钟。旧语义不据此声称任意稀疏调用均不漏分钟。时钟至少每个 32 位毫秒周期调用一次。 |
| A7 输入—规则—演出 | PASS | `integration_test.cpp::inputs/sproutFoodInputs`；`python tests/host/run_integration_tests.py --pack sprout --case inputs --require-all`，退出0 | Sprout 实际 GPIO→按钮/菜单→生产规则→保存→队列/回调/串口渲染：Dew+Sunseed+Moonberry 触发连携，Spring Rain(101)/Starlight(107) 分别选择，HP50→63→68、积水0→6→7，每次特殊食物日志仅一次；动画回 PAGE_IDLE。三种含 Pebble 的不同食物组合均无连携。动画期间连按不重复结算或写档；五种组合均保存、启动加载、再次保存/重启保持状态。原有成长/图鉴/销毁/队列满及清空断言继续执行。 | 本轮187断言，ASan/UBSan同样通过；Artoria沿用前轮真实输入证据。两包实机仍属B10。Moonberry只有healthy改变，health_delta=2、稳定ID42及积水/成长规则不变，界面仍三选不同食物。 |
| A8 容量/数值边界 | PASS（有明确上限） | `audit_test.cpp::sequenceBoundary/formatBoundary`，case 10/11；`engine_test.cpp::serialAndCapacity`；`run_content_boundaries.py`；`integration_test.cpp::offlineCapacity` | UINT32_MAX−1 的旧档可写一次到 UINT32_MAX，随后明确拒绝保存，重启仍加载最新档，不发生 sequence=0 覆盖。生产导入拒绝异常长度、扩展长度/版本、ID、图鉴计数且 RAM/NVS 不变。生产生成器 33 个数量/ID/重复测试。Sprout 积水在65535饱和；年龄65535拒绝日结。最大离线366天实跑（最终普通主机运行 Artoria9ms / Sprout31ms），超限/年龄及 epoch 容量拒绝；单条分钟批处理上限1440，先拒绝再乘法。 | 内容生成器：形态1–32、普通食物4–32、特殊食物1–32、ID0–254（引擎 ID 类型仍 uint16）；扩展64字节、图鉴32。容量已写明，不承诺无限离线或无限寿命。离线耗时是主机测量，不能替代 B13 看门狗/卡顿。 |
| A9 干净构建 | PASS | `run_clean_builds.py --pio /tmp/artoria-p0/venv/bin/pio --manifest /tmp/artoria-p0/supplement-source-manifest.json` | 独立临时目录复制完整交付源码；初始不存在项目 `.pio`，两包×SSD1351/Serial四环境全部退出0。资源重新生成逐字节一致；保存源码 SHA256 清单、包/库版本和各环境 Flash/RAM。 | 使用已安装全局工具链与下载缓存；没有复用项目构建对象/项目 libdeps。构建成功不是硬件运行成功；清单是构建开始时快照，随后更新的文档/日志不属于构建输入。最终 src/tools/assets/SaveManager/tests/platformio.ini 哈希逐项核对一致。 |
| B10 两包实机冒烟 | **BLOCKED** | 未执行 | 缺少测试设备；没有烧录。 | 屏幕、三键/长按、资源与动画、投喂—成长—保存—重启需实测。 |
| B11 UART与真实旧档 | **BLOCKED** | 未执行；既有目标工具链 v9 夹具证据只作补充 | 无实际 UART、无已备份真实 v9 设备档。 | 目标夹具不能替代真实旧档迁移；本轮没有把夹具改称真实样本。 |
| B12 深睡/掉电 | **BLOCKED** | 未执行 | 无设备；未执行深睡、复位或专用数据断电实验。 | 离线中断测试不能替代实际 NVS 掉电、RTC 和复位原因行为。 |
| B13 运行资源 | **BLOCKED** | 未执行 | 仅有编译静态 Flash/RAM 与主机离线耗时。 | 未测最低堆、栈、PSRAM不足、长期内存下降、看门狗及实际卡顿。 |

## Moonberry 决策后的定向回归

仅修改 `src/content/sprout/manifest.json` 的 Moonberry `healthy` 并重新生成 `src/content/sprout/food_table.h`。没有改生产策略、输入选择、引擎、保存编码或 Artoria 内容。`sproutFoodInputs` 对五种场景独立初始化，通过真实 GPIO 短按选择抽取结果中的稳定 ID；没有调用 debug 注入或直接执行特殊食物来代替界面。两种特殊食物分别验证真实生产回调日志、精确 HP/积水、动画期间保存字节不变、队列结束/页面返回待机及两次重新加载。原 `--require-all` 的 Sprout 硬编码失败已由这些必执行断言接替，断言失败返回1；普通及严格模式都包含这些断言。新增 `--pack`/`--case inputs` 仅用于限制回归范围，默认仍执行两包原有全部集成测试。

| 本轮命令 | 实际退出码 | 结果 / 证据 |
|---|---:|---|
| `python tools/engine/generate_content.py --content sprout` | 0 | `sprout-combo-content.log`，更新生产内容表 |
| `python tools/engine/generate_content.py --content sprout --check` | 0 | `sprout-combo-content.log`，生成内容一致 |
| `python tests/host/run_engine_tests.py --pack sprout` | 0 | `sprout-combo-rules.log`，715断言 |
| `python tests/host/run_integration_tests.py --pack sprout --case inputs --require-all` | 0 | `sprout-combo-integration.log`，187断言 |
| `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python tests/host/run_integration_tests.py --pack sprout --case inputs --require-all` | 0 | `sprout-combo-sanitized-integration.log`，187断言 |
| `/tmp/artoria-p0/venv/bin/pio run -e sprout-ssd1351 -e sprout-serial` | 0 | `sprout-combo-builds.log`，两后端构建成功；SSD1351 Flash413721/RAM22480，Serial Flash390121/RAM22232字节 |

只运行以上受影响检查。Artoria构建、旧档、资源像素再生成、完整存储故障矩阵和全量 `run_all.py` 本轮均未重跑；沿用下方历史证据。内容表已重新生成并检查，不涉及资源映射变化。四项硬件验收仍未执行。构建静态占用不能替代实机堆/栈测量。

## 前轮实际命令与证据（历史，未重跑）

`python` 为 Python3.14.7，g++16.2.1。`/tmp/artoria-p0/venv` 为可替换的工具环境，不属于交付源码。日志中列出完整编译命令，不要求复用本机临时路径。

| 命令 | 实际退出码 | 证据 |
|---|---:|---|
| `python tests/host/run_audit_tests.py` | 0 | `supplement-storage.log`（case 1–11；case 11 增补长度后另见下行） |
| `python tests/host/run_audit_tests.py --case 11` | 0 | `supplement-format-boundaries.log` |
| `python tests/host/run_protocol_tests.py` | 0 | `supplement-protocol.log` |
| `python tests/host/run_integration_tests.py` | 0 | `supplement-integration.log`；这里的 PASS 指已覆盖断言，不含不可达的 Sprout 连携 |
| `python tests/host/run_integration_tests.py --require-all` | **2** | `supplement-a7-strict.log`：规划决策前历史失败；当前由上方真实输入回归接替 |
| `python tests/host/run_engine_tests.py` | 0 | `supplement-engine.log` |
| `python tests/host/run_storage_tests.py` | 0 | `supplement-existing-storage.log` |
| `python tests/host/run_compatibility.py` | 0 | `supplement-compatibility.log`，原始/当前35条生产规则轨迹一致，含全字段和随机数消耗 |
| `python tests/host/run_content_boundaries.py` | 0 | `supplement-content.log` |
| `python tests/host/check_architecture.py` | 0 | `supplement-architecture.log` |
| `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python tests/host/run_audit_tests.py` | 0 | `supplement-sanitized-storage.log` |
| 同样标志运行 `run_audit_tests.py --case 11` | 0 | `supplement-sanitized-format-boundaries.log` |
| 同样标志运行 `run_protocol_tests.py` | 0 | `supplement-sanitized-protocol.log` |
| 同样标志运行 `run_integration_tests.py` | 0 | `supplement-sanitized-integration.log` |
| 同样标志运行 `run_engine_tests.py` | 0 | `supplement-sanitized-engine.log` |
| `/tmp/artoria-p0/venv/bin/python tests/host/run_clean_builds.py --pio /tmp/artoria-p0/venv/bin/pio --manifest /tmp/artoria-p0/supplement-source-manifest.json` | 0 | `supplement-clean-builds.log`、`supplement-source-manifest.json` |

没有修改前重跑已通过的全套、资源和四环境；修改后仅回归公共保存、协议、时间/UI、容量所影响的套件，以及必需的独立干净构建。旧 v9 夹具/旧边界检查未重新执行，沿用 [save-format.md](save-format.md) 和历史验收证据；v9 字节编码与夹具本轮未改。`run_all.py` 纳入严格 A7 检查，原硬编码失败现已由真实流程断言接替。全量入口本轮未执行，不将定向回归说成重新跑过全套。

## 修复及生产接线

- SaveManager 打开失败不能走新游戏；启动不自动重写不可读的活动槽配置。sequence 耗尽拒绝写入，保留所有既有有效/未知记录。
- `SAVE_EXPORT_OWNER` 导出经过校验的恢复前主人备份；仅宠物来源在导出缓冲区补成带当前形态最小图鉴的完整备份，NVS副本原字节不变；主机 `export --slot -1` 调用同一入口。普通启动仍只保护数据，不自动清理副本。
- 保存串口命令从 `console.cpp` 原位提取到 `save_protocol.h`，实际固件和主机桥接共用；console 保留时间对齐与 UI 收尾。整行接收器共用，超长行全部丢弃、半行和会话30秒超时。
- 新 Python 工具看到 `offset=1` 后发送 `SAVE_IMPORT_DATA @<offset> <hex>`，同偏移相同数据幂等确认；旧追加语法继续可用。连接旧固件时不自动重发无偏移分包，避免重复追加。断连由30秒会话超时/明确ABORT终止。
- Python 保留超时返回的半行，完整换行后才解释 ACK/成功；导出同时核对 BEGIN/END 声明长度，失败不覆盖主机备份。
- 首次设时回调检查保存服务/恢复锁，复用现有持久保护标记覆盖设时和离线补偿；设备态、最终保存及完成标记全部检查成功后才提示正常运行，失败保留恢复锁。
- TimeManager 复用已注入的单调时钟/诊断端口，逐次累积整秒并保留毫秒余数；页面 hold/toast 使用回绕安全截止判断。
- 三键销毁组合确认后抑制松键产生新操作，必须另一次按键确认；没有改销毁菜单或规则。
- 加入有界离线/批处理、年龄拒绝与 Sprout 积水饱和，避免数值回绕。普通支持范围内的 Artoria 行为对照保持一致。
- `workflow_services.h` 只声明现有时间/UI/保存服务，使真实 `workflows.cpp` 与 `ui_binding.cpp` 可离线编译；没有增加替代实现。测试外围只替换 millis/GPIO/UART/PSRAM读写和无关电源活动通知，真实按钮状态机、输入映射、菜单、导演、队列、DisplayManager、串口渲染器、资源、时间与应用回调均参与。

## 人工恢复复现

先在恢复锁仍存在时取回备份，核实主机文件，再明确删除并恢复；以下命令均已有生产路径测试。

```sh
# 有效主人仍在普通槽中：选择实际的冻结主人槽（这里仅示例0）
python SaveManager/save_manager.py --port PORT export --slot 0 --out owner.bin
# 若只剩恢复副本：无需先清空 NVS
python SaveManager/save_manager.py --port PORT export --slot -1 --out owner.bin
# 在串口监视器明确发送，删除动作不会由普通启动执行
# SAVE_RECOVERY_ERASE CONFIRM
python SaveManager/save_manager.py --port PORT import --slot 0 --in owner.bin
```

上述真实设备命令**本轮未对设备执行**。离线测试通过同一命令解析器和同一 Python 函数闭环验证。恢复前 v10 主人副本保持原始字节；旧 v9 来源保留原旧键，副本为既有迁移生成的规范 v10。未知/损坏数据仍拒绝自动覆盖。

## 静态资源和构建环境

构建大小与最后一次干净构建日志一致，见 [supplement-build-summary.json](evidence/supplement-build-summary.json)。这里只列静态占用，不是最低可用堆或栈测量。

| 环境 | Flash bytes | 静态 RAM bytes |
|---|---:|---:|
| esp32-s3-devkitc-1 | 3742209 | 22488 |
| artoria-serial | 1680385 | 22240 |
| sprout-ssd1351 | 413721 | 22480 |
| sprout-serial | 390121 | 22232 |


平台锁为 espressif32 7.1.3，Arduino 4.20017.260907+sha.dcc1105b，Xtensa 8.4.0+2021r2-patch5；实际使用库锁为 GFX1.12.6、BusIO1.17.4、SSD1351 1.3.3。构建辅助包与传递依赖实际解析版本完整保存在 `pio pkg list` 输出中，不声称所有传递安装依赖都已锁死。资源为 Artoria214序列、Sprout占位索引0像素序列，临时再生成字节一致。

## 剩余问题

1. A7 原 Sprout 内容可达性缺口已按规划决策解决，证据见上方本轮定向回归；原失败日志保留作为历史。
2. B10–B13 全部未执行且 BLOCKED；需要专用测试设备和已备份的真实 v9 旧档。
3. 上表已记录原分钟/离线时间语义限制，不声称离线测试已消除这些原有玩法语义。
4. 原 `MenuController::confirmFeed` 的未使用 `sR` 编译警告仍存在；未进行无关整理。主机外围将 Arduino目标32位格式与主机64位格式的告警单独禁用，不将主机内存布局用于旧档证据。

本轮不提交或推送 Git，不动用户已有改动；未生成项目缓存或本机配置进入交付。下一步为规划方复核本轮证据及有设备后的四项验收，不继续扩展引擎架构。

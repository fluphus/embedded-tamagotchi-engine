# 自动实机补充验收（2026-10-13）

> 版本范围提示：这是D1–D6修复前的实机结果。后续[定向修复报告](deep-fixes-2026-10-13.md)已完成主机回归和新固件构建，尚未刷写或实测；不得用下面的30分钟采集等结果证明新源码已实机通过。

对象：引擎工作区，两包×两显示后端。用户已明确授权接入开发板上的全部操作；不涉及独立原版维修工作树；本轮新增修复没有回移到该独立工作树。最终判定仍由规划验收方负责。

## 设备与数据保护

USB `303a:1001`，MAC `a4:cb:8f:d8:91:bc`，实测 ESP32-S3 QFN56 rev0.2，16 MB Flash、8 MB embedded PSRAM。起初串口权限拒绝，用户本机授予 ACL 后读取成功。工具为临时虚拟环境 `/tmp/artoria-hardware-venv`，PlatformIO6.2.0、pyserial3.5、Pillow12.3.0，esptool4.11.0。没有把本机虚拟环境加入源码。

完整刷写前镜像位于 `/home/omni/artoria-device-backups/2026-10-13-a4cb8fd891bc/before-full-flash.bin`，16777216 bytes；SHA256 `962937d4a47dc3ef61678cb298eef33aa92e17de69dd6ed19d3aecaac7990f1c`。这是备份读取时的完整镜像；随后原固件正常启动可能继续轮转保存。原始镜像、真实旧档和设备数据保存在仓库外，不当作可公开生成夹具提交。

板上原固件实际导出 v9 84-byte full、72-byte pet：成年 Rhongomyniad、HP50、SR100、age_days121、图鉴3/12、设备rounds3。首先保留原 NVS 刷写 Artoria 串口引擎；实际启动迁移后 UART 导出 v10 228-byte full。`verify_real_v9.cpp` 调用生产迁移和编码器，与设备结果比较从offset24起的全部状态、扩展和图鉴字节，PASS；不是只在主机解码样本。序号和保存时间允许正常自动保存推进。

## 测试入口

所有板测脚本通过真实 USB 串口，调用实际固件命令解析、菜单、规则、队列、NVS 与 Python工具。`btn` 是原固件已有的模拟按键入口，绕过GPIO电气输入；不据此宣布三键硬件、物理长按或屏幕视觉验收通过。

```sh
/tmp/artoria-hardware-venv/bin/python tests/hardware/run_board_acceptance.py \
  --env ENV --port /dev/ttyACM0 \
  --backup /home/omni/artoria-device-backups/2026-10-13-a4cb8fd891bc/before-full-flash.bin \
  --authorize-board-tests
# ENV: artoria-serial / sprout-serial / esp32-s3-devkitc-1 / sprout-ssd1351
# 此入口会刷写并擦除0x9000..0xdfff NVS，必须使用已备份的专用测试板。
/tmp/artoria-hardware-venv/bin/python tests/hardware/run_uart_edges.py --label ENV --timeouts
/tmp/artoria-hardware-venv/bin/python tests/hardware/run_sprout_foods.py --label ENV
```

`--visits-only` 跳过食品、成长部分，保留初始化、三个主人槽和协议保存路径；用于定向补验，不替代完整套件。脚本逐项PASS仅表示对应断言；只有末项 `suite completed` 才表示该脚本完整通过。未捕获异常、超时或非零退出不能算通过。完整输出在 `evidence/hardware/`。完整105/118断言套件的OLED快照另保留在`esp32-s3-devkitc-1-full/`、`sprout-ssd1351-full/`，后续稳定性初始化不会覆盖该证据。

## 本轮修复

1. 串口模拟输入以前直接调用 `handleAction`，绕过物理输入遵守的页面动画锁。实测动画中连按可打开状态页。`injectButton` 加相同输入门禁；正常特殊食品等待输入仍可选。生产路径回归验证动画中模拟按键不改变上下文、不重复结算。
2. 首次串口设时的首笔存档没有绑定已解锁的初始图鉴，导出后再次导入显示0/2。串口等待设时分支复用现有受保护的UI设时回调；该回调在最终宠物保存前检查图鉴持久化，失败保留恢复锁。两包增加实际首笔槽绑定图鉴及图鉴写失败重启保护测试。
3. 增加仅观察的 `devinfo` 堆、历史最低堆、最大块、PSRAM、loop任务栈余量输出，实际抽取食物稳定ID日志，以及ctx的动画播放/等待/阻塞状态。没有改变内容规则或存档格式。ESP-IDF该版本的栈高水位单位为bytes，代码与报告使用bytes。

4. Sprout销毁过渡的串口渲染复用了未赋值的进化字段，打印Unknown→Unknown；现在在ANIM_DESTROY分支读取实际destroyedForm。物理销毁输入路径回归检查实际角色名。

5. 正常运行期间成功导入没有清理正在选择的食物或待处理特殊食品。console的成功提交现在调用`MenuController::onPetImported()`，清空旧队列、停止旧动画并重新初始化菜单；失败导入不调用该收尾。两包覆盖选食物中和特殊食品等待时的真实导入/收尾，后续按键不再结算旧操作。

6. 真正的 USB Serial/JTAG 重启/重开连接场景中，完整导入停在 `[SaveImport] READY slot=`。额外空行不能推动发送，下一条ABORT的输出才带出后半行；真实复现见 `usb-reboot-ready-probe.json`。仅增加超时，以及仅在命令结束调用一次SDK flush，均未通过完整流程（保留失败日志）。最终 `platform/arduino/diagnostics.h::diagnosticLine` 对该板实际使用的HWCDC输出按最多32-byte分段，每段使用SDK有界flush排空；其他串口模式保持原输出，字节与协议格式不变。该适配器是main装配的实际日志输出者，不属于规则引擎；空闲帧不进行发送等待。`run_usb_reopen.py` 包含实际设时、状态、设备查询、六次复位及实际Python行组装；原先省略设时的短测不足以证明修复，完整流程结果另见下表。没有重试COMMIT来掩盖失败。


测试脚本的错误假设也已修正：食物默认光标为1；完整导入后的最终保存可能写入另一个活动槽，所以访客目标不能直接由导入目标推断。Artoria没有映射“长按左键取消投喂”，随机抽取不足三份健康食品时使用实际完整导入恢复测试基线再抽取；断连分片测试只发送一次后半命令，避免测试脚本自身重复发送。UART诊断采集也改为等到完整换行/响应标记后判断，避免把USB串口半行误判为流程失败。首次脚本失败记录保留，不当作生产回归成功证据。 串口占位后端每帧输出动画日志，读取命令响应可能跨过整个1秒特殊食品动画；之前再发的三次按键实际落在待机，合法打开状态页。回归现在将确认与三次连按作为同一批真实USB字节发送，在规则提交/队列阻塞阶段验证一次效果和最终待机，没有放宽状态断言。

## 未验证边界

- SSD1351 SPI后端启动和流程存活不等于屏幕确实接线、显示正确；需要画面观察。串口模拟输入不验证实际按钮电气、消抖或组合长按。
- 当前生产深睡只配置按键EXT1唤醒，USB串口不提供自主唤醒命令；本轮不加入测试专用唤醒机制来冒充原深睡路径。USB Serial/JTAG在深睡时也可能断开，当前进程只有现有TTY节点的ACL，不能自动授予重新枚举节点权限。按键深睡唤醒、真实断电、写入掉电需物理操作或外部可控GPIO/电源，BLOCKED。
- 堆/栈是本轮短测试期间的真实测量；两个OLED环境先完成330秒采集，再分别执行1800秒连续稳定性测试（结果以stability-results.json为准）；这仍不能替代小时/天级运行，不能据此证明无长期泄漏。PSRAM不足及最大366天离线补偿由隔离的测试固件测量，见结果表；测试固件不会作为最终固件保留。
- 固定Arduino SDK的main.cpp在启动时设`loopTaskWDTEnabled=false`，项目未调用`enableLoopWDT`。日志中没有WDT/复位只能说明当前实际配置下未观察到异常，不能当作开启loop任务看门狗后的验收；本轮不擅自改变生产看门狗配置。
- 本轮只定向回归新增输入/设时修改所影响的主机集成与必要固件构建，没有重跑已经通过的无关存储故障矩阵。

## 受影响主机回归及构建

- `python tests/host/run_integration_tests.py`：退出0，Artoria213 / Sprout359断言；`HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g'` 同样退出0。两包包含实际规则、菜单、显示管理、NVS facade、首次设时故障保护及导入时旧演出清理；没有重跑无关完整存储故障矩阵。
- `python tests/host/run_diagnostics_tests.py`：退出0，实际生产输出适配器在USB mode1、mode0和非USB三个配置下分别36/16/16断言；对照原println输出字节、32-byte容量和零写入/部分短写时停止且不追加行结束。ASan/UBSan同样退出0。该检查加入`run_all.py`，本轮仅单独执行受影响入口。
- `/tmp/artoria-hardware-venv/bin/pio run -e esp32-s3-devkitc-1 -e artoria-serial -e sprout-ssd1351 -e sprout-serial`：退出0；四个正常环境构建数据及镜像hash见`evidence/hardware/final-builds.json`。最终恢复刷写的镜像另由`final-restore/final-state.json`记录，不能用静态RAM代替运行堆/栈。
- `/tmp/artoria-hardware-venv/bin/python tests/host/run_clean_builds.py --pio /tmp/artoria-hardware-venv/bin/pio --manifest docs/engine/evidence/hardware/final-source-manifest.json`：退出0，独立临时目录无项目`.pio`，重新生成两包资源一致，再构建两包×两后端并列出实际依赖。首次尝试因默认Python缺Pillow在资源检查处退出1，补装固定Pillow12.3.0后重试成功；见`clean-build-pillow-missing.log`、`pillow-setup.log`及`final-clean-builds.log`。源码清单记录实际拷贝文件的hash，避免运行中的证据日志读写竞争；后续更新的报告/日志不属于冻结源码清单。
- `python -m py_compile tests/host/run_all.py tests/host/run_diagnostics_tests.py tests/hardware/*.py`：退出0。不把Python语法检查当作设备或业务检查。

## 隔离的资源压力测量

`tests/hardware/platformio-probes.ini` 继承实际包/SSD1351配置；全部探针只从测试目录额外构建，不进入四个正常环境。`pressure.cpp` 在Arduino已有的`initVariant`弱钩子处预留PSRAM，使剩余外部RAM低于4096 bytes，再运行同一setup/loop、实际菜单、投喂、成长、保存和重启。不是通过主机分配失败模拟器替代MCU测量；屏幕视觉仍未观察。

`offline.cpp` 通过链接器wrap调用真实`loop`，仅在无任何存档的新测试NVS上调用生产首次设时回调，再调用生产回调推进最大366天，并通过实际SaveManager重读、实际复位验证年龄。已有数据时明确SKIPPED；不自动覆盖已有宠物。测量包含真实规则、日结、NVS和演出入队的成本，不是复制规则的循环基准。

```sh
# 两包各执行一次；以下命令会擦除测试NVS，需要板自身的完整备份与授权flag。
/tmp/artoria-hardware-venv/bin/python tests/hardware/run_board_acceptance.py \
  --env PACK-pressure --project-conf tests/hardware/platformio-probes.ini --skip-visits \
  --backup /home/omni/artoria-device-backups/2026-10-13-a4cb8fd891bc/before-full-flash.bin \
  --authorize-board-tests
/tmp/artoria-hardware-venv/bin/python tests/hardware/run_offline_probe.py \
  --env PACK-offline \
  --backup /home/omni/artoria-device-backups/2026-10-13-a4cb8fd891bc/before-full-flash.bin \
  --authorize-board-tests
# PACK = artoria / sprout
```

复位期间曾发生两次esptool读芯片信息/上传RAM stub中断（非零退出日志保留）；重新连接后数据仍能正常加载。为了复位而反复上传stub没有必要，最终板测改用`reset_board.py`调用该固定esptool版本自己的`HardReset(uses_usb=True)` RTS时序，随后强制检查新的`[Boot] Reset reason:`日志；不以命令返回0冒充实际重启。刷写、读取原始Flash仍使用实际esptool，不隐瞒失败或自动重试COMMIT。

普通与探针PlatformIO配置使用同一默认构建目录。切换配置可能清理另一配置的产物，不能并行运行两种配置，也不能把已被清理的旧ELF当作现有证据。板测脚本每次构建/刷写指定环境并保留独立日志；最终恢复只使用普通默认固件。

## 旧档和最终清理命令

```sh
g++ -std=c++17 -Isrc tests/hardware/verify_real_v9.cpp -o /tmp/artoria-verify-real-v9
/tmp/artoria-verify-real-v9 \
  /home/omni/artoria-device-backups/2026-10-13-a4cb8fd891bc/real-v9-slot0-full.bin \
  /home/omni/artoria-device-backups/2026-10-13-a4cb8fd891bc/migrated-v10-slot0-full.bin
# 下面脚本还会实际验证v9完整导入/访客导入，然后恢复原始NVS；仅使用该板自身的备份。
/tmp/artoria-hardware-venv/bin/python tests/hardware/restore_board_data.py \
  --backup /home/omni/artoria-device-backups/2026-10-13-a4cb8fd891bc/before-full-flash.bin \
  --authorize-board-tests
```

最后一个脚本依赖备份目录中真实导出的`real-v9-slot0-full.bin`和`real-v9-slot0-pet.bin`，不是通用任意设备恢复器。先检查原分区表，再恢复原NVS；未复位启动前读回原始20KB并逐字节比较。最后留下默认Artoria SSD1351引擎固件，恢复测试前宠物、图鉴、设备轮次及电源配置。是否实际成功以`final-restore/results.json`的`suite completed`和`final-state.json`为准；该段是操作说明，不单独作为通过证明。
## 最终结果总表

本轮补齐后的13项结果为 **11 PASS / 0 未解决FAIL / 2 BLOCKED**。A1–A9沿用已验收离线证据并增加下列实机证据；B13的PASS限定本次两包各30分钟及所列压力测量，不是无限期可靠性保证。这是执行证据，不代替规划方集中验收。

| 编号 | 状态 | 测试位置 / 命令 | 实际结果 | 限制 |
|---|---|---|---|---|
| A1 串门槽位排列 | PASS | `audit_test.cpp::permutations`；`run_board_acceptance.py` 三主人槽循环 | 两包主人0/1/2均经过进入、访客71→82、重启、连续保存、结束与再次保存/重启；冻结字节不变，最终主人HP50。反向排列沿用每包276断言。 | 物理写入中断由B12单列。 |
| A2 导入中断重启 | PASS | `audit_test.cpp::importInterruptions/restoreOwner` | 沿用每包22个导入中断点及7个主人恢复中断点，生产启动保持Recovery或完整结果。 | 单键原子替换模型；不冒充真实掉电。 |
| A3 存储服务故障 | PASS | `audit_test.cpp::serviceFaults`；`integration_test.cpp` 设时故障 | 沿用已验收故障矩阵；本轮两包增加图鉴首次持久化失败及保护重启验证，实际集成213/359断言。 | 未向真实NVS人为注入硬件故障。 |
| A4 人工恢复闭环 | PASS | `run_protocol_tests.py`；`restore_board_data.py` | 沿用真实命令备份/明确恢复擦除/导入；板上真实v9完整及pet导入、owner_restore导出、主人恢复完成。 | 损坏未知格式不能伪装为有效可加载档；原始Flash另备份。 |
| A5 串口/Python | PASS | `run_protocol_tests.py`；`run_uart_edges.py`；`run_usb_reopen.py` | 实际USB字节流及Python工具完成分片、重复包、非法HEX、ABORT、未完成/重复COMMIT、真实30秒超时；修复READY尾部排队，六次完整重启复现通过。 | 实测板载USB Serial/JTAG；未测外接TTL UART电气。 |
| A6 时间与调度 | PASS（旧语义限制保留） | `integration_test.cpp`；`run_uart_edges.py`；offline探针 | 跨闰日/月/年各一次日结，实际366天回调及重启，串门冻结对齐沿用集成；两包30分钟多次实际自动保存（次数见资源表）。 | 毫秒回绕采用生产主机测试；未等待设备实际运行49天。旧分钟语义见基线。 |
| A7 输入—规则—演出 | PASS | `integration_test.cpp::inputs/sproutFoodInputs/importPresentation`；两包板测 | 普通投喂、两种Sprout特殊食物、连按只结算一次、成长/图鉴/销毁、导入清旧演出通过；含Pebble的三种组合均不连携。 | GPIO真实按键和画面仍属B10。 |
| A8 容量与数值 | PASS（声明上限） | `audit_test.cpp::sequenceBoundary/formatBoundary`；`offlineCapacity`；低PSRAM探针 | 沿用sequence最大值/未知数据拒绝/扩展及ID边界；板上PSRAM仅391 bytes仍完成生命周期，366天保存恢复通过。 | 离线同步处理约13–15秒；不承诺无限时长。 |
| A9 干净构建 | PASS | `run_clean_builds.py`，`final-clean-builds.log` | 最新固件源码在独立临时目录无项目.pio，两包资源重新生成一致、四环境编译及依赖清单全部退出0。 | 允许复用下载的工具链/库源码，未复用项目目标文件。 |
| B10 两包实机冒烟 | BLOCKED（现场项） | 四环境板测完整套件，见下表 | 软件初始化、互动、成长、保存/重启、串口模拟长按图鉴均通过。 | 无屏幕观测/按键执行器；实体屏幕、三键、物理长按未验收。 |
| B11 实际串口/真实旧档 | PASS | `verify_real_v9.cpp`；`restore_board_data.py`；实际Python工具 | 真实板上v9完整及pet备份、启动迁移、导入、串门和主人恢复完成；原始NVS读回逐字节相等，最终迁移payload再次比较通过。 | 一个真实Rhongomyniad样本、两个旧槽；其他旧状态另由目标编译器夹具覆盖。 |
| B12 深睡与掉电 | BLOCKED | 多次真实RTS复位已验证，其他未执行 | 正常/串门的复位加载及保存通过。 | 原深睡只有EXT1按键唤醒；无远程物理按键/电源控制。未掉电、未做写入期间断电。 |
| B13 运行资源 | PASS（限定测试范围） | `run_uart_edges.py --soak-only --soak-seconds 1800`；压力/离线探针 | 每包30分钟持续资源采集（样本数见资源表），未观察到复位或持续内存下降；PSRAM不足及366天实测如下。 | 不代替小时/天级老化；当前loop任务WDT未启用，未声称开启后通过。 |

## 完整套件证据

| 套件 | PASS断言数 | 结果位置 |
|---|---:|---|
| Artoria 串口完整套件 | 105 | `evidence/hardware/artoria-serial/results.json` |
| Artoria SSD1351 完整套件 | 105 | `evidence/hardware/esp32-s3-devkitc-1-full/results.json` |
| Sprout 串口完整套件 | 127 | `evidence/hardware/sprout-serial/results.json` |
| Sprout SSD1351 完整套件 | 118 | `evidence/hardware/sprout-ssd1351-full/results.json` |
| Artoria 低PSRAM生命周期 | 43 | `evidence/hardware/artoria-pressure/results.json` |
| Sprout 低PSRAM生命周期 | 57 | `evidence/hardware/sprout-pressure/results.json` |
| Artoria 366天离线 | 8 | `evidence/hardware/artoria-offline/results.json` |
| Sprout 366天离线 | 8 | `evidence/hardware/sprout-offline/results.json` |
| Artoria 30分钟稳定性 | 5 | `evidence/hardware/esp32-s3-devkitc-1/stability-results.json` |
| Sprout 30分钟稳定性 | 5 | `evidence/hardware/sprout-ssd1351/stability-results.json` |
| 真实旧档导入及原数据恢复 | 16 | `evidence/hardware/final-restore/results.json` |

OLED完整套件快照在`*-full`目录；常规环境目录里的`results.json`是后来长稳前的生命周期初始化，不用于替代完整三槽套件。所有列出套件均含末项`suite completed`。阶段命令及退出码保存在`completed-stage-exits.json`、`final-stage-exits.json`、`stability-and-restoration-exits.json`；前两个文件同时保留被修复/重跑的历史失败，不能把其中所有行宣称为0。

## 实测资源

| 环境 | 时长/样本 | 最低当次剩余堆 bytes | SDK历史最低堆 bytes | 堆波动 bytes | 最大块最低值 bytes | PSRAM最低剩余 bytes | 任务栈最低余量 bytes | 自动保存 |
|---|---|---:|---:|---:|---:|---:|---:|---|
| esp32-s3-devkitc-1 | 1800秒 / 900 | 356452 | 351064 | 144 | 319476 | 8333015 | 4532 | sequence advances=6 minimum=5 |
| sprout-ssd1351 | 1800秒 / 900 | 356452 | 351064 | 144 | 319476 | 8333015 | 4772 | sequence advances=6 minimum=5 |

生命周期检查点（含投喂、导入/串门、恢复、重启前采集；不与长稳采样混为同一堆栈）：

| 环境 | 观测SDK最低堆 bytes | 观测最低栈余量 bytes | 最低剩余PSRAM bytes |
|---|---:|---:|---:|
| artoria-serial | 351644 | 3548 | 8365799 |
| sprout-serial | 351644 | 3792 | 8365799 |
| esp32-s3-devkitc-1 | 351064 | 3552 | 8333015 |
| sprout-ssd1351 | 351064 | 3788 | 8333015 |
| artoria-pressure | 342856 | 3552 | 391 |
| sprout-pressure | 342856 | 3792 | 391 |

静态构建资源（不会替代上述实测）：

| 环境 | Flash bytes | 静态RAM bytes |
|---|---:|---:|
| esp32-s3-devkitc-1 | 3743585 | 22488 |
| artoria-serial | 1681805 | 22240 |
| sprout-ssd1351 | 415037 | 22488 |
| sprout-serial | 391529 | 22240 |

最低值是实际已覆盖路径/采样期间观察值，不是所有未来内容和玩法的静态最坏值。完整生命周期的调用栈峰值另见各套件UART日志。

| 包 | 模拟离线时长 | 实际处理秒数 | 重读及重启年龄 | 观测最低堆 bytes | 栈余量 bytes |
|---|---:|---:|---|---:|---:|
| artoria | 366天 | 13.273189 | age_days366 / 显示Day367 | 350980 | 3980 |
| sprout | 366天 | 14.851124 | age_days366 / 显示Day367 | 351132 | 4560 |

离线回调是同步处理，约13–15秒期间不能响应新的UI/串口命令；记录该既有路径的实际成本，没有将其伪装成无卡顿。探针调用生产回调的堆栈深度与物理按钮入口不同；它不能替代真实断电恢复。低PSRAM两包均观测free_psram=391并记录解码器退到8192-byte内部RAM缓冲，生命周期/保存恢复正常；画面降级效果未观察。

30分钟稳定性命令（在对应普通OLED测试固件及专用数据上）：

```sh
/tmp/artoria-hardware-venv/bin/python tests/hardware/run_uart_edges.py \
  --label esp32-s3-devkitc-1 --soak-only --soak-seconds 1800
/tmp/artoria-hardware-venv/bin/python tests/hardware/run_uart_edges.py \
  --label sprout-ssd1351 --soak-only --soak-seconds 1800
```

## 最终设备状态与限制

最终留下普通`esp32-s3-devkitc-1` Artoria SSD1351引擎固件。恢复原始20KB NVS前校验整片备份SHA，写入后在应用启动前读回逐字节比较；应用随后正常执行v9迁移。最终为原Rhongomyniad、HP50、SR100、age_days121、图鉴3/12、设备rounds3、非串门。恢复原电源配置，游戏时间保留旧设备锚点，没有强行推进到电脑当前日期。

真实旧档再次比较：`real-v9-final-comparison.log`；设备镜像hash、状态、电源配置及NVS读回结果：`final-restore/final-state.json`。原始16MB镜像和真实旧档仍在仓库外私人备份目录；未将用户Flash/NVS原始数据放进交付源码。当前固件是引擎，完整原固件镜像仍可从备份取回。

尚待现场：B10实体画面/三键/长按，B12深睡唤醒及专用数据掉电测试。30分钟不证明小时/天级无泄漏；未启用loop任务WDT后的行为没有验收。独立原版维修工作树本轮未同步这些新增引擎修复；不能把引擎板测结果宣称为该工作树实机通过。没有宣布整个引擎集中验收通过。

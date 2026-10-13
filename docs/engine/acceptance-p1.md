# P0 + P1 交付与复验记录

日期：2026-10-12（Asia/Shanghai）。工作目录：`/home/omni/esp32-artoria-tamagotchi`。
基线：`448dbe158595c1db9f1d2fc37caa15810c53de1c`。
本报告供规划/验收模型复核，未宣布整个引擎验收通过，未开始 P2。

## 交付范围

- 生产 `src/pet/seriousness.cpp` 不再依赖 Arduino/Serial。
- `seriousness.h` 增加可选的有类型同步日志 sink；时间沿用调用方传入的 epoch 秒，测试可直接控制。
- `src/platform/arduino/seriousness_logging.cpp` 装配原有 seriousnessSystem 全局符号及 Serial sink，
  保持原日志内容和既有调用路径。真实默认固件已编译、链接此文件。
- `tests/host/seriousness_test.cpp` 调用同一生产实现；run_tests.py 仅编译该生产 cpp 与测试，
  不链接 Arduino、不伪造 Arduino.h、不复制规则。构建输出使用临时目录，自动清理。
- `tests/host/check_legacy_boundary.py` 将 9 个关键原文件与基线 Git 对象逐字节比较，并输出 SHA-256。
- 架构及行为记录见 [architecture.md](architecture.md)、[behavior-baseline.md](behavior-baseline.md)。
- `.gitignore` 原来忽略整个 docs；本轮仅开放 docs/engine 以交付报告，其他 docs 子项仍忽略。

## 工具与构建环境

初始没有 pio/platformio 可执行文件，`python3 -m platformio --version` 报模块缺失。
第一次原始固件尝试 `python3 -m platformio run -e esp32-s3-devkitc-1` 退出码 1，
原因 `No module named platformio`，见 [baseline-initial.log](evidence/baseline-initial.log)。
随后在 `/tmp/artoria-p0/venv` 创建隔离 venv，通过 pip 安装 PlatformIO，成功退出码 0；
没有修改项目依赖配置或安装项目本机配置文件。

实际环境（完整包列表见 [environment.log](evidence/environment.log)）：

| 项目 | 版本/配置 |
| --- | --- |
| 主机 C++ | g++ 16.2.1，C++17 |
| Python | 3.14.7 |
| PlatformIO Core | 6.2.0 |
| 平台 | espressif32 7.1.3 |
| 框架 | framework-arduinoespressif32 4.20017.260907+sha.dcc1105b |
| Xtensa 工具链 | 8.4.0+2021r2-patch5 |
| Adafruit | GFX 1.12.6；SSD1351 1.3.3；BusIO 1.17.4（其余传递依赖见日志） |
| 目标 | esp32-s3-devkitc-1 / Arduino / gnu++17 / 默认 SSD1351 后端 |

platformio.ini 原本没有固定平台和库版本，本轮未修改；未来自动解析版本可能变化，复验应对照环境日志。
本次平台工具下载缓存位于 PlatformIO 默认用户缓存和 .pio（Git 忽略），未加入交付文件。

## 实际执行与退出码

以下命令均从工作目录执行（基线固件构建使用隔离源码目录）。

| 命令 | 退出码 | 结果/证据 |
| --- | --- | --- |
| `python3 -m platformio run -e esp32-s3-devkitc-1`（初始） | 1 | 工具缺失，后已解决 |
| `python3 -m venv /tmp/artoria-p0/venv` | 0 | 临时工具环境 |
| `/tmp/artoria-p0/venv/bin/python -m pip install platformio` | 0 | 安装成功 |
| `/tmp/artoria-p0/venv/bin/pio run -d /tmp/artoria-p0/baseline -e esp32-s3-devkitc-1` | 0 | 原始默认固件 SUCCESS，[完整日志](evidence/baseline-build.log) |
| `/tmp/artoria-p0/venv/bin/pio run -e esp32-s3-devkitc-1` | 0 | P1 默认固件 SUCCESS，[完整日志](evidence/p1-build.log) |
| `python3 tests/host/run_tests.py` | 0 | PASS: 7 cases, 91 checks，[日志](evidence/host-tests.log) |
| `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 tests/host/run_tests.py` | 0 | 同样 7/91，未报告 sanitizer 错误，[日志](evidence/host-sanitizers.log) |
| `python3 tests/host/check_legacy_boundary.py` | 0 | 9 文件 byte-identical，[日志](evidence/legacy-source-guard.log) |
| `g++ -std=c++17 -Isrc -MM src/pet/seriousness.cpp` | 0 | 非系统依赖仅自身头、game_state、game_config；见 environment.log |
| `git diff --check` | 0 | 无空白错误 |

两份固件日志的 `warning:`/`error:` 搜索无匹配（rg 退出码 1 表示无匹配，不是构建失败）。
测试编译开启 `-Wall -Wextra -Werror -pedantic`；仅关闭原有未使用参数的告警（getRhongoState 的 currentTime），
不为告警修改原规则。日志中临时输出路径每次执行不同，属于正常情况。

## 主机用例与基线对应

| 测试函数 | 覆盖 |
| --- | --- |
| interaction | 正常 poke、SR/分级结果、互动时间、生效时间、暂停、余数保留；feed 固定扣减且不延长暂停；无关状态保持 |
| cooldown | 重复 +0/+1/+1799，不扣 SR 但延长暂停；+1800 和独立 +1801 可生效 |
| pauseExpiry | 到期前 1 秒、恰好到期、到期后，余数=4 时恰好增长 |
| longAdvance | 暂停跨越的一小时 batch，手工预期 35→41/余数1；与逐分钟生产 tick 一致；再推一日封顶；零分钟无变化 |
| boundsAndTiers | 扣减封底、idle/惩罚封顶、分级 32/33/65/66/99/100 |
| rhongoAndDiagnostics | 白狮 48h 前一秒/当秒，Start/Trigger/Reset/Penalty 诊断；触发不重复；80 保留计时、79 清除 |
| terminalAndLegacyEdges | 白/黑终态 poke/tick/batch 不结算；原有惩罚不对称；epoch=0 哨兵与倒退无符号差特征 |

预期数字直接取旧 onInteract/onIdleTick/onIdleBatch/updateRhongoTimer/clamp 和 game_config，
推导详见 behavior-baseline.md。没有新增玩法，未纠正已发现的旧异常。
主机用例不覆盖整个菜单、动画或 NVS；7/91 不能解释为全项目行为已回归。

## 构建容量（静态构建数据，非硬件实测）

| 同一构建环境 | Flash / 6,553,600 bytes | RAM / 327,680 bytes |
| --- | --- | --- |
| 原始基线 | 3,724,197（56.8%） | 21,904（6.7%） |
| P1 | 3,724,397（56.8%） | 21,912（6.7%） |
| 差值 | +200 | +8 |

这些数据来自链接后 PlatformIO size 输出，不代表运行时峰值、堆使用、帧时间或功耗。
无新增生产队列、动态分配或每帧复制。未执行任何 upload、erase、串口连接或真实设备导入。

## v9 和串口保护证据的限度

保护脚本核对：game_state.h、save_manager.h/.cpp、game_config.h、gallery.h、main.cpp、
menu_controller.cpp、SaveManager/save_manager.py、platformio.ini。
这些文件的字段顺序、枚举值、版本常量、checksum、memcpy/sizeof、串口解析/发送实现与基线完全一致。
SeriousnessSystem 运行时对象增加一个日志函数指针，不是任何被保存的结构体成员。
规则 cpp 的改动仅替换 Arduino/Serial 依赖及四类诊断，不更改算术、分支、PetState 写入或返回逻辑。

**旧 v9 实际样本测试：未执行。ESP32 v9 布局/偏移独立验证：未执行。**
本轮没有迁移代码，未用主机结构体转储冒充旧档。
源码未变与固件编译成功是本轮保护证据；不能据此宣称旧档兼容已通过样本验证，
也不能证明 NVS 故障、断电中间状态或导入拒绝/回退的运行行为。

## 重新执行

主机验证只需 Python 3 和支持 C++17 的编译器，无需 PlatformIO：

```bash
cd /home/omni/esp32-artoria-tamagotchi
python3 tests/host/run_tests.py
HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 tests/host/run_tests.py
python3 tests/host/check_legacy_boundary.py
git diff --check
```

若没有 PlatformIO，可另建临时 venv。下列命令只构建，不接触设备：

```bash
python3 -m venv /tmp/artoria-review-venv
/tmp/artoria-review-venv/bin/python -m pip install platformio==6.2.0
/tmp/artoria-review-venv/bin/pio run -e esp32-s3-devkitc-1
mkdir -p /tmp/artoria-review-baseline
git archive 448dbe158595c1db9f1d2fc37caa15810c53de1c | tar -x -C /tmp/artoria-review-baseline
/tmp/artoria-review-venv/bin/pio run -d /tmp/artoria-review-baseline -e esp32-s3-devkitc-1
```

基线目录应为空的新目录；原始构建日志采用同样的 git archive 方法。
环境/依赖未锁定，比较 Flash/RAM 时务必使用相同包版本。

## 尚未执行与后续验收清单

- 默认固件硬件运行、SSD1351/按键、RTC/深睡、断电恢复：未执行。
- 串口占位后端构建、串口工具实机往返：未执行，留后续阶段。
- 完整投喂/连携/麻婆/Nobu/成长/日结/离线/UI 停留/图鉴/串门集成测试：未执行，本轮仅源码基线。
- 旧档样本、损坏长度/校验、未知版本/包/ID、有效档回退与不覆盖测试：未执行，P2 必备。
- 通用引擎、新格式、第二内容包及教程：未实施。
- 原有问题及行为不明项见 behavior-baseline.md；本轮没有借重构修改这些问题。

建议评审先验 P1 的实际依赖闭包、测试预期及固件链接，再批准 P2 的最小契约与存档样本计划。
本次交付到此停止，等待规划/验收模型评审。

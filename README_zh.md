[English](README.md) | [简体中文](README_zh.md) | [日本語](README_ja.md)

# 🌱 Embedded Tamagotchi Engine

一个面向嵌入式设备、支持更换内容包的 **C++17 电子宠物引擎**，目前提供 **ESP32-S3** 完整实现。

定义自己的形态、食物、成长规则和资源，复用同一套引擎、存档机制与演出流程，制作属于你的电子宠物。本项目由 [Artoria Tamagotchi](https://github.com/fluphus/esp32-artoria-tamagotchi) 演进而来，内置保留原有玩法的阿尔托莉雅包，以及独立原创示例 **Pocket Sprout**。

## ✨ 特性

- **共用引擎：** 命令处理、时间推进、状态变化、语义事件与存档调度。日历秒与单调毫秒分开处理，时钟和随机源可注入，支持确定性测试。
- **内容包：** 形态和食物使用稳定 ID；普通差异采用生成的数据表，专属状态版本化，特殊规则通过类型明确的 C++ 策略实现。内容包在编译期选择。
- **互动与演出：** 投喂、戳一戳、成长、图鉴和销毁确认；有界动画队列、输入阻塞与页面停留，将规则结算和动画播放分开。
- **持久化：** 三个物理槽、两槽轮转，显式版本化编码、校验和、读回校验，支持阿尔托莉雅 v9 迁移、完整备份与仅宠物串门。受保护的多键操作中断后进入恢复状态。
- **两种显示后端：** 使用 **Adafruit GFX / Adafruit SSD1351** 的 SSD1351 128×128 OLED，以及串口占位显示。可将 PNG 转为固件内嵌资源，缺少美术时使用占位图。
- **主机回归：** 通过注入外围依赖测试真实规则、菜单、工作流、存储和协议路径，覆盖故障注入及重启、中断场景。

## 📦 内置内容包

| 内容包 | 玩法 | 构建选择 |
|---|---|---|
| **Artoria（阿尔托莉雅）** — 默认 | 从 Saber Lily 开始，包含白/黑成长路线、投喂窗口、连携、特殊食物，以及 Rhongomyniad、Nobu 路线。 | `esp32-s3-devkitc-1` / `artoria-serial` |
| **Pocket Sprout** | Seed → Bloom，四种不同食物，通过投喂积累水量；年龄至少一天且水量达到六时成长。Dew + Sunseed + Moonberry 触发特殊食物选择，含 Pebble 的组合不触发连携。 | `sprout-ssd1351` / `sprout-serial` |

两个包复用同一套引擎与存档机制。Sprout 拥有独立成长策略和稳定 ID，并非复制引擎或只改名字的静态演示。

## 🧩 架构与平台支持

```text
应用装配层
├── 引擎核心             标准 C++17 契约与共用机制
├── 内容包               定义、策略与版本化专属状态
├── 平台适配             时钟、随机数、存储与设备生命周期
└── 输入 / 演出 / 显示
```

`src/engine/` 仅依赖标准库和自身契约。当前完整固件面向 **ESP32-S3 / Arduino**，包括 NVS、深睡、GPIO 与屏幕接入。主机运行路径用于测试；移植到其他开发板仍需实现平台适配和应用接入。目前没有运行时内容包加载或脚本虚拟机。

详见[架构说明](docs/engine/architecture.md)与[新增内容包教程](docs/engine/add-content-pack.md)。当前应用支持最多 32 种形态、4–32 种普通食物、1–32 种特殊食物，稳定 ID 范围为 0–254，内容扩展状态最多 64 字节。离线补偿最多支持 366 天，同时受年龄和时间戳容量限制。

## 🛠️ 硬件

现有配置面向 **ESP32-S3 N16R8**：16 MB Flash、8 MB PSRAM、SSD1351 1.5 英寸 128×128 彩色 OLED 与三个按键。按键按下时将对应 GPIO 接到 GND。

| 功能 | GPIO |
|---|---:|
| 左 / 中 / 右键 | 4 / 5 / 6 |
| OLED DC / 复位 / CS | 8 / 9 / 10 |
| OLED MOSI / SCLK | 11 / 12 |

串口占位后端仍运行在 ESP32-S3 固件上，可以不接屏幕；它与本机 C++ 主机测试是两条不同路径。

## 🚀 构建与运行

准备带 PlatformIO 的 Python 环境；主机测试还需要支持 C++17 的编译器（默认 `g++`）和 Git。以下命令在仓库根目录运行。

```sh
python -m venv .venv
# Linux / macOS
. .venv/bin/activate
# Windows PowerShell: .venv\Scripts\Activate.ps1
python -m pip install platformio==6.2.0 Pillow==12.3.0 pyserial==3.5
```

| 环境 | 内容包 | 显示 |
|---|---|---|
| `esp32-s3-devkitc-1` — 默认 | Artoria | SSD1351 |
| `artoria-serial` | Artoria | 串口占位 |
| `sprout-ssd1351` | Pocket Sprout | SSD1351 |
| `sprout-serial` | Pocket Sprout | 串口占位 |

```sh
pio run                                      # 默认阿尔托莉雅 OLED 构建
pio run -e sprout-ssd1351                     # Sprout OLED 构建
pio run -e artoria-serial -e sprout-serial     # 串口后端构建

# 将所选环境烧录到自己的开发板，再打开串口。
pio run -e esp32-s3-devkitc-1 -t upload
pio device monitor -b 115200
```

首次启动或断电后，通过按键确认日期与时间，也可用串口 `SET_TIME <unix_seconds>` 完成设时流程。当前使用软件维护的时钟与基于 RTC 的深睡时间恢复，未实现网络自动校时。

存档绑定内容包。更换内容包前先导出数据；其他包或未知格式的数据会受保护，不会被静默覆盖为新游戏。

## 🎮 操作

| 页面 | L | M | R |
|---|---|---|---|
| 待机 | 开始投喂 | 状态；长按打开图鉴 | 戳一戳 |
| 食物选择 | 上一个食物 | 切换选中状态，选满三个自动提交 | 下一个食物 |
| 特殊食物 | 上一个选项 | 确认 | 下一个选项 |
| 图鉴 | 上一个条目 | 关闭 | 下一个条目 |
| 销毁确认 | YES | 确认 | NO |
| 日期时间设置 | 增加当前字段 | 下一字段 / 确认 | 上一字段 |

状态面板按 M 关闭。同时按住 **L + M + R 五秒**进入销毁确认，默认选择 NO。设时页面长按 L 恢复默认日期。动画播放与结果页面停留期间阻塞玩法输入，特殊食物选择会明确等待输入。

可在 115200 波特率下通过串口探索：

```text
s                    显示当前状态
h                    显示全部可用命令
fl                   显示食物
btn l / btn m / btn r 模拟短按
btnl m               模拟长按中键
ctx                  显示界面与演出状态
save / load          保存或加载当前宠物
reset                重置宠物；串门中则恢复主人
devinfo              显示设备状态及运行内存观测
t 60                 调试：推进60分钟的分钟规则
d                    调试：执行一次日结并推进日期
```

`t` 支持 1–10080 分钟并按有界批次执行，保留原有仅推进分钟规则的调试语义。串门期间限制改时调试命令和再次导入；等待首次设时或存档恢复时，禁止玩法调试操作。

## 💾 备份、串门与恢复

使用实际 Python 工具，将 `/dev/ttyACM0` 替换为自己的串口（Windows 例如 `COM5`）：

```sh
python SaveManager/save_manager.py --port /dev/ttyACM0 status
python SaveManager/save_manager.py --port /dev/ttyACM0 export --slot 0 --out full.bin
python SaveManager/save_manager.py --port /dev/ttyACM0 export --slot 0 --out guest.bin --pet-only
python SaveManager/save_manager.py --port /dev/ttyACM0 import --slot 1 --in full.bin
```

- **完整导入：** 恢复备份内的宠物和图鉴，并将宠物时间戳对齐到本机时钟。
- **仅宠物导入：** 开始串门，冻结最新活动主人槽，访客在另外两个槽轮转。禁止导入到主人槽。`reset` 或确认销毁可结束串门并恢复主人。
- **恢复状态：** 受保护操作未完成时，禁止后台写入及玩法操作。先导出可读槽；若保留了主人/重置前宠物副本，可用 `export --slot -1 --out owner.bin` 或固件命令 `SAVE_EXPORT_OWNER` 取回。
- **明确恢复：** 使用 `SAVE_RECOVERY_ERASE CONFIRM` 后，重新导入已导出的完整备份。该命令会删除存档数据，仅在恢复锁生效时接受；普通启动不会自动执行这一步。

串口导入使用 `SAVE_IMPORT_BEGIN`、`SAVE_IMPORT_DATA`、`SAVE_IMPORT_COMMIT` 与 `SAVE_IMPORT_ABORT`，工具负责分包和响应确认。未知版本、内容包与 ID 会被拒绝。当前 v10 存档使用显式字段编码；旧版阿尔托莉雅 v9 迁移证据见验收报告。

## 🎨 资源与自定义内容

编辑内容包的 `manifest.json`，定义形态、食物、文案和资源映射，再生成内容表：

```sh
python tools/engine/generate_content.py
python tools/engine/generate_content.py --check

# 可选的 PNG 转换，需要 Pillow。
python tools/convert_assets.py --content artoria
python tools/convert_assets.py --content sprout
```

源码已包含生成的内容表和资源。资源转换需手动执行，PlatformIO 预构建钩子默认关闭。阿尔托莉雅资源位于 `assets/`，Sprout 使用 `assets/sprout/`；美术覆盖程度不同，Sprout 使用带名称的占位图。

详见[资源命名说明](assets/README.md)与[新增内容包教程](docs/engine/add-content-pack.md)。新包提供数据和类型明确的 C++ 策略，再注册应用中的构建与资源选择，无需复制引擎。

## 🧪 测试与验证

```sh
python tests/host/check_architecture.py  # 独立引擎依赖及策略检查
python tests/host/run_all.py            # 两包真实生产路径回归
python tests/host/run_all.py --resources
```

完整套件会读取原始 Git 基线，验证 v9 来源与原版玩法一致性；请使用保留该历史的 Git 工作副本，源码 ZIP 不包含所需历史。测试覆盖生命周期、输入和动画、存档损坏、写入失败、中断导入/重置、主人恢复、时间边界及实际 Python 协议工具。ASan/UBSan 命令见报告。

- [验收总览](docs/engine/acceptance.md)
- [最新 D1–D6 修复与回归证据](docs/engine/deep-fixes-2026-10-13.md)
- [规划模型 D1–D6 独立复验](docs/engine/deep-reacceptance-2026-10-13.md)
- [此前实机测试及其版本范围](docs/engine/hardware-acceptance.md)
- [原始行为与保留的时间语义](docs/engine/behavior-baseline.md)

最新修复已完成主机回归与四环境构建，尚未刷写新固件开展新一轮实机测试。实体屏幕/按键、真正的深睡唤醒和写入期间断电仍待验证。最大离线补偿同步执行，此前固件实测处理366天约需13–15秒。

## 📄 许可与致谢

代码采用 [MIT 许可](LICENSE)。项目源自 [Artoria Tamagotchi](https://github.com/fluphus/esp32-artoria-tamagotchi)；阿尔托莉雅是 Fate/Grand Order 主题示例，Pocket Sprout 是原创示例内容包。

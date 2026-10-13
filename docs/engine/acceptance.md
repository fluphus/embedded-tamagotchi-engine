# P2 + P3 + P4 集中验收报告

> 最新执行修复：2026-10-13 [D1–D6定向修复与回归](deep-fixes-2026-10-13.md)。两包各2525条新增生产路径断言、ASan/UBSan、既有存储/导入/主人恢复/协议/集成套件及四环境构建通过；新源码未刷写设备。六项已通过[规划模型定向复验](deep-reacceptance-2026-10-13.md)，以下此前实机11 PASS/2 BLOCKED不代表本轮新固件已实测。

> 2026-10-13 实机自动化补验已完成（11项PASS / 2项现场BLOCKED）：[hardware-acceptance.md](hardware-acceptance.md)。以下“未连接真实设备”及9 PASS/4 BLOCKED为此前离线轮次的历史范围，不再代表当前设备执行状态。最终板测结果以新报告为准。

> 上一轮离线补充验收见 [final-supplementary-acceptance.md](final-supplementary-acceptance.md)：**9 PASS / 0 FAIL / 4 BLOCKED**。按规划决策将 Moonberry(ID42) healthy 改为 true，真实输入严格回归187断言、Sprout规则715断言及两个显示后端构建均退出0；实机项目均未执行。其余项目沿用已通过证据，未重跑无关存储故障矩阵。下方与 offline-regression.md 中“全部通过”仅指当时已覆盖套件，不代表本轮全部验收通过。

日期：2026-10-12（Asia/Shanghai）。目录：`/home/omni/esp32-artoria-tamagotchi`。
原始基线 `448dbe158595c1db9f1d2fc37caa15810c53de1c`；P0/P1 已由规划模型验收，历史报告保存在 [acceptance-p1.md](acceptance-p1.md)。
本报告保留2026-10-12离线集中复核证据，不代替规划模型最终验收；该历史轮次未操作真实设备。2026-10-13在用户明确授权后执行的板测、刷写和恢复见顶部实机补验报告。

## 实现结果

- 同一标准 C++17 引擎负责命令、分钟推进、公共互动机制、语义事件、保存调度、有界动画时间线及显式存档/槽机制。
- 阿尔托莉雅规则和23字节专属扩展属于默认内容包。复杂玩法仍是生产 C++ 策略；不是仅移动目录，公共机制和平台端口已实际进入原固件路径。
- 第二包 Pocket Sprout 有形态 Seed(40)/Bloom(47)、独立食物、8字节积水状态、不同互动参数和至少1日/积水6的成长规则。
  两包都完成生产 facade 的初始化、投喂、poke、推进、成长、图鉴、保存/恢复及 reset 主机路径，并复用相同 app/engine/storage/display。
- main.cpp 拆为装配/主循环与 console/ui_binding/workflows/context；NVS 仅由 Arduino 适配器接触 Preferences。
- 内容 manifest 驱动表、文案及资源映射，图鉴不再固定12；Artoria 214序列保持相同资源，Sprout 使用带名称占位图。
- v10 带显式字段、版本、包身份、稳定 ID；v9 由原始 Git 头文件和 ESP32-S3 工具链生成样本并测试迁移，原 NVS 旧键保留。

设计与容量见 [architecture.md](architecture.md)，字节边界与拒绝政策见 [save-format.md](save-format.md)，新增包流程见 [add-content-pack.md](add-content-pack.md)。

## 环境与复验入口

工具环境沿用 P1 临时 venv `/tmp/artoria-p0/venv`：PlatformIO6.2.0、Python3.14.7、Pillow12.3.0；主机 g++16.2.1。
ESP 平台 espressif32 7.1.3、Arduino framework 4.20017.260907+sha.dcc1105b、Xtensa8.4.0+2021r2-patch5；Adafruit GFX1.12.6/SSD1351 1.3.3。
可在任何临时 venv 安装 platformio/Pillow 后替换下列前缀；主机纯规则检查只需 Python3 与 C++17 编译器。
本轮已锁定实际构建平台、框架、目标工具链与直接使用库；完整版本见最终补充验收和干净构建日志。无项目本机配置或缓存加入交付。

```sh
# 当前解释器需 Pillow；否则省略 --resources 并单独使用带 Pillow 的解释器运行资源检查
python3 tests/host/run_all.py --resources
HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 tests/host/run_tests.py
HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 tests/host/run_engine_tests.py
HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 tests/host/run_storage_tests.py
python3 tools/engine/generate_v9_fixtures.py  # 需已安装 ESP32-S3 目标工具链；非必需的重新生成
pio run -e esp32-s3-devkitc-1 -e artoria-serial -e sprout-ssd1351 -e sprout-serial
```

所有路径从工作目录运行。run_all.py 不运行历史 P1 源码保护脚本，也不会操作硬件。
默认 `pio run` 仅构建 esp32-s3-devkitc-1；另外三个环境显式选择内容包/后端。

## 实际命令及结果

下表中的命令均已执行；退出码0为成功。最终总入口日志含每个子命令及退出码。

| 命令 | 退出码 | 结果 / 日志 |
| --- | --- | --- |
| `/tmp/artoria-p0/venv/bin/python tests/host/run_all.py --resources` | 0 | 全部主机检查，[all-host-final.log](evidence/all-host-final.log) |
| `python3 tools/engine/generate_content.py --check` | 0 | 两包生成表与 manifest 一致（总入口执行） |
| `python3 tests/host/check_architecture.py` | 0 | 10个引擎头独立编译，第三个 MiniPolicy 可运行，无平台/内容导入 |
| `python3 tests/host/check_v9_fixtures.py` | 0 | 原 Git 头及8个目标编译器夹具 SHA256 相符 |
| `python3 tests/host/run_tests.py` | 0 | P1 保留7例91断言 |
| `python3 tests/host/run_engine_tests.py` | 0 | Artoria720 / Sprout712项检查 |
| `python3 tests/host/run_storage_tests.py` | 0 | 实际 SaveManager：Artoria69 / Sprout67项检查 |
| `python3 tests/host/run_compatibility.py` | 0 | 原始/当前35条轨迹完全一致，含全部宠物字段及RNG调用数 |
| `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 tests/host/run_tests.py` | 0 | 7/91，无诊断，[日志](evidence/host-sanitizers-final.log) |
| 同上 flags + `python3 tests/host/run_engine_tests.py` | 0 | 两包720/712，无诊断，[日志](evidence/engine-sanitizers-final.log) |
| 同上 flags + `python3 tests/host/run_storage_tests.py` | 0 | 两包69/67，无诊断，[日志](evidence/storage-sanitizers-final.log) |
| `python3 tools/engine/generate_v9_fixtures.py` | 0 | 原始头+目标编译器提取布局与夹具，[日志](evidence/v9-fixtures-final.log) |
| `/tmp/artoria-p0/venv/bin/python tests/host/check_resources.py` | 0 | 两包重生成逐字节一致，214/0序列；见总入口日志 |
| `/tmp/artoria-p0/venv/bin/pio run -e esp32-s3-devkitc-1 -e artoria-serial -e sprout-ssd1351 -e sprout-serial` | 0 | 四环境SUCCESS，[完整日志](evidence/p4-builds-final.log) |
| `python3 -m py_compile SaveManager/save_manager.py tools/convert_assets.py tools/engine/generate_content.py tools/engine/generate_v9_fixtures.py tests/host/*.py` | 0 | Python语法检查；不涉及串口通信 |
| `git diff --check` | 0 | 无空白错误 |
| `python3 tests/host/check_legacy_boundary.py` | 1（预期） | 首个保护文件已合法迁移，历史工具保持原基线，[日志](evidence/legacy-guard-p4.log) |

P1 guard 的适用范围是“九文件字节不变”，P2–P4 没有删除它或更新基线。兼容验收由目标格式夹具、生产格式/存储测试和旧规则行为对照接替。

## 测试覆盖与证据强度

| 领域 | 已执行验证 |
| --- | --- |
| 默认关键玩法 | P1正常/重复互动、1800秒冷却边界、暂停到期边界、长推进、上下界；原规则35轨迹覆盖普通食物、时间窗、连携、特殊食物、毕业/阵营/分级、白娱乐锁定、白狮、麻婆黑狮、Nobu/Oda、日结 |
| 两包生命周期 | 初始化→投喂/间隔/每日限制→poke→长分钟推进→成长→图鉴→完整保存恢复→reset；一致输入/时钟/RNG的可重复结果 |
| 日历与串门 | 生产日结/离线计划/首次初始化辅助函数、冻结持续时间、主人与导入时间戳对齐；实际SaveManager冻结主人、访客轮转、恢复主人、访客坏档时不误载主人 |
| 新格式 | 宠物/完整往返，字节重编码一致；所有截短长度、坏checksum、未知版本/包/形态/图鉴ID/扩展、非法保留值，拒绝不修改输出 |
| 旧格式 | 目标编译器白成体/白狮/黑狮/Nobu样本迁移，原图鉴/设备/串门设备态读取；未知版本/ID、坏校验/长度拒绝；Sprout拒绝Artoria旧数据 |
| 实际存储 | 三槽活动轮转、序号、粘滞重试、stage写失败/读回损坏/commit失败保留旧记录；最新坏回退有效槽，全坏锁写，未知数据不覆盖，附属数据坏长度/坏设备态/图鉴拒绝并阻止UI自动重写 |
| 表现/容量 | 时间线500ms边界、等待输入、页面节点队列、溢出拒绝、动画不执行规则；图鉴32上限；HEX非法字符/奇数/空/超容量拒绝 |
| 资源与构建 | manifest生成结果一致、映射使用稳定ID、两包资源逐字节再现；两包×SSD1351/串口占位四种固件实际链接 |

主机测试编译真实生产 facade、内容实现、AnimationQueue、SaveManager，不复制一套测试规则，不模拟整个Arduino。
原始行为对照侧仅使用两份最小日志/随机声明 shim；当前生产侧不使用它们。
八个v9夹具来自原始固件头与目标编译器，**不是实机旧档**。来源与每个偏移/SHA256见 tests/fixtures/v9/manifest.json。
此证据支持这些样本的迁移及格式拒绝测试，不声称所有历史设备版本均兼容。

## 构建容量

静态链接报告，不是实机峰值RAM/栈测量。可用RAM327680字节，应用分区6553600字节。

| 包 / 后端 | Flash字节 | 静态RAM字节 | 结果 |
| --- | ---: | ---: | --- |
| 原始Artoria / SSD1351（P0） | 3724197 | 21904 | SUCCESS |
| P1 Artoria / SSD1351 | 3724397 | 21912 | SUCCESS |
| 最终Artoria / SSD1351 | 3737821 | 22464 | SUCCESS |
| 最终Artoria / 串口占位 | 1675969 | 22216 | SUCCESS |
| 最终Sprout / SSD1351 | 409601 | 22456 | SUCCESS |
| 最终Sprout / 串口占位 | 386017 | 22208 | SUCCESS |

默认包相对原始Flash +13624 / 静态RAM +560字节。Sprout没有链接Artoria像素大表。
最终增量构建日志没有warning/error；原始基线及P1完整构建日志保留在evidence。

## 遇到的问题及处理

初始P0缺PlatformIO，安装临时venv后解决，见历史报告。
原始串口占位构建实际因renderer缺AnimState声明失败（退出1），[原始日志](evidence/baseline-serial.log)；本次补显示类型include使该后端可构建。
开发中出现资源空表声明/查找函数名及新模块链接问题，修复后由最终四组合构建复核；未通过删除功能绕过。
必要的数据保护变化：损坏/未知启动数据禁止自动覆盖；主人读取失败保留串门和主人档。其依据是任务书要求，未改变正常玩法。
旧epoch零哨兵、倒退无符号差、日结清暂停、导入未来时间clamp和终态惩罚不对称均记录并保留，见 behavior-baseline.md。

## 未执行与剩余限制

- 未执行真实ESP32烧录、屏幕视觉/按键操作、UART往返、导入导出、实机睡眠唤醒、断电恢复及NVS故障注入；未操作设备。
- 未取得真实旧档；目标编译器样本不能代表所有运行时padding/历史编译选项/损坏来源。
- NVS失败不覆盖保证建立在单键原子替换契约上；尚未实测芯片掉电。一次多键操作不是全局事务。
- 未实测动态峰值RAM、栈及长离线耗时；分钟推进仍按旧语义O(minutes)。
- UI保留旧PetState投影及旧演出结果类型，核心状态/新存档没有角色专属字段。选包注册和资源宏在应用/工具中是编译期配置，非动态插件。
- 串口占位后端仅报告编译及纯HEX编解码通过，不能解释为完整串口硬件运行验收。默认规则35条轨迹是关键回归，不是全部状态穷举。

集中验收建议：复跑总入口、目标夹具生成及四环境构建；核查核心依赖方向、包切换和扩展编码；对照旧规则轨迹与失败保护断言。
后续获得授权及设备后再安排真实旧档、屏幕/按键/UART、睡眠及掉电验证；本轮没有执行这些操作。


## 集中验收反馈：五项定向审计修订

已修复图鉴索引/稳定ID混用、串门唯一访客恢复将主人槽重新加入活动对、仅附属数据被当作新游戏、导入失败假成功、有效档回退后未知槽被当空槽覆写。
详见 [targeted-audit.md](targeted-audit.md) 的逐项PASS/FAIL依据、复现条件、生产测试函数、故障矩阵、恢复操作及最终构建数据。

- 定向生产路径套件：Artoria964 / Sprout762断言，ASan/UBSan通过，退出0。
- 受影响的原实际SaveManager套件：69/67断言，退出0。
- 修改后四环境构建全部SUCCESS，退出0；未重跑资源和不受影响的规则套件。
- 导入部分失败用6字节持久恢复标记锁写；原记录可以导出。需用户明确清理后再恢复，未声称自动回滚。
- 不操作设备，不声明实机或集中验收通过。


## 结束串门恢复主人：遗漏路径定向修订

前轮五项已由规划模型复验。本轮只修复串口 reset / 按键销毁结束串门的持久化错误处理；两入口共用 `app::restoreVisitOwner`，全部必要写入成功后才提交宠物/设备RAM并宣布成功。失败或中断使用已有持久恢复标记跨重启锁写；恢复前主人记录有界保留在 `owner_restore`，最后明确写回主人槽并保证最新序号。主人不可读时保留数据、报告需要恢复，移除错误的强制结束/重置访客文案。

逐项矩阵、生产测试位置、七个写入断点及恢复限制详见 [visit-restore-audit.md](visit-restore-audit.md)。

| 本轮实际命令 | 结果 | 退出码 |
| --- | --- | ---: |
| `python3 tests/host/run_audit_tests.py --case 6` | 两包各493断言；三指定失败点、额外失败点、七个重启断点、正常后续保存恢复均PASS | 0 |
| `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 tests/host/run_audit_tests.py --case 6` | 两包各493断言；无ASan/UBSan报告 | 0 |
| `python3 tests/host/run_audit_tests.py --case 4` | 受影响的共享标记/生产导入回归，两包各409断言 | 0 |
| `python3 tests/host/run_storage_tests.py` | 公共存档回归69/67断言 | 0 |
| `/tmp/artoria-p0/venv/bin/pio run -e esp32-s3-devkitc-1 -e artoria-serial -e sprout-ssd1351 -e sprout-serial` | 两包/两后端SUCCESS | 0 |
| `git diff --check` | 无空白错误 | 0 |

最终本轮Flash/静态RAM字节：Artoria SSD1351 3739881/22464；Artoria串口1678057/22216；Sprout SSD1351 411309/22456；Sprout串口387737/22208。完整日志归档 `evidence/visit-restore-*.log`，本小节数据取代此前构建数据作为当前结果。

未重跑无关资源/图鉴/全量规则检查。未执行实机、真实串口、按键屏幕、NVS掉电及栈峰值验证；没有操作真实设备。本轮失败恢复仍采用明确人工恢复策略，无自动回滚；单键原子替换契约及相关限制未变。v9/v10宠物/设备编码和串口格式未改，不宣称集中验收或实机已通过。


## 原有缺陷修复后的完整离线复验

本轮完整执行当前总入口（包括最新结束串门恢复主人case6），而非只沿用此前定向结果。前两轮修复已覆盖上述原有存档缺陷，本轮无新增生产代码修改，未发现需追加修复的回归。

- `run_all.py --resources`：退出0，两包引擎720/712、存档69/67、六项审计1457/1255、P1 7例91断言；35条原规则轨迹一致，v9来源/哈希和资源重生成检查通过。
- `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' run_all.py`：退出0，无ASan/UBSan诊断。
- 四环境 `pio run`：退出0，全部SUCCESS，增量构建；大小与上一轮恢复主人修复后相同。
- `git diff --check`：退出0。

实际完整命令、原有问题来源与当前测试对应、日志及限制见 [offline-regression.md](offline-regression.md)。本轮未操作设备、未重新生成v9夹具，也未改变已记录的旧玩法异常。此结论限于离线回归，不替代规划模型或实机验收。

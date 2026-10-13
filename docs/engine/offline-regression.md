# 原有存档缺陷修复后的完整离线回归

执行日期：2026-10-12。原始提交：448dbe158595c1db9f1d2fc37caa15810c53de1c。
本轮范围为上一轮对话确认的原有存档/串门缺陷及其当前生产修复，不扩展为改变旧玩法语义。相关修复已在前两轮进入工作区，本轮没有重复修改生产代码；实际执行完整主机回归、故障注入、资源检查、sanitizer套件及四环境构建，未发现需追加修复的问题。工作区原有未提交改动全部保留。

## 原有问题与当前验证

来源判断依据 `git show 448dbe1:src/main.cpp` 及 `src/core/save_manager.cpp` 静态对照，不冒称重新运行了原固件的全部故障实验。

| 原有缺陷 | 原始实现依据 | 当前生产修复/测试 | 本轮 |
| --- | --- | --- | --- |
| 唯一有效访客导致主人冻结槽参与轮转 | save_manager.cpp:193–195 无串门条件重建普通活动对 | load保持访客活动对且逐槽排除冻结主人；audit case2 | PASS，两包各38断言 |
| 仅附属数据被当新游戏 | save_manager.cpp:639–646 hasSave只看有效宠物头 | 原始键存在即保护启动，附属数据拒绝自动覆盖；audit case3 | PASS，两包各184断言 |
| 导入失败假成功 | main.cpp:1251–1257、1317–1323 保存失败仍输出成功；persistDeviceState丢弃结果 | 生产commitImport逐步检查、候选RAM、持久恢复标记；audit case4 | PASS，两包各409断言 |
| 未知记录被当空槽覆盖 | save_manager.cpp:594–608 非法头按sequence0选写入槽 | 只写真正空槽/本包有效记录；未知版本、未知包保留；audit case5 | PASS，两包各102断言；跨包场景是重构新增保护要求 |
| 恢复主人失败后仍结束串门 | main.cpp:372–387、807–825 提前提交RAM/设备态，最终保存失败仍报成功 | 两入口共用restoreVisitOwner；备份主人、跨重启锁写、成功后提交；audit case6 | PASS，两包各493断言 |

两个主机内容包均调用实际生产代码，不复制规则/恢复流程。case6含slot_cfg、device_v10、最终主人写入失败，七个持久化完成断点后重启，以及正常恢复后连续保存/重启。此前图鉴非连续稳定ID问题也纳入全套case1回归，Artoria231/Sprout29断言通过；它属于重构适配问题，与上表原有问题区分。

## 本轮实际命令、结果与退出码

仓库根目录执行，使用已存在的临时venv，无设备连接命令。

| 命令 | 退出码 | 结果/证据 |
| --- | ---: | --- |
| `/tmp/artoria-p0/venv/bin/python tests/host/run_all.py --resources` | 0 | 所有子检查退出0，[完整日志](evidence/offline-regression-all.log) |
| `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' /tmp/artoria-p0/venv/bin/python tests/host/run_all.py` | 0 | 所有子检查退出0，无ASan/UBSan诊断，[完整日志](evidence/offline-regression-sanitizers.log) |
| `/tmp/artoria-p0/venv/bin/pio run -e esp32-s3-devkitc-1 -e artoria-serial -e sprout-ssd1351 -e sprout-serial` | 0 | 两包/两后端均SUCCESS，[完整日志](evidence/offline-regression-builds.log) |
| `git diff --check` | 0 | 无空白错误 |

全套主机结果：生成内容表检查通过；10个引擎头及独立最小策略通过；原始Git头和8个目标编译器v9夹具哈希通过；P1 7例91断言；两包引擎720/712断言；公共存档69/67断言；六项定向审计1457/1255断言；原始/当前35条玩法轨迹一致（包括字段及RNG调用数）；两包资源在临时目录重生成与现有文件逐字节一致，Artoria214序列、Sprout占位资源0像素序列。

构建为增量编译/链接及大小检查，没有执行clean完整重建。Flash/静态RAM字节：

| 环境 | Flash | 静态RAM |
| --- | ---: | ---: |
| Artoria SSD1351 | 3739881 | 22464 |
| Artoria串口占位 | 1678057 | 22216 |
| Sprout SSD1351 | 411309 | 22456 |
| Sprout串口占位 | 387737 | 22208 |

## 未执行及边界

- 没有操作真实设备；屏幕/按键/UART、实际掉电、真实NVS故障、栈峰值均未执行。主机故障注入依据既有单键原子替换契约。
- 本轮验证现有v9夹具来源/哈希和迁移路径，没有重新生成目标工具链夹具，也没有获取真实旧档。
- 原始/当前35条对照针对正常玩法；危险持久化行为已按要求修复，不要求它们与原缺陷保持一致。
- 日历倒退、epoch0哨兵、终态惩罚不对称等旧语义仍按此前任务要求保留，见behavior-baseline.md；本轮未自行制定新玩法或时间政策。
- 失败后保持恢复锁和原记录，不提供跨键自动回滚；需要明确人工恢复，操作说明见visit-restore-audit.md及targeted-audit.md。

本轮结论：上述已确认原有存档缺陷的修复通过当前完整离线回归；不等同于规划模型最终验收或实机验收，也不声称项目所有历史缺陷已消除。

# 结束串门、恢复主人：定向修复证据

本轮只处理串口 reset 和按键销毁结束串门的遗漏路径。前轮五项已由规划模型复验，本轮不重跑无关规则、资源及图鉴检查。没有操作真实设备。

## 缺陷与生产路径

用户提供的原缺陷：主人槽0 HP50，访客槽1 HP71、槽2 HP82；先切换活动对并写非串门设备态，最后 s0_v10 写失败，重启错误地加载 HP71 且 is_visiting=false。本轮未再次运行旧实现；上述旧结果来源于验收反馈。

`src/app/workflows.cpp::doReset` 和 `src/app/ui_binding.cpp::onDestroyExecuted` 现均调用 `app::restoreVisitOwner`，实现位于 `src/app/import_workflow.cpp`，无独立测试版逻辑。入口不再自行复制恢复、忽略返回值或提前宣布成功。正常销毁的其他分支未改。

恢复顺序：

1. 写现有 `import_guard` 为 pending，冻结主人槽；复用之前通过审计的启动锁写机制。
2. 验证主人并将恢复前记录保存、读回核验到 `owner_restore`。
3. 只修改候选宠物，沿用原有冻结时间补齐、当日投喂重置规则。
4. 持久化恢复后的活动槽对。
5. 持久化候选非串门设备态。
6. 明确写回主人槽：stage 写入、读回校验、committed 替换。序号高于所有现存槽，避免访客成为最新档。
7. 将 `import_guard` 写为 completed。
8. 提交宠物/设备 RAM、更新时间基准、输出 `Owner restored`。

任何返回错误均置 `recoveryRequired`、输出具体失败阶段，不提交宠物和设备 RAM，不宣布成功。管理器内部活动对等暂存配置可能已变化，但所有普通写入被锁。中断后以 NVS pending 标记识别 Recovery，不能将已经非串门的设备态与旧访客组合为普通主人。stage 从不作为启动加载来源。

`owner_restore` 是一个有界、完整自描述的恢复前 v10 记录，最多 `engine::MaxSaveBytes`，v10 来源逐字节保留；v9 来源经既有目标布局迁移得到规范 v10 副本，原 v9 hdr/dat/gal 键仍不改写。副本在成功后也保留，直到下一次明确发起恢复覆盖它，或用户明确删除全部存档。它不是普通槽，不参与轮转/自动加载；本轮没有新增自动回滚。它也纳入 hasSave，避免只剩该副本时被当作新游戏覆盖。宠物、设备态、旧串口协议的编码未变。

若最后主人写入成功、但完成标记失败/中断，槽0已有补齐时间的主人，原始字节仍保存在 `owner_restore`，启动仍为 Recovery。若主人不可读，保留损坏记录及串门 RAM，输出保存数据/需要恢复；已删除错误的“Force-ending visit, guest reset as new pet”文案及对应无实际重置行为的旧辅助函数。

## 定向测试结果

生产测试：`tests/host/audit_test.cpp::restoreOwner`（case 6）。真实调用 `restoreVisitOwner`、SaveManager、启动判断 `loadStartup` 和后续普通保存。Fake KeyValue 只模拟存储和失败，不复制恢复实现。

每个场景均建立三个实际编码槽，HP分别50/71/82、序号1/2/3，设备串门、活动对1/2，从实际启动路径加载访客82后再恢复。两包结果一致：

| 注入/场景 | 实际结果 | 结果 |
| --- | --- | --- |
| slot_cfg 失败 | RAM82/串门；设备仍串门；主人槽原字节不变；重启Recovery | PASS |
| device_v10 失败 | RAM82/串门；设备仍串门；主人槽原字节不变；重启Recovery | PASS |
| 最终 s0_v10 失败 | RAM82/串门；持久设备已非串门；主人槽原字节不变；重启Recovery，拒绝访客普通加载 | PASS |
| owner_restore 写失败 | 原主人槽不变，RAM不提交；pending保护跨重启 | PASS |
| s0_stage 写失败 | 原主人槽不变，RAM不提交；pending保护跨重启 | PASS |
| 完成标记写失败 | 原主人副本保留、槽0为HP50；RAM仍访客；重启Recovery | PASS |
| 主人记录损坏 | 原损坏字节保留；无强制结束/重置日志；重启Recovery | PASS |
| 正常结束 | RAM主人50、非串门、出生时间按原规则+3600；成功日志只在最后输出 | PASS |
| 正常结束后4次保存/重启 | 始终加载主人50、非串门 | PASS |
| 仅剩 owner_restore 副本 | hasSave=true，启动Recovery，副本不被自动写入覆盖 | PASS |

每个失败场景同时验证：无成功日志、RAM编码/设备字段不变、拒绝自动保存/宠物保存/图鉴保存/设备保存、拒绝绕过标记单槽加载、拒绝直接重试恢复且NVS不变。已损坏主人以外的失败场景仍可从冻结槽导出主人50用于人工恢复。

模拟中途重启不是在底层只调用 decode：KeyValue 在一次原子写实际完成后抛出测试专用 PowerLoss，立即终止整个生产流程，再建立新的 SaveManager，走 `init`/`loadStartup` 及自动保存保护。每包逐个覆盖：

| 最后完成的持久化步骤 | 重启结果 |
| --- | --- |
| 1 pending import_guard | Recovery；原主人槽保留 |
| 2 owner_restore | Recovery；原主人副本逐字节保留 |
| 3 slot_cfg | Recovery |
| 4 device_v10（已非串门） | Recovery；不加载访客 |
| 5 s0_stage | Recovery；原主人槽不变 |
| 6 s0_v10 | Recovery；原主人副本保留，槽0主人50 |
| 7 completed import_guard | Loaded主人50/非串门；随后保存并重启仍主人50 |

断点后、完成标记之前均无成功日志、RAM尚未提交；每次保护启动后还验证普通写入被拒绝且NVS字节不再变化。断点7代表所有必要持久化已完成、RAM提交前掉电，可安全加载新主人。

## 实际命令与退出码

在仓库根执行，完整输出归档于 `docs/engine/evidence/visit-restore-*.log`：

| 命令 | 结果 | 退出码 |
| --- | --- | ---: |
| `python3 tests/host/run_audit_tests.py --case 6` | Artoria493 / Sprout493断言 | 0 |
| `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 tests/host/run_audit_tests.py --case 6` | 两包各493断言，ASan/UBSan无报告 | 0 |
| `python3 tests/host/run_audit_tests.py --case 4` | 共享恢复标记的实际完整导入/串门导入回归，两包各409断言 | 0 |
| `python3 tests/host/run_storage_tests.py` | 受影响的公共SaveManager回归69/67断言 | 0 |
| `/tmp/artoria-p0/venv/bin/pio run -e esp32-s3-devkitc-1 -e artoria-serial -e sprout-ssd1351 -e sprout-serial` | 四环境SUCCESS | 0 |

构建Flash/静态RAM字节：Artoria SSD1351 3739881/22464；Artoria串口1678057/22216；Sprout SSD1351 411309/22456；Sprout串口387737/22208。此次增量构建日志无 warning/error，不代表硬件运行。

## 限制与恢复

不执行实机按键、真实UART、NVS故障/掉电或栈峰值测量。主机调用两个入口共用的生产函数；两入口的Arduino装配以四环境编译核验，不声称主机执行了整个Arduino界面。

故障注入依据现有单键原子替换契约；不是跨键事务，也没有新增自动回滚。失败后停止自动写入，需明确人工恢复：保持原存储先导出可读主人槽备份，再沿用已存在的显式恢复/删除操作。历史轮次中 `owner_restore` 仅作附加保留证据；最终补充验收已增加并测试 `SAVE_EXPORT_OWNER` / Python `export --slot -1`，无需先擦除即可导出该键；当前主人槽始终仍是原主人或已成功写入的同一主人。已损坏原主人不能凭空恢复，本轮不清除它。

未重跑无关的资源生成、图鉴及全部规则套件；不宣称集中验收或实机已通过。

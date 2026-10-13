# 集中验收后的五项定向审计

本轮仅修复指定缺陷及建立真实生产路径的回归检查，未扩展引擎/内容包架构、修改玩法或资源。
下列 PASS 是本轮主机定向检查结果，不是集中验收或实机通过声明。
修复前日志来自本轮开始时的工作区（已有 P2–P4 实现尚未提交），不是原始 P0 Git HEAD。

## 结论、复现条件与测试位置

测试入口：`tests/host/run_audit_tests.py`，实现：`tests/host/audit_test.cpp`。
每个内容包编译一次真实 SaveManager、COMMIT 流程、GallerySystem 和原内容规则；各审计项独立进程运行。
没有复制规则、存档写入实现或 Arduino 仿真环境。Store 仅是可注入故障的内存 KeyValue 适配器。

| 项目 | 修复前证据 | 修复后结果 | 对应测试函数 |
| --- | --- | --- | --- |
| 1 稳定ID图鉴 | 确定缺陷：两个drawGallery及串口光标均直接把索引转Form | PASS；实际显示模型绑定和两后端共用的cell投影验证Artoria全部12个原名称/ID；Sprout Seed40/Bloom47的名称、锁定/解锁和光标一致 | galleryRendering |
| 2 冻结主人槽 | 两包均动态复现FAIL：主人0坏、访客1唯一有效、2为空时，连续保存覆写0 | PASS；另覆盖2损坏。重启后活动对不含0，连续6次访客保存均逐字节保留0；损坏槽不当空槽 | frozenOwner |
| 3 仅附属数据 | 两包均动态复现FAIL：hasSave=false，会进入setup新游戏分支 | PASS；旧gallery/dev_state和新gallery_v10/device_v10，分别覆盖合法、截短、非法值/未知包、额外长度/未知版本。setup实际使用的loadStartup走Recovery；随后自动调度、宠物/图鉴/设备写入均被拒绝，原数据保留 | sidecars |
| 4 导入失败假成功 | 源审确认原console忽略gallery/slot_cfg/device/final save错误，继续输出成功；未在主机运行原整段Arduino console | PASS；两包×完整/串门×5个失败点注入，捕获实际COMMIT输出，检查RAM、NVS、重启保护和显式恢复；另有两种正常导入对照 | imports |
| 5 有效档混合未知档 | 两包均动态复现FAIL：有效槽加载后，未知槽按seq0被覆盖 | PASS；未知版本和另一包的真实有效记录，分别放槽0/1；6次自动保存及再次重启后，未知槽逐字节不变 | unknownSlots |

旧 gallery/dev_state 没有包身份/版本字段，不能声称测试其不存在的“未知版本”。旧键检查合法、非法长度、非法bool/多余字节；Sprout将Artoria旧键拒绝为不兼容。
新键的未知包/版本测试重新计算校验，避免把身份拒绝混同为坏校验。
第5项另一内容包记录由另一个已编译的内容包测试进程调用生产显式编码器生成，临时目录传递；不是只改一个包ID字节冒充另一包完整有效档。
修复前2/3/5日志见 [audit-before.log](evidence/audit-before.log)，修复后各项及失败详情见 [audit-sanitizers.log](evidence/audit-sanitizers.log)。

## 生产修复范围

1. display/gallery_view.h提供bindGallery和galleryCell；DisplayManager使用前者，串口/SSD1351的行、名称、解锁、光标使用后者。
   最终形态由content::formAt获取，两个后端不再保留索引强转路径。主机验证该实际数据投影；绘图API由四环境构建复核。
2. SaveManager::load在串门时保持访客活动对，唯一有效槽/全槽回退不套用普通导入选槽。
   最终写入筛选另行排除冻结主人槽，不依赖其校验有效；损坏主人原始字节仍受保护。
3. hasSave纳入附属旧/新键及未完成导入标记；setup和测试共用app/startup.h的启动判断。
   只有附属数据、没有可恢复宠物时进入恢复锁，禁止新游戏的自动持久化。
4. 从console.cpp提取app::commitImport，串口COMMIT和故障注入调用同一生产函数。
   保存候选RAM状态，逐项检查返回值；仅完成全部写入后提交RAM和输出成功。
   setActivePair*/syncGlobalGalleryToSlot现在返回SaveResult；persistDeviceState返回检查结果并委托同一persistDeviceChecked函数，失败记录诊断并锁写。
   串口时钟对齐先规划时间，成功后再修改TimeManager；删除成功输出后的额外未检查设备态写入。
5. 自动目标仅能是无原始数据的空槽或本包已验证有效槽；未知版本、另一内容包、损坏数据均不作为seq0空槽。
   粘滞重试也重新检查目标资格，无法写入时明确返回失败。

## 导入失败矩阵与恢复语义

每个包均执行以下完整/串门两种流程。基准：原宠物H50、轮次4、非串门，主人/原档在槽0，导入目标1，最终对齐保存目标2；导入宠物H81，时差+3600秒。
每条故障的具体输出、RAM活动对、NVS键长度/内容校验摘要都在日志的OBSERVED行中；字节不变结论来自vector逐字节断言，而非仅比较摘要。

| 注入点 | 实际串口文本阶段 | 失败后的NVS | RAM与重启 |
| --- | --- | --- | --- |
| gallery_v10写失败 | FAILED stage=gallery | 槽1已提交初始导入宠物；图鉴、slot_cfg、设备仍旧；无最终槽2 | 宠物/图鉴/设备RAM均为导入前值；guard有效，重启Recovery |
| slot_cfg写失败 | FAILED stage=slot_cfg | 槽1初始宠物和新图鉴已提交；slot_cfg仍旧，设备仍旧 | RAM内容未提交；RAM活动对可能已更新但所有自动写入锁住；重启Recovery |
| device_v10写失败 | FAILED stage=device_v10（另有Device错误诊断） | 槽1及其绑定图鉴、slot_cfg已提交；设备仍旧 | RAM内容未提交；重启Recovery |
| 时间对齐后最终保存失败 | FAILED stage=final_save | 槽1、图鉴、slot_cfg、设备已提交；槽2仅stage，没有committed记录 | RAM内容未提交；即使NVS设备已是访客也不继续加载或自动保存；重启Recovery |
| 导入完成标记写失败 | FAILED stage=guard_complete | 所有导入记录可能已提交，但guard仍未完成 | 仍然失败、保持RAM旧值并锁写；重启Recovery |

所有失败均没有Full backup restored或Visit started；串门流程槽0原始字节在各失败点、后续拒绝写入及重启中不变。
正常对照确认时间戳+3600、H81、轮次5、完整/串门标志、图鉴、重启加载；串门正常导入后继续保存也不改主人0。

新增NVS import_guard只有6字节：magic 0xa7、状态、主人槽、版本1、前4字节rotate/add校验。
导入开始前写入未完成标记，最终写入成功后才写完成标记；长度、身份、版本或校验不合法一律视为待恢复。
它不是多键事务或自动回滚：部分成功记录保留，失败后停止一切自动持久化，重启不会把它们误报为完整导入成功。
load/loadFromSlot/importSlotRaw不能在标记未完成时隐式解除保护；读取/导出原槽仍可用。

明确恢复步骤：先通过既有按槽导出保存有效主人/其他记录；用户可显式执行`SAVE_RECOVERY_ERASE CONFIRM`，仅恢复锁状态接受该命令，清理所有数据后再完整导入所保留备份。
该命令不会自动执行。本轮在内存NVS中测试了“导出槽0→显式erase→生产完整COMMIT→再次重启加载”，确认可以恢复H50原宠物。
没有自动选择丢弃主人，也不把部分写入冒称已回滚。无用户清理动作时，数据保持并持续锁写。

## 实际验证及退出码

本轮没有在修改前重跑已通过的主机全套、资源检查或四环境构建；先仅执行新增的2/3/5复现条件。
修复后仅运行受影响的定向套件、实际存储回归和必要构建。

| 命令 | 退出码 | 结果 |
| --- | --- | --- |
| `python3 tests/host/run_audit_tests.py --reproduce`（修复前测试版本） | 1 | 两包2/3/5均FAIL，见audit-before.log；当前脚本同参数运行的是已修复代码，不会恢复历史源码 |
| `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 tests/host/run_audit_tests.py` | 0 | Artoria964 / Sprout762断言，所有5项PASS，无ASan/UBSan诊断 |
| `python3 tests/host/run_storage_tests.py` | 0 | 两包原有实际存储检查69/67项通过，[日志](evidence/audit-storage-regression.log) |
| `/tmp/artoria-p0/venv/bin/pio run -e esp32-s3-devkitc-1 -e artoria-serial -e sprout-ssd1351 -e sprout-serial` | 0 | 四环境SUCCESS，[日志](evidence/audit-builds.log) |
| `python3 -m py_compile tests/host/run_all.py tests/host/run_audit_tests.py` | 0 | 修改后的测试入口语法检查 |
| `git diff --check` | 0 | 无空白错误 |

定向测试编译真实DisplayModel时，g++16会对其既有clear()的memset产生class-memaccess警告；仅该定向入口关闭此旧警告，其余-Wall/-Wextra/-Werror继续启用。
未为本轮审计修改DisplayModel::clear或其语义。run_all.py已纳入run_audit_tests，供后续完整验收使用；本轮没有因此重跑无关套件。

| 最终构建环境 | Flash字节 | 静态RAM字节 |
| --- | ---: | ---: |
| esp32-s3-devkitc-1 | 3739613 | 22464 |
| artoria-serial | 1677805 | 22216 |
| sprout-ssd1351 | 411029 | 22456 |
| sprout-serial | 387465 | 22208 |

## 未验证项

未运行真实屏幕、按键、UART、NVS芯片掉电/故障或烧录；没有操作设备。
串口文本通过生产诊断端口捕获（固件端口装配为Serial输出）；没有把主机文本捕获写成真实UART往返。
主机执行的是setup实际使用的启动决策和console实际使用的完整COMMIT持久化流程；Arduino命令读取循环、TimeManager运行时设时和菜单切换本轮仅源审/编译复核。
多键导入没有全局原子性或自动回滚，保护依赖单键原子替换契约；真实掉电条件仍未实测。
此前资源生成、默认玩法轨迹、v9夹具及其他引擎检查本轮未重跑，沿用上轮证据；本轮未改其实现、格式或资源。

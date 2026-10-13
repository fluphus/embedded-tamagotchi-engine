# 引擎边界与实现（P0–P4）

基线提交 `448dbe158595c1db9f1d2fc37caa15810c53de1c`；日期 2026-10-12。
此文记录实现，最终验收由规划模型负责。P1 历史报告保存在 acceptance-p1.md。

## 原有耦合及当前依赖

原 main.cpp 同时负责串口、时间、NVS 导入导出、日结与演出装配；PetState 混合角色路线、公共指标和存档 ABI。
规则直接调用 Serial/esp_random，显示按 12 形态与食物数组序号查资源；存档直接转储结构体。
当前实际依赖为：

```text
main / app（装配、日历和串门流程、串口、兼容视图）
 ├─ engine（标准 C++17 契约与通用机制）
 ├─ content/selected → artoria 或 sprout → engine 契约
 ├─ platform/arduino → engine Ports / KeyValue
 └─ input / presentation / display → 内容定义与状态快照
```

`src/engine` 的 10 个头文件只包含标准库及同目录契约。check_architecture.py 检查引用方向，并使用独立 MiniPolicy 编译运行全部头文件；不靠禁用生产代码通过检查。

## 通用机制与内容策略

- contract.h：State、稳定 Id、命令、语义事件、Gallery、有界队列和 Ports。没有 rhongomyniad/mapo/nobu 字段。
- engine.h：同一个 `Engine<Policy>` 处理命令、分钟推进、状态差事件及单调时钟存档调度。策略静态绑定，不引入虚函数体系。
- physiology.h：互动扣减、冷却、暂停与分钟余数机制；不同包提供 InteractionRules。
- sampling.h：有限池抽取不重复食物，随机数来自 Ports。
- bytes/save_codec/slot_store/storage：显式编码、版本、包身份、拒绝检查、三槽选择、暂存验证与原子键写入契约。
- timeline.h：固定容量非阻塞动画时间线，presentation/AnimationQueue 提供现有节点适配。
- diagnostics.h：可选同步诊断端口，有界格式化；没有 Serial 依赖。

阿尔托莉雅的实际 feeding/evolution/seriousness 位于 content/artoria，复杂路线保留为类型明确的 C++ 策略。
其阵营、连携、时间窗、麻婆、白狮、Nobu 等状态由该包的 state_codec.h 显式编码为版本 1 的 23 字节扩展。
原创 Pocket Sprout 策略使用独立 8 字节 WaterState 扩展，食物 ID 17/23/42/90，形态 ID 40/47；积水至少 6 且年龄至少 1 日才从 Seed 成长为 Bloom。
它具有不同的互动冷却/暂停/增长参数，无阿尔托莉雅特殊路线；复用相同引擎、三槽机制、菜单、图鉴及显示流程。
更换构建环境只选择内容策略和资源表，未修改或复制 engine。

manifest.json 提供形态、名称、稳定 ID、资源 key、食物、特殊食物、参数、分级/阶段文案。generate_content.py 生成选包定义、配置与食物表。
运行时数组下标仅作菜单的临时选择；进入命令与存档时转换为稳定 ID，未知 ID 不允许截断成有效食物。

## 应用与平台装配

main.cpp 保留启动和主循环；app/context 管理共享装配，console 负责串口，ui_binding 负责 UI 回调，workflows 负责日历/离线/串门流程。
app/day_rules 与 time_rules 是实际流程使用的可测试纯函数；其规则调用经 runtime/生产 facade 进入同一引擎。
原 seriousnessSystem 符号由 app/legacy_logging 装配，平台只提供 Ports 的日志出口。
Arduino NVS 适配器 nvs_storage.cpp 实现 engine::KeyValue；时钟分别注入 epoch 秒与 millis 毫秒，随机源注入 esp_random。
深睡及 ESP 入口位于 platform/arduino/lifecycle.cpp。按键、屏幕和串口仍是显式平台外围模块。

保留 `PetState` 为应用/旧 UI 的兼容投影视图，定义明确隔离在 content/artoria/legacy_view.h；core/game_state.h 仅兼容 include。
它仍有旧字段名，且 Sprout 的显示投影把无关标志设为默认值；这是现有界面的兼容成本，不是引擎状态或新存档 ABI。
生产 facade 在命令边界 capture/project；没有把旧专属字段改成含糊名称塞进 engine。
旧演出结果类型也保留为 UI 适配契约，策略返回确定的 typed Result。

引擎输出语义事件与结果/快照；现有表现层使用结果与快照，主循环清空事件队列。渲染器不执行规则。
特殊食物结算仍在原菜单确认路径一次发生，动画播放只消费已结算结果；时间线测试验证动画不会改变宠物状态。
这不是以事件总线替换全部 UI 的大重写，事件不是第二个规则入口。

## 容量与时间

| 项目 | 明确上限/失败行为 |
| --- | --- |
| 扩展状态 | 64 字节，长度/版本由内容包验证，超限拒绝存档 |
| 图鉴 | 32 个稳定 ID，满容量 unlock 返回 false；当前兼容 UI ID 限 0..254 |
| 命令事件 | 16 条 FIFO，拒绝新增并保留 overflow latch，app 每轮消费 |
| 动画队列 | 16 节点，拒绝溢出并可查询 overflowed；等待输入无阻塞等待 |
| 抽样池 | 32 个食物；生成器限制普通食物 4..32 |
| 存档 | 宠物 160 / 完整 228 字节，栈上有限缓冲，不转储对象 |
| 日志 | 192 字节格式化缓冲，可截断，sink 必须同步使用 |

规则使用日历秒；存档调度和动画使用单调毫秒，类型区分两种时间。测试注入固定时钟和随机序列，验证相同输入可重复。
分钟推进保留旧逐分钟语义，复杂度 O(minutes)，并非每帧无界分配；长离线按原日结流程分段。
每个命令复制固定 State，未引入每帧全量资源复制、动态插件或脚本运行时。

## 阶段完成及边界

P0/P1：基线、严肃值规则平台隔离及 7/91 测试，已由规划模型验收。
P2：通用契约、生产内容策略、应用拆分、平台端口、v10 显式格式与 v9 目标布局迁移。
P3：表现/图鉴/资源内容化，独立 Sprout 完整生命周期，两包编译期选择。
P4：主机生命周期/格式/实际存储故障注入/旧行为对照、四种固件组合构建、资源复现与教程。

未扩展到任意游戏类型、任意屏幕或运行时插件。硬件视觉/时钟/NVS 掉电测试未执行；详细验证与限制见 acceptance.md。


集中验收反馈后的修订仅涉及现有显示/启动/持久化边界：gallery_view共用实际图鉴投影；startup共用setup决策；import_workflow共用console的完整COMMIT持久化路径，提供故障注入位置与明确返回值。
这些是为修复/测试现有入口所需的小切片，不增加引擎机制或新内容。细节见targeted-audit.md。


## 最终补充验收的容量界限

本轮仅修复验收缺陷：单条分钟批处理最多1440分钟；应用离线补偿最多366天，且拒绝年龄/epoch加法溢出。年龄65535拒绝日结，sequence耗尽拒绝新写，Sprout积水65535饱和。内容生成器明确限制形态1–32、普通食物4–32、特殊食物1–32、稳定ID0–254（当前应用8位视图限制），引擎Id类型保持uint16。
保存协议切片与行接收器提取供固件/离线共用，未改引擎/内容/平台依赖方向。完整证据与未通过项见 final-supplementary-acceptance.md。

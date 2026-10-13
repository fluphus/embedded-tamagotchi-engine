# 增加编译期内容包

可运行的最小参考是 `src/content/sprout`，不需要复制 engine。当前 UI 限 32 形态、4..32 普通食物、ID 0..254；扩展状态最多 64 字节。
身份一旦发布不要重用，展示名称与数组顺序不作为存档身份。改扩展布局必须提升内容扩展版本并写明确迁移/拒绝规则。

1. 新建 `src/content/<包名>/manifest.json`。参照 Sprout 填唯一 package_id、title、asset_dir、forms、foods、special_foods、config、tier_names、stage_names；普通差异在这里表达。
   例如 Sprout 保留两个不连续形态 ID 40/47、四种食物 ID 17/23/42/90。资源 asset ID 与稳定形态身份明确对应。
2. 执行 `python3 tools/engine/generate_content.py`。生成 definition.inc、food_table.h、config.h；生成器扫描 content 子目录的 manifest，不修改引擎。
3. 编写该包 policy.h：提供 Result、handle(State&,Command,Ports)、terminal、summarize、resetOption，及 capture/project/validate。
   简单互动复用 engine::interact/idleMinute，随机只用 Ports；复杂成长由 typed C++ 策略实现。Sprout 演示积水成长，阿尔托莉雅演示复杂路线。
   扩展用 engine::Reader/Writer 显式编码版本化字段，validate 拒绝未知形态、非法范围、版本/长度；不要转储 C++ 对象。
4. 在内容装配处注册编译宏：content/selected.h 与 definition.h、config/game_config.h 与 food_table.h 选择该包。
   现有兼容 UI 需要 capture/project 映射公共指标、日投喂记录与 content_extension；无关旧角色标志保持默认。
   这些是应用/内容注册，不修改 src/engine。新包可逐步提供独立投影视图，但不要求重写整个 UI。
5. 可先使用占位图。建立 manifest 的 asset_dir；有 PNG 时沿用 assets 的分类/命名，key 取 manifest 的稳定资源 key。
   执行 `python tools/convert_assets.py --content <包名>`，生成 generated_<包名>。
   增加编译宏时同步在转换器末尾的 source guard 注册、asset_loader.cpp 的生成头选择注册，避免两个包的 PROGMEM 定义同时链接。
   转换器从 manifest 取角色/食物映射，不需要在引擎中增加角色判断。缺 Pillow 会明确失败且不改现有资源。
6. platformio.ini 增加 extends 默认环境的配置，带内容宏；再加串口占位后端宏 DISPLAY_BACKEND_SERIAL_PLACEHOLDER=1。
   默认宏未设时仍选择 Artoria；现有 Sprout 配置是 sprout-ssd1351 / sprout-serial。
7. 给新包加入 run_engine_tests.py / run_storage_tests.py 的构建矩阵与生命周期断言，检查 init/feed/poke/time/growth/gallery/save/restore，以及未知包互拒和扩展版本拒绝。
   资源检查增加新包目录。运行下列主机检查与两个后端固件构建；无需连接设备。

```sh
python3 tools/engine/generate_content.py --check
python3 tests/host/check_architecture.py
python3 tests/host/run_tests.py
python3 tests/host/run_engine_tests.py
python3 tests/host/run_storage_tests.py
python3 tests/host/run_compatibility.py
python tests/host/check_resources.py  # 此解释器需 Pillow
pio run -e sprout-ssd1351 -e sprout-serial
```

Sprout 的实际体验：抽四选三投喂获得积水；健康三连可以确认一份特殊食物；poke 改变严肃值并暂停增长。
推进一天且积水至少 6 后成长为 Bloom，图鉴增加，存档后恢复同一形态及水量。它不是静态改名展示。

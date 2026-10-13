[English](README.md) | [简体中文](README_zh.md) | [日本語](README_ja.md)

# 🌱 Embedded Tamagotchi Engine

A reusable **C++17 virtual pet engine** for embedded devices, with interchangeable content packs and an **ESP32-S3** implementation.

Build your own virtual pet: define its forms, food, growth rules and resources while sharing the engine, save system and presentation flow. This project grew out of [Artoria Tamagotchi](https://github.com/fluphus/esp32-artoria-tamagotchi) and includes both the original Artoria gameplay and an independent **Pocket Sprout** example.

## ✨ Features

- **Shared engine:** Commands, time progression, state changes, semantic events and save scheduling. Calendar seconds and monotonic milliseconds are separate; clocks and randomness are injectable for deterministic tests.
- **Content packs:** Stable IDs for forms and food, generated data tables, versioned content state and typed C++ policies for special rules. Packs are selected at compile time.
- **Interaction and presentation:** Feed, poke, growth, gallery and destroy confirmation; bounded animation queues, input blocking and page holds keep rule settlement separate from animation playback.
- **Persistent storage:** Three physical slots with two-slot rotation, explicit versioned encoding, checksums, readback checks, Artoria v9 migration, full backups and pet-only visits. Interrupted multi-key workflows enter a protected recovery state.
- **Two display backends:** SSD1351 128×128 OLED using **Adafruit GFX / Adafruit SSD1351**, or serial placeholders. PNG assets can be embedded in firmware; missing art uses placeholders.
- **Host regression:** Production rules, menu, workflows, storage and protocol paths run against injected peripherals, including fault injection and restart/interruption tests.

## 📦 Included Content Packs

| Pack | Gameplay | Build selection |
|---|---|---|
| **Artoria** — default | Saber Lily, white/black growth routes, feeding windows, combos, special foods, Rhongomyniad and Nobu routes. | `esp32-s3-devkitc-1` / `artoria-serial` |
| **Pocket Sprout** | Seed → Bloom, four distinct foods and accumulated water. Growth requires at least one day and six water. Dew + Sunseed + Moonberry triggers a special-food choice; combinations containing Pebble do not. | `sprout-ssd1351` / `sprout-serial` |

Both packs use the same engine and persistence mechanisms. Sprout has its own growth strategy and stable IDs, rather than a copied engine or a renamed static demo.

## 🧩 Architecture and Platform Support

```text
Application assembly
├── Engine               Standard C++17 contracts and shared mechanisms
├── Content packs        Definitions, policies and versioned content state
├── Platform adapters    Clock, randomness, storage and device lifecycle
└── Input / presentation / display
```

`src/engine/` depends only on the standard library and its own contracts. The complete firmware currently targets **ESP32-S3 / Arduino**, including NVS, deep sleep, GPIO and display integration. Host execution is provided for testing; another board needs platform adapters and application integration. There is no runtime pack loading or scripting VM.

See [architecture](docs/engine/architecture.md) and the [content-pack tutorial](docs/engine/add-content-pack.md). Current application limits include 32 forms, 4–32 normal foods, 1–32 special foods, stable IDs 0–254, and 64 bytes of content extension state. Supported offline compensation is bounded to 366 days, subject to age and timestamp capacity.

## 🛠️ Hardware

The supplied configuration targets **ESP32-S3 N16R8**: 16 MB flash, 8 MB PSRAM, an SSD1351 1.5-inch 128×128 color OLED and three buttons. Buttons connect their GPIO to GND when pressed.

| Function | GPIO |
|---|---:|
| Left / middle / right button | 4 / 5 / 6 |
| OLED DC / reset / CS | 8 / 9 / 10 |
| OLED MOSI / SCLK | 11 / 12 |

The serial placeholder backend runs on the same ESP32-S3 firmware without requiring a screen. It is distinct from the native host test harness.

## 🚀 Build and Run

Use a Python environment with PlatformIO; host tests additionally need a C++17 compiler (`g++` by default) and Git. Run commands from the repository root.

```sh
python -m venv .venv
# Linux / macOS
. .venv/bin/activate
# Windows PowerShell: .venv\Scripts\Activate.ps1
python -m pip install platformio==6.2.0 Pillow==12.3.0 pyserial==3.5
```

| Environment | Content | Display |
|---|---|---|
| `esp32-s3-devkitc-1` — default | Artoria | SSD1351 |
| `artoria-serial` | Artoria | Serial placeholders |
| `sprout-ssd1351` | Pocket Sprout | SSD1351 |
| `sprout-serial` | Pocket Sprout | Serial placeholders |

```sh
pio run                                      # Default Artoria OLED build
pio run -e sprout-ssd1351                     # Sprout OLED build
pio run -e artoria-serial -e sprout-serial     # Serial builds

# Upload the chosen environment to your board, then open the console.
pio run -e esp32-s3-devkitc-1 -t upload
pio device monitor -b 115200
```

Confirm the date and time with the buttons on first boot or after power loss. Serial `SET_TIME <unix_seconds>` also completes the time-setup flow. The current clock is software-maintained with RTC-based deep-sleep recovery; network time synchronization is not implemented.

Saves belong to their content pack. Export your data before switching packs: foreign or unknown records are protected instead of being silently replaced with a new game.

## 🎮 Controls

| Screen | L | M | R |
|---|---|---|---|
| Idle | Start feeding | Status; long press opens gallery | Poke |
| Food selection | Previous food | Toggle selection; three selected foods submit | Next food |
| Special food | Previous choice | Confirm | Next choice |
| Gallery | Previous entry | Close | Next entry |
| Destroy confirmation | YES | Confirm | NO |
| Date/time setup | Increase field | Next field / confirm | Previous field |

M closes the status panel. Hold **L + M + R together for five seconds** to open destroy confirmation; the default is NO. In time setup, long L resets the editable date to its default. During animations or result-page holds, gameplay input is blocked; special-food selection explicitly waits for input.

For serial exploration at 115200 baud:

```text
s                    Show current status
h                    Show all available commands
fl                   Show foods
btn l / btn m / btn r Simulate short button presses
btnl m               Simulate a long middle-button press
ctx                  Show UI and presentation state
save / load          Save or load the current pet
reset                Reset pet; during a visit, restore the owner
devinfo              Show device state and runtime memory observations
t 60                 Debug: advance minute rules by 60 minutes
d                    Debug: process a day and advance the calendar
```

`t` accepts 1–10080 minutes in bounded batches; it keeps the original minute-only debug semantics. Time-changing debug commands and new imports are restricted during visits. Debug gameplay commands are blocked while awaiting initial time setup or saved-data recovery.

## 💾 Backups, Visits and Recovery

Use the actual Python helper, replacing `/dev/ttyACM0` with your port (for example, `COM5` on Windows):

```sh
python SaveManager/save_manager.py --port /dev/ttyACM0 status
python SaveManager/save_manager.py --port /dev/ttyACM0 export --slot 0 --out full.bin
python SaveManager/save_manager.py --port /dev/ttyACM0 export --slot 0 --out guest.bin --pet-only
python SaveManager/save_manager.py --port /dev/ttyACM0 import --slot 1 --in full.bin
```

- **Full import** restores the bundled pet and gallery and aligns pet timestamps to the local clock.
- **Pet-only import** starts a visit: the newest active owner slot stays frozen, while the guest uses the other two slots. Importing into the owner's slot is rejected. `reset` or confirmed destruction ends the visit and restores the owner.
- **Recovery** blocks background writes and gameplay after an incomplete protected workflow. Export readable slots first. When a retained owner/previous-pet backup exists, use `export --slot -1 --out owner.bin` or the firmware command `SAVE_EXPORT_OWNER`.
- **Explicit recovery** uses `SAVE_RECOVERY_ERASE CONFIRM`, then reimports your exported full backup. This command deletes saved data; it is only accepted under a recovery lock. Normal startup does not perform this deletion automatically.

Serial import uses `SAVE_IMPORT_BEGIN`, `SAVE_IMPORT_DATA`, `SAVE_IMPORT_COMMIT` and `SAVE_IMPORT_ABORT`; the helper handles chunking and acknowledgements. Unknown versions, content packs and IDs are rejected. Current v10 files use explicit field encoding; legacy Artoria v9 migration evidence is recorded in the acceptance reports.

## 🎨 Assets and Custom Content

Edit the pack's `manifest.json` for forms, foods, text and resource mappings, then regenerate tables:

```sh
python tools/engine/generate_content.py
python tools/engine/generate_content.py --check

# Optional PNG conversion; requires Pillow.
python tools/convert_assets.py --content artoria
python tools/convert_assets.py --content sprout
```

Generated tables and resources are included in the source. Asset conversion is manual; the PlatformIO pre-build hook is disabled. Artoria assets use `assets/`, and Sprout uses `assets/sprout/`; artwork coverage varies and Sprout uses labeled placeholders.

See the [asset naming guide](assets/README.md) and [adding a content pack](docs/engine/add-content-pack.md). A new pack supplies data and a typed C++ policy, then registers its build and asset selection in the application. No engine copy is needed.

## 🧪 Tests and Verification

```sh
python tests/host/check_architecture.py  # Standalone engine dependency / policy check
python tests/host/run_all.py            # Both packs, production-path regressions
python tests/host/run_all.py --resources
```

The full regression suite uses the original Git baseline for v9 provenance and gameplay comparison; run it from a checkout retaining that history rather than a source ZIP. Tests cover lifecycle, input and animation, save corruption, failed writes, interrupted imports/resets, owner recovery, time boundaries and the actual Python protocol tool. ASan/UBSan commands are documented in the reports.

- [Acceptance overview](docs/engine/acceptance.md)
- [Latest D1–D6 fixes and regression evidence](docs/engine/deep-fixes-2026-10-13.md)
- [Independent D1–D6 reacceptance](docs/engine/deep-reacceptance-2026-10-13.md)
- [Earlier hardware tests and their version scope](docs/engine/hardware-acceptance.md)
- [Original behavior and preserved time semantics](docs/engine/behavior-baseline.md)

Latest fixes have host regression and four-environment build evidence; their new firmware has not been flashed for a fresh hardware run. Physical screen/button checks, true sleep/wake and power-interruption tests remain outstanding. Maximum offline processing is synchronous; measured earlier firmware took approximately 13–15 seconds for 366 days.

## 📄 License and Credits

Code is licensed under [MIT](LICENSE). Based on [Artoria Tamagotchi](https://github.com/fluphus/esp32-artoria-tamagotchi); Artoria is the Fate/Grand Order-themed example, and Pocket Sprout is the original example content pack.

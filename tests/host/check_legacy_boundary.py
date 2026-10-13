#!/usr/bin/env python3
"""P1 source guard, NOT an ESP32 ABI or old-save compatibility test."""
import hashlib
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BASE = "448dbe158595c1db9f1d2fc37caa15810c53de1c"
PROTECTED = (
    "src/core/game_state.h", "src/core/save_manager.h", "src/core/save_manager.cpp",
    "src/config/game_config.h", "src/pet/gallery.h", "src/main.cpp",
    "src/input/menu_controller.cpp", "SaveManager/save_manager.py", "platformio.ini",
)
for name in PROTECTED:
    old = subprocess.check_output(["git", "show", f"{BASE}:{name}"], cwd=ROOT)
    current = (ROOT / name).read_bytes()
    if old != current:
        raise SystemExit(f"FAIL: protected source changed: {name}")
    print(f"UNCHANGED {hashlib.sha256(current).hexdigest()} {name}")
print(f"PASS: {len(PROTECTED)} protected files byte-identical to {BASE}")
print("Old v9 sample / ESP32 layout validation: NOT EXECUTED")

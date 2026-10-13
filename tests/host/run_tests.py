#!/usr/bin/env python3
"""Build real production rules with C++17; no Arduino headers/stubs required."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="artoria-host-") as build:
    binary = str(Path(build) / "seriousness_test")
    command = [os.environ.get("CXX", "g++"), "-std=c++17", "-Wall", "-Wextra",
               "-Werror", "-Wno-unused-parameter", "-pedantic",
               *shlex.split(os.environ.get("HOST_TEST_FLAGS", "")),
               "-I", str(ROOT / "src"), str(ROOT / "src/pet/seriousness.cpp"),
               *[str(ROOT / ("src/content/artoria/"+name+".cpp")) for name in ("seriousness","feeding","evolution")],
               str(ROOT / "tests/host/seriousness_test.cpp"), "-o", binary]
    print("+", shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    subprocess.run([binary], check=True)

#!/usr/bin/env python3
"""Compare current production default rules against original Git implementation.
Only historical code gets two tiny log/random shims. Current production needs no Arduino.
"""
import difflib, os, shlex, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE='448dbe158595c1db9f1d2fc37caa15810c53de1c'
with tempfile.TemporaryDirectory(prefix='artoria-oracle-') as directory:
    temp=Path(directory);old=temp/'old';include=temp/'include';include.mkdir()
    for name in ['src/core/game_state.h','src/config/game_config.h','src/config/food_table.h',*[f'src/pet/{stem}.{suffix}' for stem in ('feeding','seriousness','evolution') for suffix in ('h','cpp')]]:
        target=old/name;target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(subprocess.check_output(['git','show',f'{BASE}:{name}'],cwd=ROOT))
    (include/'Arduino.h').write_text('#pragma once\nstruct LogOnlySerial {template<class T> void println(T){} template<class... T> void printf(const char*,T...){} }; inline LogOnlySerial Serial;\n')
    (include/'esp_random.h').write_text('#pragma once\n#include <stdint.h>\nuint32_t esp_random();\n')
    outputs=[]
    for legacy in (True,False):
        binary=str(temp/('old-trace' if legacy else 'current-trace'))
        source=old if legacy else ROOT
        command=[os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Wno-unused-parameter',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),'-I'+str(source/'src')]
        if legacy:command+=['-DLEGACY_ORACLE','-I'+str(include)]
        command += [str(ROOT/'tests/host/behavior_trace.cpp')]+[str(source/f'src/pet/{name}.cpp') for name in ('feeding','evolution','seriousness')]
        if not legacy:command += [str(ROOT/'src/app/legacy_logging.cpp')] + [str(ROOT/f'src/content/artoria/{name}.cpp') for name in ('feeding','evolution','seriousness')]
        command+=['-o',binary]
        subprocess.run(command,cwd=ROOT,check=True)
        outputs.append(subprocess.check_output([binary],cwd=ROOT,text=True))
    if outputs[0]!=outputs[1]:
        print(''.join(difflib.unified_diff(outputs[0].splitlines(True),outputs[1].splitlines(True),fromfile='original',tofile='current')))
        raise SystemExit(1)
    print(f'PASS: {len(outputs[0].splitlines())} deterministic original/current trace records match, including all pet fields and RNG counts')

#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sources=['tests/host/save_manager_test.cpp','src/core/save_manager.cpp','src/app/legacy_logging.cpp','src/pet/feeding.cpp','src/pet/evolution.cpp','src/pet/seriousness.cpp']+[f'src/content/artoria/{name}.cpp' for name in ('feeding','evolution','seriousness')]
with tempfile.TemporaryDirectory(prefix='storage-tests-') as temp:
    for pack in ('artoria','sprout'):
        command=[os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-parameter',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),'-Isrc']
        if pack=='sprout':command+=['-DCONTENT_SPROUT=1']
        binary=str(Path(temp)/pack);command+=sources+['-o',binary]
        print('+',shlex.join(command),flush=True);subprocess.run(command,cwd=ROOT,check=True);subprocess.run([binary],cwd=ROOT,check=True)

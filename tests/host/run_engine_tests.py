#!/usr/bin/env python3
import argparse, os, shlex, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument("--pack",choices=("artoria","sprout"));args=parser.parse_args()
SOURCES=['tests/host/engine_test.cpp','src/pet/feeding.cpp','src/pet/evolution.cpp','src/pet/seriousness.cpp','src/presentation/animation_queue.cpp','src/app/legacy_logging.cpp']
SOURCES += ['src/content/artoria/'+name+'.cpp' for name in ('feeding','evolution','seriousness')]
with tempfile.TemporaryDirectory(prefix='engine-tests-') as directory:
    for package in ([args.pack] if args.pack else ('artoria','sprout')):
        binary=str(Path(directory)/package)
        command=[os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-pedantic',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),'-Isrc']
        if package=='sprout':command+=['-DCONTENT_SPROUT=1']
        command+=SOURCES+['-o',binary]
        print('+',shlex.join(command),flush=True)
        subprocess.run(command,cwd=ROOT,check=True)
        subprocess.run([binary],cwd=ROOT,check=True)

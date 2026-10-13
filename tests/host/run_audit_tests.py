#!/usr/bin/env python3
import argparse,os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('--reproduce',action='store_true',help='run original storage reproduction cases against CURRENT code');parser.add_argument('--case',choices=[str(i) for i in range(1,12)],help='run only one affected production path');args=parser.parse_args()
sources=['tests/host/audit_test.cpp','src/core/save_manager.cpp','src/app/import_workflow.cpp','src/pet/gallery.cpp','src/app/legacy_logging.cpp','src/pet/feeding.cpp','src/pet/evolution.cpp','src/pet/seriousness.cpp']+[f'src/content/artoria/{n}.cpp' for n in ('feeding','evolution','seriousness')]
failed=False
with tempfile.TemporaryDirectory(prefix='targeted-audit-') as temp:
    binaries={}
    for pack in ('artoria','sprout'):
        binary=str(Path(temp)/pack)
        command=[os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-class-memaccess',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),'-Isrc']
        if pack=='sprout':command+=['-DCONTENT_SPROUT=1']
        command+=sources+['-o',binary]
        print('+',shlex.join(command),flush=True);subprocess.run(command,cwd=ROOT,check=True)
        binaries[pack]=binary
    for pack,binary in binaries.items():
        subprocess.run([binary,'emit',str(Path(temp)/(pack+'.bin'))],cwd=ROOT,check=True)
    for pack,binary in binaries.items():
        foreign=str(Path(temp)/('sprout.bin' if pack=='artoria' else 'artoria.bin'))
        for case in ((args.case,) if args.case else (('2','3','5') if args.reproduce else tuple(str(i) for i in range(1,12)))):
            result=subprocess.run([binary,case,foreign],cwd=ROOT);print('EXIT',result.returncode,pack,case,flush=True);failed|=result.returncode!=0
if failed:raise SystemExit(1)

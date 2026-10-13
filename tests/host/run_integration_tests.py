#!/usr/bin/env python3
import argparse,os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('--require-all',action='store_true',help='assert all input workflows, including both Sprout special foods');parser.add_argument('--pack',choices=('artoria','sprout'));parser.add_argument('--case',choices=('all','inputs'),default='all');args=parser.parse_args()
SOURCES=['tests/host/integration_test.cpp','src/core/time_manager.cpp','src/core/save_manager.cpp','src/app/import_workflow.cpp','src/app/workflows.cpp','src/app/ui_binding.cpp','src/pet/gallery.cpp','src/pet/feeding.cpp','src/pet/evolution.cpp','src/pet/seriousness.cpp','src/app/legacy_logging.cpp','src/input/button_driver.cpp','src/input/input_manager.cpp','src/input/menu_controller.cpp','src/display/DisplayManager.cpp','src/display/display_renderer.cpp','src/display/display_assets.cpp','src/display/asset_loader.cpp','src/presentation/animation_queue.cpp','src/presentation/animation_director.cpp','src/presentation/idle_resolver.cpp','src/presentation/pet_state_query.cpp']+[f'src/content/artoria/{n}.cpp' for n in ('feeding','evolution','seriousness')]+['src/display/generated_assets/generated_asset_data.cpp','src/display/generated_sprout/generated_asset_data.cpp']
with tempfile.TemporaryDirectory(prefix='app-integration-') as temp:
    for pack in ([args.pack] if args.pack else ('artoria','sprout')):
        binary=str(Path(temp)/pack)
        cmd=[os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Wno-unused-parameter','-Wno-class-memaccess','-Wno-format','-DDISPLAY_BACKEND_SERIAL_PLACEHOLDER=1',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),'-Itests/host/peripherals','-Isrc']
        if pack=='sprout':cmd+=['-DCONTENT_SPROUT=1']
        cmd+=SOURCES+['-o',binary];print('+',shlex.join(cmd),flush=True)
        subprocess.run(cmd,cwd=ROOT,check=True)
        result=subprocess.run([binary,'--case',args.case]+(['--require-all'] if args.require_all else []),cwd=ROOT)
        if result.returncode:raise SystemExit(result.returncode)

#!/usr/bin/env python3
"""Run production host acceptance checks, preserving failure exit codes."""
import argparse
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('--resources',action='store_true',help='also regenerate PNG assets (requires Pillow)');args=parser.parse_args()
commands=[['tools/engine/generate_content.py','--check']]+[[f'tests/host/{name}.py'] for name in ('check_architecture','check_v9_fixtures','run_tests','run_engine_tests','run_storage_tests','run_audit_tests','run_protocol_tests','run_integration_tests','run_deep_regression','run_diagnostics_tests','run_content_boundaries','run_compatibility')]
for command in commands:
    if command[0]=='tests/host/run_integration_tests.py':command.append('--require-all')
if args.resources:commands.append(['tests/host/check_resources.py'])
exit_code=0
for command in commands:
    print('+',sys.executable,*command,flush=True)
    result=subprocess.run([sys.executable,*command],cwd=ROOT)
    print('EXIT',result.returncode,command[0],flush=True)
    if result.returncode:exit_code=result.returncode
if exit_code:raise SystemExit(exit_code)
print('PASS all requested host acceptance checks')

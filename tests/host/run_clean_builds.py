#!/usr/bin/env python3
"""Build the complete delivered source in a fresh tree without project .pio.
Global toolchain/download caches are allowed; project binaries/libraries are not.
"""
import argparse,hashlib,json,shutil,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('--pio',default='pio');parser.add_argument('--manifest',type=Path);args=parser.parse_args()
files=subprocess.check_output(['git','ls-files','-co','--exclude-standard','-z'],cwd=ROOT).decode().split('\0')
manifest={}
with tempfile.TemporaryDirectory(prefix='pet-clean-delivery-') as directory:
    target=Path(directory)
    for name in sorted(set(files)):
        if not name or name.startswith(('.pio/','.git/')):continue
        path=ROOT/name
        if not path.is_file():continue
        dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)
        manifest[name]=hashlib.sha256(dest.read_bytes()).hexdigest()
    assert not (target/'.pio').exists()
    print('FRESH SOURCE',target,'files',len(manifest),flush=True)
    print('SOURCE MANIFEST SHA256',hashlib.sha256(json.dumps(manifest,sort_keys=True).encode()).hexdigest(),flush=True)
    if args.manifest:args.manifest.write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    commands=[[sys.executable,'tests/host/check_resources.py']]
    commands += [[args.pio,'run','-e',env] for env in ('esp32-s3-devkitc-1','artoria-serial','sprout-ssd1351','sprout-serial')]
    commands += [[args.pio,'pkg','list']]
    for command in commands:
        print('+',*command,flush=True);result=subprocess.run(command,cwd=target)
        print('EXIT',result.returncode,*command,flush=True)
        if result.returncode:raise SystemExit(result.returncode)
print('PASS fresh delivery: reproducible resources and two packs x two backends')

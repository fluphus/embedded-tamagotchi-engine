#!/usr/bin/env python3
"""Validate provenance and hashes without claiming host structs prove the target ABI."""
import hashlib
import json
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[2]
folder=ROOT/'tests/fixtures/v9'
manifest=json.loads((folder/'manifest.json').read_text())
header=subprocess.check_output(['git','show',manifest['source_commit']+':src/core/game_state.h'],cwd=ROOT)
assert hashlib.sha256(header).hexdigest()==manifest['source_header_sha256']
for name,digest in manifest['fixtures'].items():
    assert hashlib.sha256((folder/name).read_bytes()).hexdigest()==digest,name
layout=manifest['layout']
assert (layout['pet_size'],layout['header_size'],layout['device_size'],layout['gallery_size'])==(56,16,12,12)
print('PASS: original Git header and 8 target-compiler fixture hashes; regenerate with tools/engine/generate_v9_fixtures.py')

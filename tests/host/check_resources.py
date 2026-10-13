#!/usr/bin/env python3
"""Rebuild assets in temporary directories and compare to shipped artifacts."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[2]
subprocess.run([sys.executable, 'tools/engine/generate_content.py', '--check'], cwd=ROOT, check=True)
with tempfile.TemporaryDirectory(prefix='pet-resources-') as directory:
    for pack in ('artoria', 'sprout'):
        manifest = json.loads((ROOT/'src/content'/pack/'manifest.json').read_text())
        for key in ('forms', 'foods', 'special_foods'):
            entries = manifest[key]
            mapping = {entry['key']: entry['id'] for entry in entries}
            assert mapping == {entry['key']: entry['id'] for entry in reversed(entries)}
            assert len(set(mapping.values())) == len(entries)
        target = Path(directory)/pack
        result = subprocess.run([sys.executable, 'tools/convert_assets.py', '--content', pack,
                                 '--output-dir', str(target)], cwd=ROOT, check=True,
                                capture_output=True, text=True)
        shipped = ROOT/'src/display'/('generated_assets' if pack == 'artoria' else 'generated_sprout')
        for name in ('generated_asset_data.cpp', 'generated_asset_index.h'):
            data = (target/name).read_bytes()
            assert data == (shipped/name).read_bytes(), f'{pack}/{name}: stale artifact'
            print(f'PASS {pack}/{name}: {len(data)} bytes SHA256 {hashlib.sha256(data).hexdigest()}')
        print(next(line.strip() for line in result.stdout.splitlines() if 'Sequences:' in line))
print('PASS manifests, stable key/ID mapping, reproducible generated assets for both packs')

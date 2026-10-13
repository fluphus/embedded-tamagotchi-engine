#!/usr/bin/env python3
"""Exercise the production manifest generator at declared count/ID boundaries."""
import copy,importlib.util,json,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('generator',ROOT/'tools/engine/generate_content.py')
generator=importlib.util.module_from_spec(spec);spec.loader.exec_module(generator)
with tempfile.TemporaryDirectory(prefix='content-limits-') as directory:
    generator.ROOT=Path(directory);target=generator.ROOT/'src/content/test';target.mkdir(parents=True)
    base=json.loads((ROOT/'src/content/sprout/manifest.json').read_text())
    def run(manifest,rejected):
        (target/'manifest.json').write_text(json.dumps(manifest))
        before={p.name:p.read_bytes() for p in target.glob('*') if p.name!='manifest.json'}
        try:generator.generate('test')
        except ValueError:
            assert rejected
            assert before=={p.name:p.read_bytes() for p in target.glob('*') if p.name!='manifest.json'}
        else:assert not rejected
    for field in ('forms','foods','special_foods'):
        for count in (0,1,3,4,32,33):
            manifest=copy.deepcopy(base);template=manifest[field][0]
            manifest[field]=[dict(template,id=i,key=f'entry_{i}') for i in range(count)]
            run(manifest,not (4<=count<=32 if field=='foods' else 1<=count<=32))
    for field in ('forms','foods','special_foods'):
        for value in (-1,254,255,65535):
            manifest=copy.deepcopy(base);manifest[field][0]['id']=value;run(manifest,value!=254)
        manifest=copy.deepcopy(base);manifest[field][1]['id']=manifest[field][0]['id'];run(manifest,True)
print('PASS production generator: 33 count/ID/duplicate boundary cases; rejects leave outputs unchanged')

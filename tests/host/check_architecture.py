#!/usr/bin/env python3
import os,re,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
headers=list((ROOT/'src/engine').glob('*.h'))
for path in headers:
    text=path.read_text()
    for include in re.findall(r'#include\s+"([^"]+)"',text):
        if '/' in include or not (path.parent/include).is_file():raise SystemExit('engine imports outside its contracts: '+str(path))
    if re.search(r'Arduino|Preferences|Serial|esp_|rhongomyniad|mapo|nobu|CONTENT_SPROUT',text):raise SystemExit('engine dependency leak: '+str(path))
with tempfile.TemporaryDirectory(prefix='engine-contract-') as directory:
    source=Path(directory)/'check.cpp'
    source.write_text(''.join('#include "engine/'+p.name+'"\n' for p in headers)+'''
struct MiniPolicy {
    struct Result{};
    static bool terminal(const engine::State&){return false;}
    static Result handle(engine::State& s,const engine::Command&,const engine::Ports&){++s.health;return {};}
    static void summarize(const engine::State&,const engine::State&,const engine::Command&,Result&){}
};
int main(){engine::Engine<MiniPolicy> e;engine::State s;e.execute(s,{});return s.health==51?0:1;}
''')
    binary=str(Path(directory)/'check')
    subprocess.run([os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Werror','-pedantic','-Isrc',str(source),'-o',binary],cwd=ROOT,check=True)
    subprocess.run([binary],check=True)
print('PASS: '+str(len(headers))+' standard-C++ engine headers, no platform/content imports; independent minimal policy executed')

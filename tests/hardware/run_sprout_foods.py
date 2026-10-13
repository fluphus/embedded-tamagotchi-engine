#!/usr/bin/env python3
"""On-device Sprout input workflow: two special choices and all Pebble triples."""
import argparse,importlib.util,json,re,sys,time
from pathlib import Path
import serial
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--port',default='/dev/ttyACM0');p.add_argument('--label',required=True);a=p.parse_args();out=ROOT/'docs/engine/evidence/hardware'/a.label
log=(out/'sprout-foods.log').open('w',buffering=1);results=[]
spec=importlib.util.spec_from_file_location('tool',ROOT/'SaveManager/save_manager.py');m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m)
def cmd(c,wait=.25):
 with serial.Serial(a.port,115200,timeout=.05) as s:
  s.reset_input_buffer();s.write((c+'\n').encode());minimum=time.monotonic()+wait;end=minimum+2;b=b''
  while time.monotonic()<end:
   b+=s.read(max(1,s.in_waiting))
   if time.monotonic()>=minimum and b.endswith(b'\n'):
    if c=='ctx' and b'Presentation:' not in b:continue
    if c=='s' and b.count(b'========================================')<2:continue
    break
 text=b.decode(errors='replace');log.write('TX '+repr(c)+'\nRX '+repr(text)+'\n');return text
def ck(name,b,detail=''):
 results.append({'test':name,'result':'PASS' if b else 'FAIL','detail':detail});print(results[-1],flush=True)
 if not b:raise AssertionError(name+': '+detail)
try:
 for scenario,(foods,expected) in enumerate([((17,23,42),68),((17,23,42),68),((17,23,90),59),((17,42,90),54),((23,42,90),57)]):
  cmd('reset',2);cmd('SET_TIME 1704096000');time.sleep(2)
  text=cmd('btn l',3);match=re.search(r'Draw food IDs: (\d+) (\d+) (\d+) (\d+)',text);assert match,text;ids=list(map(int,match.groups()));cursor=1
  for food in foods:
   target=ids.index(food)
   while cursor!=target:cmd('btn r',.1);cursor=(cursor+1)%4
   text=cmd('btn m',.1)
  ck('scenario '+str(scenario)+' feed confirmation',text.count('Feed confirmed!')==1,text)
  cmd('btn m\nbtn m\nbtn m',.2);time.sleep(5);ctx=cmd('ctx')
  if scenario<2:
   ck('healthy triple combo','Context: SPECIAL_FOOD' in ctx,ctx)
   if scenario==1:cmd('btn r')
   text=cmd('btn m',.15);ck('special '+str(scenario)+' once',text.count('Special food selected: '+str(scenario))==1,text)
   cmd('btn m\nbtn m\nbtn m',.2);time.sleep(3)
  else:ck('Pebble triple no combo','Context: IDLE' in ctx,ctx)
  state=cmd('s');hp=int(re.search(r'Health:\s*(\d+)',state).group(1));ck('scenario '+str(scenario)+' HP and count',hp==expected and 'Fed today:  1 /' in state,state);idle=cmd('ctx');ck('animation returns IDLE','Context: IDLE' in idle and 'blocking=0 playing=0 waiting=0' in idle,idle)
  ck('save success','[Save] OK' in cmd('save'))
  statuses=m.cmd_status(a.port,115200,.6);newest=max((int(re.search(r'seq=(\d+)',x.raw_line).group(1)),x.slot) for x in statuses if 'seq=' in x.raw_line)[1];path=out/('sprout-scenario-'+str(scenario)+'.bin');m.cmd_export(a.port,115200,5,newest,path);ck('actual exported health',int.from_bytes(path.read_bytes()[26:28],'little')==expected)
 ck('suite completed',True)
finally:(out/'sprout-foods-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n');log.close()

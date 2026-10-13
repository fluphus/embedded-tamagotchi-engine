#!/usr/bin/env python3
"""Real UART adversarial cases against already running test firmware.
Uses shipped save tool; no codecs or rules are reimplemented.
"""
import argparse, importlib.util, json,re,sys,time
from pathlib import Path
import serial
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--port',default='/dev/ttyACM0');p.add_argument('--label',required=True);p.add_argument('--timeouts',action='store_true');p.add_argument('--soak-seconds',type=int);p.add_argument('--soak-only',action='store_true');a=p.parse_args();duration=a.soak_seconds if a.soak_seconds is not None else (330 if a.label in ('sprout-ssd1351','esp32-s3-devkitc-1') else 30)
out=ROOT/'docs/engine/evidence/hardware'/a.label;out.mkdir(parents=True,exist_ok=True);log=(out/('stability.log' if a.soak_only else 'uart-edges.log')).open('w',buffering=1);results=[]
spec=importlib.util.spec_from_file_location('tool',ROOT/'SaveManager/save_manager.py');m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m)
def cmd(text,wait=.3):
 with serial.Serial(a.port,115200,timeout=.05) as s:
  s.reset_input_buffer();s.write((text+'\n').encode());minimum=time.monotonic()+wait;end=minimum+2;raw=b''
  while time.monotonic()<end:
   raw+=s.read(max(1,s.in_waiting))
   if time.monotonic()>=minimum and raw.endswith(b'\n'):
    if text=='s' and raw.count(b'========================================')<2:continue
    break
 data=raw.decode(errors='replace');log.write('TX '+repr(text)+'\nRX '+repr(data)+'\n');return data
def ck(name,test,detail=''):
 r={'test':name,'result':'PASS' if test else 'FAIL','detail':detail};results.append(r);print(r,flush=True)
 if not test:raise AssertionError(name+': '+detail)
def snapshots(prefix):
 statuses=m.cmd_status(a.port,115200,.6);blobs={}
 for status in statuses:
  if 'seq=' in status.raw_line:
   path=out/(prefix+'-'+str(status.slot)+'.bin');m.cmd_export(a.port,115200,5,status.slot,path);blobs[status.slot]=path.read_bytes()
 return blobs
def mem():
 data=cmd('devinfo');v=re.search(r'heap=(\d+) min_heap=(\d+) max_alloc=(\d+) psram=(\d+) free_psram=(\d+) stack_bytes=(\d+)',data);assert v,data;return list(map(int,v.groups()))
try:
 if not a.soak_only:
  before=snapshots('edges-before');assert before
  cmd('SAVE_IMPORT_BEGIN 0');ck('illegal HEX rejected','Invalid hex' in cmd('SAVE_IMPORT_DATA GG'));ck('illegal HEX session aborted','No active session' in cmd('SAVE_IMPORT_COMMIT'))
  cmd('SAVE_IMPORT_BEGIN 0');one=cmd('SAVE_IMPORT_DATA @0 0001');two=cmd('SAVE_IMPORT_DATA @0 0001');ck('same indexed chunk idempotent','RECV 2/' in one and 'RECV 2/' in two)
  ck('conflicting indexed retry rejected','Invalid hex' in cmd('SAVE_IMPORT_DATA @0 0002'));cmd('SAVE_IMPORT_ABORT')
  ck('overlong line discarded','overlong command' in cmd('hp 99'+' '*100));ck('no truncated hp command executed','Health:     99 /' not in cmd('s'))
  # Connection closes between each command: fragments are held until newline.
  with serial.Serial(a.port,115200,timeout=.1) as s:s.write(b'SAVE_SLOT_');s.flush()
  received=cmd('STATUS');ck('disconnect/reconnect fragmented line',all('slot'+str(i)+':' in received for i in range(3)),received)
  if a.timeouts:
   cmd('SAVE_IMPORT_BEGIN 0');cmd('SAVE_IMPORT_DATA @0 00');time.sleep(31)
   ck('real 30sec session expiry','No active session' in cmd('SAVE_IMPORT_COMMIT'))
   with serial.Serial(a.port,115200,timeout=.1) as s:s.write(b'hp ');s.flush()
   time.sleep(31);cmd('99');ck('real partial line expiry','Health:     99 /' not in cmd('s'))
  ck('faults preserve all valid slot bytes',snapshots('edges-after')==before)
  # Calendar boundaries through actual SET_TIME/advance/application daily path.
  import datetime
  for date,expected in [((2024,2,28),'2024-02-29 00:00'),((2024,2,29),'2024-03-01 00:00'),((2024,12,31),'2025-01-01 00:00')]:
   epoch=int(datetime.datetime(*date,23,59,tzinfo=datetime.timezone.utc).timestamp());cmd('SET_TIME '+str(epoch));age=int(re.search(r'Age:\s*Day (\d+)',cmd('s')).group(1));cmd('t 1',1);state=cmd('s');age2=int(re.search(r'Age:\s*Day (\d+)',state).group(1));ck('calendar '+expected,expected in state and age2==age+1,state)
   time.sleep(.3);ck('daily not repeated',int(re.search(r'Age:\s*Day (\d+)',cmd('s')).group(1))==age2)
 soakBefore=snapshots('soak-before') if duration>=300 else {}
 samples=[];start=time.monotonic();pending=b'';nextQuery=start
 with serial.Serial(a.port,115200,timeout=.05) as s:
  while time.monotonic()-start<duration:
   if time.monotonic()>=nextQuery:s.write(b'devinfo\n');nextQuery+=2
   raw=s.read(max(1,s.in_waiting))
   if not raw:continue
   log.write('SOAK RX '+repr(raw)+'\n');pending+=raw
   while b'\n' in pending:
    line,pending=pending.split(b'\n',1);text=line.decode(errors='replace')
    if any(marker in text for marker in ('[Boot]','Guru Meditation','Stack smashing','TASK_WDT','INT_WDT','watchdog')):ck('soak unexpected reset/panic',False,text)
    match=re.search(r'heap=(\d+) min_heap=(\d+) max_alloc=(\d+) psram=(\d+) free_psram=(\d+) stack_bytes=(\d+)',text)
    if match:
     samples.append(list(map(int,match.groups())))
     if len(samples)%15==0:print('SOAK elapsed='+str(round(time.monotonic()-start))+'s last='+str(samples[-1]),flush=True)
 ck('soak remained responsive',len(samples)>=duration//2-3,'samples='+str(len(samples)))
 ck(str(duration)+'sec heap remains stable',max(x[0] for x in samples)-min(x[0] for x in samples)<4096,str(samples));ck('positive stack margin',min(x[5] for x in samples)>512,str(samples));
 if duration>=300:
  after=snapshots('soak-after')
  advances=max(int.from_bytes(b[12:16],'little') for b in after.values())-max(int.from_bytes(b[12:16],'little') for b in soakBefore.values())
  minimum=max(1,duration//300-1)
  ck('real five-minute automatic save',advances>=minimum,'sequence advances='+str(advances)+' minimum='+str(minimum))
 ck('suite completed',True)
finally:(out/('stability-results.json' if a.soak_only else 'uart-edges-results.json')).write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n');log.close()

#!/usr/bin/env python3
"""Authorized, destructive board acceptance; actual console, Python tool and NVS.
Requires a full 16MB backup. Does not simulate physical keys, power loss or pixels.
"""
import argparse, contextlib, hashlib, importlib.util, json, re, subprocess, sys, time, shutil, datetime
from pathlib import Path
import serial
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--port',default='/dev/ttyACM0');p.add_argument('--env',required=True,choices=['artoria-serial','sprout-serial','esp32-s3-devkitc-1','sprout-ssd1351','artoria-pressure','sprout-pressure']);p.add_argument('--pio',default=shutil.which('pio') or str(Path(sys.executable).with_name('pio')));p.add_argument('--visits-only',action='store_true');p.add_argument('--skip-visits',action='store_true');p.add_argument('--project-conf');p.add_argument('--backup',type=Path,required=True);p.add_argument('--authorize-board-tests',action='store_true');a=p.parse_args()
assert a.authorize_board_tests,'Explicit board-test authorization flag required'
assert a.backup.stat().st_size==0x1000000,'A complete pre-test flash backup is required'
OUT=ROOT/'docs/engine/evidence/hardware'/(a.env+('-visits' if a.visits_only else ''));OUT.mkdir(parents=True,exist_ok=True)
transcript=(OUT/'uart.log').open('w',buffering=1);results=[]
spec=importlib.util.spec_from_file_location('save_tool',ROOT/'SaveManager/save_manager.py');tool=importlib.util.module_from_spec(spec);sys.modules[spec.name]=tool;spec.loader.exec_module(tool)
class LoggedSerial(serial.Serial):
 def write(self,data):transcript.write('TX '+repr(data)+'\n');return super().write(data)
 def readline(self,*args):
  self._reading_line=True
  try:data=super().readline(*args)
  finally:self._reading_line=False
  if data:transcript.write('RX '+repr(data)+'\n')
  return data
 def read(self,*args):
  data=super().read(*args)
  if data and not getattr(self,'_reading_line',False):transcript.write('RX '+repr(data)+'\n')
  return data
# Use the shipped tool unchanged, with only its serial transport logged.
tool._open_serial=lambda port,baud,timeout: LoggedSerial(port,baud,timeout=min(timeout,.2),write_timeout=timeout)
def read_for(s,seconds):
 end=time.monotonic()+seconds;out=b''
 while time.monotonic()<end:out+=s.read(max(1,s.in_waiting))
 return out.decode(errors='replace')
def cmd(line,wait=.4):
 with LoggedSerial(a.port,115200,timeout=.05,write_timeout=2) as s:
  read_for(s,.1);s.write((line+'\n').encode());s.flush();out=read_for(s,wait);deadline=time.monotonic()+2
  while time.monotonic()<deadline:
   complete=(bool(re.search(r'Presentation: blocking=\d playing=\d waiting=\d\r?\n',out)) if line=='ctx' else out.count('========================================')>=2 if line=='s' else out.endswith('\n'))
   if complete:break
   out+=read_for(s,.1)
  return out
def check(name,condition,detail=''):
 results.append({'test':name,'result':'PASS' if condition else 'FAIL','detail':detail});print(results[-1],flush=True)
 if not condition:raise AssertionError(name+': '+detail)
def run(args,name):
 with (OUT/(name+'.log')).open('w') as log:
  r=subprocess.run(args,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
 check(name,r.returncode==0,'exit='+str(r.returncode))
def reboot():
 memory('pre-reboot')
 run([sys.executable,str(ROOT/'tests/hardware/reset_board.py'),'--port',a.port],'reset-'+str(len(results)))
 with LoggedSerial(a.port,115200,timeout=.05) as s:boot=read_for(s,3)
 check('actual restart observed','[Boot] Reset reason:' in boot,boot[-400:])
 check('reboot no panic','Guru Meditation' not in boot and 'Stack smashing' not in boot,boot[-400:])
 return boot
def hp():
 s=cmd('s');m=re.search(r'Health:\s*(\d+)',s);assert m,s;return int(m[1])
def export(slot,name,pet=False):
 path=OUT/(name+'.bin');tool.cmd_export(a.port,115200,10,slot,path,pet_only=pet);return path
def imp(slot,path):tool.cmd_import(a.port,115200,10,slot,path)
def idle(timeout=20):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  s=cmd('ctx',.2)
  if '[MC] Context: IDLE' in s and 'blocking=0 playing=0 waiting=0' in s:return
 raise AssertionError('Animation/menu did not return to IDLE: '+s)
def current_slot():
 ss=tool.cmd_status(a.port,115200,.8);v=[]
 for s in ss:
  m=re.search(r'seq=(\d+)',s.raw_line)
  if m:v.append((int(m[1]),s.slot))
 assert v,ss;return max(v)[1]
def save():check('save console success','[Save] OK' in cmd('save'))
def memory(label):
 s=cmd('devinfo');m=re.search(r'Memory: heap=(\d+) min_heap=(\d+) max_alloc=(\d+) psram=(\d+) free_psram=(\d+) stack_bytes=(\d+)',s);check(label+' memory telemetry',bool(m),s)
 values=list(map(int,m.groups()));check(label+' PSRAM detected',7*1024*1024<values[3]<=8*1024*1024,str(values));
 if 'pressure' in a.env:check(label+' external RAM exhausted',values[4]<4096,str(values))
 return values
try:
 # Upload preserves NVS; explicit erase below establishes test data only after backup.
 run([a.pio,'run','-e',a.env,'-t','upload','--upload-port',a.port]+(['--project-conf',a.project_conf] if a.project_conf else []),'upload')
 (OUT/'firmware-receipt.json').write_text(json.dumps({'environment':a.env,'firmware_sha256':hashlib.sha256((ROOT/'.pio/build'/a.env/'firmware.bin').read_bytes()).hexdigest(),'backup_sha256':hashlib.sha256(a.backup.read_bytes()).hexdigest(),'started':datetime.datetime.now(datetime.timezone.utc).isoformat()},indent=2)+'\n')
 boot=cmd('h',2);check('console responds','Console ===' in boot,boot[-1000:])
 # Erase via bootloader only the declared NVS partition, avoiding unknown-data locks.
 run([sys.executable,str(Path.home()/'.platformio/packages/tool-esptoolpy/esptool.py'),'--port',a.port,'erase_region','0x9000','0x5000'],'erase-test-nvs')
 time.sleep(2)
 check('initial SET_TIME','entering normal operation' in cmd('SET_TIME 1704096000',2))
 cmd('dim_t 0');cmd('off_t 0');idle();baseMemory=memory('baseline')
 check('new pet HP50',hp()==50)
 save();base=export(current_slot(),'initial-full')
 check('full v10 bytes',len(base.read_bytes())==228)
 check('initial gallery bound',base.read_bytes()[160]==1)
 if not a.visits_only:
  # Poke uses menu mapping, production rules and animation callback.
  cmd('sr 30');out=cmd('btn r',1.5);check('poke production effect','Poke result:' in out,out)
  idle();out=cmd('btn r',1.5);check('poke cooldown','cooldown (animation only)' in out,out);idle()
  # Serial button injection exercises the menu/rules; GPIO electrical tests remain manual.
  manifest=json.loads((ROOT/'src/content'/('sprout' if 'sprout' in a.env else 'artoria')/'manifest.json').read_text())
  healthy={f['id'] for f in manifest['foods'] if f['healthy']}
  choices=range(2) if 'sprout' in a.env else range(1)
  for choice in choices:
   imp(0,base);idle()
   # Artoria random draw may have fewer than three healthy foods; cancel and redraw.
   for attempt in range(30):
    out=cmd('btn l',2);m=re.search(r'Draw food IDs: (\d+) (\d+) (\d+) (\d+)',out);assert m,out
    ids=list(map(int,m.groups()));wanted=[i for i,x in enumerate(ids) if x in healthy]
    if len(wanted)>=3:break
    time.sleep(2);imp(0,base);idle()
   check('healthy draw available',len(wanted)>=3,str(ids))
   time.sleep(2);cursor=1
   for pickIndex,target in enumerate(wanted[:3]):
    while cursor!=target:cmd('btn r',.12);cursor=(cursor+1)%4
    # Queue bounce presses with confirmation, before reading animation logs.
    # Serial rendering can stream for >1s; a later write may arrive after idle.
    out=cmd('btn m'+('\nbtn m'*3 if pickIndex==2 else ''),.15)
   end=time.monotonic()+20;seen=''
   while time.monotonic()<end:
    ctx=cmd('ctx',.2);seen+=ctx
    if 'Context: SPECIAL_FOOD' in ctx:break
   check('combo reaches special selection','Context: SPECIAL_FOOD' in seen,seen[-500:])
   foodDeltas={food['id']:food['health_delta'] for food in manifest['foods']}
   expectedHP=63 if 'sprout' in a.env else min(100,50+sum(foodDeltas[ids[i]] for i in wanted[:3])+15)
   check('healthy HP delta',hp()==expectedHP,'expected='+str(expectedHP))
   if choice:cmd('btn r')
   out=cmd('btn m\nbtn m\nbtn m\nbtn m',.15);check('special effect once',out.count('Special food selected:')==1,out)
   idle();s=cmd('s');check('feed counted once','Fed today:  1 /' in s,s)
   if 'sprout' in a.env:check('Sprout special HP68',hp()==68)
   save();fed=export(current_slot(),'fed-'+str(choice));beforeHP=hp();reboot();cmd('SET_TIME '+str(int.from_bytes(fed.read_bytes()[16:20],'little')),1);idle();check('fed restart health',hp()==beforeHP)
  # Growth uses production daily workflow, not forced grad.
  cmd('hp 80');cmd('age '+('0' if 'sprout' in a.env else '2'));cmd('d',3);idle();s=cmd('s');check('daily growth adult',('Stage:      Bloom' if 'sprout' in a.env else 'Stage:      Adult') in s,s);save()
  grown=export(current_slot(),'grown');reboot();cmd('SET_TIME '+str(int.from_bytes(grown.read_bytes()[16:20],'little')),1);idle();s=cmd('s');check('growth survives restart',('Stage:      Bloom' if 'sprout' in a.env else 'Stage:      Adult') in s,s)
  out=cmd('btnl m',1);check('gallery production menu','Context: GALLERY' in cmd('ctx'),out);cmd('btn m');idle()
 # Actual full and pet-only Python transfers; frozen slot raw bytes across saves/reboot.
 for slot in range(0 if a.skip_visits else 3):
  imp(slot,base);idle();cmd('hp 50');save();ownerFull=export(current_slot(),'owner-'+str(slot))
  cmd('hp 71');save();visitor=export(current_slot(),'visitor-'+str(slot),True)
  imp(slot,ownerFull);idle();check('full import owner50',hp()==50)
  for attempt in range(3):
   if current_slot()==slot:break
   save()
  check('requested frozen owner slot',current_slot()==slot,'slot='+str(slot))
  expectedOwner=current_slot();imp((expectedOwner+1)%3,visitor);idle();s=cmd('devinfo');m=re.search(r'Owner slot:\s*(\d+)',s);assert m,s;frozen=int(m[1]);check('pet import visiting','Visiting:     YES' in s and hp()==71,s)
  frozenBefore=export(frozen,'frozen-before-'+str(slot)).read_bytes();save();cmd('hp 82');save();reboot();cmd('SET_TIME '+str(int.from_bytes(visitor.read_bytes()[16:20],'little')),1);idle();check('visit reboot stays visiting','Visiting:     YES' in cmd('devinfo') and hp()==82)
  check('frozen owner byte equality',export(frozen,'frozen-after-'+str(slot)).read_bytes()==frozenBefore,'actual frozen slot='+str(frozen))
  out=cmd('reset',2);idle();check('owner restored',hp()==50 and 'Visiting:     NO' in cmd('devinfo'),out);save();save();ownerRestored=export(current_slot(),'restored-'+str(slot));reboot();cmd('SET_TIME '+str(int.from_bytes(ownerRestored.read_bytes()[16:20],'little')),1);idle();check('restored owner restart',hp()==50 and 'Visiting:     NO' in cmd('devinfo'))
 # Protocol rejects incomplete/aborted/duplicate commits; fragmented command on UART.
 before=export(current_slot(),'protocol-before').read_bytes()
 with LoggedSerial(a.port,115200,timeout=.05) as s:
  s.write(b'SAVE_SLOT_');read_for(s,.2);s.write(b'STATUS\n');out=read_for(s,1)
 check('fragmented UART command',all('slot'+str(i)+':' in out for i in range(3)),out)
 cmd('SAVE_IMPORT_BEGIN 0');cmd('SAVE_IMPORT_DATA @0 00');out=cmd('SAVE_IMPORT_COMMIT');check('incomplete commit rejected','Incomplete' in out,out);cmd('SAVE_IMPORT_ABORT');out=cmd('SAVE_IMPORT_COMMIT');check('duplicate/aborted commit rejected','No active session' in out,out)
 check('bad transfer does not alter valid slot',export(current_slot(),'protocol-after').read_bytes()==before)
 endMemory=memory('end');check('heap headroom',endMemory[1]>16000 and endMemory[5]>512,str(endMemory))
 check("suite completed",True)
 print('BOARD SUITE COMPLETE',flush=True)
finally:
 (OUT/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n');transcript.close()

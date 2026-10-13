#!/usr/bin/env python3
"""Measure production maximum offline advancement on an authorized physical MCU."""
import argparse,json,re,shutil,subprocess,sys,time
from pathlib import Path
import serial
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--env',required=True,choices=['artoria-offline','sprout-offline']);p.add_argument('--port',default='/dev/ttyACM0');p.add_argument('--backup',type=Path,required=True);p.add_argument('--authorize-board-tests',action='store_true');p.add_argument('--pio',default=shutil.which('pio') or str(Path(sys.executable).with_name('pio')));a=p.parse_args();assert a.authorize_board_tests and a.backup.stat().st_size==0x1000000
out=ROOT/'docs/engine/evidence/hardware'/a.env;out.mkdir(parents=True,exist_ok=True);results=[]
def ck(name,test,detail=''):
 row={'test':name,'result':'PASS' if test else 'FAIL','detail':detail};results.append(row);print(row,flush=True)
 if not test:raise AssertionError(name+': '+detail)
def run(cmd,label):
 with (out/(label+'.log')).open('w') as f:r=subprocess.run(cmd,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT)
 ck(label,r.returncode==0,'exit='+str(r.returncode))
esptool=str(Path.home()/'.platformio/packages/tool-esptoolpy/esptool.py')
try:
 run([sys.executable,esptool,'--port',a.port,'--after','no_reset','erase_region','0x9000','0x5000'],'erase-test-nvs')
 run([a.pio,'run','--project-conf','tests/hardware/platformio-probes.ini','-e',a.env,'-t','upload','--upload-port',a.port],'upload')
 with serial.Serial(a.port,115200,timeout=.05) as s, (out/'uart.log').open('wb',buffering=0) as log:
  end=time.monotonic()+90;data=b''
  while time.monotonic()<end:
   raw=s.read(max(1,s.in_waiting));log.write(raw);data+=raw
   if re.search(rb'\[HW Offline\] (?:PASS|FAIL)[^\n]*\n',data):break
 text=data.decode(errors='replace');match=re.search(r'\[HW Offline\] PASS seconds=(\d+) elapsed_us=(\d+) age=(\d+) restored_age=(\d+) heap=(\d+) min_heap=(\d+) stack_bytes=(\d+)',text)
 ck('physical maximum offline callback',bool(match),text[-1000:]);values=list(map(int,match.groups()));ck('366 days and persisted age',values[0]==366*86400 and values[2]==366 and values[3]==366,str(values));ck('no observed watchdog/panic',not any(x in text for x in ['Guru Meditation','TASK_WDT','INT_WDT','Stack smashing']))
 (out/'measurement.json').write_text(json.dumps(dict(zip(['seconds','elapsed_us','age','restored_age','heap','min_heap','stack_bytes'],values)),indent=2)+'\n')
 run([sys.executable,str(ROOT/'tests/hardware/reset_board.py'),'--port',a.port],'reboot')
 with serial.Serial(a.port,115200,timeout=.1) as s:
  end=time.monotonic()+3;data=b''
  while time.monotonic()<end:data+=s.read(max(1,s.in_waiting))
  s.write(b's\n');end=time.monotonic()+2
  while time.monotonic()<end:data+=s.read(max(1,s.in_waiting))
 text=data.decode(errors='replace');(out/'restart.log').write_text(text);ck('maximum offline survives actual restart','Age:        Day 367' in text and '[HW Offline] SKIPPED existing data' in text,text[-1000:]);ck('suite completed',True)
finally:(out/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')

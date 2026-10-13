#!/usr/bin/env python3
"""Real USB reopen regression. BEGIN/ABORT never persist a pet record."""
import importlib.util,json,subprocess,sys,time
from pathlib import Path
import serial
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('save_tool',ROOT/'SaveManager/save_manager.py');tool=importlib.util.module_from_spec(spec);sys.modules[spec.name]=tool;spec.loader.exec_module(tool)
port=sys.argv[1] if len(sys.argv)>1 else '/dev/ttyACM0'
rows=[]
def read(s,seconds):
 end=time.monotonic()+seconds;raw=b''
 while time.monotonic()<end:raw+=s.read(max(1,s.in_waiting))
 return raw
for i in range(6):
 r=subprocess.run([sys.executable,str(ROOT/'tests/hardware/reset_board.py'),'--port',port],capture_output=True);assert r.returncode==0,r.stderr
 with serial.Serial(port,115200,timeout=.05) as s:read(s,3)
 for command,seconds in [('SET_TIME 1704182400',1),('ctx',.2),('s',.4),('devinfo',.4)]:
  with serial.Serial(port,115200,timeout=.05) as s:
   read(s,.1);s.write((command+'\n').encode());s.flush();read(s,seconds)
 with serial.Serial(port,115200,timeout=.2) as s:
  tool._write_line(s,'SAVE_IMPORT_BEGIN 1')
  response=tool._wait_for_lines(s,lambda line:line.startswith('[SaveImport] READY'),1,2)[0]
  assert 'slot=1 target_bytes=228' in response and response.endswith('offset=1'),response
  tool._write_line(s,'SAVE_IMPORT_ABORT')
  tool._wait_for_lines(s,lambda line:line=='[SaveImport] ABORTED',1,2)
 row={'iteration':i,'result':'PASS','response':response};rows.append(row);print(row,flush=True)
(ROOT/'docs/engine/evidence/hardware/usb-reopen-regression.json').write_text(json.dumps(rows,indent=2)+'\n')

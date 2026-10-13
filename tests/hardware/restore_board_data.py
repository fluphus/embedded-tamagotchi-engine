#!/usr/bin/env python3
"""Final authorized test cleanup: default engine + original backed-up NVS.
Also exercises real v9 full/pet UART imports before restoring the raw partition.
Requires the device's own pre-test flash and actual v9 UART backups beside it.
"""
import argparse,hashlib,importlib.util,json,re,shutil,struct,subprocess,sys,time
from pathlib import Path
import serial
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--verify-only',action='store_true');p.add_argument('--port',default='/dev/ttyACM0');p.add_argument('--backup',required=True,type=Path);p.add_argument('--authorize-board-tests',action='store_true');p.add_argument('--pio',default=shutil.which('pio') or str(Path(sys.executable).with_name('pio')));a=p.parse_args();assert a.authorize_board_tests
base=a.backup.parent;image=a.backup.read_bytes();assert len(image)==0x1000000
receipt=json.loads((base/'receipt.json').read_text())
assert receipt['size']==len(image) and receipt['sha256']==hashlib.sha256(image).hexdigest(),'Original flash backup checksum mismatch'
full=base/'real-v9-slot0-full.bin';pet=base/'real-v9-slot0-pet.bin';assert full.stat().st_size==84 and pet.stat().st_size==72
if a.verify_only:
 print('PASS original backup SHA256 and real v9 file sizes; no serial opened');raise SystemExit(0)
out=ROOT/'docs/engine/evidence/hardware/final-restore';out.mkdir(parents=True,exist_ok=True);log=(out/'uart.log').open('w',buffering=1);results=[]
spec=importlib.util.spec_from_file_location('tool',ROOT/'SaveManager/save_manager.py');m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m)
def ck(name,test,detail=''):
 r={'test':name,'result':'PASS' if test else 'FAIL','detail':detail};results.append(r);print(r,flush=True)
 if not test:raise AssertionError(name+': '+detail)
def run(cmd,name):
 with (out/(name+'.log')).open('w') as f:r=subprocess.run(cmd,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT)
 ck(name,r.returncode==0,'exit='+str(r.returncode))
def cmd(c,seconds=1):
 with serial.Serial(a.port,115200,timeout=.05) as s:
  s.reset_input_buffer();s.write((c+'\n').encode());end=time.monotonic()+seconds;b=b''
  while time.monotonic()<end:b+=s.read(max(1,s.in_waiting))
 text=b.decode(errors='replace');log.write('TX '+repr(c)+'\nRX '+repr(text)+'\n');return text
def export(slot,name,private=False):
 f=(base if private else out)/(name+'.bin');m.cmd_export(a.port,115200,8,slot,f);return f
def newest():
 statuses=m.cmd_status(a.port,115200,1);return max((int(re.search(r'seq=(\d+)',s.raw_line).group(1)),s.slot) for s in statuses if 'seq=' in s.raw_line)[1]
def hp():return int(re.search(r'Health:\s*(\d+)',cmd('s')).group(1))
try:
 run([a.pio,'run','-e','esp32-s3-devkitc-1','-t','upload','--upload-port',a.port],'default-engine-upload')
 time.sleep(2)
 # At this point the previous test firmware is Sprout; foreign content locks startup.
 # The explicit user command is the recovery/delete trigger; startup never clears it.
 text=cmd('SAVE_RECOVERY_ERASE CONFIRM');ck('explicit cross-package recovery erase','Explicit erase completed' in text,text)
 saved=int.from_bytes(full.read_bytes()[12:16],'little');cmd('SET_TIME '+str(saved),2);cmd('dim_t 0');cmd('off_t 0')
 m.cmd_import(a.port,115200,8,0,full);time.sleep(3);status=cmd('s');ck('real v9 full import key fields','Form:       Rhongomyniad' in status and 'Health:     50 /' in status and 'Seriousness:100 /' in status and 'Age:        Day 122' in status and 'Gallery:    3 / 12' in status,status)
 # Repeat COMMIT after a completed real import; snapshot stays identical.
 before=export(newest(),'real-v9-imported',True).read_bytes();ck('completed COMMIT not reapplied','No active session' in cmd('SAVE_IMPORT_COMMIT'));ck('duplicate COMMIT preserves result',export(newest(),'real-v9-after-repeat',True).read_bytes()==before)
 cmd('hp 61');ck('owner test save','[Save] OK' in cmd('save'));owner=newest();before=export(owner,'real-v9-owner-frozen',True).read_bytes()
 m.cmd_import(a.port,115200,8,(owner+1)%3,pet);ck('real v9 pet visit',hp()==50 and 'Visiting:     YES' in cmd('devinfo'))
 cmd('save');cmd('save');ck('real v9 visit frozen owner preserved',export(owner,'real-v9-owner-after-visit',True).read_bytes()==before)
 cmd('reset',3);ck('real v9 visit restores owner',hp()==61 and 'Visiting:     NO' in cmd('devinfo'));export(-1,'real-owner-recovery-copy',True)
 # Recover the complete original NVS, including original device rounds and power config.
 found=False
 for offset in range(0x8000,0x8c00,32):
  magic,typ,sub,start,size,name,flags=struct.unpack('<HBBII16sI',image[offset:offset+32])
  if magic!=0x50aa:break
  if name.rstrip(b'\0')==b'nvs':
   ck('original NVS partition layout',start==0x9000 and size==0x5000);found=True;break
 assert found
 nvs=base/'before-nvs.bin';nvs.write_bytes(image[start:start+size]);esptool=str(Path.home()/'.platformio/packages/tool-esptoolpy/esptool.py')
 run([sys.executable,esptool,'--port',a.port,'--after','no_reset','write_flash',hex(start),str(nvs)],'restore-original-nvs')
 verify=base/'restored-nvs-readback.bin'
 run([sys.executable,esptool,'--port',a.port,'read_flash',hex(start),hex(size),str(verify)],'verify-original-nvs')
 ck('raw NVS restored byte exact',verify.read_bytes()==nvs.read_bytes(),hashlib.sha256(nvs.read_bytes()).hexdigest())
 time.sleep(2);status=cmd('s');device=cmd('devinfo');power=cmd('pwrinfo');ck('final original pet and gallery','Form:       Rhongomyniad' in status and hp()==50 and 'Seriousness:100 /' in status and 'Age:        Day 122' in status and 'Gallery:    3 / 12' in status,status);ck('original device state','Rounds:       3' in device and 'Visiting:     NO' in device,device)
 export(0,'final-migrated-original',True)
 (out/'final-state.json').write_text(json.dumps({'environment':'esp32-s3-devkitc-1','firmware_sha256':hashlib.sha256((ROOT/'.pio/build/esp32-s3-devkitc-1/firmware.bin').read_bytes()).hexdigest(),'original_flash_backup':str(a.backup),'original_flash_sha256':hashlib.sha256(image).hexdigest(),'nvs_restored_and_readback_verified':True,'status':status,'device':device,'power_info':power},ensure_ascii=False,indent=2)+'\n')
 ck('suite completed',True)
finally:(out/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n');log.close()

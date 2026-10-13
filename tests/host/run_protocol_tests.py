#!/usr/bin/env python3
"""Actual Python commands communicate with the firmware's portable save protocol.
Only byte transport and NVS are substituted. No rule/codec/protocol copies.
"""
import importlib.util, os, shlex, subprocess, tempfile, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'SaveManager'))
import save_manager as tool
SOURCES=['tests/host/audit_test.cpp','src/core/save_manager.cpp','src/app/import_workflow.cpp','src/pet/gallery.cpp','src/app/legacy_logging.cpp','src/pet/feeding.cpp','src/pet/evolution.cpp','src/pet/seriousness.cpp']+[f'src/content/artoria/{n}.cpp' for n in ('feeding','evolution','seriousness')]
class Transport:
    def __init__(self,binary):
        self.proc=subprocess.Popen([binary,'protocol'],cwd=ROOT,stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,bufsize=1)
        self.pending=[];self.drop_ack=False;self.drop_all_acks=False;self.drop_export_line=False;self.disconnect=False;self.disconnect_after=None;self.sent_commands=0;self.fragment=b''
    def __enter__(self):return self
    def __exit__(self,*args):return False
    def close(self):self.proc.stdin.close();assert self.proc.wait(timeout=5)==0
    def exchange(self,text):
        self.proc.stdin.write(text+'\n');self.proc.stdin.flush();lines=[]
        while True:
            line=self.proc.stdout.readline().strip()
            assert line, 'bridge died'
            if line=='BRIDGE_END':break
            lines.append(line)
        return lines
    def write(self,data):
        self.sent_commands+=1
        if self.disconnect or self.disconnect_after==self.sent_commands:raise OSError('simulated disconnected transport')
        text=data.decode().rstrip('\r\n')
        # Split every real host-tool command across multiple reads.
        cut=len(text)//2;self.pending+=self.exchange('PART '+text[:cut])
        result=self.exchange(text[cut:])
        if (self.drop_ack or self.drop_all_acks) and text.startswith('SAVE_IMPORT_DATA'):
            self.drop_ack=False;result=[line for line in result if 'RECV ' not in line]
        if self.drop_export_line and text.startswith('SAVE_EXPORT'):
            self.drop_export_line=False
            for i,line in enumerate(result):
                if line.startswith('SAVE_EXPORT_DATA '):del result[i];break
        self.pending+=result;return len(data)
    def flush(self):pass
    def reset_input_buffer(self):self.pending=[]
    def reset_output_buffer(self):pass
    def readline(self):
        if self.fragment:
            result=self.fragment;self.fragment=b'';return result
        if not self.pending:return b''
        line=(self.pending.pop(0)+'\n').encode();cut=max(1,len(line)//2)
        self.fragment=line[cut:];return line[:cut]

def reject(action,kind):
    try:action()
    except (RuntimeError,TimeoutError,OSError,ValueError):print('PASS reject',kind);return
    raise AssertionError('unexpected success '+kind)
with tempfile.TemporaryDirectory(prefix='save-protocol-') as tmp:
    for pack in ('artoria','sprout'):
        binary=str(Path(tmp)/pack)
        cmd=[os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-class-memaccess',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),'-Isrc']
        if pack=='sprout':cmd+=['-DCONTENT_SPROUT=1']
        cmd+=SOURCES+['-o',binary];print('+',shlex.join(cmd),flush=True);subprocess.run(cmd,cwd=ROOT,check=True)
        serial=Transport(binary);tool._open_serial=lambda *args:serial
        full=Path(tmp)/'full.bin';pet=Path(tmp)/'pet.bin';owner=Path(tmp)/'owner.bin'
        tool.cmd_export('mock',115200,0.05,0,full)
        tool.cmd_export('mock',115200,0.05,0,pet,pet_only=True)
        serial.drop_ack=True
        tool.cmd_import('mock',115200,0.05,1,full)
        assert any('No active session' in x for x in serial.exchange('SAVE_IMPORT_COMMIT'))
        unchanged=full.read_bytes();serial.drop_export_line=True
        reject(lambda:tool.cmd_export('mock',115200,0.05,0,full),'truncated export')
        assert full.read_bytes()==unchanged
        baseline=serial.exchange('STATE')[0]
        raw=pet.read_bytes()
        # An exact indexed retransmission is acknowledged without appending.
        serial.exchange('SAVE_IMPORT_BEGIN 2');hx=raw[:16].hex()
        first=serial.exchange('SAVE_IMPORT_DATA @0 '+hx)
        assert serial.exchange('SAVE_IMPORT_DATA @0 '+hx)==first
        for command in ('SAVE_IMPORT_DATA @16 GG','SAVE_IMPORT_DATA @999 00'):
            serial.exchange('SAVE_IMPORT_BEGIN 2');serial.exchange(command)
            assert any('No active session' in x for x in serial.exchange('SAVE_IMPORT_COMMIT'))
        serial.exchange('SAVE_IMPORT_BEGIN 2');serial.exchange('SAVE_IMPORT_DATA @0 00')
        assert any('Incomplete' in x for x in serial.exchange('SAVE_IMPORT_COMMIT'))
        serial.exchange('SAVE_IMPORT_BEGIN 2');serial.exchange('SAVE_IMPORT_ABORT')
        assert any('No active session' in x for x in serial.exchange('SAVE_IMPORT_COMMIT'))
        serial.exchange('SAVE_IMPORT_BEGIN 2');serial.exchange('TICK 30000')
        assert any('No active session' in x for x in serial.exchange('SAVE_IMPORT_COMMIT'))
        assert any('overlong' in x for x in serial.exchange('SAVE_IMPORT_BEGIN 2'+' '*80))
        serial.exchange('PART SAVE_IMPORT_BE');serial.exchange('TICK 30000')
        serial.exchange('GIN 2');assert any('No active session' in x for x in serial.exchange('SAVE_IMPORT_COMMIT'))
        serial.disconnect=True
        reject(lambda:tool.cmd_import('mock',115200,0.05,2,pet),'disconnect');serial.disconnect=False
        serial.disconnect_after=serial.sent_commands+3
        reject(lambda:tool.cmd_import('mock',115200,0.05,2,pet),'mid-transfer disconnect')
        serial.disconnect_after=None;serial.exchange('SAVE_IMPORT_ABORT')
        serial.drop_all_acks=True
        reject(lambda:tool.cmd_import('mock',115200,0.05,2,pet),'all ACKs lost')
        serial.drop_all_acks=False;serial.exchange('SAVE_IMPORT_ABORT');serial.pending=[];serial.fragment=b'';serial._save_line_buffer=b''
        assert serial.exchange('STATE')[0]==baseline  # rejected/disconnected/duplicate packets never persist
        # Real command recovery loop after failed full import: export BEFORE erase.
        serial.exchange('FAULT gallery')
        reject(lambda:tool.cmd_import('mock',115200,0.05,1,full),'gallery write')
        assert 'recovery=1' in serial.exchange('STATE')[0]
        tool.cmd_export('mock',115200,0.05,0,owner)
        serial.exchange('CLEAR_FAULT');serial.exchange('RESTART')
        serial.exchange('FAULT erase');before=serial.exchange('STATE')[0]
        assert any('FAILED' in x for x in serial.exchange('SAVE_RECOVERY_ERASE CONFIRM'))
        assert serial.exchange('STATE')[0]==before
        serial.exchange('CLEAR_FAULT');serial.exchange('SAVE_RECOVERY_ERASE CONFIRM')
        tool.cmd_import('mock',115200,0.05,0,owner)
        assert any('Loaded HP=50' in x for x in serial.exchange('RESTART'))
        # Failed owner restoration, and then ONLY retained owner_restore.
        tool.cmd_import('mock',115200,0.05,2,pet)
        serial.exchange('FAULT owner');serial.exchange('RESTORE')
        assert 'recovery=1' in serial.exchange('STATE')[0]
        tool.cmd_export('mock',115200,0.05,-1,owner)
        serial.exchange('BACKUP_ONLY');assert any('Recovery' in x for x in serial.exchange('RESTART'))
        second=Path(tmp)/'backup-only.bin';tool.cmd_export('mock',115200,0.05,-1,second)
        assert second.read_bytes()==owner.read_bytes()
        serial.exchange('BACKUP_PET_ONLY');assert any('Recovery' in x for x in serial.exchange('RESTART'))
        pet_source=Path(tmp)/'pet-source-owner.bin'
        tool.cmd_export('mock',115200,0.05,-1,pet_source)
        assert len(pet_source.read_bytes())==len(full.read_bytes())
        serial.exchange('CLEAR_FAULT');serial.exchange('SAVE_RECOVERY_ERASE CONFIRM')
        tool.cmd_import('mock',115200,0.05,0,pet_source)
        assert any('Loaded HP=50' in x for x in serial.exchange('RESTART'))
        serial.close();print('PASS production protocol + actual Python tool + explicit recovery:',pack,flush=True)

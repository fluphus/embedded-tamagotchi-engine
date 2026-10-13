#!/usr/bin/env python3
"""Read-only production-code probes for the separate original Artoria repair tree."""
import argparse, importlib.util, subprocess, sys, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--original',type=Path,required=True);args=p.parse_args();old=args.original.resolve()
def run(command):
    print('+',*map(str,command),flush=True)
    subprocess.run(list(map(str,command)),cwd=old,check=True)
run(['git','diff','--exit-code','448dbe158595c1db9f1d2fc37caa15810c53de1c','--','src/core/time_manager.cpp','src/input/button_driver.cpp','src/input/button_driver.h','src/display/DisplayManager.cpp','SaveManager/save_manager.py'])
with tempfile.TemporaryDirectory(prefix='original-shared-review-') as temp:
    temp=Path(temp)
    peripheral=temp/'peripheral.cpp'
    peripheral.write_text(r'''
#include "core/time_manager.h"
#include "input/button_driver.h"
#include <iostream>
uint32_t hostMillis=0;int hostPins[64];HostSerial Serial;
int main(){
 timeManager.init();timeManager.setSimulatedTime(2024,1,1,0,0);
 auto previous=timeManager.now();bool reversed=false;
 for(int i=0;i<50;++i){hostMillis+=100000000;auto now=timeManager.now();
  if(now<previous){std::cout<<"REPRO time reversal previous="<<previous<<" now="<<now<<" elapsed_step="<<i+1<<"\n";reversed=true;break;}previous=now;}
 if(!reversed)return 1;
 hostMillis=0;for(auto& pin:hostPins)pin=HIGH;buttonDriver.init();
 for(auto pin:BTN_PIN_MAP)hostPins[pin]=LOW;
 hostMillis+=31;buttonDriver.update();hostMillis+=BTN_DESTROY_HOLD_MS+1;buttonDriver.update();
 if(!buttonDriver.isComboTriggered())return 2;
 buttonDriver.resetCombo();for(auto pin:BTN_PIN_MAP)hostPins[pin]=HIGH;
 hostMillis+=31;buttonDriver.update();int presses=0;
 for(int i=0;i<buttonDriver.getEventCount();++i)if(buttonDriver.getEvents()[i].type==BTN_EVENT_PRESS)++presses;
 std::cout<<"REPRO destroy chord release PRESS events="<<presses<<"\n";
 return presses==3?0:3;
}
''')
    run(['g++','-std=c++17','-Wno-format','-I'+str(ROOT/'tests/host/peripherals'),'-I'+str(old/'src'),peripheral,old/'src/core/time_manager.cpp',old/'src/input/button_driver.cpp','-o',temp/'peripheral'])
    run([temp/'peripheral'])
    storage=temp/'storage.cpp'
    storage.write_text('#define main existing_regression_main\n#include "'+str(old/'tests/host/save_repair_test.cpp')+'"\n#undef main\n'+r'''
int main(){
 saveio::diagnostic=[](const char*s){output+=s;};
 Store s;SaveManager m;PetState p;DeviceState d;GalleryData g;seed(s,m,p,d,g);
 SaveHeader h;memcpy(&h,s.values["s0_hdr"].data(),sizeof(h));h.sequence=UINT32_MAX;s.values["s0_hdr"]=raw(h);
 SaveManager boot;CHECK(boot.init()==SAVE_OK);CHECK(save_workflow::loadStartup(boot,p)==save_workflow::Startup::Loaded);
 p.health=73;const auto result=boot.save(p,1700000300);SaveHeader next;memcpy(&next,s.values["s1_hdr"].data(),sizeof(next));
 SaveManager again;CHECK(again.init()==SAVE_OK);CHECK(save_workflow::loadStartup(again,p)==save_workflow::Startup::Loaded);
 std::cout<<"REPRO sequence save_result="<<int(result)<<" written_sequence="<<next.sequence<<" reboot_HP="<<p.health<<" expected_new_HP=73\n";
 CHECK(result==SAVE_OK&&next.sequence==0&&p.health==50);
 s.attach();saveio::storage.open=[](void*,const char*,bool){return false;};SaveManager unavailable;
 const auto init=unavailable.init();const auto start=save_workflow::loadStartup(unavailable,p);
 std::cout<<"REPRO open failure init_result="<<int(init)<<" startup_NewGame="<<(start==save_workflow::Startup::NewGame)<<"\n";
 CHECK(init==SAVE_ERR_NVS_INIT&&start==save_workflow::Startup::NewGame);
}
''')
    run(['g++','-std=c++17','-Wno-return-type','-I'+str(old/'src'),storage,*[old/'src/core'/n for n in ('save_manager.cpp','save_io.cpp','save_workflows.cpp')],'-o',temp/'storage'])
    run([temp/'storage'])
    spec=importlib.util.spec_from_file_location('original_save_tool',old/'SaveManager/save_manager.py')
    mod=importlib.util.module_from_spec(spec);sys.modules[spec.name]=mod;spec.loader.exec_module(mod)
    class Serial:
        def __init__(self,lines):self.lines=iter(lines)
        def readline(self):return next(self.lines,b'')
        def write(self,data):return len(data)
        def flush(self):pass
        def __enter__(self):return self
        def __exit__(self,*args):pass
    serial=Serial([b'SAVE_EXPORT_BE',b'GIN slot=0 bytes=84\n'])
    first,second=mod._read_line(serial),mod._read_line(serial)
    assert first=='SAVE_EXPORT_BE' and second=='GIN slot=0 bytes=84'
    print('REPRO Python readline fragments treated as two complete lines:',repr(first),repr(second),flush=True)
    mod._open_serial=lambda *args:Serial([b'SAVE_EXPORT_BEGIN slot=0 bytes=84\n',b'SAVE_EXPORT_DATA AA\n',b'SAVE_EXPORT_END slot=0 bytes=84\n'])
    path=temp/'export.bin';mod.cmd_export('MOCK',115200,1,0,path)
    assert path.read_bytes()==b'\xaa'
    print('REPRO Python export announced 84 bytes, saved 1 byte and reported success',flush=True)
print('PASS audit reproduction assertions (confirms defects, not repaired behavior)',flush=True)

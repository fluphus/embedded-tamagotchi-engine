#!/usr/bin/env python3
"""Production D1-D6 regression: real menu/commands/storage/lifecycle, fake peripherals.
No board access. Exit 0 requires correct behavior, not defect reproduction.
"""
import ast, os, shlex, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
tree=ast.parse((ROOT/'tests/host/run_integration_tests.py').read_text())
node=next(n for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='SOURCES' for t in n.targets))
sources=eval(compile(ast.Expression(node.value),'<existing source list>','eval'))[1:]
with tempfile.TemporaryDirectory(prefix='engine-deep-regression-') as temp:
    tmp=Path(temp);(tmp/'esp32').mkdir();(tmp/'driver').mkdir()
    arduino=(ROOT/'tests/host/peripherals/Arduino.h').read_text().replace('std::string output;', 'std::string output; void flush(){} int available(){return 0;} int read(){return -1;}')
    arduino+='\ninline void delay(unsigned ms){hostMillis+=ms;}\n'
    (tmp/'Arduino.h').write_text(arduino)
    causes='UNDEFINED EXT0 EXT1 TIMER TOUCHPAD ULP GPIO UART WIFI COCPU COCPU_TRAP_TRIG BT'.split()
    sleep='''#pragma once
#include <cstdint>
using esp_err_t=int; constexpr int ESP_OK=0;
constexpr int ESP_PD_DOMAIN_RTC_PERIPH=0,ESP_PD_OPTION_ON=0,ESP_EXT1_WAKEUP_ANY_LOW=0;
inline const char* esp_err_to_name(int){return "mock";}
inline int esp_sleep_pd_config(int,int){return 0;}
inline int esp_sleep_enable_ext1_wakeup(uint64_t,int){return 0;}
extern int sleepCalls; inline void esp_deep_sleep_start(){++sleepCalls;}
'''
    sleep+='enum esp_sleep_wakeup_cause_t {'+','.join('ESP_SLEEP_WAKEUP_'+c for c in causes)+'};\n'
    (tmp/'esp_sleep.h').write_text(sleep)
    (tmp/'esp_system.h').write_text('#pragma once\nenum esp_reset_reason_t {'+','.join('ESP_RST_'+c for c in 'POWERON EXT SW PANIC INT_WDT TASK_WDT WDT DEEPSLEEP BROWNOUT SDIO'.split())+'};\n')
    (tmp/'esp_random.h').write_text('#pragma once\n#include <cstdint>\nextern uint32_t hostRandom; inline uint32_t esp_random(){return hostRandom;}\n')
    (tmp/'esp32/rtc.h').write_text('#pragma once\n#include <cstdint>\ninline uint64_t esp_rtc_get_time_us(){return 1000000;}\n')
    (tmp/'driver/rtc_io.h').write_text('#pragma once\nusing gpio_num_t=int;\ninline bool rtc_gpio_is_valid_gpio(int){return true;}\n'+'\n'.join('inline void '+f+'(int){}' for f in ['rtc_gpio_pulldown_dis','rtc_gpio_pullup_en','rtc_gpio_hold_en','rtc_gpio_hold_dis']))
    for pack in ('artoria','sprout'):
        binary=tmp/pack
        cmd=[os.environ.get('CXX','g++'),'-std=c++17','-Wno-format','-DDISPLAY_BACKEND_SERIAL_PLACEHOLDER=1',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),f'-I{tmp}','-I.','-Isrc','-Itests/host/peripherals']
        if pack=='sprout':cmd+=['-DCONTENT_SPROUT=1']
        cmd+=[str(HERE/'deep_regression_test.cpp'),*sources,'src/app/console.cpp','src/platform/arduino/lifecycle.cpp','-o',str(binary)]
        print("+",shlex.join(cmd),flush=True)
        subprocess.run(cmd,cwd=ROOT,check=True)
        subprocess.run([str(binary)],cwd=ROOT,check=True)

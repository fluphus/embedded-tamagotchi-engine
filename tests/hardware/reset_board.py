#!/usr/bin/env python3
"""Actual USB Serial/JTAG RTS reset using esptool's own HardReset strategy.
No ROM inspection, stub upload, flash write or simulated firmware reset.
"""
import argparse,sys,site
from pathlib import Path
import serial
p=argparse.ArgumentParser();p.add_argument('--port',default='/dev/ttyACM0');a=p.parse_args()
package=Path.home()/'.platformio/packages/tool-esptoolpy'
site.addsitedir(str(package/'_contrib'));sys.path.insert(0,str(package))
from esptool.reset import HardReset
port=serial.Serial(port=None,baudrate=115200,timeout=.1)
port.dtr=False;port.rts=False;port.port=a.port;port.open()
with port:
 HardReset(port,uses_usb=True)()
print('[HW reset] actual RTS hard reset; no flash operations')

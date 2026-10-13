#!/usr/bin/env python3
"""Production output adapter: byte preservation, bounded packets, failed writes."""
import os,shlex,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='diagnostic-adapter-') as directory:
 for usb,mode in [(1,1),(1,0),(0,1)]:
  binary=str(Path(directory)/f'{usb}-{mode}')
  command=[os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Werror',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),'-Itests/host/serial_adapter','-Isrc',f'-DARDUINO_USB_CDC_ON_BOOT={usb}',f'-DARDUINO_USB_MODE={mode}','tests/host/diagnostics_test.cpp','-o',binary]
  subprocess.run(command,cwd=root,check=True);subprocess.run([binary],cwd=root,check=True)

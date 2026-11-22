import argparse
import time
from pathlib import Path
import serial

p=argparse.ArgumentParser()
p.add_argument('--seconds',type=float,default=30)
p.add_argument('--output',required=True)
p.add_argument('--send',default='')
a=p.parse_args()
target=Path(a.output);target.parent.mkdir(parents=True,exist_ok=True)
with serial.Serial('COM4',115200,timeout=0.2) as port, target.open('w',encoding='utf-8') as out:
    if a.send:port.write(a.send.encode('ascii'));port.flush()
    start=time.monotonic();pending=b''
    print('COM4 capture started',flush=True)
    while time.monotonic()-start<a.seconds:
        pending+=port.read(port.in_waiting or 1)
        while b'\n' in pending:
            line,pending=pending.split(b'\n',1)
            text='%8.3f %s'%(time.monotonic()-start,line.decode('ascii',errors='backslashreplace').rstrip())
            print(text,flush=True);out.write(text+'\n');out.flush()
    if pending:out.write(repr(pending)+'\n')
print('Saved',target,flush=True)

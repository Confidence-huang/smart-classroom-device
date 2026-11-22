from pathlib import Path
import re
import sys
import json
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'k230'))
from protocol import frame
root=Path(__file__).resolve().parents[1]/'uart-evidence'
tx={}
for packet in re.findall(r'UART TX (\$EV1,[^\r\n]+)',(root/'k230-final-tx.log').read_text(encoding='utf-8')):
    body,crc=packet[1:].split('*')
    assert frame(body).decode().strip()==packet
    _,link,boot,seq,uptime,count,valid=body.split(',')
    tx[(link,boot,int(seq))]=(int(uptime),int(count),int(valid))
rx=[]
pattern=r'RX seq=(\d+) count=(\d+) valid=(\d+) uptime=(\d+) link=([0-9a-f]{16}) boot=([0-9a-f]{16})'
for seq,count,valid,uptime,link,boot in re.findall(pattern,(root/'k230-restart-final.log').read_text(encoding='utf-8')):
    key=(link,boot,int(seq))
    assert key in tx, ('missing TX evidence',key)
    assert tx[key]==(int(uptime),int(count),int(valid)),key
    rx.append(int(seq))
assert len(rx)>=10
assert all(b-a==1 for a,b in zip(rx,rx[1:])),rx
summary=dict(matched_real_uart_samples=len(rx),seq_first=rx[0],seq_last=rx[-1],all_fields_match=True,all_captured_tx_crc_valid=True,scope='recorded contiguous window; not a long-duration test')
(root/'wire-comparison.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print(json.dumps(summary))

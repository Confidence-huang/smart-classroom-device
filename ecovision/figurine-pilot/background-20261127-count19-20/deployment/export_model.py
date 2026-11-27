from pathlib import Path
import json,hashlib
from PIL import Image
import torch,onnx
from ultralytics import YOLO

out=Path(__file__).resolve().parent
root=out.parent
weights=root/'selected/figurine_newbg_v4_20261127.pt'
expected=json.loads((root/'selected/evaluation.json').read_text())['sha256']['V4']
assert hashlib.sha256(weights.read_bytes()).hexdigest()==expected
calib=out/'calibration-top-left';calib.mkdir(exist_ok=True)
# Only training images enter calibration. Reuse the already deployed top-left padding.
for p in sorted((root/'images/train').glob('*')):
    raw=Image.open(p).convert('RGB')
    w,h=raw.size;scale=min(320/w,320/h)
    im=Image.new('RGB',(320,320),(128,128,128))
    im.paste(raw.resize((int(w*scale),int(h*scale)),Image.Resampling.BILINEAR),(0,0))
    im.save(calib/(p.stem+'.png'))
model=YOLO(str(weights)).model.cpu().eval()
head=model.model[-1];head.export=True;head.format='onnx';head.dynamic=False
x=torch.zeros(1,3,320,320)
with torch.no_grad():
    assert tuple(model(x).shape)==(1,5,2100)
    torch.onnx.export(model,x,str(out/'figurine_newbg_v4_20261127.onnx'),opset_version=11,
                      input_names=['images'],output_names=['output0'],dynamo=False)
onnx.checker.check_model(str(out/'figurine_newbg_v4_20261127.onnx'))
print('ONNX checked; output [1,5,2100]; calibration',len(list(calib.glob('*.png'))),flush=True)

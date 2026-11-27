import os
os.environ['POLARS_SKIP_CPU_CHECK']='1'
from pathlib import Path
from ultralytics import YOLO
import torch

def main():
    root=Path(__file__).resolve().parent
    print('GPU:',torch.cuda.get_device_name(0),flush=True)
    model=YOLO(str(root.parent/'background-20261127/selected/figurine_newbg_20261127.pt'))
    model.train(data=str(root/'data.yaml'),project=str(root/'runs'),name='count17_18_v3',
                imgsz=320,epochs=80,patience=22,batch=8,device=0,workers=0,
                optimizer='AdamW',lr0=.0002,lrf=.1,weight_decay=.0005,
                amp=False,mosaic=0,scale=.1,translate=.03,degrees=2,fliplr=.5,
                hsv_h=.01,hsv_s=.15,hsv_v=.15,seed=2718,deterministic=True,plots=True,save_period=20)
    print('TRAINING COMPLETE',flush=True)

if __name__=='__main__':main()

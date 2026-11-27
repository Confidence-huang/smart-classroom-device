import os
os.environ['POLARS_SKIP_CPU_CHECK'] = '1'
from pathlib import Path
from ultralytics import YOLO
import torch, json, argparse

def main():
    root = Path(__file__).resolve().parent
    parser=argparse.ArgumentParser()
    parser.add_argument('--data',default=str(root/'data.yaml'))
    parser.add_argument('--name',default='newbg320')
    args=parser.parse_args()
    weights = root.parent/'multi-15-training/selected/figurine_multi_candidate.pt'
    print('GPU:', torch.cuda.get_device_name(0), flush=True)
    model = YOLO(str(weights))
    model.train(data=args.data, project=str(root/'runs'), name=args.name,
                imgsz=320, epochs=100, patience=25, batch=8, device=0, workers=0,
                optimizer='AdamW', lr0=.0005, lrf=.1, weight_decay=.0005,
                amp=False, mosaic=.15, close_mosaic=15, mixup=0,
                scale=.12, translate=.04, degrees=3, fliplr=.5,
                hsv_h=.01, hsv_s=.15, hsv_v=.2,
                seed=27, deterministic=True, plots=True, save_period=20)
    print('TRAINING COMPLETE', flush=True)

if __name__ == '__main__':
    main()

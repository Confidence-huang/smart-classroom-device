"""Select count thresholds on validation only; report untouched-layout pilot test."""
from pathlib import Path
import json, shutil, hashlib, math
from ultralytics import YOLO
from PIL import Image, ImageDraw
root=Path(__file__).resolve().parent
data=root/'stage2'
rows=json.loads((data/'annotations.json').read_text())
candidate=root/'runs/newbg320_v1/weights/best.pt'
old=root.parent/'multi-15-training/selected/figurine_multi_candidate.pt'
def infer(model,split,conf,iou):
    selected=sorted([r for r in rows if r['split']==split],key=lambda r:r['name'])
    results=list(model.predict([str(root/'raw'/r['name']) for r in selected],imgsz=320,conf=conf,iou=iou,max_det=60,device=0,verbose=False))
    details=[]
    for r,p in zip(selected,results):
        boxes=[dict(xyxy=[round(float(v),2) for v in b.xyxy[0]],conf=round(float(b.conf[0]),4)) for b in p.boxes]
        details.append(dict(index=r['index'],name=r['name'],actual=r['count'],predicted=len(boxes),boxes=boxes))
    errors=[abs(d['predicted']-d['actual']) for d in details]
    return dict(total=len(details),exact=sum(v==0 for v in errors),exact_rate=sum(v==0 for v in errors)/len(errors),
                mae=sum(errors)/len(errors),within_one=sum(v<=1 for v in errors)/len(errors),max_error=max(errors),
                details=details)

model=YOLO(str(candidate)); baseline=YOLO(str(old))
grid=[]
for conf in (.2,.25,.35,.45,.55):
    for iou in (.3,.45,.6):
        score=infer(model,'val',conf,iou)
        grid.append(dict(conf=conf,iou=iou,**{k:v for k,v in score.items() if k!='details'}))
best=min(grid,key=lambda r:(r['mae'],-r['exact'],abs(r['conf']-.35),abs(r['iou']-.3)))
report=dict(validation_grid=grid,chosen=best,
    old_test=infer(baseline,'test',.35,.3),
    new_test_fixed=infer(model,'test',.35,.3),
    new_test=infer(model,'test',best['conf'],best['iou']),
    scope='Same-session layout holdout, 12 images; not a new-session or 25-person acceptance test')
val=model.val(data=str(data/'data.yaml'),split='val',imgsz=320,batch=8,device=0,workers=0,plots=True,
              project=str(root/'evaluation'),name='validation',verbose=False)
report['detection_val']={k:float(v) for k,v in val.results_dict.items()}
out=root/'selected';out.mkdir(exist_ok=True)
shutil.copy2(candidate,out/'figurine_newbg_20261127.pt')
report['model_sha256']=hashlib.sha256(candidate.read_bytes()).hexdigest()
(out/'evaluation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
settings=dict(imgsz=320,conf=best['conf'],nms=best['iou'],max_det=60,max_count_observed=max(r['count'] for r in rows),
              target_max_count=25,status='float model trained; K230 quantization and live acceptance pending')
(out/'settings.json').write_text(json.dumps(settings,indent=2),encoding='utf-8')
# Save every pilot test prediction for review. Labels above each image are measured counts.
def annotated(detail,label):
    im=Image.open(root/'raw'/detail['name']).convert('RGB');d=ImageDraw.Draw(im)
    for b in detail['boxes']:d.rectangle(b['xyxy'],outline='lime',width=2)
    strip=Image.new('RGB',(640,510),'#f1f3f5');strip.paste(im,(0,30));dr=ImageDraw.Draw(strip)
    dr.text((8,8),f'{label} | GT {detail["actual"]}  PRED {detail["predicted"]} | #{detail["index"]}',fill='black')
    return strip
for d in report['new_test']['details']:annotated(d,'NEW').save(out/f'test-{d["index"]:03d}.jpg',quality=94)
comparison=Image.new('RGB',(1280,2040),'white')
old_rows=report['old_test']['details'];new_rows=report['new_test']['details']
chosen=sorted(range(len(old_rows)),key=lambda i:abs(old_rows[i]['predicted']-old_rows[i]['actual']),reverse=True)[:4]
for row,i in enumerate(chosen):
    comparison.paste(annotated(old_rows[i],'OLD conf0.35 NMS0.3'),(0,row*510))
    comparison.paste(annotated(new_rows[i],f'NEW conf{best["conf"]} NMS{best["iou"]}'),(640,row*510))
comparison.save(out/'old-vs-new.jpg',quality=95)
for k in ('old_test','new_test_fixed','new_test'):
    print(k,{a:b for a,b in report[k].items() if a!='details'},flush=True)
print('Thresholds selected on val:',best,flush=True)

from pathlib import Path
import json,hashlib,shutil
from ultralytics import YOLO
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parent
rows=json.loads((root/'annotations.json').read_text())
old_path=root.parent/'background-20261127/selected/figurine_newbg_20261127.pt'
new_path=root/'runs/extra16_v2/weights/best.pt'
old,new=YOLO(str(old_path)),YOLO(str(new_path))
def score(model,split,conf,iou):
    rr=sorted([r for r in rows if r['split']==split],key=lambda r:r['name'])
    pred=list(model.predict([str(root/'images'/split/r['name']) for r in rr],imgsz=320,conf=conf,iou=iou,max_det=60,device=0,verbose=False))
    details=[]
    for r,p in zip(rr,pred):
        boxes=[dict(xyxy=[round(float(x),2) for x in b.xyxy[0]],conf=round(float(b.conf[0]),4)) for b in p.boxes]
        details.append(dict(index=r['index'],cohort=r['cohort'],name=r['name'],actual=r['count'],predicted=len(boxes),boxes=boxes))
    return details
def metrics(details):
    e=[abs(r['actual']-r['predicted']) for r in details]
    return dict(total=len(e),exact=sum(x==0 for x in e),exact_rate=sum(x==0 for x in e)/len(e),mae=sum(e)/len(e),within_one=sum(x<=1 for x in e)/len(e),max_error=max(e))
grid=[]
for conf in (.25,.35,.45,.55):
    for iou in (.3,.45,.6):grid.append(dict(conf=conf,iou=iou,**metrics(score(new,'val',conf,iou))))
best=min(grid,key=lambda r:(r['mae'],-r['exact'],abs(r['conf']-.35),abs(r['iou']-.45)))
before=score(old,'test',.35,.45)
after_fixed=score(new,'test',.35,.45)
after=score(new,'test',best['conf'],best['iou'])
report=dict(chosen_on_val=best,validation_grid=grid,old=metrics(before),new_fixed=metrics(after_fixed),new=metrics(after),
            old_details=before,new_details=after,
            cohorts={c:dict(old=metrics([r for r in before if r['cohort']==c]),new=metrics([r for r in after if r['cohort']==c])) for c in ('previous','extra')})
val=new.val(data=str(root/'data.yaml'),split='val',imgsz=320,batch=8,workers=0,device=0,plots=True,project=str(root/'evaluation'),name='validation',verbose=False)
report['detection_val']={k:float(v) for k,v in val.results_dict.items()}
out=root/'selected';out.mkdir(exist_ok=True)
shutil.copy2(new_path,out/'figurine_newbg_v2_20261127.pt')
report['sha256']=hashlib.sha256(new_path.read_bytes()).hexdigest()
(out/'evaluation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
(out/'settings.json').write_text(json.dumps(dict(imgsz=320,conf=best['conf'],nms=best['iou'],max_det=60,target_max_count=20,observed_max_count=16,status='float model; K230 conversion and 17-20 scene acceptance pending'),indent=2),encoding='utf-8')
for name in ('results.png','results.csv','args.yaml'):shutil.copy2(root/'runs/extra16_v2'/name,out/name)
def picture(r,label):
    im=Image.open(root/'images/test'/r['name']).convert('RGB');d=ImageDraw.Draw(im)
    for b in r['boxes']:d.rectangle(b['xyxy'],outline='lime',width=2)
    pic=Image.new('RGB',(640,510),'#eee');pic.paste(im,(0,30));d=ImageDraw.Draw(pic)
    d.text((8,8),f'{label} | {r["cohort"]} #{r["index"]} GT {r["actual"]} PRED {r["predicted"]}',fill='black')
    return pic
for r in after:picture(r,'V2').save(out/f'test-{r["cohort"]}-{r["index"]}.jpg',quality=94)
cases=sorted(range(len(before)),key=lambda i:abs(before[i]['actual']-before[i]['predicted']),reverse=True)[:2]
cases += [i for i,r in enumerate(before) if r['cohort']=='extra'][:2]
canvas=Image.new('RGB',(1280,len(cases)*510),'white')
for j,i in enumerate(cases):canvas.paste(picture(before[i],'V1'),(0,j*510));canvas.paste(picture(after[i],'V2'),(640,j*510))
canvas.save(out/'v1-vs-v2.jpg',quality=95)
print('Previous model',report['old'],flush=True)
print('V2 fixed thresholds',report['new_fixed'],flush=True)
print('V2 val-selected thresholds',report['new'],flush=True)
print('Cohorts',report['cohorts'],flush=True)
print('Settings',best,flush=True)

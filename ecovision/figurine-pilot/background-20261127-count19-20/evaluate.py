"""Compare four frozen model versions; tune V4 thresholds on validation only."""
from pathlib import Path
import json, hashlib, shutil, csv
from PIL import Image, ImageDraw
from ultralytics import YOLO

root=Path(__file__).resolve().parent
rows=json.loads((root/'annotations.json').read_text())
out=root/'selected';out.mkdir(exist_ok=True)
paths={
 'V1':root.parent/'background-20261127/selected/figurine_newbg_20261127.pt',
 'V2':root.parent/'background-20261127-extra20/selected/figurine_newbg_v2_20261127.pt',
 'V3':root.parent/'background-20261127-count17-18/selected/figurine_newbg_v3_20261127.pt',
 'V4':root/'runs/count19_20_v4/weights/best.pt',
}
models={k:YOLO(str(p)) for k,p in paths.items()}

def score(model,split,conf,iou):
    rr=sorted((r for r in rows if r['split']==split),key=lambda r:r['name'])
    preds=model.predict([str(root/'images'/split/r['name']) for r in rr],imgsz=320,conf=conf,iou=iou,max_det=60,device=0,verbose=False)
    result=[]
    for r,p in zip(rr,preds):
        cohort=r['cohort'] if r['cohort']!='previous' else ('regression0_16' if r['count']<=16 else 'regression17_18')
        boxes=[dict(xyxy=[round(float(x),2) for x in b.xyxy[0]],conf=round(float(b.conf[0]),4)) for b in p.boxes]
        result.append(dict(index=r['index'],cohort=cohort,name=r['name'],actual=r['count'],predicted=len(boxes),boxes=boxes))
    assert len(result)==len(rr)
    return result

def metrics(rr):
    errors=[abs(r['actual']-r['predicted']) for r in rr]
    return dict(total=len(rr),exact=sum(e==0 for e in errors),exact_rate=sum(e==0 for e in errors)/len(rr),mae=sum(errors)/len(rr),max_error=max(errors),within_one=sum(e<=1 for e in errors)/len(rr))

grid=[]
for conf in (.25,.35,.45,.55):
    for iou in (.3,.45,.6,.75):
        grid.append(dict(conf=conf,iou=iou,**metrics(score(models['V4'],'val',conf,iou))))
best=min(grid,key=lambda r:(r['mae'],-r['exact'],abs(r['conf']-.35),abs(r['iou']-.45)))
fixed={k:score(m,'test',.35,.45) for k,m in models.items()}
selected=score(models['V4'],'test',best['conf'],best['iou'])
cohorts={c:{k:metrics([r for r in rr if r['cohort']==c]) for k,rr in dict(fixed,V4_selected=selected).items()} for c in ('regression0_16','regression17_18','new19','new20')}
report=dict(chosen_on_val=best,validation_grid=grid,fixed_settings=dict(conf=.35,nms=.45),fixed={k:metrics(v) for k,v in fixed.items()},
            selected=metrics(selected),cohorts=cohorts,details_fixed=fixed,details_selected=selected,
            sha256={k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in paths.items()},
            limitations='Same-day same-background capture holdout; new14 consist of two groups, shared stationary figurines and similar layouts across sets. Old21 only regression. Model-assisted boxes visually reviewed by assistant; no independent annotation. No quantized K230 verification.')
val=models['V4'].val(data=str(root/'data.yaml'),split='val',imgsz=320,batch=8,workers=0,device=0,plots=True,project=str(root/'evaluation'),name='validation',verbose=False)
report['detection_val']={k:float(v) for k,v in val.results_dict.items()}
(out/'evaluation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
shutil.copy2(paths['V4'],out/'figurine_newbg_v4_20261127.pt')
for name in ('results.png','results.csv','args.yaml'):
    shutil.copy2(root/'runs/count19_20_v4'/name,out/name)
(out/'settings.json').write_text(json.dumps(dict(imgsz=320,conf=best['conf'],nms=best['iou'],max_det=60,target_max_count=20,observed_max_count=20,status='float candidate; K230 conversion, independent capture and time stability acceptance pending'),indent=2),encoding='utf-8')

def picture(r,label):
    im=Image.open(root/'images/test'/r['name']).convert('RGB');d=ImageDraw.Draw(im)
    for b in r['boxes']:d.rectangle(b['xyxy'],outline='lime',width=2)
    pic=Image.new('RGB',(640,510),'#eee');pic.paste(im,(0,30))
    ImageDraw.Draw(pic).text((8,8),f'{label} | {r["cohort"]} #{r["index"]}: GT {r["actual"]}, predicted {r["predicted"]}',fill='black')
    return pic

for r in selected:picture(r,'V4 selected').save(out/f'test-{r["cohort"]}-{r["index"]}.jpg',quality=94)
cases=[i for i,r in enumerate(fixed['V3']) if r['cohort'] in ('new19','new20')]
cases=sorted(cases,key=lambda i:abs(fixed['V4'][i]['actual']-fixed['V4'][i]['predicted']),reverse=True)[:6]
canvas=Image.new('RGB',(1280,len(cases)*510),'white')
for j,i in enumerate(cases):
    canvas.paste(picture(fixed['V3'][i],'V3 fixed .35/.45'),(0,j*510))
    canvas.paste(picture(fixed['V4'][i],'V4 fixed .35/.45'),(640,j*510))
canvas.save(out/'v3-v4-new-cases.jpg',quality=95)
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
fig,ax=plt.subplots(figsize=(9,4.8))
values=[report['fixed'][k]['exact_rate']*100 for k in paths]+[report['selected']['exact_rate']*100]
bars=ax.bar(['V1 fixed','V2 fixed','V3 fixed','V4 fixed','V4 val-selected'],values,color=['#a5b1bb','#b9c6ce','#779ca4','#198c86','#e5a33d'])
for b,v in zip(bars,values):ax.text(b.get_x()+b.get_width()/2,v+1,f'{v:.1f}%',ha='center')
ax.set_ylim(0,112);ax.set_ylabel('Exact count accuracy (%)');ax.set_title('Frozen 35-image comparison (same-day capture holdout)')
ax.spines[['top','right']].set_visible(False);fig.tight_layout();fig.savefig(out/'count-comparison.png',dpi=160);plt.close(fig)
epochs=len(list(csv.DictReader((out/'results.csv').open(encoding='utf-8-sig'))))
lines=['# 19/20人补拍 V4 训练结果','',
       '本轮只完成PC浮点训练和评估，未转换或替换K230模型。','',
       '## 数据与标注','',
       '用户提供.19人及.20人两个文件夹，各19张640×480图片。共38张，经候选框逐图视觉核对，删除右侧黄色衣服、中间浅蓝衣等重复局部框，保留相邻遮挡小人。标注采用模型辅助后视觉复核，未经独立标注员复核。原图及既有版本保留。',
       '新增19训练/5验证/14测试。19到20的过渡组14～21号放在同一测试集合，20人31～36号也整组留出。新增测试14图只有两组连续相关布局，且背景及固定小人在不同集合间相同，不是14个独立场景。合并后161独立训练/22验证/35测试。新增训练图2倍采样，每轮180次读取；所有集合图像SHA无交集。旧21张仅作回归。','',
       '## 训练与阈值','',
       f'从V3继续微调，RTX5070 Laptop GPU，YOLO单类别figurine，320×320、batch8、AdamW lr0=.00015、AMP关闭、mosaic0、seed2720，最多80轮，patience22，实际{epochs}轮。best.pt按验证检测指标保存；计数阈值仅在22张验证图选择。',
       f'选中conf={best["conf"]}、NMS={best["iou"]}；验证人数完全正确{best["exact"]}/{best["total"]}，MAE={best["mae"]:.3f}。','',
       '## 人数完全正确结果','',
       '| 集合 | V1固定 | V2固定 | V3固定 | V4固定 | V4验证选阈值 |',
       '|---|---:|---:|---:|---:|---:|']
for c,label in [('regression0_16','原0～16人15张'),('regression17_18','原17/18人6张'),('new19','新19人6张'),('new20','新20人8张')]:
    mm=cohorts[c];lines.append('|'+label+'|'+'|'.join(f'{mm[k]["exact"]}/{mm[k]["total"]}' for k in list(paths)+['V4_selected'])+'|')
lines+=['|综合35张|'+'|'.join(f'{metrics(rr)["exact"]}/35' for rr in list(fixed.values())+[selected])+'|','',
        '固定阈值均为conf=.35/NMS=.45；验证选择的阈值单独列出。图表只比较本次统一的35张，不能直接与上一轮21张准确率比较。',
        '综合MAE：'+', '.join(f'{k}={metrics(rr)["mae"]:.3f}' for k,rr in dict(fixed,V4_selected=selected).items())+'。','',
        '## V4验证阈值下的错误','']
lines += [f'- {r["cohort"]} #{r["index"]}：真实{r["actual"]}，预测{r["predicted"]}。' for r in selected if r['actual']!=r['predicted']] or ['无计数错误；仍需逐框及独立实测。']
lines+=['','检测验证指标：'+str(report['detection_val'])+'。检测mAP不等于人数完全正确率。','',
        '## 产物与边界','',
        '候选权重：figurine_newbg_v4_20261127.pt；完整评估：evaluation.json；训练曲线：results.png；人数对比：count-comparison.png；案例：v3-v4-new-cases.jpg。',
        'SHA256：'+report['sha256']['V4'],'',
        '目前数据已含20人，但同日静态图片不能证明跨日或连续视频稳定识别。后续须独立采集0～20人测试视频及不同摆法，量化后重新核对板端输出和稳定性。',
        '本轮未git：本机集成工作区不是Git仓库。']
(out/'训练结果.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(root/'README.md').write_text('# 1127 19/20人补拍 V4\n\n新增各19张，合并161训练/22验证/35测试。结果见selected/训练结果.md。prepare.py核对标签与拆分，train.py训练，evaluate.py只按验证集选择阈值并比较统一留出集。未操作板端。\n',encoding='utf-8')
print('FIXED',report['fixed'],flush=True)
print('SELECTED',report['selected'],'SETTINGS',best,flush=True)
print('COHORTS',cohorts,flush=True)
assert hashlib.sha256((out/'figurine_newbg_v4_20261127.pt').read_bytes()).hexdigest()==report['sha256']['V4']
print('EVALUATION COMPLETE; artifact SHA verified',flush=True)

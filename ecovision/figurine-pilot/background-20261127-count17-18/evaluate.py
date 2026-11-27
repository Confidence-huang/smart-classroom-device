"""Validation-only threshold selection and frozen V1/V2/V3 regression comparison."""
from pathlib import Path
import json, hashlib, shutil, csv
from PIL import Image, ImageDraw, ImageFont
from ultralytics import YOLO

root=Path(__file__).resolve().parent
rows=json.loads((root/'annotations.json').read_text())
out=root/'selected';out.mkdir(exist_ok=True)
paths={
 'V1':root.parent/'background-20261127/selected/figurine_newbg_20261127.pt',
 'V2':root.parent/'background-20261127-extra20/selected/figurine_newbg_v2_20261127.pt',
 'V3':root/'runs/count17_18_v3/weights/best.pt',
}
models={k:YOLO(str(p)) for k,p in paths.items()}

def score(model,split,conf,iou):
    rr=sorted((r for r in rows if r['split']==split),key=lambda r:r['name'])
    preds=model.predict([str(root/'images'/split/r['name']) for r in rr],imgsz=320,conf=conf,iou=iou,max_det=60,device=0,verbose=False)
    result=[]
    for r,p in zip(rr,preds):
        boxes=[dict(xyxy=[round(float(x),2) for x in b.xyxy[0]],conf=round(float(b.conf[0]),4)) for b in p.boxes]
        result.append(dict(index=r['index'],cohort=r['cohort'],name=r['name'],actual=r['count'],predicted=len(boxes),boxes=boxes))
    return result

def metrics(rr):
    errors=[abs(r['actual']-r['predicted']) for r in rr]
    return dict(total=len(rr),exact=sum(e==0 for e in errors),exact_rate=sum(e==0 for e in errors)/len(rr),mae=sum(errors)/len(rr),max_error=max(errors),within_one=sum(e<=1 for e in errors)/len(rr))

grid=[]
for conf in (.25,.35,.45,.55):
    for iou in (.3,.45,.6,.75):
        grid.append(dict(conf=conf,iou=iou,**metrics(score(models['V3'],'val',conf,iou))))
best=min(grid,key=lambda r:(r['mae'],-r['exact'],abs(r['conf']-.35),abs(r['iou']-.45)))
fixed={k:score(m,'test',.35,.45) for k,m in models.items()}
selected=score(models['V3'],'test',best['conf'],best['iou'])
cohorts={c:{k:metrics([r for r in rr if r['cohort']==c]) for k,rr in dict(fixed,V3_selected=selected).items()} for c in ('previous','new17','new18')}
report=dict(chosen_on_val=best,validation_grid=grid,
            fixed_settings=dict(conf=.35,nms=.45),fixed={k:metrics(v) for k,v in fixed.items()},
            selected=metrics(selected),cohorts=cohorts,details_fixed=fixed,details_selected=selected,
            sha256={k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in paths.items()},
            limitations='Same-day same-camera layout holdout. Prior15 already inspected: regression only. New6 cover just two nearby layout groups. Assistant-reviewed model-assisted boxes, not independent annotation. No19/20 images or K230 quantized acceptance.')
val=models['V3'].val(data=str(root/'data.yaml'),split='val',imgsz=320,batch=8,workers=0,device=0,plots=True,project=str(root/'evaluation'),name='validation',verbose=False)
report['detection_val']={k:float(v) for k,v in val.results_dict.items()}
(out/'evaluation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
shutil.copy2(paths['V3'],out/'figurine_newbg_v3_20261127.pt')
for name in ('results.png','results.csv','args.yaml'):
    shutil.copy2(root/'runs/count17_18_v3'/name,out/name)
(out/'settings.json').write_text(json.dumps(dict(imgsz=320,conf=best['conf'],nms=best['iou'],max_det=60,target_max_count=20,observed_max_count=18,status='float candidate; K230 conversion and independent field acceptance pending'),indent=2),encoding='utf-8')

def picture(r,label):
    im=Image.open(root/'images/test'/r['name']).convert('RGB');d=ImageDraw.Draw(im)
    for b in r['boxes']:d.rectangle(b['xyxy'],outline='lime',width=2)
    pic=Image.new('RGB',(640,510),'#eee');pic.paste(im,(0,30))
    ImageDraw.Draw(pic).text((8,8),f'{label} | {r["cohort"]} #{r["index"]}: GT {r["actual"]}, predicted {r["predicted"]}',fill='black')
    return pic

for r in selected:picture(r,'V3 selected').save(out/f'test-{r["cohort"]}-{r["index"]}.jpg',quality=94)
cases=[i for i,r in enumerate(fixed['V1']) if r['cohort']!='previous']
canvas=Image.new('RGB',(1280,len(cases)*510),'white')
for j,i in enumerate(cases):
    canvas.paste(picture(fixed['V1'][i],'V1 fixed .35/.45'),(0,j*510))
    canvas.paste(picture(fixed['V3'][i],'V3 fixed .35/.45'),(640,j*510))
canvas.save(out/'v1-v3-new-cases.jpg',quality=95)
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
fig,ax=plt.subplots(figsize=(9,4.8))
values=[report['fixed'][k]['exact_rate']*100 for k in ('V1','V2','V3')]+[report['selected']['exact_rate']*100]
bars=ax.bar(['V1 fixed','V2 fixed','V3 fixed','V3 val-selected'],values,color=['#8b9ba8','#b7c4ce','#198c86','#e5a33d'])
for b,v in zip(bars,values):ax.text(b.get_x()+b.get_width()/2,v+1,f'{v:.1f}%',ha='center')
ax.set_ylim(0,112);ax.set_ylabel('Exact count accuracy (%)');ax.set_title('Frozen 21-image count comparison (same-day layout holdout)')
ax.spines[['top','right']].set_visible(False);fig.tight_layout();fig.savefig(out/'count-comparison.png',dpi=160);plt.close(fig)

epochs=len(list(csv.DictReader((out/'results.csv').open(encoding='utf-8-sig'))))
lines=['# 17/18人补拍 V3 训练结果','',
       '本轮只完成PC浮点训练和评估，未转换或替换K230模型。','',
       '## 数据与标注','',
       '新增35张640×480图片：19张18人、16张17人。用户确认17人文件夹第一张仍为18人。候选框逐图视觉核对，补后排浅蓝衣遮挡个体、浅灰漏检并删除重复框；标注为模型辅助后人工视觉复核，未经独立标注员复核。原图及既有版本保留。',
       '新增24训练/5验证/6测试。相近摆法按组放置；新增6测试仅两组近邻布局，并非6个独立场景。合并后142独立训练/17验证/21测试。新增训练图2倍采样，每轮166次读取；所有集合图像SHA无交集。旧15张仅作回归，且来自同日同背景；不代表跨日真实系统准确率。','',
       '## 训练与阈值','',
       f'从V1继续微调，RTX5070 Laptop GPU，YOLO单类别figurine，320×320、batch8、AdamW lr0=.0002、AMP关闭、mosaic0、seed2718，最多80轮，patience22，实际{epochs}轮。best.pt按验证检测指标保存；计数阈值仅在17张验证图选择。',
       f'选中conf={best["conf"]}、NMS={best["iou"]}；验证人数完全正确{best["exact"]}/{best["total"]}，MAE={best["mae"]:.3f}。','',
       '## 人数完全正确结果','',
       '| 集合 | V1固定阈值 | V2固定阈值 | V3固定阈值 | V3验证选阈值 |',
       '|---|---:|---:|---:|---:|']
for c,label in [('previous','旧15张回归'),('new17','新17人3张'),('new18','新18人3张')]:
    mm=cohorts[c]
    lines.append('|'+label+'|'+'|'.join(f'{mm[k]["exact"]}/{mm[k]["total"]}' for k in ('V1','V2','V3','V3_selected'))+'|')
lines+=['|综合21张|'+'|'.join(f'{metrics(rr)["exact"]}/21' for rr in list(fixed.values())+[selected])+'|','',
        '固定阈值均为conf=.35/NMS=.45；验证选择的阈值单独列出，不混称为同阈值模型改善。',
        '综合MAE（每张人数绝对误差平均值）：'+', '.join(f'{k}={metrics(rr)["mae"]:.3f}' for k,rr in dict(fixed,V3_selected=selected).items())+'。','',
        '## V3验证阈值下的错误','']
lines += [f'- {r["cohort"]} #{r["index"]}：真实{r["actual"]}，预测{r["predicted"]}。' for r in selected if r['actual']!=r['predicted']] or ['无计数错误；仍需核对框及独立实测。']
lines+=['','检测验证指标：'+str(report['detection_val'])+'。检测mAP不等于人数完全正确率。','',
        '## 产物与边界','',
        '候选权重：figurine_newbg_v3_20261127.pt；完整评估：evaluation.json；训练曲线：results.png；人数对比：count-comparison.png；新增案例：v1-v3-new-cases.jpg。',
        'SHA256：'+report['sha256']['V3'],'',
        '目前采集/标注最多18人。19、20人尚未验证；遮挡与新摆法须独立补拍。量化后需重新核对板端输出和稳定性，不以PC结果替代K230验收。',
        '本轮未git：本机集成工作区不是Git仓库。']
(out/'训练结果.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(root/'README.md').write_text('# 1127 17/18人补拍 V3\n\n用户核实17人文件夹第一张为18人。新增19张18人、16张17人。\n\n结果见selected/训练结果.md。prepare.py核对标签与独立拆分，train.py训练，evaluate.py只按验证集选择阈值并比较同一留出集。未操作板端。\n',encoding='utf-8')
print('FIXED',report['fixed'],flush=True)
print('SELECTED',report['selected'],'SETTINGS',best,flush=True)
print('COHORTS',cohorts,flush=True)
assert hashlib.sha256((out/'figurine_newbg_v3_20261127.pt').read_bytes()).hexdigest()==report['sha256']['V3']
print('EVALUATION COMPLETE; artifact SHA verified',flush=True)

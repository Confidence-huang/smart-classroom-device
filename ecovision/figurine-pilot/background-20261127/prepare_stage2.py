"""Accept reviewed assisted labels, exclude ambiguous/held-out neighbour layouts."""
from pathlib import Path
import json, shutil, hashlib
import numpy as np
root=Path(__file__).resolve().parent
manual=json.loads((root/'annotations_reviewed.json').read_text())
for r in manual:
    if r['index']==149:
        r['boxes'].append([10,270,61,360]) # visible light-grey figure at left edge
        r['count']=len(r['boxes'])
pred=json.loads((root/'round2_predictions.json').read_text())
manifest=json.loads((root/'manifest.json').read_text())
lookup={r['index']:r for r in manual}
approved=set(range(1,76))-{18,31,43,74}
approved.update([76,78,79,80,82,83,84,86,87,90,91,92,94,99,100,102,103,104,110,111,112,114,115,116,123,124,126,127,128,130,131,132,134,135,136,138,139,140,150,152,155,156,158,159,160,162,163,166,167,168,170,171,174,175,176,182,183,184,186,187,188,190,191,192,194,195,196,198,199,200,202,203,207])
def similarity(a,b):
    if len(a)!=len(b) or not a:return 0.
    a=np.array(a,dtype=float);b=np.array(b,dtype=float)
    lo=np.maximum(a[:,None,:2],b[None,:,:2]);hi=np.minimum(a[:,None,2:],b[None,:,2:])
    inter=np.maximum(hi-lo,0).prod(2)
    area_a=(a[:,2:]-a[:,:2]).prod(1);area_b=(b[:,2:]-b[:,:2]).prod(1)
    iou=inter/(area_a[:,None]+area_b[None,:]-inter+1e-6)
    used_a=set();used_b=set();values=[]
    for flat in np.argsort(-iou,axis=None):
        x,y=np.unravel_index(int(flat),iou.shape)
        if x not in used_a and y not in used_b:
            used_a.add(x);used_b.add(y);values.append(iou[x,y])
    return float(np.mean(values))

rows=list(manual); exclusions=[]
held=[r for r in manual if r['split']!='train']
for idx in range(1,len(manifest)+1):
    if idx in lookup:continue
    if idx not in approved:
        exclusions.append(dict(index=idx,reason='ambiguous or not accepted in visual audit'));continue
    anchor=min(manual,key=lambda r:abs(r['index']-idx))
    if anchor['split']!='train':
        exclusions.append(dict(index=idx,reason='temporal neighbour of held-out group',anchor=anchor['index']));continue
    boxes=[] if idx<=17 else [b['xyxy'] for b in pred[idx-1]['boxes']]
    if idx in (42,54):boxes=[b for b in boxes if b[1]>100] # remove blue-wire false positives
    near=[r['index'] for r in held if similarity(boxes,r['boxes'])>.62]
    if near:
        exclusions.append(dict(index=idx,reason='near-identical held-out arrangement',anchors=near));continue
    rows.append(dict(index=idx,name=manifest[idx-1]['name'],boxes=boxes,count=len(boxes),split='train',group=anchor['group'],
                     annotation='phase1 model-assisted, visually accepted overlay; not independent expert labels',sha256=manifest[idx-1]['sha256']))

out=root/'stage2';out.mkdir(exist_ok=True)
for row in rows:
    split=row['split']
    for sub in ('images','labels'):(out/sub/split).mkdir(parents=True,exist_ok=True)
    p=root/'raw'/row['name'];shutil.copy2(p,out/'images'/split/p.name)
    lines=[f'0 {(a+c)/1280:.6f} {(b+d)/960:.6f} {(c-a)/640:.6f} {(d-b)/480:.6f}' for a,b,c,d in row['boxes']]
    (out/'labels'/split/(p.stem+'.txt')).write_text('\n'.join(lines)+ ('\n' if lines else ''),encoding='utf-8')
for r in json.loads((root/'replay_manifest.json').read_text()):
    p=Path(r['source']);name=r['name']
    shutil.copy2(p,out/'images/train'/name)
    shutil.copy2(root/'labels/train'/(Path(name).stem+'.txt'),out/'labels/train'/(Path(name).stem+'.txt'))
(out/'annotations.json').write_text(json.dumps(sorted(rows,key=lambda r:r['index']),indent=2),encoding='utf-8')
(out/'excluded.json').write_text(json.dumps(exclusions,indent=2),encoding='utf-8')
(out/'data.yaml').write_text(f'path: "{out.as_posix()}"\ntrain: images/train\nval: images/val\ntest: images/test\nnames:\n  0: figurine\n',encoding='utf-8')
# Structural data checks; labels and image hashes cannot cross subsets.
sets={s:{hashlib.sha256(p.read_bytes()).hexdigest() for p in (out/'images'/s).iterdir()} for s in ('train','val','test')}
assert not sets['train']&sets['val'] and not sets['train']&sets['test'] and not sets['val']&sets['test']
for s in sets:
    for p in (out/'images'/s).iterdir():
        for line in (out/'labels'/s/(p.stem+'.txt')).read_text().splitlines():
            c,x,y,w,h=map(float,line.split());assert c==0 and 0<x<1 and 0<y<1 and 0<w<=1 and 0<h<=1
            assert x-w/2>=-1e-5 and x+w/2<=1.00001 and y-h/2>=-1e-5 and y+h/2<=1.00001
print('Accepted new images',len(rows),'new split',{s:sum(r['split']==s for r in rows) for s in sets},'excluded',len(exclusions),'old replay',12)
print('Total images including replay:',{s:len(list((out/'images'/s).iterdir())) for s in sets})

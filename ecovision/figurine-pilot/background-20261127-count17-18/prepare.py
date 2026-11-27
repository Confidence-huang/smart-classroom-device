"""Reviewed 17/18-person capture merge; preserves all previous splits."""
from pathlib import Path
import json, hashlib, shutil
from collections import Counter
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parent
prior = root.parent / 'background-20261127-extra20'
rows = json.loads((root/'proposals.json').read_text())
# Coordinates reviewed against raw images; proposal indices are zero based.
cyan = [249, 194, 278, 267]
adds = {i: [cyan.copy()] for i in list(range(1,19))+list(range(24,36))}
adds[8].append([295,234,335,306])
adds[9].append([295,234,333,312])
adds[10].append([295,234,333,312])
adds[33].append([337,340,368,412])
adds[34].append([337,340,368,412])
remove = {9: {15}, 15: {16}}
new = []
for p in rows:
    i = p['index']
    count = 18 if i <= 19 else 17
    boxes = [b['xyxy'] for j,b in enumerate(p['boxes']) if j not in remove.get(i,set())]
    boxes += adds.get(i,[])
    assert len(boxes) == count, (i,len(boxes),count)
    for a,b,c,d in boxes:
        assert 0 <= a < c <= 640 and 0 <= b < d <= 480
    split = 'test' if i in (16,17,18,33,34,35) else 'val' if i in (8,9,30,31,32) else 'train'
    group = '18_holdout_16_18' if i in (16,17,18) else '17_holdout_33_35' if i in (33,34,35) else '18_val_8_9' if i in (8,9) else '17_val_30_32' if i in (30,31,32) else 'training_layouts'
    new.append(dict(index=i,name=p['name'],source=p['source'],sha256=p['sha256'],count=count,boxes=boxes,
                    split=split,group=group,cohort=f'new{count}',
                    annotation='model-assisted boxes visually corrected; folder counts supplied by user; first image in 17-folder confirmed 18 by user'))

merged = json.loads((prior/'annotations.json').read_text())
for r in merged:
    r['cohort'] = 'previous'
for s in ('train','val','test'):
    for sub in ('images','labels'):
        (root/sub/s).mkdir(parents=True,exist_ok=True)
        for p in (prior/sub/s).iterdir():
            if p.suffix.lower() in ('.png','.jpg','.jpeg','.txt'):
                shutil.copy2(p,root/sub/s/p.name)
for r in new:
    s=r['split'];p=root/'raw'/r['name']
    assert hashlib.sha256(p.read_bytes()).hexdigest()==r['sha256']
    shutil.copy2(p,root/'images'/s/p.name)
    lines=[f'0 {(a+c)/1280:.6f} {(b+d)/960:.6f} {(c-a)/640:.6f} {(d-b)/480:.6f}' for a,b,c,d in r['boxes']]
    (root/'labels'/s/(p.stem+'.txt')).write_text('\n'.join(lines)+'\n',encoding='utf-8')
merged+=new
paths=[str(p.resolve()) for p in sorted((root/'images/train').glob('*'))]
paths += [str((root/'images/train'/r['name']).resolve()) for r in new if r['split']=='train']
(root/'train.txt').write_text('\n'.join(paths)+'\n',encoding='utf-8')
(root/'data.yaml').write_text(f'path: "{root.as_posix()}"\ntrain: train.txt\nval: images/val\ntest: images/test\nnames:\n  0: figurine\n',encoding='utf-8')
(root/'annotations.json').write_text(json.dumps(merged,indent=2),encoding='utf-8')
(root/'new_annotations.json').write_text(json.dumps(new,indent=2),encoding='utf-8')
(root/'manifest.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
sets={s:{hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'images'/s).glob('*')} for s in ('train','val','test')}
assert not sets['train']&sets['val'] and not sets['train']&sets['test'] and not sets['val']&sets['test']
for r in merged:
    p=root/'labels'/r['split']/(Path(r['name']).stem+'.txt')
    assert len(p.read_text().splitlines())==r['count'], r['name']
for start in range(0,len(new),4):
    canvas=Image.new('RGB',(1280,1020),'white')
    for k,r in enumerate(new[start:start+4]):
        im=Image.open(root/'raw'/r['name']).convert('RGB');d=ImageDraw.Draw(im)
        for j,box in enumerate(r['boxes']):
            d.rectangle(box,outline='lime',width=2);d.text((box[0],box[1]-10),str(j+1),fill='red',stroke_width=1,stroke_fill='white')
        pic=Image.new('RGB',(640,510),'white');pic.paste(im,(0,30));ImageDraw.Draw(pic).text((5,7),f'#{r["index"]}: {r["count"]} people | {r["split"]}',fill='black')
        canvas.paste(pic,((k%2)*640,(k//2)*510))
    canvas.save(root/'review'/f'corrected-{start//4+1:02d}.jpg',quality=95)
print('New',dict(Counter((r['count'],r['split']) for r in new)))
print('Unique split sizes',{s:len(v) for s,v in sets.items()},'training exposures',len(paths))
print('Integrity checks passed: counts, geometry, source hashes, cross-split SHA separation')

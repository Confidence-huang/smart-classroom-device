"""Reviewed 19/20-person capture merge; preserves all previous splits."""
from pathlib import Path
import json, hashlib, shutil
from collections import Counter
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parent
prior = root.parent / 'background-20261127-count17-18'
rows = json.loads((root/'proposals.json').read_text())
# Coordinates reviewed against raw images; proposal indices are zero based.
adds = {}
remove = {4:{19},20:{20},21:{20},23:{20,21},25:{18},26:{20},27:{20,21},28:{20},29:{20,21},30:{20},31:{19,20,21,22},32:{20,21,22},33:{20,21,22,23},34:{20},35:{20},36:{19,20,21},37:{19,21}}
for i in range(8,20):
    n = len(rows[i-1]['boxes'])
    remove[i] = set(range(n-(n-18),n))
    # All rightmost low-confidence alternatives represent the same yellow-shirt figurine.
    adds[i] = [[573,304,608,397]]
adds[31] = [[571,308,607,400]]
adds[36] = [[570,307,607,398]]
new = []
for p in rows:
    i = p['index']
    count = 19 if i <= 19 else 20
    boxes = [b['xyxy'] for j,b in enumerate(p['boxes']) if j not in remove.get(i,set())]
    boxes += adds.get(i,[])
    assert len(boxes) == count, (i,len(boxes),count)
    for a,b,c,d in boxes:
        assert 0 <= a < c <= 640 and 0 <= b < d <= 480
    # Keep capture groups including the 19-to-20 transition together.
    split = 'test' if i in tuple(range(14,22))+tuple(range(31,37)) else 'val' if i in (8,9,10,29,30) else 'train'
    group = 'transition_19_20_holdout' if i in range(14,22) else '20_layout_holdout' if i in range(31,37) else '19_validation' if i in (8,9,10) else '20_validation' if i in (29,30) else 'training_layouts'
    new.append(dict(index=i,name=p['name'],source=p['source'],sha256=p['sha256'],count=count,boxes=boxes,
                    split=split,group=group,cohort=f'new{count}',
                    annotation='model-assisted boxes visually corrected; folder counts supplied by user; folder19 and folder20 physical counts supplied by user; duplicated partial boxes removed after visual review'))

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

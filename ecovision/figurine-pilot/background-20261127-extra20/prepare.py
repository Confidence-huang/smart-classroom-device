"""Merge verified 16-person extra captures without changing the prior dataset."""
from pathlib import Path
import json,shutil,hashlib
root=Path(__file__).resolve().parent
previous=root.parent/'background-20261127'
proposals=json.loads((root/'proposals.json').read_text())
replacements={
 1:{12:[244,154,278,215],13:[253,201,283,270]},
 2:{11:[245,158,279,220],16:[254,204,286,271]},
 3:{14:[244,157,280,221],13:[253,202,285,272]},
 4:{7:[209,202,250,282],12:[244,157,280,221]},
 5:{12:[244,157,280,221]},6:{11:[244,158,280,221]},
 7:{13:[244,158,280,222]},8:{13:[244,158,281,223]},
 9:{13:[244,158,281,223]},
10:{13:[244,158,280,224],15:[254,203,286,272]},
11:{12:[244,158,280,224]},
12:{11:[244,158,280,225],13:[253,204,286,275]},
13:{14:[244,158,280,225],15:[253,204,286,275]},
14:{13:[244,158,280,225],15:[253,204,286,275]},
}
adds={1:[[0,302,62,390]],4:[[192,192,216,250],[253,202,285,273]],
      5:[[253,202,285,273]],6:[[253,203,285,274]],7:[[253,203,285,274]],8:[[253,203,286,275]],
      9:[[253,203,286,275]],11:[[253,204,286,275]]}
remove={1:{15},2:{15},14:{16}}
extra=[]
for row in proposals:
    idx=row['index']; boxes=[replacements.get(idx,{}).get(i,b['xyxy']) for i,b in enumerate(row['boxes']) if i not in remove.get(idx,set())]
    boxes+=adds.get(idx,[])
    assert len(boxes)==16,(idx,len(boxes))
    split='val' if idx==7 else 'test' if idx in (12,13,14) else 'excluded' if idx==8 else 'train'
    extra.append(dict(index=idx,name=row['name'],boxes=boxes,count=16,split=split,
        group='extra_layout_7_8' if idx in (7,8) else f'extra_layout_{idx}',
        annotation='user corrected physical count to 16; assistant visually corrected individual boxes',
        sha256=hashlib.sha256((root/'raw'/row['name']).read_bytes()).hexdigest()))

old_rows=json.loads((previous/'stage2/annotations.json').read_text())
merged=[]
for s in ('train','val','test'):
    for sub in ('images','labels'):(root/sub/s).mkdir(parents=True,exist_ok=True)
    for p in (previous/'stage2/images'/s).iterdir():
        shutil.copy2(p,root/'images'/s/p.name)
        shutil.copy2(previous/'stage2/labels'/s/(p.stem+'.txt'),root/'labels'/s/(p.stem+'.txt'))
for row in old_rows:
    merged.append(dict(row,cohort='previous'))
for row in extra:
    split=row['split']
    if split=='excluded':continue
    p=root/'raw'/row['name'];shutil.copy2(p,root/'images'/split/p.name)
    lines=[f'0 {(a+c)/1280:.6f} {(b+d)/960:.6f} {(c-a)/640:.6f} {(d-b)/480:.6f}' for a,b,c,d in row['boxes']]
    (root/'labels'/split/(p.stem+'.txt')).write_text('\n'.join(lines)+'\n',encoding='utf-8')
    merged.append(dict(row,cohort='extra'))
train_paths=[str(p.resolve()) for p in sorted((root/'images/train').iterdir())]
new_train=[str((root/'images/train'/r['name']).resolve()) for r in extra if r['split']=='train']
train_paths+=new_train*2 # 3x exposure for the 9 new layouts; not 27 independent images.
(root/'train.txt').write_text('\n'.join(train_paths)+'\n',encoding='utf-8')
(root/'data.yaml').write_text(f'path: "{root.as_posix()}"\ntrain: train.txt\nval: images/val\ntest: images/test\nnames:\n  0: figurine\n',encoding='utf-8')
(root/'annotations.json').write_text(json.dumps(merged,indent=2),encoding='utf-8')
(root/'extra_annotations.json').write_text(json.dumps(extra,indent=2),encoding='utf-8')
sets={s:{hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'images'/s).iterdir()} for s in ('train','val','test')}
assert not sets['train']&sets['val'] and not sets['train']&sets['test'] and not sets['val']&sets['test']
print('Unique images by split:',{s:len(v) for s,v in sets.items()},'training exposures',len(train_paths))
print('Extras:',{s:sum(r['split']==s for r in extra) for s in ('train','val','test','excluded')})

"""Freeze a visually reviewed subset; raw predictions are never ground truth."""
import json, shutil, hashlib
from pathlib import Path
from PIL import Image, ImageDraw
root = Path(__file__).resolve().parent
rows = json.loads((root / 'curated_proposals.json').read_text())
# Rectangles corrected from full-resolution images / indexed review sheets.
# Keys of replace/remove refer to ORIGINAL proposal IDs.
edits = {
61: {'add': [[146,224,179,310]]},
73: {'replace': {1:[298,206,332,286]}, 'add':[[270,248,308,313]]},
77: {'replace': {2:[229,220,267,323]}},
81: {'replace': {1:[229,217,266,320]}},
85: {'replace': {4:[171,177,205,267]}, 'add':[[196,171,231,252]]},
89: {'add': [[138,219,174,311]]},
93: {'replace': {3:[315,181,350,266]}, 'add':[[299,204,327,280]]},
101:{'remove':[7]},
113:{'add':[[245,243,278,339]]},
117:{'replace': {3:[246,179,279,269]}},
121:{'replace': {4:[247,175,279,264]}},
125:{'remove':[3], 'add':[[294,246,327,341]]},
129:{'add':[[251,49,281,119],[371,203,410,297]]},
133:{'remove':[2], 'replace':{3:[211,118,238,204]},'add':[[454,226,485,327]]},
137:{'remove':[4], 'replace':{3:[192,184,221,270],11:[439,277,477,391]}},
141:{'add':[[561,310,597,411]]},
145:{'remove':[1], 'add':[[570,276,608,385]]},
149:{'replace':{9:[359,266,395,381]}},
153:{'replace':{11:[425,146,458,226]},'add':[[408,206,445,291]]},
157:{'replace':{4:[235,268,270,337]},'add':[[235,321,274,419],[289,244,326,341]]},
161:{'replace':{4:[236,258,271,334],7:[323,324,366,430]},'add':[[231,316,272,406]]},
169:{'replace':{3:[212,200,245,268]},'add':[[208,239,246,315]]},
173:{'replace':{0:[125,280,157,381]},'add':[[143,264,174,340],[378,193,411,270],[394,262,430,354]]},
177:{'replace':{3:[195,220,232,312],4:[222,196,253,274]},'add':[[353,211,386,290],[382,193,414,269]]},
181:{'replace':{1:[133,209,162,279],6:[349,307,383,410]},'add':[[138,258,176,350],[382,193,414,269]]},
185:{'replace':{0:[133,209,162,279]},'add':[[137,258,177,350],[382,193,414,269]]},
189:{'remove':[3], 'replace':{1:[128,281,168,393]},'add':[[382,193,414,269]]},
197:{'replace':{3:[134,210,171,280]},'add':[[128,256,171,338]]},
201:{'replace':{1:[133,211,172,280]},'add':[[129,256,171,340]]},
205:{'replace':{3:[141,215,177,282],6:[232,266,274,347]},'add':[[144,261,183,354],[231,334,278,425],[358,216,387,293],[383,198,416,279]]},
208:{'replace':{7:[254,176,286,239]},'add':[[262,205,289,282]]},
}
# Ambiguous adjacent figures in 105 are not accepted as training ground truth.
rows = [row for row in rows if row['index'] != 105]
for row in rows:
    edit = edits.get(row['index'], {})
    row['boxes'] = [edit.get('replace', {}).get(i, b) for i, b in enumerate(row['boxes']) if i not in edit.get('remove', [])]
    row['boxes'] += edit.get('add', [])
    row['annotation'] = 'assistant visual review, proposal-assisted, corrected visible-person rectangles'
    for b in row['boxes']:
        assert 0 <= b[0] < b[2] <= 640 and 0 <= b[1] < b[3] <= 480

# Held-out layouts selected before training. Group names and fixed subsets are
# retained so later frames of the same scene can be kept out of training.
val_ids = {11,29,45,61,81,97,117,137,157,177,197}
test_ids = {15,33,49,65,85,101,121,141,161,181,201,208}
for row in rows:
    idx = row['index']
    row['split'] = 'test' if idx in test_ids else 'val' if idx in val_ids else 'train'
    row['group'] = f'new_layout_{idx:03d}'
    part = row['split']
    for sub in ('images','labels'): (root/sub/part).mkdir(parents=True,exist_ok=True)
    source = root/'raw'/row['name']; target = root/'images'/part/row['name']
    shutil.copy2(source,target)
    lines = [f'0 {(a+c)/1280:.6f} {(b+d)/960:.6f} {(c-a)/640:.6f} {(d-b)/480:.6f}' for a,b,c,d in row['boxes']]
    (root/'labels'/part/(source.stem+'.txt')).write_text('\n'.join(lines)+ ('\n' if lines else ''),encoding='utf-8')
    row['count'] = len(row['boxes']);row['sha256'] = hashlib.sha256(source.read_bytes()).hexdigest()

(root/'annotations_reviewed.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
(root/'data.yaml').write_text(f'path: "{root.as_posix()}"\ntrain: images/train\nval: images/val\ntest: images/test\nnames:\n  0: figurine\n',encoding='utf-8')
for start in range(0,len(rows),6):
    canvas=Image.new('RGB',(1280,1512),'#eee'); draw=ImageDraw.Draw(canvas)
    for j,row in enumerate(rows[start:start+6]):
        im=Image.open(root/'raw'/row['name']).convert('RGB'); d=ImageDraw.Draw(im)
        for k,b in enumerate(row['boxes']): d.rectangle(b,outline='lime',width=2);d.text((b[0],b[1]-10),str(k),fill='red',stroke_width=1,stroke_fill='white')
        x=(j%2)*640;y=(j//2)*504;canvas.paste(im,(x,y+24));draw.text((x+4,y+4),f'{row["index"]:03d} {row["split"]} GT {len(row["boxes"])}',fill='black')
    canvas.save(root/'review'/f'checked-{start//6+1:02d}.jpg',quality=95)
print('New reviewed:',len(rows),'splits:',{s:sum(r['split']==s for r in rows) for s in ('train','val','test')},'counts:',sorted(set(r['count'] for r in rows)))

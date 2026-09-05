"""Independent cabinet-interior and original-viewport reference checks.

The ordinary C++ test also checks all analytic interior samples, independently
of the cached Whole geometry getter. This script checks the rendered Lab route.
"""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image

parser=argparse.ArgumentParser()
parser.add_argument('run',type=Path)
args=parser.parse_args()
rows=json.loads((args.run/'samples.json').read_text(encoding='utf-8'))
by_name={r['label']:r for r in rows}
checks=[]
def check(label, passed, **data):
    checks.append(dict(check=label,passed=bool(passed),**data))

first=by_name['stage1_far_partial_live']['objects']['OcclusionWhole']
near=by_name['stage3_near_full_live']['objects']['OcclusionWhole']
check('far view is confirmed and physically partial',first['confirmed'] and 0<first['coverage']<.95)
check('near view is fully legal',near['coverage']>.99)
expected=by_name['reference_direct_full_history']['objects']['OcclusionWhole']
reference=np.asarray(Image.open(args.run/'reference_direct_full_history.png').convert('RGB'),dtype=float)
h,w=reference.shape[:2]
assert abs(w/h-2233/911)<.01, 'Camera viewport aspect changed; review and update the image oracle'
# Fixed, interior front-face area in the unchanged camera. The old wall cut
# crosses this region. A direct-full independent session supplies the image.
region=(slice(int(h*.48),int(h*.54)),slice(int(w*.66),int(w*.68)))
first_record=by_name['stage2_H1']['objects']['OcclusionWhole']['capture']['records'][0]
for name,row in by_name.items():
    if name not in ('stage2_H1','stage4_H2') and not name.startswith(('first_exit','second_exit','repeat')):
        continue
    b=row['objects']['OcclusionWhole']
    if b['current'] or not b['capture']['records']:
        check(name+' sealed without delay',False)
        continue
    records=b['seam']['records']
    interior=[point for r in records for point in r['row'] if -530<point[0]<-370]
    missing=sum(point[3]*point[4]<.999 for point in interior)
    check(name+' analytic interior',len(interior)>100 and missing==0,samples=len(interior),missing=missing)
    record=b['capture']['records'][0]
    check(name+' same resources',record['epoch']==first_record['epoch'] and record['proxy']==first_record['proxy'] and record['texture']==first_record['texture'])
    check(name+' equals direct full texture',record['texture_hash']==expected['capture']['records'][0]['texture_hash'])
    check(name+' actual binding',not record['proxy_hidden'] and all(p['ready']==1 and p['texture']==record['texture'] for p in record['bindings']))
    pixels=np.asarray(Image.open(args.run/(name+'.png')).convert('RGB'),dtype=float)
    error=float(np.abs(pixels[region]-reference[region]).mean())
    check(name+' original image',error<3,mean_absolute_rgb_error=error)
result=dict(passed=all(c['passed'] for c in checks),checks=checks)
(args.run/'reobservation-oracle.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(dict(passed=result['passed'],checks=len(checks),failed=[c for c in checks if not c['passed']]),indent=2))
raise SystemExit(0 if result['passed'] else 1)

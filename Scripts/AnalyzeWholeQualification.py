"""Independent per-frame color continuity plus qualification/binding contracts.

The fixed cabinet face patch is inside unchanged physical geometry, away from
silhouettes and the newly revealed edge. RGB and chroma detect both gray reset
and source disappearance. No runtime 'seam passed' classification is trusted.
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
checks=[]
episodes=[]

def check(name,passed,**evidence):
    checks.append(dict(check=name,passed=bool(passed),**evidence))

def patch(row):
    with Image.open(args.run/(row['label']+'.png')) as image:
        assert image.size==(2233,911), 'Keep the audited fixed camera and original resolution'
        return np.asarray(image.convert('RGB'),dtype=float)[440:490,1128:1172]

for room in (1,3,5):
    original=None
    whole_bindings=None
    local_bindings=None
    for session in range(3):
        prefix=f'room{room}_session{session}'
        seq=[r for r in rows if r['label'].startswith(prefix+'_')]
        assert seq, 'Missing room/session'
        crossing=next(i for i,r in enumerate(seq) if r['reveal']['confirmed'])
        assert '_cross_' in seq[crossing]['label']
        pre=seq[crossing-1]
        baseline=patch(pre)
        channel=1 if room==3 else 2
        baseline_chroma=float(np.mean(baseline[:,:,channel]-baseline[:,:,0]))
        check(prefix+' settled colored surface before threshold',baseline_chroma>30,
              chroma=baseline_chroma)
        check(prefix+' held same surface before confirmation',crossing>=7 and all(
            float(np.mean(patch(r)[:,:,channel]-patch(r)[:,:,0]))>.9*baseline_chroma
            for r in seq[crossing-3:crossing]),frames=3)
        min_ratio=1.0
        max_mae=0.0
        for i,row in enumerate(seq):
            name=row['label']
            state=row['pipeline']['final']
            current=row['current']>0
            confirmed=row['reveal']['confirmed']
            if i:
                # GFrameCounter also counts editor-only frames. The world update
                # counter and fixed game time prove no game transition was lost.
                check(name+' captures consecutive game frames',row['resources']['frame']==seq[i-1]['resources']['frame']+1
                      and abs(row['game_time']-seq[i-1]['game_time']-1/60)<1.e-5
                      and row['pipeline']['frame']>seq[i-1]['pipeline']['frame'])
            check(name+' same configured threshold',row['reveal']['minimum_span_cm']==100 and row['reveal']['effective_span_cm']==100)
            check(name+' bounded records',state['records']<=2 and (not confirmed or state['records']==1))
            check(name+' no Whole cap',row['caps']==0)
            check(name+' exclusive probe ownership',state['historical_draws']+(state['parts'][0]['submitted'][0]>0 and current)<=1)
            if '_exit_' in name:
                check(name+' first loss seals then ends eligibility',not confirmed and row['reveal']['observed_span_cm']==0
                      and not current and row['stale']==1 and state['records']==1)
                record=row['capture']['records'][0]
                check(name+' complete first-frame history',record['whole_capture'] and record['capture_valid']
                      and record['capture_set']>0 and record['zero_aa']==0 and record['empty']==0
                      and not record['proxy_hidden'] and record['texture']!='None')
                identity=[record[k] for k in ('epoch','proxy','texture','capture_set')]
                original=original or identity
                check(name+' original history resources',identity==original)
                continue
            check(name+' valid live source',state['valid'] and current and all(p['ready']==1 and p['visible'] for p in state['parts']))
            bindings=[p['texture'] for p in state['parts']]
            if confirmed:
                whole_bindings=whole_bindings or bindings
                check(name+' one shared complete output',all(p['uniform'] and p['size']==[1,1]
                    and p['whole']==state['whole_state'] for p in state['parts']))
                check(name+' reuse whole binding',bindings==whole_bindings)
                check(name+' inherited color without reset',state['whole_state'][0]>=.999 and state['whole_state'][1]>=.999)
                check(name+' no residual temporary suppression',state['transient']==0)
                if '_small_' not in name:
                    pixels=patch(row)
                    ratio=float(np.mean(pixels[:,:,channel]-pixels[:,:,0]))/baseline_chroma
                    mae=float(np.mean(np.abs(pixels-baseline)))
                    min_ratio=min(min_ratio,ratio);max_mae=max(max_mae,mae)
                    check(name+' actual pixels never return gray or disappear',ratio>=.90 and mae<=12,
                          color_ratio=ratio,rgb_mae=mae)
            else:
                local_bindings=local_bindings or bindings
                check(name+' below threshold stays local',row['reveal']['observed_span_cm']<100 and not any(p['uniform'] for p in state['parts']))
                check(name+' reuse local binding',bindings==local_bindings)
            if session>0:
                check(name+' no repeated texture creation',state['texture_creations']==6)
        episodes.append(dict(session=prefix,frames=len(seq),qualification_frame=seq[crossing]['label'],
                             minimum_color_ratio=min_ratio,maximum_rgb_mae=max_mae))

result=dict(passed=all(c['passed'] for c in checks),checks=len(checks),frames=len(rows),episodes=episodes,
            failures=[c for c in checks if not c['passed']],results=checks)
(args.run/'qualification-oracle.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k!='results'},indent=2))
raise SystemExit(0 if result['passed'] else 1)

"""Summarize every measured transition frame, with no warmup exclusion per phase."""
import argparse
import json
from pathlib import Path
import numpy as np

parser=argparse.ArgumentParser()
parser.add_argument('run',type=Path)
args=parser.parse_args()
rows=json.loads((args.run/'timing.json').read_text(encoding='utf-8'))
stages={r['label']:r for r in json.loads((args.run/'timing-stages.json').read_text(encoding='utf-8'))}
assert stages['stage1_far_partial_live']['confirmed'] and 0<stages['stage1_far_partial_live']['coverage']<.95
assert stages['stage3_near_full_live']['coverage']>.99
assert all(stages[s]['current']==0 and stages[s]['stale']==1 and stages[s]['records']==1 for s in ('stage2_H1','stage4_H2'))
def stats(values):
    a=np.asarray(values,dtype=float)
    return dict(zip(('p50','p95','p99','max'),[float(x) for x in np.percentile(a,[50,95,99,100])]))
phases={}
for name in dict.fromkeys(r['phase'] for r in rows):
    selected=[r for r in rows if r['phase']==name]
    phases[name]=dict(frames=len(selected),
        system_ms=stats([r['frame']['game_thread_us']/1000 for r in selected]),
        wall_ms=stats([r['wall_ms'] for r in selected]),
        game_delta_ms=stats([r['game_delta']*1000 for r in selected]),
        resources={k:[min(r['frame'][k] for r in selected),max(r['frame'][k] for r in selected)]
            for k in ('records','proxies','textures','mids','caps','fine_bytes','working_set')},
        work={k:sum(r['frame'][k] for r in selected)
            for k in ('texture_creations','mid_creations','gpu_texture_uploads','cap_rebuilds')})
repeats=[r for r in rows if r['phase'].startswith('repeat')]
resource_ranges={k:[min(r['frame'][k] for r in repeats),max(r['frame'][k] for r in repeats)]
    for k in ('records','proxies','textures','mids','caps','fine_bytes')}
result=dict(frames=len(rows),phases=phases,repeated_resource_ranges=resource_ranges,
    repeated_allocations={k:sum(r['frame'][k] for r in repeats) for k in ('texture_creations','mid_creations')},
    note='System CPU and complete wall frames differ. Protocol completion is not performance acceptance; includes telemetry overhead.')
(args.run/'timing-summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))

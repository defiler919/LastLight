"""Offline analysis of paired replay and native Apartment runs. No UE state changes."""
import json, math, sys
from pathlib import Path

def stats(values):
    v=sorted(values)
    return dict(mean=sum(v)/len(v),p95=v[math.ceil(.95*len(v))-1],p99=v[math.ceil(.99*len(v))-1],maximum=v[-1])

output={}
poses={}
for name in sys.argv[1:]:
    root=Path(name)
    assert (root/'complete.txt').exists() and not (root/'invalid.txt').exists(),root
    frames=[json.loads(s) for s in (root/'frames.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    frames=[f for f in frames if f['memory']['frame_data']['game_thread_us']>0]
    assert all(f['engine']['foreground']==1 and f['engine']['viewport']==[1280,720] and f['engine']['aa']==4 for f in frames)
    r={'frames':len(frames),'completion':(root/'complete.txt').read_text(encoding='utf-8-sig')}
    r['engine_ms']={k:stats([f['engine'][k] for f in frames]) for k in ('game_ms','render_ms','rhi_ms','gpu_ms')}
    r['wall_ms']=stats([f['wall_ms'] for f in frames])
    r['object_memory']={k:stats([f['memory']['frame_data'][k] for f in frames]) for k in ('game_thread_us','coverage_us','historical_us','current_reveal_us','coverage_queries','coverage_scans','epochs','candidates','texture_uploads','cap_rebuilds','ownership_us')}
    r['static_update_us']=stats([f['static']['update_us'] for f in frames])
    r['historical_over_50ms']=sum(f['memory']['frame_data']['historical_us']>=50000 for f in frames)
    r['end_storage']=(root/'end-storage.txt').read_text(encoding='utf-8-sig')
    if 'step' in frames[0]:poses[root.name]=[(f['step'],f['x'],f['y'],f['source_x'],f['source_y']) for f in frames]
    output[root.name]=r
output['paired_pose_equality']={f'{a} vs {b}':poses[a]==poses[b] for i,a in enumerate(poses) for b in list(poses)[i+1:]}
print(json.dumps(output,indent=2))

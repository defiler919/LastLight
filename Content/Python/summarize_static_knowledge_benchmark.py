"""Offline bounded benchmark reduction; run with UE's bundled Python or Python 3."""
import json, math, sys
from pathlib import Path

def stats(values):
    v=sorted(values)
    return dict(mean=sum(v)/len(v),p95=v[math.ceil(.95*len(v))-1],maximum=v[-1])

result={}
for arg in sys.argv[1:]:
    root=Path(arg)
    assert (root/'complete.txt').exists() and not (root/'invalid.txt').exists(),root
    frames=[json.loads(s) for s in (root/'frames.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    assert all(f['engine']['foreground']==1 and f['engine']['viewport']==[1280,720] for f in frames)
    r={'completion':(root/'complete.txt').read_text(encoding='utf-8-sig'), 'frames':len(frames)}
    r['engine_ms']={k:stats([f['engine'][k] for f in frames]) for k in ('game_ms','render_ms','rhi_ms','gpu_ms','game_wait_ms')}
    r['wall_ms']=stats([f['wall_ms'] for f in frames])
    # First frame resets the OM window before this frame's update; exclude it.
    r['object_memory_us']={k:stats([f['memory']['frame_data'][k] for f in frames[1:]]) for k in ('game_thread_us','tracked_us','coverage_us','current_reveal_us','ownership_us','texture_us','cap_us','historical_us')}
    r['static']={k:stats([f['static'][k] for f in frames]) for k in frames[0]['static']}
    r['final_static']=frames[-1]['static']
    r['end_storage']=(root/'end-storage.txt').read_text(encoding='utf-8-sig')
    r['render_contract']={k:frames[-1]['engine'][k] for k in ('viewport','screen_percentage','aa','dynamic_resolution','vsync','max_fps','fog_extent')}
    r['segments']=sorted(set(f['fog']['segments'] for f in frames))
    result[root.name]=r
print(json.dumps(result,indent=2))

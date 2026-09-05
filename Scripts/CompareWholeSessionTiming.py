"""Same-machine before/after whole reobservation route, including transition frames."""
import argparse
import json
from pathlib import Path
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('before', type=Path)
parser.add_argument('after', type=Path)
args = parser.parse_args()

def stats(values):
    return dict(zip(('p50', 'p95', 'p99', 'max'), map(float, np.percentile(values, [50,95,99,100]))))

def summarize(path):
    rows = json.loads((path/'timing.json').read_text(encoding='utf-8'))
    repeat = [r for r in rows if r['phase'].startswith('repeat')]
    groups = {}
    for phase in ('first_partial_observation', 'first_seal', 'near_full_reobservation', 'second_seal', 'repeated_cycles', 'all_frames'):
        selected = repeat if phase == 'repeated_cycles' else rows if phase == 'all_frames' else [r for r in rows if r['phase'] == phase]
        groups[phase] = dict(frames=len(selected), system_ms=stats([r['frame']['game_thread_us']/1000 for r in selected]),
            wall_ms=stats([r['wall_ms'] for r in selected]), wall_over_16_6=sum(r['wall_ms'] > 16.6 for r in selected),
            wall_over_33=sum(r['wall_ms'] > 33 for r in selected),
            texture_creations=sum(r['frame']['texture_creations'] for r in selected),
            mid_creations=sum(r['frame']['mid_creations'] for r in selected))
    return dict(groups=groups, repeated_resources={k:[min(r['frame'][k] for r in repeat), max(r['frame'][k] for r in repeat)]
        for k in ('records', 'proxies', 'textures', 'mids', 'caps', 'fine_bytes')},
        repeated_working_set=dict(first=repeat[0]['frame']['working_set'], last=repeat[-1]['frame']['working_set'],
            peak=max(r['frame']['working_set'] for r in repeat)),
        process=json.loads((path/'summary.json').read_text(encoding='utf-8-sig')))

result = dict(before=summarize(args.before), after=summarize(args.after),
    note='Serial D3D12/SM6 runs; identical unfixed-time route, quality and telemetry. Includes all transition frames. Single paired run is not a long-soak or full-frame performance acceptance.')
(args.after/'before-after-comparison.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
for phase in result['before']['groups']:
    a, b = (result[key]['groups'][phase] for key in ('before', 'after'))
    print(phase, 'system p95/max', [round(x['system_ms'][s], 3) for x in (a,b) for s in ('p95','max')],
          'wall p95/max', [round(x['wall_ms'][s], 3) for x in (a,b) for s in ('p95','max')])
print(json.dumps({key:{k:result[key][k] for k in ('repeated_resources','repeated_working_set','process')} for key in ('before','after')}, indent=2))

"""Continuous short/requalified Whole sessions, retaining the original gray oracle.

Use RunGrayMemoryAudit.ps1 -Protocol Reobservation -WholeSessions. Run the existing
current/depth and history analyzers on the same evidence as well.
"""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('run', type=Path)
args = parser.parse_args()
rows = json.loads((args.run/'samples.json').read_text(encoding='utf-8'))
stages = {r['label']: r for r in rows}
checks = []

def check(name, passed, **evidence):
    checks.append(dict(check=name, passed=bool(passed), **evidence))

def obj(name):
    return stages[name]['objects']['OcclusionWhole']

initial = obj('stage2_H1')['capture']['records'][0]
reference = np.asarray(Image.open(args.run/'reference_direct_full_history.png').convert('RGB'), dtype=float)
h, w = reference.shape[:2]
assert abs(w/h-2233/911) < .01, 'Review the original viewport oracle if its aspect changes'
region = (slice(int(h*.48), int(h*.54)), slice(int(w*.66), int(w*.68)))
whole_bindings = None
local_bindings = None
warm_creations = None
session_rows = [r for r in rows if r['label'].startswith('session')]
assert len(session_rows) == 48, 'All four continuous session cycles must finish'
for row in session_rows:
    name = row['label']
    b = row['objects']['OcclusionWhole']
    p = b['reveal']
    record = next((r for r in b['capture']['records'] if r['epoch'] == initial['epoch']), None)
    check(name+' fixed policy', p['minimum_span_cm'] == 100 and p['effective_span_cm'] == 100
          and p['reveal_mode'] == 0 and p['history_mode'] == 1)
    check(name+' keeps original legal memory', record is not None and all(
        record[k] == initial[k] for k in ('capture_set', 'remembered', 'zero_aa', 'min_aa', 'empty', 'superseded')))
    check(name+' keeps original historical resources', record is not None and all(
        record[k] == initial[k] for k in ('epoch', 'proxy', 'texture', 'content_revision')))
    bindings = [r['texture'] for r in b['capture']['source_bindings']]
    if name.endswith(('short_current', 'new_short')):
        check(name+' fresh session below threshold', b['valid'] and 0 < b['coverage'] < .95
              and 0 < p['observed_span_cm'] < p['effective_span_cm']
              and not b['confirmed'] and b['current'] == 1 and not b['uniform'] and b['source_visible'],
              span_cm=p['observed_span_cm'], coverage=b['coverage'])
        check(name+' candidate is independent of old memory', b['records'] == 2 and b['stale'] == 1
              and sum(r['whole_capture'] for r in b['capture']['records']) == 1)
        local_bindings = local_bindings or bindings
        check(name+' reuses local source textures', bindings == local_bindings)
    elif name.endswith(('qualified_current', 'qualified_small_contact')):
        check(name+' this session qualifies and displays whole', b['confirmed'] and b['uniform']
              and b['source_visible'] and b['records'] == 1 and b['current'] == 1 and b['stale'] == 0)
        whole_bindings = whole_bindings or bindings
        check(name+' reuses whole source textures', bindings == whole_bindings)
        if name.endswith('small_contact'):
            short = obj(name.split('_')[0]+'_short_current')
            check(name+' same small contact retains earned whole display', abs(b['coverage']-short['coverage']) < 1e-6)
    else:
        check(name+' exit ends only session qualification', not b['confirmed'] and p['observed_span_cm'] == 0
              and b['current'] == 0 and b['stale'] == 1 and b['records'] == 1 and not b['source_visible'])
        check(name+' full history is immediately ready', record is not None and not record['proxy_hidden']
              and record['texture_hash'] == initial['texture_hash'] and all(
                  r['ready'] == 1 and r['texture'] == record['texture'] for r in record['bindings']))
        interior = [pt for r in b['seam']['records'] for pt in r['row'] if -530 < pt[0] < -370]
        check(name+' independent analytic gray interior', len(interior) > 100 and all(pt[3]*pt[4] >= .999 for pt in interior))
        frame = np.asarray(Image.open(args.run/(name+'.png')).convert('RGB'), dtype=float)
        error = float(np.abs(frame[region]-reference[region]).mean())
        check(name+' original gray image matches direct full reference', error < 3, mean_absolute_rgb_error=error)
    if b['current']:
        warm_creations = warm_creations if warm_creations is not None else b['live']['texture_creations']
        check(name+' no source texture churn after warmup', b['live']['texture_creations'] == warm_creations,
              creations=b['live']['texture_creations'])

result = dict(passed=all(c['passed'] for c in checks), checks=checks)
(args.run/'whole-session-oracle.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(dict(passed=result['passed'], checks=len(checks), failed=[c for c in checks if not c['passed']]), indent=2))
raise SystemExit(0 if result['passed'] else 1)

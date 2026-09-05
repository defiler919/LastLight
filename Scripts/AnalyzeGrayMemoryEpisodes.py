"""Independent Room 02 solid-interior oracle for recorded episode diagnostics.

The fixture contains one solid 620x75 cuboid at the origin in X. The full 90deg
view establishes every interior sample; later subset views cannot remove it.
This reads submitted surface values, not the runtime's seam classification.
"""
import argparse
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('evidence', type=Path)
a = p.parse_args()
rows = json.loads((a.evidence/'samples.json').read_text(encoding='utf-8'))
out = []
full_seen = False
for row in rows:
    if row['label'] == 'cycle_4_history':
        full_seen = True
    if not full_seen or not (row['label'].endswith('history') or row['label'].startswith('diagnostic_')):
        continue
    submitted = {}
    for record in row['evidence']['records']:
        for x, state, aa, blue, gate in record['row']:
            submitted[x] = submitted.get(x, 0) + blue * gate
    interior = [(x, value) for x, value in submitted.items() if -280 < x < 280]
    deficits = [(x, value) for x, value in interior if value < .999]
    out.append(dict(label=row['label'], records=len(row['evidence']['records']),
                    interior_samples=len(interior), deficits=deficits))
assert out and all(x['interior_samples'] > 0 for x in out), 'missing product-oracle samples'
report = dict(surface_pass=all(not x['deficits'] for x in out), samples=out)
(a.evidence/'surface_oracle.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
raise SystemExit(0 if report['surface_pass'] else 1)

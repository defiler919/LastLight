"""Confirmed Whole current contract, plus real camera-depth positive control.

Run AnalyzeGrayReobservation.py separately for the complete history oracle.
"""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('run', type=Path)
args = parser.parse_args()
rows = json.loads((args.run / 'samples.json').read_text(encoding='utf-8'))
by_name = {r['label']: r for r in rows}
checks = []

def check(name, passed, **data):
    checks.append(dict(check=name, passed=bool(passed), **data))

def pixels(name):
    return np.asarray(Image.open(args.run / (name + '.png')).convert('RGB'), dtype=float)

reference = pixels('reference_direct_full_live')
reference_geometry = by_name['reference_direct_full_live']['objects']['OcclusionWhole']['divider']['whole_presentation']
h, w = reference.shape[:2]
assert abs(w / h - 2233 / 911) < .01, 'Review fixed-camera oracle after viewport changes'
region = (slice(int(h*.48), int(h*.54)), slice(int(w*.66), int(w*.68)))
for row in rows:
    b = row['objects']['OcclusionWhole']
    if not b['confirmed'] or not b['current']:
        continue
    name = row['label']
    check(name + ' whole current with real contact',
          b['uniform'] and b['source_visible'] and b['divider']['object_contact'])
    check(name + ' complete current geometry',
          b['divider']['whole_presentation'] == reference_geometry and
          b['divider']['divider_source'] == 'UNKNOWN')
    check(name + ' actual source bindings', all(
        p['visible'] and p['ready'] == 1 and p['texture'] != 'None'
        for p in b['capture']['source_bindings']))

first = by_name['stage1_far_partial_live']['objects']['OcclusionWhole']
check('wall still limits legal coverage and confirmation input',
      0 < first['coverage'] < .95 and
      0 < first['divider']['physical_occlusion']['set'] < first['divider']['full_geometry']['set'])
for name in ('stage1_far_partial_live', 'stage3_near_full_live'):
    # Player/torch position changes illumination, so raw RGB equality would test
    # lighting instead of missing geometry. This old wall-cut region must now be
    # predominantly blue cabinet foreground (the before-fix control is 0%).
    patch = pixels(name)[region]
    foreground = float(((patch[:,:,2] > patch[:,:,0]+20) & (patch[:,:,1] > patch[:,:,0]+8)).mean())
    check(name + ' original cabinet face has blue foreground throughout old cut',
          foreground > .90, blue_foreground_fraction=foreground)

wall = pixels('camera_depth_wall')
control = pixels('camera_depth_wall_hidden_control')
restored = pixels('camera_depth_wall_restored')
# Camera points directly at the cabinet through the existing opaque wall.
# Hiding only the wall renderer must reveal the source at the same screen center;
# restoring it must again cover that source. Fog occluder segments never change.
center = (slice(int(h*.44), int(h*.56)), slice(int(w*.47), int(w*.53)))
change = float(np.abs(wall[center] - control[center]).mean())
restore_error = float(np.abs(wall[center] - restored[center]).mean())
check('camera wall occludes submitted Whole; render-only positive control',
      change > 10 and restore_error < 3,
      wall_to_control_rgb_error=change, wall_restore_rgb_error=restore_error)
for name in ('camera_depth_wall', 'camera_depth_wall_hidden_control', 'camera_depth_wall_restored'):
    b = by_name[name]['objects']['OcclusionWhole']
    check(name + ' observer retains legal contact independently of camera', b['coverage'] > .99)

result = dict(passed=all(c['passed'] for c in checks), checks=checks)
(args.run / 'whole-current-oracle.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(dict(passed=result['passed'], checks=len(checks), failed=[c for c in checks if not c['passed']]), indent=2))
raise SystemExit(0 if result['passed'] else 1)

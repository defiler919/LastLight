"""Image oracle for the fixed-camera Room 01 Whole -> history replay.

The central front-face patch of the known cabinet must remain brighter than
the adjacent gray floor after the observer faces away. This checks original
game-viewport pixels, independently of proxy counts or submitted texture data.
The normalized patches belong to the fixed camera at the captured 2233:911 aspect;
this is not a general scene classifier or a performance measurement.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('evidence', type=Path)
args = parser.parse_args()
rows = []
for path in sorted(args.evidence.glob('pie*_whole_exit_*.png')):
    pixels = np.asarray(Image.open(path).convert('RGB'), dtype=float)
    height, width, _ = pixels.shape
    assert abs(width/height - 2233/911) < .01, 'Unsupported camera aspect: re-project the fixture patches first'
    def patch(x0, x1, y0=.51, y1=.56):
        return pixels[int(height*y0):int(height*y1), int(width*x0):int(width*x1)].mean(axis=2)
    front = patch(.48, .52)
    floor = float(np.median(patch(.40, .44)))
    fraction = float(np.mean(front > floor + 25))
    rows.append(dict(image=path.name, floor=floor,
                     front_mean=float(front.mean()), visible_fraction=fraction,
                     passed=fraction > .95))
assert rows, 'Missing fixed-camera Whole exit frames'
report = dict(visual_pass=all(row['passed'] for row in rows), frames=rows)
(args.evidence/'whole_handoff_oracle.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
raise SystemExit(0 if report['visual_pass'] else 1)

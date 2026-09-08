"""Compare same-machine CPU exports and preserve unmodified D3D12 views in sheets.

Usage: python Scripts/AnalyzeUnknownPartialStripe.py BEFORE_RUN AFTER_RUN OUTPUT_PREFIX
Run arguments are folders under Saved/GrayObjectPolicy. Requires Pillow.
"""
import csv
import hashlib
import json
import shutil
import sys
from pathlib import Path
from PIL import Image, ImageDraw

repo = Path(__file__).resolve().parents[1]
before, after = [repo / 'Saved/GrayObjectPolicy' / name for name in sys.argv[1:3]]
prefix = repo / sys.argv[3]
prefix.parent.mkdir(parents=True, exist_ok=True)
summary = {'before': before.name, 'after': after.name, 'sequences': {}, 'files': []}


def record(path):
    summary['files'].append({'path': path.relative_to(repo).as_posix(),
                             'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})


sheet = Image.new('RGB', (1536, 3 * 414), '#20252b')
draw = ImageDraw.Draw(sheet)
for row, sequence in enumerate('ABC'):
    dirs = [r / 'Captures/SpatialPartialCut' / sequence for r in (before, after)]
    states = [list(csv.DictReader((d / '09_no_resurrection_samples.csv').open())) for d in dirs]
    assert len(states[0]) == len(states[1])
    cpu = geometry_b = changed_padding = 0
    for old, new in zip(*states):
        cpu += any(old[k] != new[k] for k in old if k != 'b')
        if old['b'] != new['b']:
            geometry_b += old['footprint'] == '1'
            changed_padding += old['footprint'] == '0'
    assert cpu == geometry_b == 0, 'Authority, hard A, or interior envelope changed'
    summary['sequences'][sequence] = {'samples': len(states[0]), 'cpu_and_hard_a_differences': cpu,
                                     'geometry_b_differences': geometry_b, 'exterior_b_changes': changed_padding}
    for col, directory in enumerate(dirs):
        path = directory / '09_no_resurrection_scene.png'
        record(path)
        record(directory / '09_no_resurrection_samples.csv')
        im = Image.open(path).convert('RGB')
        sheet.paste(im, (col * 384, row * 414 + 30))
        draw.text((col * 384 + 10, row * 414 + 8), f'{sequence} / 37 deg / {"BEFORE" if col == 0 else "AFTER"}', fill='white')
        crop = im.crop((150, 180, 246, 276)).resize((384, 384), Image.Resampling.NEAREST)
        sheet.paste(crop, ((col + 2) * 384, row * 414 + 30))
        draw.text(((col + 2) * 384 + 10, row * 414 + 8), f'{sequence} / {"BEFORE" if col == 0 else "AFTER"} / 4x nearest crop', fill='white')
sheet.save(prefix.with_suffix('.png'))

stages = ['07_rotated_resweep', '08_live_transaction', '09_no_resurrection', '12_rotated_reobserved']
sheet = Image.new('RGB', (1536, 3 * 414), '#20252b')
draw = ImageDraw.Draw(sheet)
for row, sequence in enumerate('ABC'):
    for col, stage in enumerate(stages):
        path = after / 'Captures/SpatialPartialCut' / sequence / (stage + '_scene.png')
        record(path)
        sheet.paste(Image.open(path).convert('RGB'), (col * 384, row * 414 + 30))
        draw.text((col * 384 + 10, row * 414 + 8), f'{sequence} / {stage}', fill='white')
sheet.save(prefix.with_name(prefix.name + '_STAGES').with_suffix('.png'))
sheet = Image.new('RGB', (1536, 800), '#20252b')
draw = ImageDraw.Draw(sheet)
for col, run in enumerate((before, after)):
    path = run / 'Captures/SpatialPartialTemporal/09_no_resurrection.png'
    record(path)
    sheet.paste(Image.open(path).convert('RGB'), (col * 768, 32))
    label = 'BEFORE' if col == 0 else 'AFTER'
    draw.text((col * 768 + 12, 10), f'{label} / 37 deg / real engine frames / temporal AA enabled', fill='white')
    shutil.copyfile(path, prefix.with_name(prefix.name + '_' + label + '_AA37').with_suffix('.png'))
sheet.save(prefix.with_name(prefix.name + '_TEMPORAL').with_suffix('.png'))
sheet = Image.new('RGB', (1536, 828), '#20252b')
draw = ImageDraw.Draw(sheet)
for index, stage in enumerate(['01_gray', '07_rotated_resweep', '08_live_transaction',
                               '08b_left_blocked', '08c_unblock_first_frame',
                               '09_no_resurrection', '12_rotated_reobserved']):
    path = after / 'Captures/SpatialPartialTemporal' / (stage + '.png')
    record(path)
    x, y = (index % 4) * 384, (index // 4) * 414
    sheet.paste(Image.open(path).convert('RGB').resize((384, 384), Image.Resampling.LANCZOS), (x, y + 30))
    draw.text((x + 10, y + 8), stage, fill='white')
sheet.save(prefix.with_name(prefix.name + '_TEMPORAL_STAGES').with_suffix('.png'))
prefix.with_suffix('.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
print(json.dumps(summary['sequences'], indent=2))

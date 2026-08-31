"""Inspect native evidence, create contact sheets; no artifact leaves ignored Saved."""
import argparse
import json
import re
from pathlib import Path
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument('--label', default='Visual01')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1] / 'Saved' / 'PropGameplayLab'
runs = []
for log in sorted(root.glob(args.label + '_*.log')):
    text = log.read_text(encoding='utf-8', errors='replace')
    folder = log.with_suffix('')
    frames = sorted(folder.glob('frame_*.png'))
    if not frames:
        continue
    dimensions = [Image.open(f).size for f in frames]
    contract = re.findall(r'^.*(?:LAB_CONTRACT_FAIL|Fatal error:|Assertion failed:|Ensure condition failed:|activation failed|Failed to compile Material).*$', text, re.M)
    python_errors = [l for l in text.splitlines() if 'LogPython: Error:' in l]
    warnings = [l for l in text.splitlines() if 'Warning:' in l]
    events = [l for l in text.splitlines() if 'PropLab EVENT' in l]
    evidence = [l for l in text.splitlines() if 'LAB_EVIDENCE' in l]
    run = dict(name=folder.name, frames=len(frames), dimensions=sorted(set(dimensions)),
               complete='LAB_CAPTURE_COMPLETE' in text, activated='PropLab activated' in text,
               contract_errors=contract, engine_python_error_lines=len(python_errors),
               warnings=warnings, events=events, evidence_samples=len(evidence))
    runs.append(run)
    selected = [frames[round(i*(len(frames)-1)/8)] for i in range(9)]
    sheet = Image.new('RGB', (1440, 864), '#131923')
    draw = ImageDraw.Draw(sheet)
    for i, frame in enumerate(selected):
        thumb = Image.open(frame).convert('RGB').resize((480,270))
        x,y=(i%3)*480,(i//3)*288
        sheet.paste(thumb,(x,y+18))
        draw.text((x+6,y+2),f'{folder.name} / {frame.stem}',fill='white')
    sheet.save(folder / 'contact.jpg', quality=92)

out = root / (args.label + '_index.json')
out.write_text(json.dumps(runs, indent=2), encoding='utf-8')
print(json.dumps(dict(runs=len(runs),complete=sum(r['complete'] for r in runs),
                     frames=sum(r['frames'] for r in runs),contract_errors=sum(len(r['contract_errors']) for r in runs),
                     index=str(out))))

"""Compare both identity policies at matched runtime timestamps, using latest complete logs."""
import argparse
import re
from pathlib import Path
from PIL import Image, ImageDraw

p = argparse.ArgumentParser()
p.add_argument('--prefix', default='Relocation')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]/'Saved'/'PropGameplayLab'
available = {}
for path in sorted(root.glob(a.prefix+'*.log'), key=lambda f: f.stat().st_mtime):
    match = re.search(r'_(1920|2560)_M(\d)_P(\d)_R(5|6)\.log$', path.name)
    if match and 'LAB_CAPTURE_COMPLETE' in path.read_text(errors='replace'):
        available[tuple(map(int,match.groups()))] = path
for width in (1920,2560):
    for mode in (0,1,2):
        for route in (5,6):
            paths = [available.get((width,mode,policy,route)) for policy in (0,1)]
            if not all(paths):
                continue
            sheet = Image.new('RGB',(1600,1422),'#131923')
            draw = ImageDraw.Draw(sheet)
            for policy,path in enumerate(paths):
                timeline = [(int(i),float(t)) for i,t in re.findall(r'LAB_EVIDENCE frame=(\d+).*?time=([\d.]+)',path.read_text(errors='replace'))]
                for row,target in enumerate((6,10,14)):
                    frame,actual = min(timeline,key=lambda pair:abs(pair[1]-target))
                    image = Image.open(path.with_suffix('')/f'frame_{frame:03}.png').convert('RGB').resize((800,450))
                    x,y = policy*800,row*474
                    sheet.paste(image,(x,y+24))
                    draw.text((x+8,y+5),f'{width} mode={mode} policy={policy} route={route} actual={actual:.3f}s',fill='white')
            target = root/f'{a.prefix}_{width}_M{mode}_R{route}_policies.jpg'
            sheet.save(target,quality=94)
            print(target)

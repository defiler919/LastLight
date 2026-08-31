import argparse
import re
from pathlib import Path
from PIL import Image, ImageDraw

p=argparse.ArgumentParser()
p.add_argument('--label',default='Visual02')
p.add_argument('--soft-label', help='Use a corrected mode-2 run without replacing earlier evidence')
p.add_argument('--output-label', default=None)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'Saved'/'PropGameplayLab'
for width in (1920,2560):
    for route in (1,2,3,4):
        folders=[root/f'{a.soft_label if m==2 and a.soft_label else a.label}_{width}_M{m}_P0_R{route}' for m in range(3)]
        if not all(f.with_suffix('.log').exists() and 'LAB_CAPTURE_COMPLETE' in f.with_suffix('.log').read_text(errors='replace') for f in folders):
            continue
        sheet=Image.new('RGB',(1920,1152),'#131923')
        draw=ImageDraw.Draw(sheet)
        for m,folder in enumerate(folders):
            log=folder.with_suffix('.log').read_text(errors='replace')
            timeline=[(int(i),float(t)) for i,t in re.findall(r'LAB_EVIDENCE frame=(\d+).*?time=([\d.]+)',log)]
            for j,target in enumerate((4.5,7.0,9.5)):
                index,actual=min(timeline,key=lambda pair:abs(pair[1]-target))
                im=Image.open(folder/f'frame_{index:03}.png').convert('RGB').resize((640,360))
                x,y=m*640,j*384
                sheet.paste(im,(x,y+24))
                draw.text((x+8,y+5),f'{width} / route {route} / mode {m} / actual t={actual:.3f}s',fill='white')
        path=root/f'{a.output_label or a.label}_{width}_R{route}_comparison.jpg'
        sheet.save(path,quality=95)
        print(path)

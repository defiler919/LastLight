"""Secondary rendered-pixel evidence; must accompany actual contact-sheet inspection."""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
p=argparse.ArgumentParser();p.add_argument('--label',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'Saved'/'PropGameplayLab'
runs=json.loads((root/(a.label+'_audit.json')).read_text())
results=[]
for width in (1920,2560):
    group=[next((r for r in runs if r['width']==width and r['mode']==m and r['route']==1),None) for m in (0,1,2)]
    if not all(group):continue
    hard=group[1]
    # Inspect every whole-object rendered frame, not only the matched examples.
    whole_fractions=[]
    for index in range(group[0]['frames']):
        img=Image.open(root/group[0]['name']/f'frame_{index:03d}.png').convert('RGB')
        pts=np.array(group[0]['polys'][str(index)]);center=pts.mean(axis=0);pts=center+(pts-center)*.95
        x0,y0=np.floor(pts.min(axis=0)).astype(int);x1,y1=np.ceil(pts.max(axis=0)).astype(int)
        patch=img.crop((int(x0),int(y0),int(x1+1),int(y1+1)))
        mask=Image.new('L',patch.size);ImageDraw.Draw(mask).polygon([tuple(v) for v in pts-[x0,y0]],fill=255)
        pixels=np.array(patch,dtype=np.int16)[np.array(mask)>0]
        whole_fractions.append(float(np.mean(pixels[:,2]-pixels[:,0]>40)))
    intermediates=[i for i,f in enumerate(whole_fractions) if .02<f<.98]
    results.append(dict(width=width,whole_object_frames=len(whole_fractions),
        whole_object_intermediate_frames=intermediates,passed=not intermediates,
        gray_frames=sum(f<=.02 for f in whole_fractions),live_frames=sum(f>=.98 for f in whole_fractions)))
    indices=[min((i for i,r in enumerate(hard['rows']) if r['time']<16),key=lambda i:abs(hard['rows'][i]['covered']-q)) for q in (.25,.5,.75)]
    for q,index in zip((.25,.5,.75),indices):
        fractions=[]
        for r in group:
            img=Image.open(root/r['name']/f'frame_{index:03d}.png').convert('RGB')
            pts=np.array(r['polys'][str(index)]);center=pts.mean(axis=0);pts=center+(pts-center)*.95
            mask=Image.new('L',img.size);ImageDraw.Draw(mask).polygon([tuple(v) for v in pts],fill=255)
            pixels=np.array(img,dtype=np.int16)[np.array(mask)>0]
            fractions.append(float(np.mean(pixels[:,2]-pixels[:,0]>40)))
        checks=[fractions[0]>.98,abs(fractions[1]-q)<.055,abs(fractions[2]-fractions[1])<.04]
        results.append(dict(width=width,frame=index,time=hard['rows'][index]['time'],target=q,rendered_blue_fraction=fractions,passed=all(checks)))
    # A fixed surface point, with the same camera and world pixel through the crossing.
    pts=np.array(hard['polys'][str(indices[1])]);x,y=np.round(pts.mean(axis=0)).astype(int)
    samples=[]
    for index in range(indices[1]-15,indices[1]+25):
        chroma=[]
        for r in group:
            img=Image.open(root/r['name']/f'frame_{index:03d}.png').convert('RGB');red,green,blue=img.getpixel((int(x),int(y)));chroma.append(blue-red)
        samples.append(dict(frame=index,time=hard['rows'][index]['time'],blue_minus_red=chroma))
    results.append(dict(width=width,pixel=[int(x),int(y)],temporal_samples=samples,
        soft_differs_during_entry=any(s['blue_minus_red'][1]-s['blue_minus_red'][2]>8 for s in samples)))
(root/(a.label+'_pixels.json')).write_text(json.dumps(results,indent=2))
for r in results:print({k:v for k,v in r.items() if k!='temporal_samples'})
if any(not r.get('passed',r.get('soft_differs_during_entry')) for r in results):raise SystemExit(1)

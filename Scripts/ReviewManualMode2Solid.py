"""Offline QA of genuine PIE frames; retain all frames and failures.
Usage: python Scripts/ReviewManualMode2Solid.py <PIE directory> <expected width>
Creates review sheets and numeric evidence in that ignored directory only.
"""
import json, sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

root=Path(sys.argv[1]); width=int(sys.argv[2]); height=width*9//16
rows=json.loads((root/'checks.json').read_text())
def file(row):
    matches=list(root.glob(Path(row['image']).stem+'*.png'))
    assert len(matches)==1,(row['image'],matches)
    return matches[0]
images=[r for r in rows if 'image' in r]
dimension_errors=[]
for row in images:
    with Image.open(file(row)) as im:
        if im.size!=(width,height):dimension_errors.append([str(file(row)),list(im.size)])
def sheet(name,chosen,crop=None,columns=3):
    tiles=[]
    for r in chosen:
        im=Image.open(file(r)).convert('RGB')
        if crop:im=im.crop(tuple(int(v*(width if i%2==0 else height)) for i,v in enumerate(crop)))
        im.thumbnail((620,350))
        tile=Image.new('RGB',(640,395),(30,30,30));tile.paste(im,((640-im.width)//2,35))
        ImageDraw.Draw(tile).text((8,8),f"{r['label']} t={r['t']:.3f} empty={r['verified']:.3f} live={r['reveal']:.3f}",fill='white')
        tiles.append(tile)
    out=Image.new('RGB',(columns*640,((len(tiles)+columns-1)//columns)*395),(20,20,20))
    for i,tile in enumerate(tiles):out.paste(tile,((i%columns)*640,(i//columns)*395))
    out.save(root/name)
important=[r for r in images if any(x in r['label'] for x in ('initial_full','half_solid_cap','verified_empty','hidden_shadow','shadow_control','rediscovered_full'))]
sheet('state_matrix.png',important)
stats={'dimensions':[width,height],'rows':len(rows),'screenshots':len(images),'cycles':[]}
stats['dimension_errors']=dimension_errors
perf=[r['t'] for r in rows if r['label']=='uncaptured_moving']
stats['uncaptured_moving_dt_quantiles']=np.quantile(np.diff(perf),[.5,.95,1]).tolist()
for cycle in range(2):
    group={key:[r for r in images if r['label']==f'cycle{cycle}_{key}'] for key in ('erase','reveal','static_cap')}
    for kind in ('erase','reveal'):
        subset=group[kind];assert len(subset)>=20,(kind,len(subset))
        idx=np.linspace(0,len(subset)-1,9).astype(int)
        sheet(f'cycle{cycle}_{kind}_sequence.png',[subset[i] for i in idx])
        middle=next((i for i,r in enumerate(subset) if .3<r['reveal' if kind=='reveal' else 'verified']<.7),len(subset)//2)
        sheet(f'cycle{cycle}_{kind}_adjacent.png',subset[max(0,middle-3):middle+6])
        if kind=='erase':
            # The scan starts from the previously photographed 72% partial
            # state. Also inspect the first *new* erasure, not just its tail.
            active=next((i for i,r in enumerate(subset) if subset[0]['verified']+.01<r['verified']<.98),0)
            sheet(f'cycle{cycle}_erase_active_adjacent.png',subset[max(0,active-2):active+7])
    # Whole viewport motion is intentionally present in moving scans. Static
    # ROI only excludes HUD, and measures pixel differences without denoising.
    stationary=[np.array(Image.open(file(r)).convert('RGB')) for r in group['static_cap']]
    diffs=[]
    for a,b in zip(stationary,stationary[1:]):
        region=np.abs(a[int(.18*height):int(.62*height)].astype(float)-b[int(.18*height):int(.62*height)].astype(float))
        diffs.append({'mean8bit':float(region.mean()),'p99_8bit':float(np.quantile(region,.99)),'over16_fraction':float((region.max(axis=2)>16).mean())})
    cap_masks=[im[int(.25*height):int(.30*height),int(.52*width):int(.55*width)].max(axis=2)<40 for im in stationary]
    cap_xor=[float((a!=b).mean()) for a,b in zip(cap_masks,cap_masks[1:])]
    areas=[]
    for r in group['reveal']:
        im=np.array(Image.open(file(r)).convert('RGB')).astype(float)
        im=im[int(.18*height):int(.42*height),int(.35*width):int(.65*width)]
        areas.append(int(((im[:,:,1]>im[:,:,0]*1.2)&(im[:,:,1]>im[:,:,2]*1.1)&(im[:,:,1]>50)).sum()))
    frames=[r for r in rows if r['label']==f'cycle{cycle}_reveal']
    dt=np.diff([r['t'] for r in frames])
    stats['cycles'].append({'cycle':cycle,'static_differences':diffs,'reveal_frames':len(frames),
        'static_cap_mask_xor_fractions':cap_xor,'green_pixel_areas':areas,
        'max_reveal_area_step_fraction':float(max(np.diff(areas))/max(areas)),
        'partial_reveal_frames':sum(0<r['reveal']<.85 for r in frames),'frame_dt_median':float(np.median(dt)),
        'frame_dt_p95':float(np.quantile(dt,.95)),'frame_dt_max':float(dt.max())})
(root/'review.json').write_text(json.dumps(stats,indent=2))
print(json.dumps(stats,indent=2))
assert not dimension_errors,dimension_errors[:3]

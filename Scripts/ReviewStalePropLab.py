"""Read captured game frames/logs; never generate substitute gameplay images."""
import argparse, json, math, re, subprocess
from pathlib import Path
from PIL import Image, ImageDraw

p=argparse.ArgumentParser();p.add_argument('--label',required=True);p.add_argument('--width',type=int,default=1920);p.add_argument('--cases',default='ABCDEF');p.add_argument('--video',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'Saved/PropGameplayLab'
out=root/(a.label+'_Review');out.mkdir(exist_ok=True)
frame_re=re.compile(r'STALE_FRAME (.+)')
def route_yaw(t,case):
    if t<6:return (-90 if case=='F' else 90)+70*math.sin(t*math.pi/3)
    if t<10:return -90
    if t<20:y=-40+(t-10)*13
    elif t<22:y=90+(4*math.sin((t-20)*2*math.pi) if case=='E' else 0)
    elif t<32:y=90+(t-22)*13
    else:return -90
    return 180-y if case=='D' else y
def parse(log):
    text=log.read_text(encoding='utf-8-sig',errors='replace')
    frames=[dict(re.findall(r'(\w+)=(\([^)]*\)|\S+)',m)) for m in frame_re.findall(text)]
    hashes=re.findall(r'STALE_GRID n=(\d+) size=(\S+) verifiedHash=(\d+)',text)
    return text,frames,hashes

summary=[]
for case in a.cases:
    runs=[root/f'{a.label}_{a.width}_M{m}_{case}' for m in range(3)]
    parsed=[parse(r.with_suffix('.log')) for r in runs]
    base=parsed[0][1]
    assert base and all(len(x[1])==len(base) for x in parsed)
    for text,frames,hashes in parsed:
        assert 'STALE_CAPTURE_COMPLETE' in text and 'STALE_FAIL' not in text
        assert 'PCD3D_SM6' in text and 'D3D12' in text
        assert not re.search(r'Fatal error:|Assertion failed:|Ensure condition failed:|GPU crashed|Failed to compile Material',text)
        ratios=[float(f['verified']) for f in frames]
        assert all(abs((float(f['yaw'])-route_yaw(float(f['t']),case)+180)%360-180)<.01 for f in frames), 'route aim overwritten'
        assert all(abs(float(y['t'])-float(x['t'])-1/30)<.00001 for x,y in zip(frames,frames[1:])), 'route clock interval differs'
        assert len({f['camera'] for f in frames if 10<=float(f['t'])<32})==1, 'camera moved during scan'
        assert all(y>=x for x,y in zip(ratios,ratios[1:])), 'resurrection'
        assert all(f['enemy']=='0' and f['torch']=='100.00' for f in frames)
        assert all(f['live']=='0' for f in frames if float(f['t'])>=8), 'B leaked'
        assert hashes==parsed[0][2], 'different spatial evidence between modes'
        for f,g in zip(base,frames):
            for key in ('n','t','case','yaw','player','camera','verified','empty','phase'):
                assert f[key]==g[key], (case,key,f,g)
        assert ratios[-1]==1 if case!='F' else 0<ratios[-1]<1
    assert all(float(f['opacity']) in (0,1) for f in base)
    hard=parsed[1][1];soft=parsed[2][1]
    assert any(0<float(f['opacity'])<1 for f in hard if float(f['t'])>10)
    assert all(float(s['opacity'])+2e-6>=float(h['opacity']) for s,h in zip(soft,hard))
    assert any(float(s['opacity'])>float(h['opacity'])+.001 for s,h in zip(soft,hard))
    for r,(_,frames,_) in zip(runs,parsed):
        assert len(list(r.glob('frame_*.png')))==len(frames)
    # Identical indices, selected by common authority; F never fabricates 75% evidence.
    indices=[min(range(len(hard)),key=lambda i:abs(float(hard[i]['t'])-t)) for t in (7,9)]
    for fraction in (.25,.5,.75):
        if fraction<=float(hard[-1]['verified']): indices.append(min(range(len(hard)),key=lambda i:abs(float(hard[i]['verified'])-fraction)))
    indices.extend([min(range(len(hard)),key=lambda i:abs(float(hard[i]['t'])-t)) for t in (31,34)])
    sheet=Image.new('RGB',(1920,390*len(indices)),(22,24,28));draw=ImageDraw.Draw(sheet)
    for row,i in enumerate(indices):
        for mode,r in enumerate(runs):
            im=Image.open(r/f'frame_{i:04d}.png').convert('RGB');im.thumbnail((640,360))
            x,y=640*mode,390*row;sheet.paste(im,(x,y+30))
            f=parsed[mode][1][i]
            draw.text((x+8,y+8),f"{case} MODE {mode} t={float(f['t']):.2f}s verified={100*float(f['verified']):.1f}% ghost={f['ghost']}",fill='white')
    sheet.save(out/f'{a.width}_{case}_matched.png')
    # Consecutive full-rate frames around a partial erase, to inspect pop/fade and stability.
    middle=indices[min(3,len(indices)-1)]
    adjacent=Image.new('RGB',(1920,220*9),(22,24,28));d=ImageDraw.Draw(adjacent)
    for row,i in enumerate(range(middle-4,middle+5)):
        for mode,r in enumerate(runs):
            im=Image.open(r/f'frame_{i:04d}.png').convert('RGB')
            w,h=im.size;im=im.crop((int(w*.12),int(h*.25),int(w*.88),int(h*.62)));im.thumbnail((640,192))
            x,y=640*mode,row*220;adjacent.paste(im,(x,y+24));d.text((x+5,y+5),f"M{mode} frame {i} t={float(hard[i]['t']):.3f}",fill='white')
    adjacent.save(out/f'{a.width}_{case}_adjacent.png')
    if a.video:
        ffmpeg=root/'Tools/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe'
        args=[str(ffmpeg),'-hide_banner','-loglevel','error','-y']
        for r in runs:args+=['-framerate','30','-i',str(r/'frame_%04d.png')]
        args+=['-filter_complex','[0:v]scale=640:360[a];[1:v]scale=640:360[b];[2:v]scale=640:360[c];[a][b][c]hstack=inputs=3[v]','-map','[v]','-c:v','libx264','-crf','18','-pix_fmt','yuv420p',str(out/f'{a.width}_{case}_three_modes.mp4')]
        subprocess.run(args,check=True)
    item={'case':case,'frames_per_mode':len(base),'duration':base[-1]['t'],'final_verified':hard[-1]['verified'],'matched_frames':indices,'first_whole_time':next((f['t'] for f in hard if f['empty']=='1'),None),'authority_hashes_equal':True,'camera_event_path_equal':True,'monotonic':True,'B_leak':False}
    summary.append(item)
(out/f'{a.width}_summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print(json.dumps(summary,indent=2))

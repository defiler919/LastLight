"""Matched real-frame comparison, geometry crops and consecutive-frame review sheets."""
import argparse,json,re,statistics,subprocess,sys
from pathlib import Path
from PIL import Image,ImageDraw
p=argparse.ArgumentParser();p.add_argument('--label',required=True);p.add_argument('--video',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'Saved'/'PropGameplayLab'
runs=[]
for log in sorted(root.glob(a.label+'_*.log')):
    if not re.search(r'_\d+_M\d_P\d_R\d+\.log$',log.name):continue
    text=log.read_text(encoding='utf-8',errors='replace')
    if 'LAB_CAPTURE_COMPLETE' not in text:continue
    width,mode,policy,route=map(int,re.search(r'_(\d+)_M(\d)_P(\d)_R(\d+)\.log$',log.name).groups())
    rows=[]
    for line in text.splitlines():
        if 'LAB_COMPARE frame=' not in line:continue
        row={k:v for k,v in re.findall(r'(\w+)=(\([^)]*\)|[^ ]+)',line.split('LAB_COMPARE ')[1])}
        row={k:([float(x) for x in v[1:-1].split(',')] if v.startswith('(') else float(v)) for k,v in row.items()}
        rows.append(row)
    polys={int(i):[[float(x) for x in point.split(',')] for point in points.split()] for i,points in re.findall(r'LAB_TOP frame=(\d+)([^\r\n]+)',text)}
    frames=sorted(log.with_suffix('').glob('frame_*.png'))
    expected=900 if route==1 else 180
    checks={'complete':len(rows)==len(frames)==expected,'numbers':[int(r['frame']) for r in rows]==list(range(expected)),
        'dimensions':all(Image.open(f).size==(width,width*9//16) for f in frames),
        'no_enemy':bool(rows) and all(r['enemy']==0 for r in rows),'full_torch_health':bool(rows) and all(r['torch']==r['health']==100 for r in rows),
        'single_island':bool(rows) and all(r['parts']==1 for r in rows),
        'mode_policy':all(r['mode']==mode and r['policy']==policy and r['route']==route for r in rows),
        'severe_scan':not re.search(r'LAB_CONTRACT_FAIL|Fatal error:|Assertion failed:|Ensure condition failed:|GPU crashed|DXGI_ERROR_DEVICE|Failed to compile Material',text),
        'd3d12_sm6_tsr':'Using Forced RHI: D3D12' in text and 'Using Forced Feature Level in Editor: SM6' in text and 'r.AntiAliasingMethod 4' in text}
    if route==1:
        checks.update(fixed_camera=all(r['camera']==rows[0]['camera'] and r['player']==rows[0]['player'] for r in rows),
            full_duration=abs(rows[-1]['time']-30)<.01,
            gray_start=all(r['covered']<.001 and not r['live'] for r in rows if r['time']<1.9),
            midpoint_hold=all(abs(r['yaw']-40)<.001 for r in rows if 8.01<r['time']<9.99),
            intermediate_coverage=all(any(abs(r['covered']-target)<.02 for r in rows if r['time']<16) for target in (.25,.5,.75)))
    run=dict(name=log.stem,width=width,mode=mode,policy=policy,route=route,checks=checks,passed=all(checks.values()),frames=len(frames),rows=rows,polys=polys)
    runs.append(run)
    print(('PASS' if run['passed'] else 'FAIL'),log.stem,checks)
    folder=log.with_suffix('')
    sheet=Image.new('RGB',(1440,864),'#17202b');draw=ImageDraw.Draw(sheet)
    for i,index in enumerate(round(j*(len(rows)-1)/8) for j in range(9)):
        img=Image.open(frames[index]).convert('RGB').resize((480,270));x,y=(i%3)*480,(i//3)*288
        sheet.paste(img,(x,y+18));draw.text((x+5,y+2),f'M{mode} R{route} f{index} t={rows[index]["time"]:.3f}',fill='white')
    sheet.save(folder/'contact.jpg',quality=94)
    if a.video:
        sys.path.insert(0,str(root/'Tools'));import imageio_ffmpeg
        fps=30 if route==1 else 15
        subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),'-y','-loglevel','error','-framerate',str(fps),'-i',str(folder/'frame_%03d.png'),'-c:v','libx264','-threads','2','-crf','18','-pix_fmt','yuv420p',str(folder/'review.mp4')],check=True)
for width in (1920,2560):
    group=[next((r for r in runs if r['width']==width and r['mode']==m and r['route']==1),None) for m in (0,1,2)]
    if not all(group):continue
    hard=group[1]
    indices=[min((i for i,r in enumerate(hard['rows']) if r['time']<16),key=lambda i:abs(hard['rows'][i]['covered']-q)) for q in (.25,.5,.75)]
    identical=all(all(abs(r['rows'][i][key]-hard['rows'][i][key])<.0001 for key in ('time','yaw','covered','rawMean')) for r in group for i in range(len(hard['rows'])))
    print('MATCHED_TRAJECTORY',width,identical,'FRAMES',indices)
    for crop in (False,True):
        tile=(800,450) if not crop else (800,240)
        sheet=Image.new('RGB',(3*tile[0],3*(tile[1]+30)),'#17202b');draw=ImageDraw.Draw(sheet)
        for row,index in enumerate(indices):
            for col,r in enumerate(group):
                img=Image.open(root/r['name']/f'frame_{index:03d}.png').convert('RGB')
                if crop:
                    pts=r['polys'][index];xs=[p[0] for p in pts];ys=[p[1] for p in pts]
                    img=img.crop((max(0,min(xs)-10),max(0,min(ys)-12),min(width,max(xs)+10),min(width*9//16,max(ys)+85)))
                x,y=col*tile[0],row*(tile[1]+30);sheet.paste(img.resize(tile),(x,y+30))
                d=r['rows'][index];draw.text((x+6,y+7),f'MODE {col} f={index} t={d["time"]:.3f}s raw={d["covered"]:.3f} actorLive={int(d["live"])}',fill='white')
        sheet.save(root/(f'{a.label}_{width}_matched'+('_detail.jpg' if crop else '.jpg')),quality=95)
    # Neighboring true rendered frames around the 50% sample, plus first whole-object reveal.
    trigger=next(i for i,r in enumerate(group[0]['rows']) if r['live'])
    for label,center in [('half',indices[1]),('jump',trigger)]:
        selected=list(range(max(0,center-2),center+5))
        sheet=Image.new('RGB',(7*400,3*170),'#17202b');draw=ImageDraw.Draw(sheet)
        for m,r in enumerate(group):
            for col,index in enumerate(selected):
                img=Image.open(root/r['name']/f'frame_{index:03d}.png').convert('RGB');pts=r['polys'][index]
                xs=[p[0] for p in pts];ys=[p[1] for p in pts]
                img=img.crop((min(xs)-8,min(ys)-8,max(xs)+8,max(ys)+80))
                x,y=col*400,m*170;sheet.paste(img.resize((400,145)),(x,y+25))
                draw.text((x+4,y+5),f'M{m} f{index} t={r["rows"][index]["time"]:.3f}',fill='white')
        sheet.save(root/f'{a.label}_{width}_adjacent_{label}.jpg',quality=96)
    if not identical:raise SystemExit('Trajectories not identical')
(root/(a.label+'_audit.json')).write_text(json.dumps(runs,indent=2),encoding='utf-8')
if any(not r['passed'] for r in runs):raise SystemExit(1)

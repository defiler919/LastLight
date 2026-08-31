"""Closed expected matrix plus explicit warning/error inventory; no hidden retries."""
import argparse,json,re,subprocess,sys
from collections import Counter
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--label',default='RetestFinal01');a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'Saved'/'PropGameplayLab'
results=json.loads((root/(a.label+'_audit.json')).read_text())
expected={f'{a.label}_{w}_M{m}_P0_R{r}' for w in (1920,2560) for m in (0,1,2) for r in (1,3,11,12,13)}
actual={r['name'] for r in results}
warnings=Counter();errors=Counter();unexpected=[];mismatches=0;videos=[]
sys.path.insert(0,str(root/'Tools'))
import imageio_ffmpeg
for name in sorted(expected):
    path=root/(name+'.log')
    if not path.exists():continue
    text=path.read_text(encoding='utf-8',errors='replace')
    # Retain and narrowly classify existing engine Toolset startup tracebacks.
    # Project Python errors or a different missing API remain failures.
    allowed_python=set()
    python_lines=[line for line in text.splitlines() if 'LogPython: Error:' in line]
    blocks=[]
    for line in python_lines:
        if 'Traceback (most recent call last):' in line:blocks.append([])
        if blocks:blocks[-1].append(line)
    for block in blocks:
        paths=re.findall(r'File "([^"]+)"','\n'.join(block))
        known_end=re.search(r"AttributeError: module 'unreal' has no attribute '(AgentSkill|ToolsetDefinition|PythonTestRunner)'$",block[-1])
        if paths and known_end and all('/Engine/Plugins/Experimental/Toolset' in path.replace('\\','/') for path in paths) and text.find(block[-1])<text.find('LAB_COMPARE frame=0 '):
            allowed_python.update(block)
    for line in text.splitlines():
        if ': Warning:' in line:warnings[re.sub(r'^.*?\]\[.*?\]','',line)]+=1
        if ': Error:' in line:
            message=re.sub(r'^.*?\]\[.*?\]','',line);errors[message]+=1
            if line not in allowed_python and not ('LogAutomationTest: Error: Condition failed' in line and text.find(line)<text.find('LogEngine: Initializing Engine...')):
                unexpected.append(dict(run=name,line=line))
    mismatches+=text.count('LAB_CONTRACT_FAIL independent component')
    video=root/name/'review.mp4'
    metadata=subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),'-hide_banner','-i',str(video)],capture_output=True,text=True).stderr
    duration=re.search(r'Duration: (\d+):(\d+):([\d.]+)',metadata)
    seconds=sum(float(v)*scale for v,scale in zip(duration.groups(),(3600,60,1))) if duration else 0
    expected_seconds=30 if name.endswith('_R1') else 12
    videos.append(dict(run=name,exists=video.exists(),seconds=seconds,passed=abs(seconds-expected_seconds)<.001))
report=dict(label=a.label,expected_runs=len(expected),actual_runs=len(actual),passed_runs=sum(r['passed'] for r in results),
            frames=sum(r['frames'] for r in results),missing=sorted(expected-actual),extra=sorted(actual-expected),
            independent_component_visibility_failures=mismatches,unexpected_error_lines=unexpected,
            video_checks=videos,
            error_inventory=dict(errors),warning_inventory=dict(warnings),
            capture_note='Fixed simulation timestep: route 1 = 30fps/30s; motion/rotation = 15fps/12s. Not a realtime performance claim.')
(root/(a.label+'_closure.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
print({k:v for k,v in report.items() if k not in ('error_inventory','warning_inventory','unexpected_error_lines','video_checks')})
print('Error lines',sum(errors.values()),'Warning lines',sum(warnings.values()),'Unexpected errors',len(unexpected))
print('Video duration checks',sum(v['passed'] for v in videos),'/',len(expected))
if expected!=actual or report['passed_runs']!=30 or unexpected or mismatches or len(videos)!=30 or not all(v['passed'] for v in videos):raise SystemExit(1)

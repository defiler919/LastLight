"""Final matrix completeness and severe-log audit. Keeps all evidence under ignored Saved."""
import json
import re
import statistics
from pathlib import Path
from PIL import Image

root=Path(__file__).resolve().parents[1]/'Saved'/'PropGameplayLab'
expected=[]
for width in (1920,2560):
    for mode in (0,1,2):
        for route in (1,2,3,4):
            expected.append(f'Visual04_{width}_M{mode}_P0_R{route}')
for width,modes in ((1920,(0,1,2)),(2560,(2,))):
    for mode in modes:
        for policy in (0,1):
            for route in (5,6):
                expected.append(f'Relocation03_{width}_M{mode}_P{policy}_R{route}')
for policy in (0,1):
    for route in (7,8,9,10):
        expected.append(f'Events03_1920_M1_P{policy}_R{route}')
expected += ['Events03_1920_M0_P0_R7','Events03_2560_M2_P0_R7']
results=[]
severe_pattern=r'LAB_CONTRACT_FAIL|Fatal error:|LowLevelFatalError|Assertion failed:|Ensure condition failed:|activation failed|Failed to compile Material|GPU crashed|DXGI_ERROR_DEVICE|Out of memory'
for name in expected:
    path=root/(name+'.log')
    if not path.exists():
        results.append(dict(run=name,passed=False,missing=True))
        continue
    text=path.read_text(encoding='utf-8',errors='replace')
    width,mode,policy,route=map(int,re.search(r'_(\d+)_M(\d)_P(\d)_R(\d+)$',name).groups())
    frames=sorted((root/name).glob('frame_*.png'))
    times=[float(t) for t in re.findall(r'LAB_EVIDENCE .*?time=([\d.]+)',text)]
    gaps=[b-a for a,b in zip(times,times[1:])]
    completed=re.search(r'LAB_CAPTURE_COMPLETE frames=(\d+)',text)
    checks={
        'complete':bool(completed),
        'all_frames_written':bool(completed) and len(frames)==int(completed[1])==len(times),
        'frame_numbers_contiguous':[f.stem for f in frames]==[f'frame_{i:03}' for i in range(len(frames))],
        'correct_resolution':bool(frames) and all(Image.open(f).size==(width,width*9//16) for f in frames),
        'd3d12_sm6':'Using Forced RHI: D3D12' in text and 'Using Forced Feature Level in Editor: SM6' in text,
        'normal_tsr':'r.AntiAliasingMethod 4' in text,
        'navigation':'LAB_NAV_READY=1' in text,
        'monotonic_time':bool(gaps) and all(g>0 for g in gaps),
        'whole_interval':bool(times) and times[0]<2.6 and times[-1]>(20.7 if route>=5 else 11.7),
        'no_severe_failure':not re.search(severe_pattern,text),
    }
    errors=[line for line in text.splitlines() if ': Error:' in line]
    warnings=[line for line in text.splitlines() if ': Warning:' in line]
    other_errors=[line for line in errors if 'LogPython: Error:' not in line]
    engine_start=text.find('LogEngine: Initializing Engine...')
    checks['other_errors_are_recorded_startup_diagnostics']=all(
        line.endswith('LogAutomationTest: Error: Condition failed') and engine_start>=0 and text.find(line)<engine_start
        for line in other_errors)
    results.append(dict(run=name,passed=all(checks.values()),checks=checks,frames=len(frames),
                        median_interval=statistics.median(gaps) if gaps else None,
                        maximum_interval=max(gaps) if gaps else None,
                        engine_python_errors=sum('LogPython: Error:' in line for line in errors),
                        other_error_lines=other_errors,warnings=warnings))
report=dict(expected_runs=len(expected),passed_runs=sum(r['passed'] for r in results),
            frames=sum(r.get('frames',0) for r in results),runs=results,
            evidence_checkpoint='a38dcfe (Build16; Automation07; MaterialGPU03; PIE02)',
            note='Engine startup diagnostics remain listed; no historical failed run is deleted or relabeled as passing.')
target=root/'FinalEvidenceAudit.json'
target.write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({key:report[key] for key in ('expected_runs','passed_runs','frames')}))
print(target)
if report['passed_runs']!=report['expected_runs']:
    for run in results:
        if not run['passed']: print('FAIL',run['run'],run.get('checks','missing'))
    raise SystemExit(1)

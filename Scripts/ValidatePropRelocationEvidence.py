"""Check observed runtime state traces, not assumed timing or screenshots alone."""
import argparse
import json
import re
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--label',default='Relocation01')
a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'Saved'/'PropGameplayLab'
results=[]
for path in sorted(root.glob(a.label+'_*.log')):
    text=path.read_text(encoding='utf-8',errors='replace')
    match=re.search(r'_M(\d)_P(\d)_R(\d+)\.log$',path.name)
    mode,policy,route=map(int,match.groups())
    times={int(i):float(t) for i,t in re.findall(r'LAB_EVIDENCE frame=(\d+).*?time=([\d.]+)',text)}
    records=[]
    for frame,id,live,valid,x,y,old in re.findall(r'LAB_PROP frame=(\d+) id=(\S+) live=(\d) valid=(\d) at=\((-?\d+),(-?\d+)\) retired=(\d+)',text):
        if int(frame) in times:
            records.append(dict(time=times[int(frame)],id=id,live=int(live),valid=int(valid),x=int(x),y=int(y),old=int(old)))
    def at(id,start,end): return [r for r in records if r['id']==id and start<r['time']<end]
    checks={}
    if route in (5,6):
        hidden=at('Lab.Fridge',5,7)
        checks['unseen_move_preserves_A_hides_B']=bool(hidden) and all(not r['live'] and r['valid'] and r['x']==-200 for r in hidden)
        first=at('Lab.Fridge',9,11)
        second=at('Lab.Fridge',13,15)
        if route==5:
            checks['A_first_invalidates_old']=bool(first) and all(not r['valid'] and not r['live'] for r in first)
            checks['then_B_is_recognized']=bool(second) and all(r['valid'] and r['live'] and r['x']==650 and r['old']==0 for r in second)
        else:
            checks['B_first_policy_split']=bool(first) and all(r['valid'] and r['live'] and r['x']==650 and r['old']==(1 if policy==0 else 0) for r in first)
            checks['then_A_retires_old_without_losing_B']=bool(second) and all(r['valid'] and r['x']==650 and r['old']==0 for r in second)
    if route==8:
        checks['twin_A_has_own_identity_at_B']=any(r['valid'] and r['live'] and r['x']==320 for r in at('Lab.TwinA',9,20))
        checks['twin_B_has_own_identity_at_A']=any(r['valid'] and r['live'] and r['x']==180 for r in at('Lab.TwinB',9,20))
    if route==9:
        checks['destroyed_box_retained_unseen']=all(r['valid'] for r in at('Lab.DestroyBox',5,7))
        checks['destroyed_box_cleared_on_inspection']=any(not r['valid'] for r in at('Lab.DestroyBox',9,20))
    if route==10:
        checks['replacement_new_identity_hidden_unseen']=all(not r['valid'] and not r['live'] for r in at('Lab.ReplaceNew',5,7))
        checks['replacement_new_identity_recognized']=any(r['valid'] and r['live'] for r in at('Lab.ReplaceNew',9,20))
        checks['replacement_old_identity_cleared']=any(not r['valid'] for r in at('Lab.ReplaceOld',9,20))
    checks['completed_and_no_authority_failures']='LAB_CAPTURE_COMPLETE' in text and 'LAB_CONTRACT_FAIL' not in text
    result=dict(run=path.stem,checks=checks,passed=all(checks.values()))
    results.append(result)
    print(('PASS ' if result['passed'] else 'FAIL ')+path.stem+' '+str(checks))
(root/(a.label+'_contracts.json')).write_text(json.dumps(results,indent=2),encoding='utf-8')
if not results or not all(r['passed'] for r in results):
    raise SystemExit(1)

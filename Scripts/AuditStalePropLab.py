"""Keep a full error/warning inventory and separately fail on severe gameplay/render errors."""
import argparse,json,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--label',required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'Saved/PropGameplayLab'
logs=sorted(root.glob(a.label+'_????_M?_?.log'));assert logs
result=[];failure=False
for path in logs:
    text=path.read_text(encoding='utf-8-sig',errors='replace')
    lines=text.splitlines()
    errors=[x for x in lines if ': Error:' in x]
    warnings=[x for x in lines if ': Warning:' in x]
    severe=[x for x in lines if re.search(r'STALE_FAIL|Fatal error:|Assertion failed:|Ensure condition failed:|GPU crashed|DXGI_ERROR_DEVICE|Failed to compile Material|Ran out of memory|Insufficient Storage',x)]
    # Only classify known startup traces, never blanket-ignore Python failures.
    # Start() can run during map initialization, before engine Python startup.
    # The first recorded gameplay frame is the conservative classification boundary.
    startup=text.split('STALE_FRAME',1)[0]
    classified=set(x for x in startup.splitlines() if x.endswith('LogAutomationTest: Error: Condition failed'))
    blocks=re.findall(r'(?:[^\n]*LogPython: Error:[^\n]*\n)+',startup)
    for block in blocks:
        if ('Engine/Plugins/Experimental/' in block.replace('\\','/') and
            re.search(r"AttributeError: module 'unreal' has no attribute '(ToolsetDefinition|AgentSkill|AgentSkillToolsetDefinition|PythonTestRunner)'",block)):
            classified.update(block.splitlines())
    unclassified=[x for x in errors if x not in classified]
    failure|=bool(severe or unclassified or 'STALE_CAPTURE_COMPLETE' not in text)
    result.append({'log':path.name,'complete':'STALE_CAPTURE_COMPLETE' in text,'error_count':len(errors),'warning_count':len(warnings),'severe':severe,'unclassified_errors':unclassified,'errors':errors,'warnings':warnings})
out=root/(a.label+'_Review');out.mkdir(exist_ok=True)
(out/'log_inventory.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({'runs':len(result),'errors_retained':sum(x['error_count'] for x in result),'warnings_retained':sum(x['warning_count'] for x in result),'severe':sum(len(x['severe']) for x in result),'unclassified':sum(len(x['unclassified_errors']) for x in result)},indent=2))
raise SystemExit(1 if failure else 0)

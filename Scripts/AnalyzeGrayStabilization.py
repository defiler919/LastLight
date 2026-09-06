"""Analyze every recorded frame, retaining invalid runs and setup/startup separately.

Usage: python Scripts/AnalyzeGrayStabilization.py Saved/Stabilization/Run [Run ...]
No outlier removal. Global engine timers are asynchronous; no additive attribution.
"""
import argparse
import json
import statistics
from collections import defaultdict
from pathlib import Path


def distribution(values):
    ordered = sorted(values)
    if not ordered:
        return None
    result = {f'p{p}': ordered[min(len(ordered)-1, int(len(ordered)*p/100))] for p in (50, 95, 99)}
    result.update(max=max(ordered), count=len(ordered))
    return result


def slow_frames(values):
    streak = maximum = 0
    for value in values:
        streak = streak+1 if value > 33 else 0
        maximum = max(maximum, streak)
    return dict(over_33=sum(v > 33 for v in values), over_100=sum(v > 100 for v in values), longest_over_33=maximum)


def analyze(path):
    rows = [json.loads(line) for line in (path/'frames.jsonl').read_text(encoding='utf-8').splitlines()]
    env = json.loads((path/'environment.json').read_text(encoding='utf-8-sig'))
    summary = json.loads((path/'summary.json').read_text(encoding='utf-8-sig'))
    cases = {c['case']: c for c in json.loads((path/'performance.json').read_text())['cases']}
    setup = {name: c['setup_ms'] for name,c in cases.items()}
    groups = defaultdict(list)
    for row in rows:
        groups[row['case']].append(row)
    invalid = []
    for row in rows:
        e = row['engine']
        if not (e['viewport'] == [1920, 1080] and e['screen_percentage'] == 100 and e['secondary_percentage'] == 100
                and e['aa'] == 4 and e['dynamic_resolution'] == 0 and e['vsync'] == 0 and e['max_fps'] == 0
                and e['fixed_step'] == 0 and e['foreground'] == 1 and e['minimized'] == 0 and e['editor_realtime_count'] == 0):
            invalid.append(dict(case=row['case'], index=row['index'], environment=e))
    result = dict(run=path.name, sha=env['sha'], mode=env['mode'], trace=env['trace'], protocol=env['protocol'],
                  summary=summary, invalid_environment_frames=len(invalid), invalid_examples=invalid[:4], cases={})
    result['valid_normal_sample'] = not invalid and not env['trace'] and env['protocol'] != 'Attribution' and summary['complete'] and summary['exit_code'] == 0 and summary['severe_lines'] == 0
    for name, items in groups.items():
        wall = [r['wall_ms'] for r in items]
        resources = {}
        for key in ('records', 'proxies', 'caps', 'textures', 'mids', 'fine_bytes', 'working_set', 'uobjects'):
            resources[key] = dict(first=items[0][key], last=items[-1][key], minimum=min(r[key] for r in items), maximum=max(r[key] for r in items))
        result['cases'][name] = dict(setup_ms=setup.get(name), wall_ms=distribution(wall), slow=slow_frames(wall),
            setup_resources=cases.get(name,{}).get('setup_resources'),
            steady_after_90_ms=distribution(wall[90:]), memory_ms=distribution([r['game_thread_us']/1000 for r in items]),
            engine_ms={key: distribution([r['engine'][key] for r in items]) for key in ('game_ms','render_ms','rhi_ms','gpu_ms','game_wait_ms','render_wait_ms','present_ms')},
            stages_us={key: distribution([r[key] for r in items]) for key in ('current_reveal_us','historical_us','coverage_us','occupancy_us','ownership_us','texture_us','cap_us','refresh_us')},
            resources=resources)
        if 'rhi_texture_bytes' in items[0]['engine']:
            resources['rhi_texture_bytes'] = dict(first=items[0]['engine']['rhi_texture_bytes'],last=items[-1]['engine']['rhi_texture_bytes'],maximum=max(r['engine']['rhi_texture_bytes'] for r in items))
    startup_path = path/'startup-frames.json'
    if startup_path.exists():
        startup = json.loads(startup_path.read_text())
        result['startup'] = {phase: distribution([r['wall_ms'] for r in startup if r.get('phase','initial_frames') == phase]) for phase in ('waiting_for_foreground','initial_frames')}
    result['all_frames'] = dict(wall_ms=distribution([r['wall_ms'] for r in rows]), slow=slow_frames([r['wall_ms'] for r in rows]))
    (path/'analysis.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    reports = [analyze(path) for path in args.runs]
    for report in reports:
        print(report['run'], 'valid=', report['valid_normal_sample'], 'invalid_frames=', report['invalid_environment_frames'])
        for name, case in report['cases'].items():
            w = case['wall_ms']
            print(f"  {name:24} wall p50/95/99/max {w['p50']:.3f}/{w['p95']:.3f}/{w['p99']:.3f}/{w['max']:.3f} setup={case['setup_ms']:.3f} memory95={case['memory_ms']['p95']:.3f} records={case['resources']['records']['maximum']} slow={case['slow']}")
    if len(reports) > 1:
        groups = defaultdict(list)
        for report in reports:
            if report['valid_normal_sample']:
                for name, case in report['cases'].items():
                    groups[(report['mode'], report['protocol'], name)].append(case['wall_ms']['p95'])
        print('All valid samples, grouped by mode/protocol/case (caller must separately check identical settings/source):')
        for key, values in groups.items():
            print(key, 'n=',len(values), 'p95 median/min/max=',statistics.median(values),min(values),max(values))

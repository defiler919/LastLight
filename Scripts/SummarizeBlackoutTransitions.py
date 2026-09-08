"""Summarize opt-in native Blackout overlap evidence, including failed runs.

The 100 ms hard long-frame ceiling is the existing SightWeave complete-step
ceiling, not a claim that the game meets a 16.6 ms frame budget. Raw wall frames
include renderer work and logging. Never substitutes callback time for frames.
"""
import argparse
import csv
import json
import re
from pathlib import Path


def stats(values):
    values = sorted(values)
    if not values:
        return None
    def percentile(q):
        return values[round((len(values) - 1) * q)]
    return dict(count=len(values), min=min(values), p50=percentile(.5),
                p95=percentile(.95), p99=percentile(.99), max=max(values))


def summarize(root):
    text = (root / 'game.log').read_text(encoding='utf-8-sig', errors='replace')
    timing = re.compile(r'BLACKOUT_TIMING frame=(\d+) root=(\w+) stage=(\w+) calls=(\d+) inclusive_ms=([\d.]+) exclusive_ms=([\d.]+)')
    edge = re.compile(r'BLACKOUT_GAME_EDGE cycle=(\d+) phase=(\d+) move_ms=([\d.]+) active=(\d+) started=(\d+)')
    contexts, edges, frame = {}, [], None
    for line in text.splitlines():
        match = timing.search(line)
        if match:
            frame, source, stage, count, inclusive, exclusive = match.groups()
            contexts.setdefault((frame, source), {})[stage] = dict(calls=int(count), inclusive_ms=float(inclusive), exclusive_ms=float(exclusive))
        match = edge.search(line)
        if match:
            cycle, phase, move, active, started = match.groups()
            source = 'Enter' if int(phase) == 0 else 'Leave'
            edges.append(dict(cycle=int(cycle), phase=int(phase), move_ms=float(move), active=int(active), started=int(started),
                              engine_frame=frame, stages=contexts.get((frame, source), {})))
    path = root / 'game_frames.csv'
    rows = list(csv.DictReader(path.open(encoding='utf-8-sig', newline=''))) if path.exists() else []
    measured = [r for r in rows if int(r['frame']) >= 300]
    result = dict(run=root.name, complete='BLACKOUT_GAME_PROBE_COMPLETE cycles=20' in text,
                  invalid='BLACKOUT_GAME_PROBE_INVALID' in text,
                  lifecycle_warning=bool(re.search(r'Warning:.*SpawnActor|SpawnActor.*Warning', text)),
                  hard_long_frame_ceiling_ms=100, edges=edges)
    for label, phase, callback, event in [('enter', 0, 'Enter', 'BeginEvent'), ('leave', 60, 'Leave', 'EndEvent')]:
        selected = [e for e in edges if e['phase'] == phase]
        windows = [float(r['wall_ms']) for r in measured if phase <= int(r['phase']) <= phase + 4]
        result[label] = dict(transitions=len(selected), cycles=[e['cycle'] for e in selected],
                            callback_calls=sum(e['stages'].get(callback, {}).get('calls', 0) for e in selected),
                            event_calls=sum(e['stages'].get(event, {}).get('calls', 0) for e in selected),
                            event_ms=stats([e['stages'][event]['inclusive_ms'] for e in selected if event in e['stages']]),
                            move_ms=stats([e['move_ms'] for e in selected]), window_frame_ms=stats(windows))
    result['all_measured_frame_ms'] = stats([float(r['wall_ms']) for r in measured])
    result['non_edge_frame_ms'] = stats([float(r['wall_ms']) for r in measured if not (0 <= int(r['phase']) <= 4 or 60 <= int(r['phase']) <= 64)])
    result['state_errors'] = sum((int(r['active']) != int(int(r['phase']) < 60) or int(r['started']) != int(int(r['phase']) < 60)) for r in measured)
    result['edge_state_errors'] = sum(e['active'] != int(e['phase'] == 0) or e['started'] != int(e['phase'] == 0) for e in edges)
    result['passed'] = result['complete'] and not result['invalid'] and not result['lifecycle_warning'] and not result['state_errors'] and not result['edge_state_errors'] and all(
        result[k]['cycles'] == list(range(20)) and result[k]['event_calls'] == 20 and result[k]['callback_calls'] == 20
        and result[k]['window_frame_ms'] and result[k]['window_frame_ms']['max'] <= 100 for k in ('enter', 'leave'))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    result = summarize(args.run_directory)
    (args.run_directory / 'transition-summary.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'edges'}, indent=2))
    raise SystemExit(0 if result['passed'] else 1)

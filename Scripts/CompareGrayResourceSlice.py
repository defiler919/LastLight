"""Compare normal, same-binary resource allocation A/B runs without trimming frames.

Run AnalyzeGrayStabilization.py first, then pass the run directories here.
Only duplicate per-part MIDs may differ; legal resource/evidence counts must match.
"""
import argparse
import hashlib
import json
import statistics
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def compare(paths):
    runs = []
    evidence_keys = ('records', 'fine_bytes', 'proxies', 'caps', 'textures',
                     'occupancy_tests', 'geometry_tests', 'occupancy_hits',
                     'resident_samples', 'samples_scanned')
    for path in paths:
        env, analysis = read(path/'environment.json'), read(path/'analysis.json')
        rows = [json.loads(line) for line in (path/'frames.jsonl').read_text().splitlines()]
        assert analysis['valid_normal_sample'] and not analysis['invalid_environment_frames'], path
        assert len(rows) == 480 and env['mode'] == 'Standalone' and env['protocol'] == 'Batch', path
        assert not any(env[k] for k in ('serial_occupancy', 'serial_cap_build',
                                      'serial_sealed_ownership', 'legacy_capture_preparation')), path
        run = dict(name=path.name, sha=env['sha'], legacy=env['legacy_record_resources'],
                   binary=env['binary_sha256'], editor_binary=env['editor_binary_sha256'],
                   driver=env['driver_sha256'], config=hashlib.sha256((path/'DefaultEngine.ini').read_bytes()).hexdigest(),
                   quality={k:v for k,v in read(path/'quality.json').items() if k != 'initial'},
                   disabled_plugins=env['disabled_authoring_plugins'], frames=len(rows), cases={})
        for name, case in analysis['cases'].items():
            selected = [r for r in rows if r['case'] == name]
            first = selected[0]
            run['cases'][name] = dict(setup_ms=case['setup_ms'], max_ms=case['wall_ms']['max'],
                p95_ms=case['wall_ms']['p95'], over100=case['slow']['over_100'],
                native_ms=first['game_thread_us']/1000, texture_ms=first['texture_us']/1000,
                cap_ms=first['cap_us']/1000, occupancy_ms=first['occupancy_us']/1000,
                setup_resources=case['setup_resources'],
                first={k:first[k] for k in (*evidence_keys, 'mids')},
                last={k:selected[-1][k] for k in ('records', 'fine_bytes', 'proxies', 'caps', 'textures', 'mids')},
                resources=case['resources'])
        runs.append(run)
    for key in ('binary', 'editor_binary', 'driver', 'config', 'quality', 'disabled_plugins'):
        assert all(r[key] == runs[0][key] for r in runs), key
    for name in runs[0]['cases']:
        for run in runs:
            case, reference = run['cases'][name], runs[0]['cases'][name]
            assert case['setup_resources'] == reference['setup_resources'], (run['name'], name, 'setup')
            for key in evidence_keys:
                assert case['first'][key] == reference['first'][key], (run['name'], name, key)
            for key in ('records', 'fine_bytes', 'proxies', 'caps', 'textures'):
                assert case['last'][key] == reference['last'][key], (run['name'], name, key, 'last')
            expected = case['first']['records'] * (3 if run['legacy'] else 1)
            assert case['first']['mids'] == expected, (run['name'], name, 'record-local MID count')
    medians = {}
    for legacy in (True, False):
        group = [r for r in runs if r['legacy'] == legacy]
        assert group, 'Both allocation paths are required'
        medians['legacy' if legacy else 'record_scoped'] = {
            name: {key:statistics.median(r['cases'][name][key] for r in group)
                   for key in ('setup_ms', 'max_ms', 'native_ms', 'texture_ms', 'cap_ms', 'occupancy_ms')}
            for name in runs[0]['cases']}
    return dict(runs=runs, medians=medians)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runs', nargs='+', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result = compare(args.runs)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result['medians'], indent=2))

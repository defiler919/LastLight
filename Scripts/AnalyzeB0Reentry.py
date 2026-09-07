"""Summarize fixed-16 B0 GT attribution. Never add overlapping RT/GPU counters to GT."""
import argparse
import json
import statistics as st
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('runs', nargs='+', type=Path)
parser.add_argument('--output', type=Path, required=True)
a = parser.parse_args()
results = []
for path in a.runs:
    env = json.loads((path/'environment.json').read_text(encoding='utf-8-sig'))
    perf = json.loads((path/'performance.json').read_text(encoding='utf-8-sig'))
    rows = [json.loads(x) for x in (path/'frames.jsonl').read_text().splitlines()]
    assert perf['foreground_bad'] == 0
    assert perf['initial_hash'] == perf['final_hash']
    batches = []
    previous_rebuilds = previous_uploads = previous_caps = 0
    for row in rows:
        r = row['residency']
        assert r['missing'] == r['failures'] == 0
        b = r.get('b0', {})
        if row['phase'] == 'creation_and_pin' or not b:
            continue
        assert b['records'] == 16 and b['whole'] == 8, b
        assert r['rebuilds'] - previous_rebuilds == 16
        assert r['rebuild_uploads'] - previous_uploads == 16
        assert r['rebuild_caps'] - previous_caps == 8
        assert b['queued'] == (24 if env['b0_mode'] == 2 else 0)
        previous_rebuilds, previous_uploads, previous_caps = r['rebuilds'], r['rebuild_uploads'], r['rebuild_caps']
        assert len(b['record_costs']) == 16
        c = b['costs']
        for k in c:
            assert abs(c[k] - sum(x[k] for x in b['record_costs'])) < .00003
        cpu = sum(c[k] for k in ['pixels','float16','signatures','cap_cpu'])
        objects = sum(c[k] for k in ['texture_object','cap_object','proxy_object','mesh_object','mid_object'])
        submit = sum(c[k] for k in ['register','material','texture_submit','cap_submit','visibility']) + b['flush_ms']
        residual = b['batch_ms'] - sum(c.values()) - b['flush_ms']
        assert residual >= -.001
        batches.append(dict(phase=row['phase'], index=row['index'], wall_ms=row['wall_ms'],
                            batch_ms=b['batch_ms'], cpu_ms=cpu, objects_ms=objects, submit_ms=submit,
                            other_ms=c['other'], outside_records_ms=residual,
                            costs=c, flush_ms=b['flush_ms'], queued=b['queued']))
    assert {b['phase'] for b in batches} >= {'boundary','turn180','teleport'}
    assert len(batches) in (3,4), len(batches)
    results.append(dict(run=path.name, dll=env['binary_sha256'], mode=env['b0_mode'],
                        evidence_hash=perf['final_hash'], max_route_ms=perf['max_aged_route_ms'],
                        batches=batches))
assert len({r['dll'] for r in results}) == 1, 'Do not combine DLLs'
assert len({r['evidence_hash'] for r in results}) == 1, 'CPU oracle mismatch'
summary = dict(runs=results, by_mode={})
for mode in sorted({r['mode'] for r in results}):
    runs = [r for r in results if r['mode'] == mode]
    bs = [b for r in runs for b in r['batches']]
    summary['by_mode'][mode] = dict(n_runs=len(runs), n_batches=len(bs),
        route_peaks=[r['max_route_ms'] for r in runs],
        median={k:st.median(b[k] for b in bs) for k in ['wall_ms','batch_ms','cpu_ms','objects_ms','submit_ms','other_ms','outside_records_ms','flush_ms']},
        cost_medians={k:st.median(b['costs'][k] for b in bs) for k in bs[0]['costs']})
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps(summary, indent=2), encoding='utf-8')
print(json.dumps(summary['by_mode'], indent=2))

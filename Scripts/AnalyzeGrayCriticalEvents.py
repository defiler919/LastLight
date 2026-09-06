"""Attribute selected Insights events by real thread ID, never exporter filename.

Run after ExportGrayTrace.ps1. Region boundaries come from its original log.
Inclusive scopes and asynchronous queues overlap; this is not an additive frame
budget or a reconstruction of causal dependencies between CPU and GPU frames.
"""
import argparse
import csv
import json
import re
from collections import defaultdict
from pathlib import Path


def read_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def percentile(values, q):
    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * q
    low = int(position)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def analyze(directory):
    threads = {int(row["Id"]): row for row in read_csv(directory / "threads.csv")}
    timers = {int(row["Id"]): row for row in read_csv(directory / "timers.csv")}
    regions = {}
    for name, start, end in re.findall(
        r"Exporting timing statistics for region '([^']+)' \[([\d.]+) \.\. ([\d.]+)\]",
        (directory / "insights.log").read_text(encoding="utf-8-sig"),
    ):
        bounds = (float(start), float(end))
        if name in regions and regions[name] != bounds:
            raise ValueError(f"Repeated region with different bounds: {name}")
        regions[name] = bounds
    if not regions:
        raise ValueError("No explicit region boundaries found")
    groups = defaultdict(lambda: {"clipped_total_ms": 0.0, "overlapping_events": 0,
                                  "boundary_events": 0, "complete_ms": []})
    for event in read_csv(directory / "events_critical_all_threads.csv"):
        thread_id, timer_id = int(event["ThreadId"]), int(event["TimerId"])
        # Missing identities must fail instead of guessing a queue from a name.
        threads[thread_id]
        timers[timer_id]
        start, end = float(event["StartTime"]), float(event["EndTime"])
        if end < start:
            raise ValueError("Negative event duration")
        for name, (region_start, region_end) in regions.items():
            overlap = min(end, region_end) - max(start, region_start)
            if overlap <= 0:
                continue
            group = groups[name, thread_id, timer_id]
            group["clipped_total_ms"] += overlap * 1000
            group["overlapping_events"] += 1
            if start >= region_start and end <= region_end:
                group["complete_ms"].append((end - start) * 1000)
            else:
                group["boundary_events"] += 1
    result = {"schema": 1, "run": directory.name,
              "percentile_method": "Linear interpolation at (n-1)*q on fully contained events; wall analyzer uses order statistics instead.",
              "note": "Inclusive selected events only. Queues and nested scopes overlap; do not add them. Totals clip to region boundaries; means/quantiles use fully contained events. Events are not matched to wall sample frame IDs.",
              "regions": {name: {"start_seconds": bounds[0], "end_seconds": bounds[1]}
                          for name, bounds in regions.items()}, "statistics": []}
    for (region, thread_id, timer_id), group in sorted(groups.items()):
        values = group.pop("complete_ms")
        result["statistics"].append({
            "region": region, "thread_id": thread_id, "thread": threads[thread_id]["Name"],
            "timer_id": timer_id, "timer_type": timers[timer_id]["Type"],
            "timer": timers[timer_id]["Name"], **group,
            "complete_events": len(values),
            "mean_ms": sum(values) / len(values) if values else None,
            "p50_ms": percentile(values, .5), "p95_ms": percentile(values, .95),
            "p99_ms": percentile(values, .99), "max_ms": max(values) if values else None,
        })
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    report = analyze(args.directory)
    output = args.directory / "critical-events-all-threads-analysis.json"
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{output}: {len(report['statistics'])} thread/timer/region groups")

"""Export measured audit cases and per-minute real long-run resource evidence.

Inputs must already have AnalyzeGrayStabilization.py reports. Output paths must
not exist. This preserves raw evidence and does not infer leak freedom or PASS
from stable object counts. No outlier or cold-frame removal.
"""
import argparse
import csv
import hashlib
import json
from collections import defaultdict
from pathlib import Path

from AnalyzeGrayStabilization import distribution, slow_frames


RESOURCE_KEYS = ("records", "proxies", "textures", "mids", "caps", "fine_bytes",
                 "working_set", "uobjects")


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def long_resources(directory, analysis):
    groups = defaultdict(list)
    for line in (directory / "frames.jsonl").read_text(encoding="utf-8").splitlines():
        row = json.loads(line)
        if row["case"] == "LongInteraction":
            groups[int(row["elapsed_seconds"] // 60)].append(row)
    if not groups:
        raise ValueError(f"No LongInteraction rows: {directory}")
    minutes = []
    for minute, rows in sorted(groups.items()):
        minutes.append({"minute": minute, "first_elapsed_seconds": rows[0]["elapsed_seconds"],
                        "last_elapsed_seconds": rows[-1]["elapsed_seconds"],
                        "wall_ms": distribution([r["wall_ms"] for r in rows]),
                        "slow": slow_frames([r["wall_ms"] for r in rows]),
                        "resources": {key: {"first": rows[0][key], "last": rows[-1][key],
                                            "minimum": min(r[key] for r in rows),
                                            "maximum": max(r[key] for r in rows)}
                                      for key in RESOURCE_KEYS},
                        "rhi_texture_bytes": {"first": rows[0]["engine"]["rhi_texture_bytes"],
                                              "last": rows[-1]["engine"]["rhi_texture_bytes"],
                                              "maximum": max(r["engine"]["rhi_texture_bytes"] for r in rows)}})
    events = [json.loads(line) for line in (directory / "long-events.jsonl").read_text().splitlines()]
    return {"schema": 1, "run": directory.name,
            "wall_span_seconds": minutes[-1]["last_elapsed_seconds"] - minutes[0]["first_elapsed_seconds"],
            "long_case": analysis["cases"]["LongInteraction"], "minutes": minutes,
            "event_count": len(events), "explicit_resets": sum(bool(e["reset"]) for e in events),
            "motion_starts": sum(bool(e["motion"]) for e in events),
            "entry_reset": read_json(directory / "long-entry-reset.json"),
            "cleanup": read_json(directory / "long-cleanup.json"),
            "note": "Minute buckets use frames.jsonl elapsed_seconds (driver start), while long-events.jsonl uses route start. WS includes Python sample dictionaries until explicit cleanup. RHI texture bytes are device statistics, not total VRAM. Legitimate history growth and explicit room resets must not be called leaks or natural forgetting."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directories", nargs="+", type=Path)
    parser.add_argument("--csv", required=True, type=Path)
    args = parser.parse_args()
    output_rows, long_reports = [], []
    for directory in args.directories:
        analysis = read_json(directory / "analysis.json")
        environment = read_json(directory / "environment.json")
        quality = read_json(directory / "quality.json")
        # Exclude protocol, frame counters and timings from quality identity.
        settings = {key: value for key, value in quality.items() if key.startswith(("sg.", "r.", "t.", "Slate."))}
        settings_hash = hashlib.sha256(json.dumps(settings, sort_keys=True).encode()).hexdigest()
        for name, case in analysis["cases"].items():
            row = {"run": analysis["run"], "sha": analysis["sha"],
                   "binary_sha256": environment["binary_sha256"],
                   "driver_sha256": environment["driver_sha256"], "quality_sha256": settings_hash,
                   "mode": analysis["mode"], "protocol": analysis["protocol"],
                   "trace": analysis["trace"], "valid_normal_sample": analysis["valid_normal_sample"],
                   "invalid_environment_frames": analysis["invalid_environment_frames"], "case": name,
                   "setup_ms": case["setup_ms"], "setup_records": case["setup_resources"]["records"]}
            row.update({"wall_" + key: value for key, value in case["wall_ms"].items()})
            row.update(case["slow"])
            row.update({"memory_" + key: value for key, value in case["memory_ms"].items()})
            for key, stats in case["resources"].items():
                for statistic in ("first", "last", "maximum"):
                    row[key + "_" + statistic] = stats[statistic]
            output_rows.append(row)
        if analysis["protocol"] == "LongRun":
            long_reports.append((directory / "long-resource-trends.json", long_resources(directory, analysis)))
    for path in [args.csv] + [path for path, _ in long_reports]:
        if path.exists():
            raise FileExistsError(f"Evidence exists: {path}")
    with args.csv.open("x", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(output_rows[0]))
        writer.writeheader()
        writer.writerows(output_rows)
    for path, report in long_reports:
        with path.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
        print(path)
    print(f"{args.csv}: {len(output_rows)} cases")


if __name__ == "__main__":
    main()

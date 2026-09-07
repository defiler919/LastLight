"""Summarize fixed WholePreparation windows without dropping Current/slow frames.

Usage: python Scripts/CompareWholePreparationBudget.py Saved/Stabilization/RUN ...
Raw evidence remains local in Saved. Timers are nested, not additive attribution.
"""
import json
import sys
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def summarize(folder):
    rows = [json.loads(line) for line in (folder / "frames.jsonl").read_text(encoding="utf-8-sig").splitlines()]
    env = read(folder / "environment.json")
    startup = read(folder / "startup-frames.json")
    current = next(r for r in rows if r["index"] == 6)
    seal = next(r for r in rows if r["index"] == 66)
    prep = [r["preparation"] for r in rows]
    peak = max(rows, key=lambda r: r["wall_ms"])
    return dict(
        run=folder.name, binary=env["binary_sha256"], driver=env["driver_sha256"],
        foreground_waits=sum(r.get("phase") == "waiting_for_foreground" for r in startup),
        bad_foreground=sum(r["engine"]["foreground"] != 1 for r in rows),
        frames=len(rows), max_full_ms=peak["wall_ms"], peak_index=peak["index"],
        current_full_ms=current["wall_ms"], current_native_ms=current["game_thread_us"] / 1000,
        seal_full_ms=seal["wall_ms"], seal_native_ms=seal["game_thread_us"] / 1000,
        prep_max_frame_ms=max(p["frame_ms"] for p in prep),
        prep_max_chunk_ms=max(p["max_step_ms"] for p in prep),
        prep_overrun_ms=max(0, max(p["frame_ms"] for p in prep) - 1),
        prep_overrun_frames=sum(p["frame_ms"] > 1 for p in prep),
        prep_sum_ms=sum(p["frame_ms"] for p in prep),
        work=sum(p["frame_work"] for p in prep), bytes_peak=max(p["bytes"] for p in prep),
        ready_index=next((r["index"] for r in rows if r["preparation"]["ready"]), None),
        current_parts=current["preparation"], last=prep[-1],
        outcomes={key: rows[-1][key] for key in ("records", "proxies", "caps", "textures", "mids", "fine_bytes")},
        phase_max_ms={key: max(p[key] for p in prep) for key in
                      ("request_ms", "snapshot_ms", "admission_ms", "step_ms", "cancel_ms", "take_ms")},
    )


if __name__ == "__main__":
    print(json.dumps([summarize(Path(arg)) for arg in sys.argv[1:]], indent=2))

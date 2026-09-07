"""Full resource-admission window, including index1 initialization; never trim peaks."""
import json
import sys
from pathlib import Path


def summarize(folder):
    def read(name):
        return json.loads((folder / name).read_text(encoding="utf-8-sig"))
    rows = [json.loads(s) for s in (folder / "frames.jsonl").read_text().splitlines()]
    assert len(rows) == 180
    assert rows[0]["history_parent"]["object_loaded"] == 0
    env = read("environment.json")
    peak = max(rows, key=lambda r: r["wall_ms"])
    return dict(run=folder.name, binary=env["binary_sha256"], driver=env["driver_sha256"],
                waits=sum(r.get("phase") == "waiting_for_foreground" for r in read("startup-frames.json")),
                bad_foreground=sum(r["engine"]["foreground"] != 1 for r in rows),
                max_full_ms=peak["wall_ms"], peak_index=peak["index"],
                init_frame_ms=rows[1]["wall_ms"], current_ms=rows[6]["wall_ms"], seal_ms=rows[66]["wall_ms"],
                current_parts=rows[6]["preparation"], parent=rows[-1]["history_parent"],
                outcomes={k: rows[-1][k] for k in ("records", "proxies", "textures", "mids", "caps", "fine_bytes")})


if __name__ == "__main__":
    print(json.dumps([summarize(Path(p)) for p in sys.argv[1:]], indent=2))

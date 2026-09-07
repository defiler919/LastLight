"""Bounded runner/engine handshake. Used only by real-window performance drivers."""
import json
import time


def write_state(root, name, value):
    temporary = root / (name + '.tmp')
    temporary.write_text(json.dumps(value), encoding='utf-8')
    # Startup-only IPC can briefly collide with Windows metadata readers or
    # scanners. Preserve atomic publication, bound retries, and fail explicitly.
    for attempt in range(8):
        try:
            temporary.replace(root / name)
            return
        except PermissionError:
            if attempt == 7:
                raise
            time.sleep(0.005)


def require_foreground(root, engine):
    if not engine.get('foreground') or engine.get('minimized'):
        write_state(root, 'foreground-lost.json', engine)
        raise RuntimeError('Foreground lost: run invalid; no refocusing during measurement')


def await_foreground(root, environment, timeout=45, stable_frames=12):
    deadline = time.perf_counter() + timeout
    consecutive = 0
    previous_frame = None
    write_state(root, 'viewport-ready.json', environment())
    while True:
        if (root / 'foreground-abort.json').exists():
            raise RuntimeError('Runner aborted foreground acquisition; see foreground-abort.json')
        if time.perf_counter() >= deadline:
            raise RuntimeError('Bounded engine foreground handshake timed out')
        engine = environment()
        if engine.get('foreground') and not engine.get('minimized'):
            if engine['frame'] != previous_frame:
                consecutive += 1
        else:
            consecutive = 0
        previous_frame = engine['frame']
        write_state(root, 'foreground-live.json', dict(engine=engine, consecutive=consecutive,
                                                     ready=consecutive >= stable_frames))
        if (root / 'foreground-approved.json').exists():
            require_foreground(root, engine)
            if consecutive < stable_frames:
                raise RuntimeError('Foreground changed during approval; run invalid')
            write_state(root, 'foreground-confirmed.json', dict(engine=engine, consecutive=consecutive))
            return
        yield 1

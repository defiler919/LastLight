"""Ten real wall-clock minutes of D3D12 overlap/distributed gray-history pressure."""

import json
import os
import statistics
import time
import traceback
from pathlib import Path

import unreal


OUTPUT = Path(os.environ["DARKWELL_GRAY_POLICY_TEN_MIN_OUTPUT"]).resolve()
OUTPUT.mkdir(parents=True, exist_ok=True)
MAP = "/Game/Maps/L_SightWeaveGrayPolicyLab"
PHASE_SECONDS = 120.0
PHASES = (
    ("Overlap64_A", 4),
    ("Distributed184_A", 6),
    ("Overlap64_B", 4),
    ("Distributed184_B", 6),
    ("Overlap64_C", 4),
)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level(MAP)
driver_started = time.monotonic()


def game_world():
    return editor.get_game_world()


def resolve_player(world):
    player = unreal.GameplayStatics.get_player_character(world, 0)
    if player is None:
        players = unreal.GameplayStatics.get_all_actors_of_class(
            world, unreal.DarkwellCharacter
        )
        assert len(players) == 1, players
        player = players[0]
    return player


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(len(ordered) * fraction))]


def longest_over(values, threshold):
    longest = 0
    current = 0
    for value in values:
        current = current + 1 if value > threshold else 0
        longest = max(longest, current)
    return longest


def summarize(name, mode, samples, wall_seconds, setup_ms):
    assert samples, name
    frame_ms = [sample["frame_ms"] for sample in samples]
    game_frame_ms = [sample["game_frame_ms"] for sample in samples]
    room_ms = [sample["game_thread_us"] / 1000.0 for sample in samples]
    stage_names = (
        "current_reveal_us",
        "candidate_us",
        "historical_us",
        "coverage_us",
        "occupancy_us",
        "ownership_us",
        "texture_us",
        "cap_us",
        "refresh_us",
        "occupancy_snapshot_us",
    )
    return {
        "case": name,
        "mode": mode,
        "samples": len(samples),
        "wall_seconds": wall_seconds,
        "setup_ms": setup_ms,
        "fps_mean": len(samples) / wall_seconds,
        "frame_ms": {
            "p50": percentile(frame_ms, 0.50),
            "p95": percentile(frame_ms, 0.95),
            "p99": percentile(frame_ms, 0.99),
            "peak": max(frame_ms),
            "over33": sum(value > 33.0 for value in frame_ms),
            "over50": sum(value > 50.0 for value in frame_ms),
            "over100": sum(value > 100.0 for value in frame_ms),
            "longest_over33": longest_over(frame_ms, 33.0),
        },
        "game_frame_ms": {
            "p50": percentile(game_frame_ms, 0.50),
            "p95": percentile(game_frame_ms, 0.95),
            "p99": percentile(game_frame_ms, 0.99),
            "peak": max(game_frame_ms),
        },
        "room_ms": {
            "p50": percentile(room_ms, 0.50),
            "p95": percentile(room_ms, 0.95),
            "p99": percentile(room_ms, 0.99),
            "peak": max(room_ms),
        },
        "history": {
            "records_max": max(sample["records"] for sample in samples),
            "active_p50": percentile([sample["epochs"] for sample in samples], 0.50),
            "active_p95": percentile([sample["epochs"] for sample in samples], 0.95),
            "active_max": max(sample["epochs"] for sample in samples),
            "candidates_p50": percentile([sample["candidates"] for sample in samples], 0.50),
            "candidates_p95": percentile([sample["candidates"] for sample in samples], 0.95),
            "candidates_max": max(sample["candidates"] for sample in samples),
            "sleeping_p50": percentile([sample["sleeping"] for sample in samples], 0.50),
            "sleeping_p95": percentile([sample["sleeping"] for sample in samples], 0.95),
            "sleeping_max": max(sample["sleeping"] for sample in samples),
        },
        "resources": {
            "fine_bytes_max": max(sample["fine_bytes"] for sample in samples),
            "textures_max": max(sample["textures"] for sample in samples),
            "caps_max": max(sample["caps"] for sample in samples),
            "mids_max": max(sample["mids"] for sample in samples),
            "uobjects_max": max(sample["uobjects"] for sample in samples),
            "working_set_max": max(sample["working_set"] for sample in samples),
        },
        "work_per_frame_mean": {
            key: statistics.fmean(sample[key] for sample in samples)
            for key in (
                "coverage_queries",
                "occupancy_tests",
                "ownership_tests",
                "samples_scanned",
                "texture_uploads",
                "cap_rebuilds",
            )
        },
        "stages_us_mean": {
            key: statistics.fmean(sample[key] for sample in samples)
            for key in stage_names
        },
        "stages_us_peak": {
            key: max(sample[key] for sample in samples) for key in stage_names
        },
    }


def run():
    levels.editor_request_begin_play()
    yield 180
    world = game_world()
    assert world is not None, "PIE did not start"
    directors = unreal.GameplayStatics.get_all_actors_of_class(
        world, unreal.DarkwellSightWeaveGrayPolicyLabDirector
    )
    rooms = unreal.GameplayStatics.get_all_actors_of_class(
        world, unreal.DarkwellMovingPropLabRoom
    )
    assert len(directors) == 1 and len(rooms) == 1
    director, room = directors[0], rooms[0]
    player = resolve_player(world)
    assert director.teleport_to_room_for_testing(6, player)
    unreal.SystemLibrary.execute_console_command(world, "t.MaxFPS 0")
    unreal.SystemLibrary.execute_console_command(world, "r.VSync 0")
    unreal.SystemLibrary.execute_console_command(world, "r.ScreenPercentage 100")
    unreal.SystemLibrary.execute_console_command(world, "r.AntiAliasingMethod 4")

    measurement_start = time.monotonic()
    all_samples = []
    phase_results = []
    for phase_index, (name, mode) in enumerate(PHASES):
        setup_start = time.perf_counter()
        assert director.set_stress_mode_for_testing(mode), name
        setup_ms = (time.perf_counter() - setup_start) * 1000.0
        room.reset_history_runtime_telemetry_for_testing()
        director.start_sweep_for_testing(160.0, True)
        phase_start = time.monotonic()
        phase_deadline = measurement_start + (phase_index + 1) * PHASE_SECONDS
        last_time = unreal.GameplayStatics.get_time_seconds(world)
        last_wall_time = time.perf_counter()
        samples = []
        while time.monotonic() < phase_deadline:
            yield 1
            current_wall_time = time.perf_counter()
            current_time = unreal.GameplayStatics.get_time_seconds(world)
            telemetry = json.loads(room.get_history_runtime_telemetry())["frame_data"]
            telemetry["frame_ms"] = max(0.0, (current_wall_time - last_wall_time) * 1000.0)
            telemetry["game_frame_ms"] = max(0.0, (current_time - last_time) * 1000.0)
            last_wall_time = current_wall_time
            last_time = current_time
            samples.append(telemetry)
            all_samples.append(telemetry)
        result = summarize(name, mode, samples, time.monotonic() - phase_start, setup_ms)
        phase_results.append(result)
        unreal.log("GRAY_POLICY_TEN_MIN_PHASE " + json.dumps(result, separators=(",", ":")))

    total_wall = time.monotonic() - measurement_start
    overall = summarize("Overall600Seconds", -1, all_samples, total_wall, 0.0)
    report = {
        "passed": total_wall >= 600.0,
        "map": MAP,
        "fixed_timestep": False,
        "rhi_required": "D3D12 SM6",
        "screen_percentage": 100,
        "anti_aliasing_method": 4,
        "wall_seconds": total_wall,
        "phases": phase_results,
        "overall": overall,
    }
    (OUTPUT / "ten_minute_performance.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    assert report["passed"], report
    unreal.log("GRAY_POLICY_LAB_V2_TEN_MIN_PASS " + str(OUTPUT))
    assert director.set_stress_mode_for_testing(0)
    yield 120
    levels.editor_request_end_play()
    yield 120
    assert editor.get_game_world() is None, "PIE did not stop"
    unreal.SystemLibrary.collect_garbage()
    yield 60
    unreal.log("GRAY_POLICY_LAB_V2_TEN_MIN_PIE_STOPPED")


sequence = run()
frames_left = 0
last_game_time = None


def tick(_delta):
    global frames_left, last_game_time
    try:
        assert time.monotonic() - driver_started < 900, "ten-minute driver exceeded 15 minutes"
        world = game_world()
        game_time = unreal.GameplayStatics.get_time_seconds(world) if world else None
        if game_time is not None and game_time == last_game_time:
            return
        last_game_time = game_time
        if frames_left > 0:
            frames_left -= 1
            return
        frames_left = max(0, next(sequence) - 1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log("GRAY_POLICY_LAB_V2_TEN_MIN_CALLBACK_UNREGISTERED")
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "QUIT_EDITOR")
    except Exception as error:
        (OUTPUT / "failed.json").write_text(
            json.dumps({"error": repr(error)}, indent=2), encoding="utf-8"
        )
        unreal.log_error("GRAY_POLICY_LAB_V2_TEN_MIN_FAIL " + traceback.format_exc())
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "QUIT_EDITOR")


handle = unreal.register_slate_post_tick_callback(tick)

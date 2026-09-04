"""Real-time D3D12 turn-hitch matrix for the isolated gray-policy lab."""

import json
import math
import os
import statistics
import time
import traceback
from pathlib import Path

import unreal


OUTPUT = Path(os.environ["DARKWELL_GRAY_POLICY_PERF_OUTPUT"]).resolve()
OUTPUT.mkdir(parents=True, exist_ok=True)
MAP = "/Game/Maps/L_SightWeaveGrayPolicyLab"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level(MAP)
started = time.monotonic()


def game_world():
    return editor.get_game_world()


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(len(ordered) * fraction))]


def summarize(name, mode, samples, wall_seconds, setup_ms):
    frame_ms = [sample["frame_ms"] for sample in samples]
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
    result = {
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
        },
        "room_ms": {
            "p50": percentile(room_ms, 0.50),
            "p95": percentile(room_ms, 0.95),
            "p99": percentile(room_ms, 0.99),
            "peak": max(room_ms),
        },
        "history": {
            "records_max": max(sample["records"] for sample in samples),
            "active_max": max(sample["epochs"] for sample in samples),
            "candidates_max": max(sample["candidates"] for sample in samples),
            "sleeping_max": max(sample["sleeping"] for sample in samples),
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
    unreal.log("GRAY_POLICY_PERF_CASE " + json.dumps(result, separators=(",", ":")))
    return result


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
    player = unreal.GameplayStatics.get_player_character(world, 0)
    assert player is not None and director.teleport_to_room_for_testing(6, player)
    unreal.SystemLibrary.execute_console_command(world, "t.MaxFPS 0")
    unreal.SystemLibrary.execute_console_command(world, "r.VSync 0")

    cases = [
        ("EmptyTurn", 0, 240, 160.0, True),
        ("OneWholeTurn", 1, 240, 160.0, True),
        ("EightWholeTurn", 2, 240, 160.0, True),
        ("ThirtyTwoWholeTurn", 3, 240, 160.0, True),
        ("OneSpatialPartialTurn", 0, 240, 160.0, True),
        ("Overlap64Turn", 4, 300, 160.0, True),
        ("SameStableId64Turn", 5, 300, 160.0, True),
        ("Distributed184OneRoomTurn", 6, 300, 160.0, True),
        ("Sweep90Overlap64", 4, 120, 90.0, False),
        ("Sweep160Overlap64", 4, 120, 160.0, False),
        ("RepeatTurn1000Distributed184", 6, 1000, 160.0, True),
    ]
    results = []
    for name, mode, count, sweep, repeat in cases:
        if name == "OneSpatialPartialTurn":
            assert director.set_stress_mode_for_testing(0)
            assert director.teleport_to_room_for_testing(2, player)
            assert director.reset_current_room_for_testing(player)
        else:
            assert director.teleport_to_room_for_testing(6, player)
            setup_start = time.perf_counter()
            assert director.set_stress_mode_for_testing(mode), name
        setup_ms = (time.perf_counter() - setup_start) * 1000.0 if name != "OneSpatialPartialTurn" else 0.0
        room.reset_history_runtime_telemetry_for_testing()
        director.start_sweep_for_testing(sweep, repeat)
        yield 90
        samples = []
        case_start = time.monotonic()
        last_time = unreal.GameplayStatics.get_time_seconds(world)
        for _ in range(count):
            yield 1
            current_time = unreal.GameplayStatics.get_time_seconds(world)
            telemetry = json.loads(room.get_history_runtime_telemetry())["frame_data"]
            telemetry["frame_ms"] = max(0.0, (current_time - last_time) * 1000.0)
            last_time = current_time
            samples.append(telemetry)
        results.append(
            summarize(name, mode, samples, time.monotonic() - case_start, setup_ms)
        )
        director.start_sweep_for_testing(1.0, False)
        yield 20
        if name == "OneSpatialPartialTurn":
            assert director.reset_current_room_for_testing(player)

    report = {
        "passed": True,
        "map": MAP,
        "fixed_timestep": False,
        "rhi_required": "D3D12 SM6",
        "cases": results,
    }
    (OUTPUT / "performance.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    unreal.log("GRAY_POLICY_LAB_V2_PERF_PASS " + str(OUTPUT))
    # Retire the synthetic pressure population while PIE still owns the world.
    # This also proves the room returns to its default-off state before teardown.
    assert director.set_stress_mode_for_testing(0)
    yield 120
    levels.editor_request_end_play()
    yield 120
    assert editor.get_game_world() is None, "PIE did not stop"
    unreal.SystemLibrary.collect_garbage()
    yield 60
    unreal.log("GRAY_POLICY_LAB_V2_PERF_PIE_STOPPED")


sequence = run()
frames_left = 0
last_game_time = None


def tick(_delta):
    global frames_left, last_game_time
    try:
        assert time.monotonic() - started < 600, "performance matrix exceeded ten minutes"
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
        unreal.log("GRAY_POLICY_LAB_V2_PERF_CALLBACK_UNREGISTERED")
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "QUIT_EDITOR")
    except Exception as error:
        (OUTPUT / "failed.json").write_text(
            json.dumps({"error": repr(error)}, indent=2), encoding="utf-8"
        )
        unreal.log_error("GRAY_POLICY_LAB_V2_PERF_FAIL " + traceback.format_exc())
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "QUIT_EDITOR")


handle = unreal.register_slate_post_tick_callback(tick)

"""Short real-D3D12 smoke for the dedicated V2 map and Chinese guidance.

The driver starts PIE, captures original SHOWUI images, stops PIE, unregisters
its callback, and closes the Editor with a preserved process exit code.
"""

import json
import time
import traceback
from pathlib import Path

import unreal


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level("/Game/Maps/L_SightWeaveGrayPolicyLab")
root = (
    Path(unreal.Paths.project_saved_dir())
    / "GrayPolicyLabV2"
    / "Company_20260904"
    / time.strftime("GPU_%Y%m%d_%H%M%S")
).resolve()
root.mkdir(parents=True, exist_ok=False)
started = time.monotonic()


def world():
    return editor.get_game_world()


def capture(name):
    path = (root / f"{name}.png").as_posix()
    unreal.SystemLibrary.execute_console_command(world(), f'Shot SHOWUI filename="{path}"')


def capture_and_wait(name):
    capture(name)
    for _ in range(180):
        yield 1
        if list(root.glob(f"{name}*.png")):
            return
    raise AssertionError(f"deferred screenshot did not land: {name}")


def face(player, yaw):
    assert player is not None, "PIE character is unavailable"
    assert player.set_actor_rotation(unreal.Rotator(pitch=0.0, yaw=yaw, roll=0.0), False)


def resolve_player(game_world):
    player = unreal.GameplayStatics.get_player_character(game_world, 0)
    if player is None:
        players = unreal.GameplayStatics.get_all_actors_of_class(
            game_world, unreal.DarkwellCharacter
        )
        assert len(players) == 1, players
        player = players[0]
    return player


def run():
    levels.editor_request_begin_play()
    yield 180
    game_world = world()
    assert game_world is not None, "PIE did not start"
    directors = unreal.GameplayStatics.get_all_actors_of_class(
        game_world, unreal.DarkwellSightWeaveGrayPolicyLabDirector
    )
    rooms = unreal.GameplayStatics.get_all_actors_of_class(
        game_world, unreal.DarkwellMovingPropLabRoom
    )
    assert len(directors) == 1, directors
    assert len(rooms) == 1, rooms
    director = directors[0]
    player = resolve_player(game_world)
    assert director.get_room_control_count_for_testing() == 27
    assert not director.is_stress_active()
    guidance = director.get_chinese_guidance_for_testing()
    for text in (
        "整体显示",
        "局部切块",
        "移动物体",
        "永不记忆",
        "遮挡与快速扫视",
        "性能压力测试",
    ):
        assert text in guidance, text
    unreal.SystemLibrary.execute_console_command(game_world, "r.ScreenPercentage 100")
    unreal.SystemLibrary.execute_console_command(game_world, "r.AntiAliasingMethod 4")
    unreal.SystemLibrary.execute_console_command(game_world, "setres 1920x1080w")
    yield 90
    yield from capture_and_wait("00_lobby_chinese_overview")
    face(player, 125.0)
    yield 30
    yield from capture_and_wait("01_lobby_room_entrances")

    assert director.teleport_to_room_for_testing(1, player)
    yield 45
    yield from capture_and_wait("02_room01_whole_entry")
    face(player, 72.0)
    yield 90
    yield from capture_and_wait("03_room01_whole_observation")
    face(player, -90.0)
    yield 45
    yield from capture_and_wait("04_room01_whole_history")

    assert director.teleport_to_room_for_testing(2, player)
    yield 45
    yield from capture_and_wait("05_room02_partial_entry")
    face(player, 70.0)
    yield 45
    yield from capture_and_wait("06_room02_partial_side_view")
    face(player, -90.0)
    yield 45
    yield from capture_and_wait("07_room02_partial_history_cap")

    assert director.teleport_to_room_for_testing(3, player)
    yield 45
    yield from capture_and_wait("08_room03_moving_whole")
    face(player, 110.0)
    yield 30
    yield from capture_and_wait("09_room03_moving_controls")
    face(player, -70.0)
    yield 30
    yield from capture_and_wait("10_room03_fresh_observation_area")

    assert director.teleport_to_room_for_testing(4, player)
    yield 45
    yield from capture_and_wait("11_room04_never_live")
    face(player, -90.0)
    yield 45
    yield from capture_and_wait("12_room04_never_offscreen")

    assert director.teleport_to_room_for_testing(5, player)
    yield 45
    yield from capture_and_wait("13_room05_occlusion_wall")
    director.start_sweep_for_testing(90.0, False)
    yield 30
    yield from capture_and_wait("14_room05_sweep90")
    director.start_sweep_for_testing(160.0, False)
    yield 30
    yield from capture_and_wait("15_room05_sweep160")
    director.start_sweep_for_testing(160.0, True)
    yield 90
    yield from capture_and_wait("16_room05_repeat_sweep")

    assert director.teleport_to_room_for_testing(6, player)
    assert director.set_stress_mode_for_testing(0)
    yield 45
    yield from capture_and_wait("17_room06_empty_baseline")
    stress_captures = (
        (1, "18_room06_one_object"),
        (2, "19_room06_eight_objects"),
        (3, "20_room06_thirty_two_objects"),
        (4, "21_room06_overlap64"),
        (5, "22_room06_same_stable_id64"),
        (6, "23_room06_distributed184"),
        (7, "24_room06_mixed_policies"),
    )
    for mode, name in stress_captures:
        assert director.set_stress_mode_for_testing(mode), name
        director.start_sweep_for_testing(160.0, False)
        yield 45
        yield from capture_and_wait(name)
    assert director.set_stress_mode_for_testing(0)
    yield 90
    yield from capture_and_wait("25_room06_reset")
    assert director.teleport_to_room_for_testing(2, player)
    assert director.reset_current_room_for_testing(player)
    face(player, -90.0)
    yield 60
    yield from capture_and_wait("26_residual_zfight_negative_regression")

    # Shot is deferred; wait until every original-resolution SHOWUI file lands.
    yield 180
    images = sorted(root.glob("*.png"))
    assert len(images) == 27, images
    result = {
        "passed": True,
        "map": "/Game/Maps/L_SightWeaveGrayPolicyLab",
        "screenshots": [str(path) for path in images],
        "screenshot_count": len(images),
        "controls": director.get_room_control_count_for_testing(),
        "stress_active": director.is_stress_active(),
        "viewport": list(
            unreal.GameplayStatics.get_player_controller(game_world, 0).get_viewport_size()
        ),
        "screen_percentage": unreal.SystemLibrary.get_console_variable_float_value(
            "r.ScreenPercentage"
        ),
        "anti_aliasing_method": unreal.SystemLibrary.get_console_variable_int_value(
            "r.AntiAliasingMethod"
        ),
    }
    (root / "checks.json").write_text(
        json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    unreal.log("GRAY_POLICY_LAB_V2_GPU_PASS " + str(root))
    levels.editor_request_end_play()
    yield 45
    assert editor.get_game_world() is None, "PIE did not stop"
    unreal.log("GRAY_POLICY_LAB_V2_PIE_STOPPED")


sequence = run()
frames_left = 0
last_game_time = None


def tick(_delta):
    global frames_left, last_game_time
    try:
        assert time.monotonic() - started < 240, "GPU smoke exceeded four minutes"
        game_world = world()
        game_time = (
            unreal.GameplayStatics.get_time_seconds(game_world) if game_world else None
        )
        if game_time is not None and game_time == last_game_time:
            return
        last_game_time = game_time
        if frames_left > 0:
            frames_left -= 1
            return
        frames_left = max(0, next(sequence) - 1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log("GRAY_POLICY_LAB_V2_CALLBACK_UNREGISTERED")
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "QUIT_EDITOR")
    except Exception as error:
        (root / "failed.json").write_text(
            json.dumps({"error": repr(error)}, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        unreal.log_error("GRAY_POLICY_LAB_V2_GPU_FAIL " + traceback.format_exc())
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "QUIT_EDITOR")


handle = unreal.register_slate_post_tick_callback(tick)

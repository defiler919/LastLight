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
    capture("00_lobby_chinese_overview")
    yield 60
    images = list(root.glob("00_lobby_chinese_overview*.png"))
    assert len(images) == 1, images
    result = {
        "passed": True,
        "map": "/Game/Maps/L_SightWeaveGrayPolicyLab",
        "screenshots": [str(path) for path in images],
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

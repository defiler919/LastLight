"""Room 02 real observation replay. Layer isolation is diagnostic only.

No knowledge, policy, masks or source geometry are injected. Full-resolution
frames and raw cross-episode samples are kept outside source control.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_SightWeaveGrayPolicyLab')
root = Path(os.environ['DARKWELL_AUDIT_OUTPUT']).resolve()
root.mkdir(parents=True, exist_ok=True)
started = time.monotonic()
rows = []
sid = unreal.Name('Lab.V2.Partial')
player = room = director = None

def world():
    return editor.get_game_world()

def face(yaw):
    player.set_actor_rotation(unreal.Rotator(yaw=yaw), False)

def shot(label):
    rows.append(dict(label=label, yaw=player.get_actor_rotation().yaw,
        coverage=room.get_last_legal_coverage_ratio_for_testing(sid),
        runtime=json.loads(room.get_history_runtime_telemetry()),
        evidence=json.loads(room.get_memory_seam_audit_for_testing(sid))))
    (root/'samples.json').write_text(json.dumps(rows), encoding='utf-8')
    assert director.capture_game_viewport_for_testing(str(root/(label+'.png')))

def run():
    global room, player, director
    levels.editor_request_begin_play()
    yield 150
    director = unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellSightWeaveGrayPolicyLabDirector)[0]
    assert director.set_audit_viewport_size_for_testing(2233,911)
    room = unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellMovingPropLabRoom)[0]
    controller = unreal.GameplayStatics.get_player_controller(world(), 0)
    player = unreal.GameplayStatics.get_player_character(world(), 0)
    controller.set_actor_tick_enabled(False)
    player.set_actor_tick_enabled(False)
    assert director.teleport_to_room_for_testing(2, player)
    assert director.reset_current_room_for_testing(player)
    face(-90)
    boom = player.get_component_by_class(unreal.SpringArmComponent)
    camera = player.get_component_by_class(unreal.CameraComponent)
    camera.set_absolute(True, True, True)
    boom.set_component_tick_enabled(False)
    origin = unreal.Vector(0, -6850, 650)
    camera.set_world_location(origin, False, True)
    camera.set_world_rotation(unreal.MathLibrary.find_look_at_rotation(origin, unreal.Vector(0, -6150, 60)), False, True)
    for cmd in ['r.ScreenPercentage 100', 'r.AntiAliasingMethod 4', 'setres 1920x1080w']:
        unreal.SystemLibrary.execute_console_command(world(), cmd)
    yield 60
    shot('00_unknown')
    yield 3
    # Cone half-angle is about 52 degrees. These poses expose overlapping strips.
    for cycle, yaw in enumerate([18, 28, 40, 52, 90, 52, 40, 28]):
        face(yaw)
        for frame in range(20):
            yield 1
            if cycle in (2, 4) and frame < 8:
                shot(f'live_{cycle}_{frame:02}')
        shot(f'cycle_{cycle}_live')
        yield 3
        face(-90)
        for frame in range(20):
            yield 1
            if cycle in (2, 4) and frame < 8:
                shot(f'exit_{cycle}_{frame:02}')
        shot(f'cycle_{cycle}_history')
        yield 3
    caps = room.get_components_by_class(unreal.DynamicMeshComponent)
    visibility = [(c, c.is_visible()) for c in caps]
    for c, _ in visibility:
        c.set_visibility(False)
    yield 3
    shot('diagnostic_caps_hidden')
    yield 3
    for c, visible in visibility:
        c.set_visibility(visible)
    yield 3
    shot('diagnostic_caps_restored')
    yield 10
    (root/'complete.json').write_text(json.dumps(dict(protocol_complete=True, frames=len(rows))), encoding='utf-8')
    unreal.log('GRAY_EPISODE_AUDIT_COMPLETE')
    assert director.set_audit_viewport_size_for_testing(0,0)
    levels.editor_request_end_play()
    yield 45
    assert world() is None
    unreal.SystemLibrary.collect_garbage()
    yield 30
    if signal := os.environ.get('DARKWELL_DEBUG_ATTACH_SIGNAL'):
        Path(signal).write_text('PIE stopped; attach before editor teardown', encoding='utf-8')
        yield 120

sequence = run()
left = 0
last_time = None
def tick(_delta):
    global left, last_time
    try:
        assert time.monotonic()-started < 300
        now = unreal.GameplayStatics.get_time_seconds(world()) if world() else None
        if now is not None and now == last_time:
            return
        last_time = now
        if left:
            left -= 1
            return
        left = max(0, next(sequence)-1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log('GRAY_EPISODE_AUDIT_STOPPED')
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'CLOSE_SLATE_MAINFRAME')
    except Exception:
        (root/'failed.txt').write_text(traceback.format_exc(), encoding='utf-8')
        unreal.log_error(traceback.format_exc())
        if director:
            director.set_audit_viewport_size_for_testing(0,0)
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'CLOSE_SLATE_MAINFRAME')
handle = unreal.register_slate_post_tick_callback(tick)

"""A0 normal-quality Whole/Partial first exit and explicit rebuild across two PIE lifecycles.

Cycle 0 stays resident; cycle 1 explicitly releases one old sealed record through the development diagnostic bridge. This is visual correctness evidence, never a performance run.
Saved telemetry is an observation aid; original screenshots need visual review.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

root = Path(os.environ['DARKWELL_AUDIT_OUTPUT']).resolve()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level('/Game/Maps/L_SightWeaveGrayPolicyLab')
started = time.monotonic()
rows, lifecycles = [], []
player = room = director = camera = None
centers = {1:(-6000,-3500), 2:(0,-6500), 3:(6000,-3500), 4:(-6000,3500), 5:(0,6500)}

def world():
    return editor.get_game_world()

def face(yaw):
    player.set_actor_rotation(unreal.Rotator(yaw=yaw), False)

def enter(index):
    assert director.teleport_to_room_for_testing(index, player)
    assert director.reset_current_room_for_testing(player)
    face(-90)
    x,y = centers[index]
    origin = unreal.Vector(x, y-350, 800 if index==3 else 650)
    camera.set_world_location(origin, False, True)
    camera.set_world_rotation(unreal.MathLibrary.find_look_at_rotation(origin, unreal.Vector(x,y+400,60)), False, True)

def shot(label, ids):
    data = dict(label=label, game_time=unreal.GameplayStatics.get_time_seconds(world()),
                yaw=player.get_actor_rotation().yaw,
                resources=json.loads(room.get_history_runtime_telemetry())['frame_data'], objects={})
    for name in ids:
        sid = unreal.Name('Lab.V2.'+name)
        data['objects'][name] = dict(records=room.get_spatial_record_count(sid),
            current=room.get_current_epoch_count_for_testing(sid),
            stale=room.get_stale_epoch_count_for_testing(sid),
            proxies=room.get_visible_historical_proxy_count_for_testing(sid),
            caps=room.get_visible_historical_cap_count_for_testing(sid),
            confirmed=room.is_reveal_confirmed_for_testing(sid),
            coverage=room.get_last_legal_coverage_ratio_for_testing(sid),
            live=json.loads(room.get_moving_live_telemetry(sid)))
    data['preparation'] = json.loads(room.get_whole_preparation_telemetry())
    data['capture_wall_seconds'] = time.monotonic()
    assert director.capture_game_viewport_for_testing(str(root/(label+'.png')))
    data['seal_to_readback_ms'] = data['preparation']['seal_age_ms'] + (time.monotonic()-data['capture_wall_seconds'])*1000
    rows.append(data)
    (root/'samples.json').write_text(json.dumps(rows), encoding='utf-8')

def frames(label, ids, count=8):
    for index in range(count):
        yield 1
        shot(f'{label}_{index:02}', ids)

def run():
    global player, room, director, camera
    for lifecycle in range(2):
        levels.editor_request_begin_play()
        yield 150
        w = world()
        player = unreal.GameplayStatics.get_player_character(w, 0)
        controller = unreal.GameplayStatics.get_player_controller(w, 0)
        room = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DarkwellMovingPropLabRoom)[0]
        director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DarkwellSightWeaveGrayPolicyLabDirector)[0]
        assert director.set_audit_viewport_size_for_testing(2233,911)
        controller.set_actor_tick_enabled(False)
        player.set_actor_tick_enabled(False)
        player.get_component_by_class(unreal.SpringArmComponent).set_component_tick_enabled(False)
        camera = player.get_component_by_class(unreal.CameraComponent)
        camera.set_absolute(True, True, True)
        for command in ['r.ScreenPercentage 100', 'r.AntiAliasingMethod 4', 'setres 1920x1080w']:
            unreal.SystemLibrary.execute_console_command(w, command)
        unreal.SystemLibrary.execute_console_command(w, 'r.Darkwell.ObjectMemory.WholeGeometryPreparation 0')
        for index, name, angle in [(1, 'Whole', 90), (2, 'Partial', 28)]:
            enter(index)
            yield 20
            face(angle)
            yield 30
            if name == 'Whole':
                assert room.is_reveal_confirmed_for_testing(unreal.Name('Lab.V2.'+name))
            face(-90)
            yield from frames(f'pie{lifecycle}_{name}_first_exit', [name], 4)
            yield 20
            sid = unreal.Name('Lab.V2.'+name)
            records = room.get_spatial_record_count(sid)
            assert records > 0
            assert room.get_visible_historical_proxy_count_for_testing(sid) > 0
            request = None
            if lifecycle == 1:
                request = room.release_oldest_presentation_for_testing(sid)
                assert request
                assert room.get_historical_presentation_resource_count_for_testing(sid) == 0
                assert room.get_spatial_record_count(sid) == records
            yield 4
            unreal.SystemLibrary.collect_garbage()
            yield 4
            assert room.get_spatial_record_count(sid) == records
            if request:
                assert room.rebuild_presentation_for_testing(request)
                assert room.get_visible_historical_proxy_count_for_testing(sid) > 0
                assert room.rebuild_presentation_for_testing(request)
            yield from frames(f'pie{lifecycle}_{name}_rebuild', [name], 4)
            if name == 'Partial':
                assert room.get_visible_historical_cap_count_for_testing(sid) > 0
            assert director.reset_current_room_for_testing(player)
            if request:
                assert not room.rebuild_presentation_for_testing(request)
            face(-90)
            yield 5
            assert room.get_spatial_record_count(sid) == 0
        lifecycles.append(dict(cycle=lifecycle, viewport=list(controller.get_viewport_size()),
            before_stop=json.loads(room.get_history_runtime_telemetry())['frame_data']))
        assert director.set_audit_viewport_size_for_testing(0,0)
        levels.editor_request_end_play()
        yield 60
        assert world() is None
        player = room = director = camera = controller = w = None
        unreal.SystemLibrary.collect_garbage()
        yield 30
    (root/'complete.json').write_text(json.dumps(dict(protocol_complete=True, frames=len(rows), lifecycles=lifecycles), indent=2), encoding='utf-8')

sequence = run()
left = 0
last_time = None
def tick(_delta):
    global left, last_time
    try:
        assert time.monotonic()-started < 600
        now = unreal.GameplayStatics.get_time_seconds(world()) if world() else None
        if now is not None and now == last_time:
            return
        last_time = now
        if left:
            left -= 1
        else:
            left = max(0, next(sequence)-1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log('GRAY_EPISODE_AUDIT_STOPPED')
        # Let ExecutePythonScript complete its notification before engine exit.
        # Directly closing Slate here leaves that notification alive until the
        # Python plugin shuts down, after the Slate application has been destroyed.
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        (root/'failed.txt').write_text(traceback.format_exc(), encoding='utf-8')
        unreal.log_error(traceback.format_exc())
        if director:
            director.set_audit_viewport_size_for_testing(0,0)
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle = unreal.register_slate_post_tick_callback(tick)

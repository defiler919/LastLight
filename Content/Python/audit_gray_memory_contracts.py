"""Continuous normal-quality policy transitions and three real PIE lifecycles.

Only camera/player controls and the existing authored motion controls are driven.
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
    rows.append(data)
    (root/'samples.json').write_text(json.dumps(rows), encoding='utf-8')
    assert director.capture_game_viewport_for_testing(str(root/(label+'.png')))

def frames(label, ids, count=8):
    for index in range(count):
        yield 1
        shot(f'{label}_{index:02}', ids)

def run():
    global player, room, director, camera
    for lifecycle in range(3):
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
        enter(1)
        yield 20
        assert room.get_spatial_record_count(unreal.Name('Lab.V2.Whole')) == 0
        face(90)
        yield from frames(f'pie{lifecycle}_whole_entry', ['Whole'])
        yield 25
        assert room.is_reveal_confirmed_for_testing(unreal.Name('Lab.V2.Whole'))
        face(40)
        yield from frames(f'pie{lifecycle}_whole_confirmed_edge', ['Whole'])
        face(-90)
        yield from frames(f'pie{lifecycle}_whole_exit', ['Whole'])
        yield 20
        if lifecycle == 0:
            enter(3)
            face(90)
            yield 30
            shot('moving_before', ['MoveWhole'])
            face(-90)
            yield 20
            sid = unreal.Name('Lab.V2.MoveWhole')
            assert room.get_stale_epoch_count_for_testing(sid) > 0
            assert room.start_gray_policy_motion(False)
            yield from frames('hidden_motion_start', ['MoveWhole'])
            yield 245
            shot('hidden_motion_stopped', ['MoveWhole'])
            assert room.get_current_epoch_count_for_testing(sid) == 0
            assert room.get_stale_epoch_count_for_testing(sid) > 0
            # Translation ends 500 cm farther away: approach while looking away
            # before attempting a fresh legal observation inside source range.
            player.set_actor_location(unreal.Vector(6000,-3500,92), False, True)
            face(90)
            yield from frames('stationary_reentry', ['MoveWhole'], 16)
            yield 20
            assert room.get_current_epoch_count_for_testing(sid) == 1
            shot('stationary_known_endpoint', ['MoveWhole'])
            face(-90)
            yield 20
            shot('stationary_endpoint_history', ['MoveWhole'])
            enter(4)
            face(90)
            yield from frames('never_entry', ['Never'])
            yield 25
            face(-90)
            yield from frames('never_exit', ['Never'])
            yield 25
            assert room.get_stale_epoch_count_for_testing(unreal.Name('Lab.V2.Never')) == 0
            assert room.get_visible_historical_proxy_count_for_testing(unreal.Name('Lab.V2.Never')) == 0
            enter(5)
            face(90)
            yield 30
            shot('wall_initial_legal_gap', ['OcclusionWhole','OcclusionPartial'])
            player.set_actor_location(unreal.Vector(300,6200,92), False, True)
            face(133)
            yield 30
            assert room.is_reveal_confirmed_for_testing(unreal.Name('Lab.V2.OcclusionWhole'))
            shot('wall_whole_observed_through_gap', ['OcclusionWhole','OcclusionPartial'])
            player.set_actor_location(unreal.Vector(0,6000,92), False, True)
            face(90)
            yield from frames('wall_confirmed_occlusion', ['OcclusionWhole','OcclusionPartial'])
            for degrees in (90,160):
                face(10)
                yield 20
                director.start_sweep_for_testing(degrees, False)
                yield from frames(f'wall_sweep{degrees}', ['OcclusionWhole','OcclusionPartial'], 24)
            face(-90)
            yield 25
            shot('wall_final_history', ['OcclusionWhole','OcclusionPartial'])
        # Reset and switch rooms release their owned state before ending PIE.
        enter(2)
        face(28)
        yield 25
        face(-90)
        yield 20
        shot(f'pie{lifecycle}_partial_outer_cut', ['Partial'])
        assert director.reset_current_room_for_testing(player)
        face(-90)
        yield 20
        assert room.get_spatial_record_count(unreal.Name('Lab.V2.Partial')) == 0
        assert room.get_historical_presentation_resource_count_for_testing(unreal.Name('Lab.V2.Partial')) == 0
        lifecycles.append(dict(cycle=lifecycle, viewport=list(controller.get_viewport_size()),
            before_stop=json.loads(room.get_history_runtime_telemetry())['frame_data']))
        assert director.set_audit_viewport_size_for_testing(0,0)
        levels.editor_request_end_play()
        yield 60
        assert world() is None
        # Release Python wrappers as well as native world ownership before GC.
        player = room = director = camera = controller = w = None
        unreal.SystemLibrary.collect_garbage()
        yield 30
    (root/'complete.json').write_text(json.dumps(dict(protocol_complete=True, frames=len(rows), lifecycles=lifecycles), indent=2), encoding='utf-8')
    if signal := os.environ.get('DARKWELL_DEBUG_ATTACH_SIGNAL'):
        Path(signal).write_text('PIE stopped; attach before editor teardown', encoding='utf-8')
        yield 120

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

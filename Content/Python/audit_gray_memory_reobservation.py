"""Room05 continuous observer-only route. No reset between the four stages.

Collects original game frames plus independent analytic cabinet-interior samples.
Protocol completion is separate from the product oracle (including before-fix runs).
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
rows, timing = [], []
timing_mode = os.environ.get('DARKWELL_REOBSERVATION_TIMING') == '1'
normal_turns = os.environ.get('DARKWELL_REOBSERVATION_NORMAL_TURNS') == '1'
phase = 'warmup'
last_wall = None
player = room = director = camera = None

def world():
    return editor.get_game_world()

def face(yaw):
    player.set_actor_rotation(unreal.Rotator(yaw=yaw), False)

def turn(yaw):
    if normal_turns:
        current=player.get_actor_rotation().yaw
        delta=(yaw-current+180)%360-180
        # Project's normal aim turn speed; this visual protocol uses fixed 60 Hz.
        count=max(1,int(abs(delta)/(280/60))+1)
        for index in range(count):
            face(current+delta*(index+1)/count)
            yield 1
    else:
        face(yaw)

def position(x, y, yaw):
    player.set_actor_location(unreal.Vector(x, y, 92), False, True)
    face(yaw)

def shot(label):
    if timing_mode:
        sid=unreal.Name('Lab.V2.OcclusionWhole')
        rows.append(dict(label=label,confirmed=room.is_reveal_confirmed_for_testing(sid),
            coverage=room.get_last_legal_coverage_ratio_for_testing(sid),
            current=room.get_current_epoch_count_for_testing(sid),
            stale=room.get_stale_epoch_count_for_testing(sid),
            records=room.get_spatial_record_count(sid)))
        return
    data = dict(label=label, game_time=unreal.GameplayStatics.get_time_seconds(world()),
        position=str(player.get_actor_location()), yaw=player.get_actor_rotation().yaw,
        resources=json.loads(room.get_history_runtime_telemetry())['frame_data'], objects={})
    for name in ('OcclusionWhole', 'OcclusionPartial'):
        sid = unreal.Name('Lab.V2.'+name)
        item = dict(pose=str(room.get_tracked_transform(sid)),
            reveal=json.loads(room.get_reveal_policy_telemetry(sid)),
            history=room.get_history_policy_telemetry(sid),
            records=room.get_spatial_record_count(sid),
            current=room.get_current_epoch_count_for_testing(sid),
            stale=room.get_stale_epoch_count_for_testing(sid),
            proxies=room.get_visible_historical_proxy_count_for_testing(sid),
            caps=room.get_visible_historical_cap_count_for_testing(sid),
            confirmed=room.is_reveal_confirmed_for_testing(sid),
            valid=room.is_last_coverage_valid_for_testing(sid),
            coverage=room.get_last_legal_coverage_ratio_for_testing(sid),
            divider=json.loads(room.get_divider_mask_telemetry_for_testing(sid)),
            seam=json.loads(room.get_memory_seam_audit_for_testing(sid)),
            fine=room.get_fine_history_telemetry(sid),
            live=json.loads(room.get_moving_live_telemetry(sid)))
        if hasattr(room, 'get_capture_refresh_audit_for_testing'):
            item['capture'] = json.loads(room.get_capture_refresh_audit_for_testing(sid))
        data['objects'][name] = item
    rows.append(data)
    (root/'samples.json').write_text(json.dumps(rows), encoding='utf-8')
    assert director.capture_game_viewport_for_testing(str(root/(label+'.png')))

def frames(label, count=8):
    for index in range(count):
        yield 1
        shot(f'{label}_{index:02}')

def run():
    global player, room, director, camera, phase
    levels.editor_request_begin_play()
    yield 150
    w = world()
    player = unreal.GameplayStatics.get_player_character(w, 0)
    controller = unreal.GameplayStatics.get_player_controller(w, 0)
    room = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DarkwellMovingPropLabRoom)[0]
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DarkwellSightWeaveGrayPolicyLabDirector)[0]
    controller.set_actor_tick_enabled(False)
    player.set_actor_tick_enabled(False)
    player.get_component_by_class(unreal.SpringArmComponent).set_component_tick_enabled(False)
    camera = player.get_component_by_class(unreal.CameraComponent)
    camera.set_absolute(True, True, True)
    origin = unreal.Vector(0, 6150, 800)
    camera.set_world_location(origin, False, True)
    camera.set_world_rotation(unreal.MathLibrary.find_look_at_rotation(origin, unreal.Vector(0,7000,60)), False, True)
    for command in ['r.ScreenPercentage 100', 'r.AntiAliasingMethod 4']:
        unreal.SystemLibrary.execute_console_command(w, command)
    assert director.teleport_to_room_for_testing(5, player)
    assert director.reset_current_room_for_testing(player)
    position(70, 6000, -90)
    yield 20
    shot('initial_unknown')
    phase = 'first_partial_observation'
    yield from turn(115)
    yield from frames('first_partial_entry')
    yield 25
    shot('stage1_far_partial_live')
    phase = 'first_seal'
    yield from turn(-90)
    yield from frames('first_exit')
    yield 20
    shot('stage2_H1')
    phase = 'approach_while_away'
    # Walk straight through the existing gap while facing away. No fixture changes.
    for y in range(6005,6751,5):
        position(70, y, -90)
        yield 1
    phase = 'near_full_reobservation'
    yield from turn(155)
    yield from frames('near_full_entry')
    yield 25
    shot('stage3_near_full_live')
    phase = 'second_seal'
    yield from turn(-90)
    yield from frames('second_exit')
    yield 20
    shot('stage4_H2')
    for cycle in range(4):
        phase = f'repeat{cycle}_far'
        position(70,6000,115)
        yield 25
        phase = f'repeat{cycle}_far_seal'
        face(-90)
        yield 20
        shot(f'repeat{cycle}_far_history')
        phase = f'repeat{cycle}_near'
        position(70,6750,155)
        yield 25
        phase = f'repeat{cycle}_near_seal'
        face(-90)
        yield from frames(f'repeat{cycle}_near_exit', 3)
        yield 20
        shot(f'repeat{cycle}_near_history')
    # Independent direct-full route is a separate reset AFTER the continuous case.
    phase = 'reference'
    assert director.reset_current_room_for_testing(player)
    position(70,6750,-90)
    yield 20
    face(155)
    yield 30
    shot('reference_direct_full_live')
    face(-90)
    yield from frames('reference_exit')
    yield 20
    shot('reference_direct_full_history')
    phase = 'teardown'
    levels.editor_request_end_play()
    yield 60
    assert world() is None
    player = room = director = camera = controller = w = None
    unreal.SystemLibrary.collect_garbage()
    yield 30
    (root/'complete.json').write_text(json.dumps(dict(protocol_complete=True,frames=len(rows),normal_turns=normal_turns,timing_mode=timing_mode)),encoding='utf-8')
    if timing_mode:
        (root/'timing.json').write_text(json.dumps(timing),encoding='utf-8')
        (root/'timing-stages.json').write_text(json.dumps(rows),encoding='utf-8')

sequence = run()
left = 0
last_time = None
def tick(_delta):
    global left, last_time, last_wall
    try:
        assert time.monotonic()-started < 600
        now = unreal.GameplayStatics.get_time_seconds(world()) if world() else None
        if now is not None and now == last_time:
            return
        wall = time.perf_counter()
        if timing_mode and room and phase not in ('warmup','teardown'):
            timing.append(dict(phase=phase, game_time=now,
                game_delta=now-last_time if last_time is not None else None,
                wall_ms=(wall-last_wall)*1000 if last_wall is not None else None,
                frame=json.loads(room.get_history_runtime_telemetry())['frame_data']))
        last_wall = wall
        last_time = now
        if left:
            left -= 1
        else:
            left = max(0,next(sequence)-1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log('GRAY_EPISODE_AUDIT_STOPPED')
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'CLOSE_SLATE_MAINFRAME')
    except Exception:
        (root/'failed.txt').write_text(traceback.format_exc(),encoding='utf-8')
        unreal.log_error(traceback.format_exc())
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'CLOSE_SLATE_MAINFRAME')
handle = unreal.register_slate_post_tick_callback(tick)

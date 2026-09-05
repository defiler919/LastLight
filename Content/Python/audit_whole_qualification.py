"""Slow legal threshold crossing: every transition frame, never endpoint-only.

The same driver collects before/after evidence; product assertions live in the
independent analyzer so a known-bad build can finish its complete recording.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

root = Path(os.environ['DARKWELL_AUDIT_OUTPUT']).resolve()
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level('/Game/Maps/L_SightWeaveGrayPolicyLab')
rows = []
started = time.monotonic()
player = room = director = camera = None
sid = None

def world():
    return editor.get_game_world()

def face(yaw):
    player.set_actor_rotation(unreal.Rotator(yaw=yaw), False)

def reveal():
    return json.loads(room.get_reveal_policy_telemetry(sid))

def shot(label):
    row = dict(label=label, game_time=unreal.GameplayStatics.get_time_seconds(world()),
        yaw=player.get_actor_rotation().yaw, reveal=reveal(),
        pipeline=json.loads(room.get_qualification_audit_for_testing(sid)),
        capture=json.loads(room.get_capture_refresh_audit_for_testing(sid)),
        live=json.loads(room.get_moving_live_telemetry(sid)),
        resources=json.loads(room.get_history_runtime_telemetry())['frame_data'],
        current=room.get_current_epoch_count_for_testing(sid),
        stale=room.get_stale_epoch_count_for_testing(sid),
        caps=room.get_visible_historical_cap_count_for_testing(sid))
    rows.append(row)
    (root/'samples.json').write_text(json.dumps(rows), encoding='utf-8')
    assert director.capture_game_viewport_for_testing(str(root/(label+'.png')))

def run():
    global player, room, director, camera, sid
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
    camera.set_absolute(True,True,True)
    for command in ['r.ScreenPercentage 100','r.AntiAliasingMethod 4']:
        unreal.SystemLibrary.execute_console_command(w,command)
    cases = [(1,'Whole',-6000,-3150), (3,'MoveWhole',5750,-3150), (5,'OcclusionWhole',-450,7000)]
    for index, name, x, y in cases:
        sid=unreal.Name('Lab.V2.'+name)
        assert director.teleport_to_room_for_testing(index,player)
        assert director.reset_current_room_for_testing(player)
        observer=(70,6750) if index==5 else (x,y-650)
        player.set_actor_location(unreal.Vector(*observer,92),False,True)
        origin=unreal.Vector(x,y-800,650)
        camera.set_world_location(origin,False,True)
        camera.set_world_rotation(unreal.MathLibrary.find_look_at_rotation(origin,unreal.Vector(x,y,65)),False,True)
        point=unreal.Vector2D(x-60,y-30)
        room.configure_qualification_audit_for_testing(sid,point)
        face(-90)
        yield 25
        for episode in range(3):
            prefix=f'room{index}_session{episode}'
            # Find a legal, unqualified view, then hold long enough for already
            # revealed local pixels to settle. No threshold or fixture overrides.
            yaw=210 if index==5 else 160
            while True:
                face(yaw)
                yield 1
                state=reveal()
                assert not state['confirmed'], 'Search must not cross qualification'
                if state['observed_span_cm']>=55:
                    break
                yaw-=.5
                assert yaw>90
            yield 30
            for f in range(5):
                yield 1
                shot(f'{prefix}_before_{f:03}')
            frame=0
            while not reveal()['confirmed']:
                yaw-=.1
                face(yaw)
                yield 1
                shot(f'{prefix}_cross_{frame:03}')
                frame+=1
                assert frame<200
            # Includes confirmation frame above and every subsequent frame
            # through completion, then a smaller still-legal contact.
            for f in range(24):
                yield 1
                shot(f'{prefix}_settle_{f:03}')
            face(yaw+1)
            for f in range(8):
                yield 1
                shot(f'{prefix}_small_{f:03}')
            face(-90)
            for f in range(8):
                yield 1
                shot(f'{prefix}_exit_{f:03}')
            yield 25
    room.configure_qualification_audit_for_testing(unreal.Name('None'),unreal.Vector2D())
    assert director.set_audit_viewport_size_for_testing(0,0)
    levels.editor_request_end_play()
    yield 60
    assert world() is None
    player=room=director=camera=controller=w=None
    unreal.SystemLibrary.collect_garbage()
    yield 30
    (root/'complete.json').write_text(json.dumps(dict(frames=len(rows),protocol_complete=True)),encoding='utf-8')

sequence=run()
left=0
last_time=None
def tick(_delta):
    global left,last_time
    try:
        assert time.monotonic()-started<900
        now=unreal.GameplayStatics.get_time_seconds(world()) if world() else None
        if now is not None and now==last_time:
            return
        last_time=now
        if left:
            left-=1
        else:
            left=max(0,next(sequence)-1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log('GRAY_EPISODE_AUDIT_STOPPED')
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'CLOSE_SLATE_MAINFRAME')
    except Exception:
        (root/'failed.txt').write_text(traceback.format_exc(),encoding='utf-8')
        unreal.log_error(traceback.format_exc())
        if director:
            director.set_audit_viewport_size_for_testing(0,0)
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'CLOSE_SLATE_MAINFRAME')
handle=unreal.register_slate_post_tick_callback(tick)

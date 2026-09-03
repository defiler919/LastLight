"""D3D12 gray object policy evidence using real observation, transforms and F controls.
No knowledge or visibility state is injected. Native tests verify the F trace and
all original fine masks. Each process stops PIE, unregisters its callback, quits.
"""
import json
import math
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
root = (Path(unreal.Paths.project_saved_dir()) / 'GrayObjectPolicy' / time.strftime('GPU_%Y%m%d_%H%M%S')).resolve()
root.mkdir(parents=True, exist_ok=False)
rows = []
player = controller = room = None
sid = unreal.Name('Lab.InWorld.Rotate.Cabinet')
start = time.monotonic()
R = unreal.SightWeaveRevealMode
H = unreal.SightWeaveHistoryMode


def world():
    return editor.get_game_world()


def pose(yaw, x=-300, y=100, target=(-300,650,75)):
    player.set_actor_location(unreal.Vector(x,y,92), False, True)
    player.set_actor_rotation(unreal.Rotator(yaw=yaw), True)
    boom = player.get_component_by_class(unreal.SpringArmComponent)
    camera = player.get_component_by_class(unreal.CameraComponent)
    boom.set_component_tick_enabled(False)
    origin = unreal.Vector(target[0]-250,target[1]+200,target[2]+225)
    camera.set_world_location(origin,False,True)
    camera.set_world_rotation(unreal.MathLibrary.find_look_at_rotation(origin,unreal.Vector(*target)),False,True)


def trigger(x,y):
    controls = unreal.GameplayStatics.get_all_actors_of_class(world(),unreal.DarkwellMovingPropLabControl)
    found = [a for a in controls if abs(a.get_actor_location().x-x)<2 and abs(a.get_actor_location().y-y)<2]
    assert len(found)==1 and found[0].trigger_for_lab_evidence(player)


def sample(label,target=None,shot=True):
    target = sid if target is None else target
    row = dict(label=label,time=unreal.GameplayStatics.get_time_seconds(world()),
        coverage=room.get_last_legal_coverage_ratio_for_testing(target),
        current=room.get_current_epoch_count_for_testing(target),stale=room.get_stale_epoch_count_for_testing(target),
        proxies=room.get_visible_historical_proxy_count_for_testing(target),caps=room.get_visible_historical_cap_count_for_testing(target),
        resources=room.get_historical_presentation_resource_count_for_testing(target),
        policy=json.loads(room.get_reveal_policy_telemetry(target)),history=room.get_history_policy_telemetry(target),
        runtime=json.loads(room.get_history_runtime_telemetry()),live=json.loads(room.get_moving_live_telemetry(target)),
        ownership=room.get_3d_ownership_telemetry_for_testing(target),
        viewport=list(controller.get_viewport_size()),
        screen_percentage=unreal.SystemLibrary.get_console_variable_float_value('r.ScreenPercentage'),
        anti_aliasing_method=unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod'))
    assert row['screen_percentage']==100 and row['anti_aliasing_method']==4
    if shot:
        row['file']=f'{len(rows):04}_{label}.png'
        unreal.SystemLibrary.execute_console_command(world(),f'Shot SHOWUI filename="{(root/row["file"]).as_posix()}"')
    rows.append(row)
    unreal.log('GRAY_GPU_FRAME '+json.dumps(row))
    assert room.get_max_3d_render_ownership_contributors_for_testing(target)<=1,row
    assert room.get_hard_ownership_filter_leak_for_testing(target)==0,row
    return row


def reset(reveal=R.WHOLE_OBJECT_AFTER_SPAN,history=H.STATIONARY_ONLY):
    trigger(1500,-1050)
    yield 5
    assert room.reset_tracked_reveal_policy_for_lab(sid,reveal,100,history)


def run():
    global player,controller,room
    levels.editor_request_begin_play()
    yield 120
    controller=unreal.GameplayStatics.get_player_controller(world(),0)
    player=unreal.GameplayStatics.get_player_pawn(world(),0)
    room=unreal.GameplayStatics.get_all_actors_of_class(world(),unreal.DarkwellMovingPropLabRoom)[0]
    controller.set_actor_tick_enabled(False)
    player.set_actor_tick_enabled(False)
    unreal.SystemLibrary.execute_console_command(world(),'r.ScreenPercentage 100')
    unreal.SystemLibrary.execute_console_command(world(),'r.AntiAliasingMethod 4')
    assert room.get_tracked_identity_count()==15
    yield from reset()
    pose(146)
    yield 45
    below=sample('whole_below_100cm')
    assert 0<below['coverage']<1 and not below['policy']['confirmed'] and below['stale']==0
    yield 3
    pose(-90)
    yield 45
    lost=sample('whole_unconfirmed_view_loss')
    assert lost['stale']==lost['resources']==0
    yield 3
    pose(90)
    yield 45
    full=sample('whole_confirmed_full')
    assert full['policy']['confirmed'] and full['coverage']==1
    yield 3
    pose(-90)
    yield 45
    gray=sample('static_whole_memory')
    assert gray['stale']==1 and gray['proxies']>0
    yield 3
    yield from reset(R.SPATIAL_PARTIAL)
    pose(146)
    yield 45
    sample('partial_legal_live')
    yield 3
    pose(-90)
    yield 45
    partial=sample('static_partial_memory_cap')
    assert partial['stale']==1 and partial['caps']>0
    yield 3
    for reveal in (R.WHOLE_OBJECT_AFTER_SPAN,R.SPATIAL_PARTIAL):
        yield from reset(reveal)
        pose(90)
        yield 45
        trigger(-300,260)
        yield 45
        sample(f'{reveal.name}_moving_live')
        yield 3
        pose(-90)
        yield 45
        off=sample(f'{reveal.name}_moving_no_new_gray')
        assert off['stale']==off['resources']==0
        yield 180
    for reveal in (R.WHOLE_OBJECT_AFTER_SPAN,R.SPATIAL_PARTIAL):
        yield from reset(reveal,H.NEVER)
        pose(90)
        yield 45
        sample(f'{reveal.name}_never_live')
        yield 3
        pose(-90)
        yield 45
        off=sample(f'{reveal.name}_never_off')
        assert off['stale']==off['resources']==0
        yield 3
    # Real scan across the six static fixtures; inspect them together afterward.
    for yaw in range(-170,-9,5):
        pose(yaw,-300,-600,target=(0,-900,70))
        yield 2
    pose(90,-300,-600,target=(0,-900,70))
    yield 45
    camera=player.get_component_by_class(unreal.CameraComponent)
    origin=unreal.Vector(-400,-1400,1600)
    camera.set_world_location(origin,False,True)
    camera.set_world_rotation(unreal.MathLibrary.find_look_at_rotation(origin,unreal.Vector(0,-900,0)),False,True)
    sample('six_policy_static_overview',unreal.Name('Lab.Gray.Static.1'))
    yield 3
    for i in range(6):
        target=unreal.Name(f'Lab.Gray.Static.{i}')
        row=sample(f'six_policy_{i}',target,False)
        assert row['policy']['reveal_mode']==(0 if i<3 else 1)
        assert row['policy']['history_mode']==i%3
    # Real observer behind the original wall: no eligibility can be inferred.
    yield from reset()
    pose(90,500,-600,target=(500,650,75))
    yield 45
    wall=sample('wall_occluded')
    assert wall['coverage']==0 and not wall['policy']['confirmed']
    yield 3
    yield from reset()
    pose(90)
    yield 45
    furniture=unreal.GameplayStatics.get_all_actors_of_class(world(),unreal.DarkwellPropLabFurniture)
    source=[a for a in furniture if a.get_editor_property('stable_id')==sid]
    assert len(source)==1
    source[0].set_actor_location(unreal.Vector(500,0,0),False,True)
    pose(90,500,-600,target=(500,0,75))
    yield 45
    halfwall=sample('confirmed_whole_partial_wall')
    assert halfwall['policy']['confirmed'] and 0<halfwall['coverage']<1
    yield 3
    # Fast and slow routes have identical endpoints outside the legal cone.
    for fast in (True,False):
        yield from reset()
        pose(0)
        yield 20
        for yaw in ([160] if fast else range(5,161,5)):
            pose(yaw)
            yield 1
        yield 30
        row=sample('fast_sweep' if fast else 'slow_sweep')
        assert row['policy']['confirmed'] and row['stale']==1 and row['coverage']==0
        yield 3
    # Repeated actual rotation and look changes, with ownership checks each second.
    yield from reset()
    pose(90)
    yield 45
    for cycle in range(60 if '-GrayGpuLongInteraction' in unreal.SystemLibrary.get_command_line() else 6):
        trigger(-300,260)
        for frame in range(300):
            pose(90+80*math.sin((cycle*300+frame)*.012))
            yield 1
            if frame%60==0:
                sample(f'interaction_{cycle}_{frame}',shot=frame==180)
        yield 30
    yield 30
    expected=[row for row in rows if 'file' in row]
    landed=[p for row in expected for p in root.glob(Path(row['file']).stem+'*.png')]
    assert len(landed)==len(expected)
    (root/'checks.json').write_text(json.dumps(dict(rows=rows,screenshots=len(landed),passed=True),indent=2),encoding='utf-8')
    unreal.log('GRAY_GPU_PASS '+str(root))
    levels.editor_request_end_play()
    yield 30
    assert editor.get_game_world() is None,'PIE did not stop'
    unreal.log('GRAY_GPU_PIE_STOPPED')


sequence=run()
frames_left=0
last_game_time=None


def tick(_delta):
    global frames_left,last_game_time
    try:
        assert time.monotonic()-start<900,'GPU evidence exceeded 15 minutes'
        game_world=world()
        game_time=unreal.GameplayStatics.get_time_seconds(game_world) if game_world else None
        if game_time is not None and game_time==last_game_time:
            return
        last_game_time=game_time
        if frames_left>0:
            frames_left-=1
            return
        frames_left=max(0,next(sequence)-1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log('GRAY_GPU_CALLBACK_UNREGISTERED')
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
    except Exception as error:
        (root/'failed.json').write_text(json.dumps(dict(error=repr(error),rows=rows),indent=2),encoding='utf-8')
        unreal.log_error('GRAY_GPU_FAIL '+traceback.format_exc())
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')


handle=unreal.register_slate_post_tick_callback(tick)

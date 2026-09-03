"""Real D3D12 continuous current reveal evidence; three policies and partial coverage.

No visibility, D/V/R, history or geometry evidence is injected. Console is used
only for screenshots/render settings; motion uses the existing F-control entry.
Native tests separately exercise its real focus/trace/interaction chain.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
root = Path(unreal.Paths.project_saved_dir()) / 'MovingLiveContinuity' / time.strftime('GPU_%Y%m%d_%H%M%S')
root.mkdir(parents=True, exist_ok=False)
rows = []
player = controller = room = None
sid = unreal.Name('Lab.InWorld.Rotate.Cabinet')
start = time.monotonic()


def world():
    return editor.get_game_world()


def pose(yaw):
    player.set_actor_location(unreal.Vector(-300, 100, 92), False, True)
    player.set_actor_rotation(unreal.Rotator(yaw=yaw), True)
    # Camera-only inspection view; player position/sight authority is unchanged.
    boom=player.get_component_by_class(unreal.SpringArmComponent)
    camera=player.get_component_by_class(unreal.CameraComponent)
    boom.set_component_tick_enabled(False)
    origin=unreal.Vector(-550,850,300)
    camera.set_world_location(origin,False,True)
    camera.set_world_rotation(unreal.MathLibrary.find_look_at_rotation(origin,unreal.Vector(-300,650,75)),False,True)


def trigger(x, y):
    controls = unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellMovingPropLabControl)
    found = [a for a in controls if abs(a.get_actor_location().x-x)<2 and abs(a.get_actor_location().y-y)<2]
    assert len(found)==1 and found[0].trigger_for_lab_evidence(player)


def sample(label, target=None):
    target = sid if target is None else target
    row = dict(label=label, time=unreal.GameplayStatics.get_time_seconds(world()),
               yaw=room.get_tracked_transform(target).rotation.rotator().yaw,
               observer_yaw=player.get_actor_rotation().yaw,
               coverage=room.get_last_legal_coverage_ratio_for_testing(target),
               current=room.get_current_epoch_count_for_testing(target),
               stale=room.get_stale_epoch_count_for_testing(target),
               proxies=room.get_visible_historical_proxy_count_for_testing(target),
               caps=room.get_visible_historical_cap_count_for_testing(target),
               history_resources=room.get_historical_presentation_resource_count_for_testing(target),
               policy=room.get_history_policy_telemetry(target),
               runtime=json.loads(room.get_history_runtime_telemetry()),
               viewport=list(controller.get_viewport_size()))
    row['moving_live']=json.loads(room.get_moving_live_telemetry(target))
    row['capture_note']='Request-time state; deferred Shot is rendered on the following frame.'
    row['file']=f'{len(rows):04}_{label}.png'
    rows.append(row)
    unreal.SystemLibrary.execute_console_command(world(),f'Shot SHOWUI filename="{(root/row["file"]).as_posix()}"')
    unreal.log('MOVING_LIVE_GPU_FRAME '+json.dumps(row))
    return row


def run():
    global player, controller, room
    levels.editor_request_begin_play()
    yield 120
    controller=unreal.GameplayStatics.get_player_controller(world(),0)
    player=unreal.GameplayStatics.get_player_pawn(world(),0)
    room=unreal.GameplayStatics.get_all_actors_of_class(world(),unreal.DarkwellMovingPropLabRoom)[0]
    controller.set_actor_tick_enabled(False)
    player.set_actor_tick_enabled(False)
    unreal.SystemLibrary.execute_console_command(world(),'r.ScreenPercentage 100')
    unreal.SystemLibrary.execute_console_command(world(),'r.AntiAliasingMethod 4')
    assert not unreal.GameplayStatics.get_all_actors_of_class(world(),unreal.DarkwellStalkerCharacter)
    modes=(unreal.SightWeaveHistoryMode.ALWAYS,unreal.SightWeaveHistoryMode.STATIONARY_ONLY,unreal.SightWeaveHistoryMode.NEVER)
    for index, mode in enumerate(modes):
        name=('Always','StationaryOnly','Never')[index]
        if index:
            trigger(1500,-1050)
            yield 5
        assert room.reset_tracked_policy_for_lab(sid,mode)
        pose(90)
        yield 60
        before=sample(name+'_initial_live')
        assert before['coverage']==1
        yield 3
        trigger(-300,260)
        while 'MOVING 1' not in room.get_history_policy_telemetry(sid):
            yield 1
        for frame in range(240):
            yield 1
            telemetry=json.loads(room.get_moving_live_telemetry(sid))
            assert telemetry['current']==1 and telemetry['stale']==0
            assert telemetry['initialize']==1 and telemetry['texture_creations']==4
            assert min(telemetry['appearance'])>.99 and min(telemetry['live'])>.99,telemetry
            if frame%3==0:
                sample(name+f'_full_rotation_{frame:03}')
        yield 30
        sample(name+'_final_live')
        yield 3
        pose(-90)
        yield 60
        off=sample(name+'_after_view_loss')
        if index==2: assert off['current']==off['stale']==off['history_resources']==0
        if index==1: assert off['stale']==1
        yield 3
        # Independent explicit reset for partial coverage and mid-motion loss.
        trigger(1500,-1050)
        yield 5
        assert room.reset_tracked_policy_for_lab(sid,mode)
        pose(90)
        yield 60
        trigger(-300,260)
        while 'MOVING 1' not in room.get_history_policy_telemetry(sid):
            yield 1
        pose(146)
        for frame in range(120):
            yield 1
            if frame%6==0: sample(name+f'_partial_rotation_{frame:03}')
        pose(-90)
        yield 150
        hidden=sample(name+'_moving_loss_hidden_finish')
        if index: assert hidden['stale']==hidden['current']==hidden['history_resources']==0
        else: assert hidden['stale']>0
        yield 3
    yield 30
    landed=[p for row in rows for p in root.glob(Path(row['file']).stem+'*.png')]
    assert len(landed)==len(rows)
    (root/'checks.json').write_text(json.dumps(dict(rows=rows,screenshots=len(landed),passed=True),indent=2),encoding='utf-8')
    unreal.log('MOVING_LIVE_GPU_PASS '+str(root))
    levels.editor_request_end_play()
    yield 30
    assert editor.get_game_world() is None, "PIE did not stop"
    unreal.log("MOVING_LIVE_GPU_PIE_STOPPED")


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
        unreal.log("MOVING_LIVE_GPU_CALLBACK_UNREGISTERED")
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
    except Exception as error:
        (root/'failed.json').write_text(json.dumps(dict(error=repr(error),rows=rows),indent=2),encoding='utf-8')
        unreal.log_error('MOVING_LIVE_GPU_FAIL '+traceback.format_exc())
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')


handle=unreal.register_slate_post_tick_callback(tick)

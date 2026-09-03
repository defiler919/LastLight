"""Real D3D12 Lab policy evidence. Host reset explicitly re-registers one object.

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
root = Path(unreal.Paths.project_saved_dir()) / 'HistoryPolicy' / time.strftime('GPU_%Y%m%d_%H%M%S')
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
    row['capture_note']='Request-time state; deferred Shot is rendered on the following frame.'
    row['file']=f'{len(rows):04}_{label}.png'
    rows.append(row)
    unreal.SystemLibrary.execute_console_command(world(),f'Shot SHOWUI filename="{(root/row["file"]).as_posix()}"')
    unreal.log('HISTORY_POLICY_GPU_FRAME '+json.dumps(row))
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
        assert sample(name+'_initial_live')['coverage']==1
        yield 3
        trigger(-300,260)
        # F starts with a one-second stationary delay. Looking away there is
        # legitimately pre-existing history, not a moving-capture violation.
        while 'MOVING 1' not in room.get_history_policy_telemetry(sid):
            yield 1
        pose(-90)
        yield 120
        before=sample(name+'_moving_hidden')
        if index: assert before['stale']==0 and before['current']==0 and before['history_resources']==0
        yield 3
        pose(90)
        for frame in range(12):
            yield 1
            sample(name+f'_moving_live_adjacent_{frame:02}')
        pose(-90)
        yield 30
        pose(146)
        yield 6
        partial=sample(name+'_partial_middle')
        yield 3
        pose(-90)
        yield 303
        hidden=sample(name+'_hidden_finish')
        assert abs(abs(hidden['yaw'])-180)<.01
        if index: assert hidden['stale']==0 and hidden['current']==0 and hidden['history_resources']==0
        else: assert hidden['stale']>0
        yield 3
        pose(90)
        yield 60
        assert sample(name+'_stationary_reobserved')['coverage']==1
        yield 3
        pose(-90)
        yield 60
        final=sample(name+'_final_gray_or_hidden')
        if index==1: assert final['stale']==1 and final['proxies']==1
        if index==2: assert final['stale']==0 and final['current']==0 and final['history_resources']==0
        yield 3
        if index==2:
            for cycle in range(3):
                pose(90)
                yield 30
                sample(f'Never_cycle_{cycle}_live')
                yield 3
                pose(-90)
                yield 30
                off=sample(f'Never_cycle_{cycle}_hidden')
                assert off['current']==off['stale']==off['history_resources']==0
                yield 3
    # Same live world, two independently moving policies plus one absent Never object.
    trigger(1500,-1050)
    yield 5
    multi=[unreal.Name('Lab.InWorld.Multi.'+suffix) for suffix in ('HighCabinet','LowCabinet','SmallBox')]
    for target, mode in zip(multi,modes):
        assert room.reset_tracked_policy_for_lab(target,mode)
        p=room.get_tracked_transform(target).translation
        player.set_actor_location(unreal.Vector(p.x,p.y+350,92),False,True)
        player.set_actor_rotation(unreal.Rotator(yaw=-90),True)
        yield 30
        sample('Multi_observe_'+str(target),target)
        yield 3
    old_stationary=room.get_stale_epoch_count_for_testing(multi[1])
    trigger(-1200,-1050)
    player.set_actor_location(unreal.Vector(-900,-150,92),False,True)
    player.set_actor_rotation(unreal.Rotator(yaw=90),True)
    yield 120
    assert 'MOVING 1' in room.get_history_policy_telemetry(multi[0])
    assert 'MOVING 1' in room.get_history_policy_telemetry(multi[1])
    assert not room.is_actual_present(multi[2])
    for target in multi:
        sample('Multi_moving_'+str(target),target)
        yield 3
    assert room.get_stale_epoch_count_for_testing(multi[1])==old_stationary
    assert room.get_stale_epoch_count_for_testing(multi[2])==0
    yield 250
    for target in multi:
        assert 'MOVING 0' in room.get_history_policy_telemetry(target)
        sample('Multi_finished_'+str(target),target)
        yield 3
    yield 30
    landed=[p for row in rows for p in root.glob(Path(row['file']).stem+'*.png')]
    assert len(landed)==len(rows)
    (root/'checks.json').write_text(json.dumps(dict(rows=rows,screenshots=len(landed),passed=True),indent=2),encoding='utf-8')
    unreal.log('HISTORY_POLICY_GPU_PASS '+str(root))
    levels.editor_request_end_play()
    yield 30


sequence=run()
frames_left=0


def tick(_delta):
    global frames_left
    if frames_left>0:
        frames_left-=1
        return
    try:
        assert time.monotonic()-start<900,'GPU evidence exceeded 15 minutes'
        frames_left=max(0,next(sequence)-1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
    except Exception as error:
        (root/'failed.json').write_text(json.dumps(dict(error=repr(error),rows=rows),indent=2),encoding='utf-8')
        unreal.log_error('HISTORY_POLICY_GPU_FAIL '+traceback.format_exc())
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')


handle=unreal.register_slate_post_tick_callback(tick)

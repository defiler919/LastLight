"""Real Editor PIE verification of the free-play room, not a runtime route.

Only this test driver positions/aims the player. The delivered room never does so
except reset/explicit teleport; it has no timer or auto-finish. Controller mouse
aim is paused by this driver and restored before exiting PIE.
"""
import json
import time
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
root=Path(unreal.Paths.project_saved_dir())/'PropGameplayLab'/('ManualSwitchPIE_'+time.strftime('%Y%m%d_%H%M%S'))
root.mkdir(parents=True,exist_ok=True)
rows=[]
deadline=0
start=time.monotonic()
failed=False
handle=None
player=controller=room=None

def world():return editor.get_game_world()
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(world(),cls)
def cmd(s):unreal.SystemLibrary.execute_console_command(world(),'Darkwell.PropLab '+s)
def pose(x,y,yaw):
    player.set_actor_location(unreal.Vector(x,y,92),False,True)
    player.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),True)
def sample(label):
    row=dict(label=label,status=room.get_status(),actual=room.has_actual_cabinet(),toggles=room.get_toggle_count(),armed=room.is_switch_armed(),verified=room.get_verified_fraction(),opacity=room.get_remaining_opacity(),coverage=room.get_cabinet_coverage())
    rows.append(row)
    unreal.log('MANUAL_PIE_CHECK '+json.dumps(row))
    assert len(actors(unreal.DarkwellStalkerCharacter))==0
    assert unreal.SystemLibrary.get_console_variable_int_value('r.Darkwell.ProjectFogVisual.LabRoute')==0
    assert 'RULE SpatialEvidenceOnly' in row['status']
    return row
def shot(name):
    unreal.SystemLibrary.execute_console_command(world(),f'Shot SHOWUI filename="{(root/(name+".png")).as_posix()}"')
def cabinet():
    items=[a for a in actors(unreal.DarkwellPropLabFurniture) if str(a.get_editor_property('stable_id'))=='Lab.ManualStale.Cabinet']
    assert len(items)<=1
    return items[0] if items else None
def hidden_source():
    a=cabinet()
    return a is None or not any(m.is_visible() for m in a.get_components_by_class(unreal.StaticMeshComponent))

def run():
    global player,controller,room
    yield 2
    levels.editor_request_begin_play()
    yield 6
    assert levels.is_in_play_in_editor()
    room=actors(unreal.DarkwellManualStaleRoom)[0]
    player=unreal.GameplayStatics.get_player_pawn(world(),0)
    controller=unreal.GameplayStatics.get_player_controller(world(),0)
    controller.set_actor_tick_enabled(False)
    unreal.log('MANUAL_PIE_TEST_DRIVER mouseAimPaused=1 runtimeRoute=0')
    pose(4500,150,90)
    yield 1
    s=sample('initial_real_observation')
    assert s['actual'] and 'Snapshot: VALID' in s['status'] and cabinet().get_actor_enable_collision()
    shot('01_top_present')
    yield .4
    # Ten full absent/present cycles. No observation of the cabinet between presses.
    for cycle in range(10):
        pose(4500,-450,90)
        yield .4
        s=sample(f'cycle_{cycle+1}_absent')
        assert not s['actual'] and s['toggles']==cycle*2+1 and s['coverage']==0
        assert s['opacity']==1 and s['verified']==0 and 'Snapshot: VALID' in s['status']
        pose(4500,-450,270)
        yield .4
        assert room.get_toggle_count()==cycle*2+1 and not room.is_switch_armed()
        if cycle==0:
            shot('02_bottom_absent_memory_retained')
            yield .4
            for m in (2,0,1):
                cmd(f'stalemanual mode {m}')
                yield .2
                q=sample(f'absent_mode_{m}')
                assert q['actual']==s['actual'] and q['verified']==s['verified'] and q['opacity']==s['opacity']
        pose(4280,-450,90)
        yield .3
        assert room.is_switch_armed()
        pose(4500,-450,90)
        yield .4
        s=sample(f'cycle_{cycle+1}_present_unseen')
        assert s['actual'] and s['toggles']==cycle*2+2 and s['coverage']==0 and hidden_source()
        assert s['verified']==0 and 'Snapshot: VALID' in s['status'] and cabinet().get_actor_enable_collision()
        pose(4280,-450,90)
        yield .3
    unreal.log('MANUAL_PIE_TEN_CYCLES_PASS presses=20')
    # Same manual input samples in all modes; there is no 36-second room timeline.
    for mode in (0,1,2):
        cmd('stalemanual reset')
        cmd(f'stalemanual mode {mode}')
        pose(4500,150,90)
        yield .5
        assert 'Snapshot: VALID' in room.get_status()
        pose(4500,-450,90)
        yield .4
        assert room.get_toggle_count()==1 and not room.has_actual_cabinet()
        pose(4500,150,0)
        yield .3
        partial=False
        for yaw in range(0,181,3):
            pose(4500,150,yaw)
            yield .09
            fraction=room.get_verified_fraction()
            if 0<fraction<1:
                if mode==0:assert room.get_remaining_opacity()==1
                if not partial and fraction>.4:
                    partial=True
                    sample(f'mode_{mode}_partial')
                    shot(f'03_mode_{mode}_partial')
                    yield .3
        yield .4
        s=sample(f'mode_{mode}_empty')
        assert partial and s['verified']==1 and s['opacity']==0 and 'Snapshot: EMPTY' in s['status']
        pose(4280,-450,90)
        yield .3
        for m in (2,0,1):
            cmd(f'stalemanual mode {m}')
            yield .15
            assert not room.has_actual_cabinet() and room.get_remaining_opacity()==0
        pose(4500,-450,90)
        yield .4
        s=sample(f'mode_{mode}_respawn_after_empty')
        assert s['actual'] and 'Snapshot: EMPTY' in s['status'] and hidden_source()
        pose(4500,150,90)
        yield .5
        s=sample(f'mode_{mode}_rediscovered')
        assert 'Snapshot: VALID' in s['status'] and not hidden_source()
    # Partial erasure must not resurrect on a mid-observation mode change.
    cmd('stalemanual reset');cmd('stalemanual mode 1')
    pose(4500,150,90)
    yield .5
    pose(4500,-450,90)
    yield .4
    pose(4500,150,55)
    yield .5
    s=sample('partial_before_mode_change')
    assert 0<s['verified']<1 and 0<s['opacity']<1
    pose(4280,-450,90)
    yield .3
    for m in (0,2,1):
        cmd(f'stalemanual mode {m}')
        yield .3
        q=sample(f'partial_after_mode_{m}')
        assert q['verified']==s['verified'] and q['opacity']<=s['opacity']+.00001 and not q['actual']
    # Neither 36 seconds nor ordinary ticks restore player/cabinet state.
    unreal.GameplayStatics.apply_damage(player,10,None,None,unreal.DamageType)
    yield 37
    assert player.get_editor_property('health')==90 and not room.has_actual_cabinet()
    assert player.get_component_by_class(unreal.DarkwellLoadoutComponent).get_editor_property('torch_charge')==100
    sample('no_timer_or_auto_restore_after_37s')
    cmd('stalemanual reset')
    yield .4
    assert player.get_editor_property('health')==100 and room.has_actual_cabinet() and room.get_toggle_count()==0
    # Exercise actual CharacterMovement/collision, not teleports: the divider
    # blocks a direct walk, while both doors and the right corridor are passable.
    pose(4500,150,90)
    yield .4
    for _ in range(90):
        player.add_movement_input(unreal.Vector(0,-1,0),1,True)
        yield 0
    assert player.get_actor_location().y>=50,'Player crossed opaque divider'
    for x,y in ((3450,150),(3450,500),(3150,500),(3150,-500),(4500,-500)):
        for _ in range(1800):
            p=player.get_actor_location();dx=x-p.x;dy=y-p.y;distance=(dx*dx+dy*dy)**.5
            if distance<35:break
            player.add_movement_input(unreal.Vector(dx/distance,dy/distance,0),1,True)
            yield 0
        else:raise RuntimeError('Right corridor walk blocked')
    yield .4
    s=sample('physical_walk_through_right_corridor')
    assert s['toggles']==1 and not s['actual'] and s['coverage']==0 and 'Snapshot: VALID' in s['status']
    shot('04_right_corridor_walk_switch')
    yield .4
    unreal.log('MANUAL_PIE_WALK_PASS dividerBlocks=1 rightCorridorPassable=1 switchTriggeredByMovement=1')
    controller.set_actor_tick_enabled(True)
    levels.editor_request_end_play()
    yield 2
    assert not levels.is_in_play_in_editor()
    (root/'checks.json').write_text(json.dumps(rows,indent=2))
    unreal.log('MANUAL_PIE_PASS modes=3 fullCycles=10 noTimer=1 reset=1')

sequence=run()
def tick(dt):
    global deadline,sequence,failed
    now=time.monotonic()
    if now<deadline:return
    try:
        assert failed or now-start<240,'Manual PIE timeout'
        deadline=now+next(sequence)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
    except Exception as error:
        import traceback
        failed=True
        unreal.log_error('MANUAL_PIE_FAIL '+repr(error)+'\n'+traceback.format_exc())
        (root/'failed_checks.json').write_text(json.dumps(rows,indent=2))
        if controller:controller.set_actor_tick_enabled(True)
        levels.editor_request_end_play()
        def quit_later():
            yield 2
        sequence=quit_later();deadline=now+2
handle=unreal.register_slate_post_tick_callback(tick)

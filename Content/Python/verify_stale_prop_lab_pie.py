"""Real Editor PIE lifecycle and command test. No simulated world or user policy choice."""
import math
import re
import time
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
start=time.monotonic();stage=0;mode=0;deadline=0;handle=None;checked=False;clock_offset=None
def cmd(s):unreal.SystemLibrary.execute_console_command(editor.get_game_world(),s)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(editor.get_game_world(),cls)
def experiment():return actors(unreal.DarkwellPropGameplayLab)[0].get_component_by_class(unreal.DarkwellStalePropLabComponent)
def assert_defaults():
    assert len(actors(unreal.DarkwellStalkerCharacter))==0
    player=unreal.GameplayStatics.get_player_pawn(editor.get_game_world(),0)
    tool=player.get_component_by_class(unreal.DarkwellLoadoutComponent)
    assert player.get_editor_property('health')==100 and tool.get_editor_property('torch_charge')==100
def route_yaw(t):
    return 90+70*math.sin(t*math.pi/3) if t<6 else -90 if t<10 or t>=32 else -40+(t-10)*13 if t<20 else 90 if t<22 else 90+(t-22)*13

def hud_yaw_error(t,yaw):
    # HUD time has only two decimals. At a phase boundary, include both
    # possible sides of its rounding interval; native diagnostics use full precision.
    return min(abs((yaw-route_yaw(sample)+180)%360-180) for sample in (t-.005,t,t+.005))

def assert_route_pose(status):
    global clock_offset
    match=re.search(r'\bt=([0-9.]+)',status)
    if not match:return
    t=float(match[1]);world=editor.get_game_world()
    player=unreal.GameplayStatics.get_player_pawn(world,0)
    camera=player.get_component_by_class(unreal.CameraComponent)
    rotation=camera.get_world_rotation();position=camera.get_world_location()
    assert abs(rotation.pitch+65)<.01 and abs(rotation.yaw-90)<.01 and abs(rotation.roll)<.01,str(rotation)
    assert abs(position.x-400)<.01 and abs(position.y+828.273)<.01 and abs(position.z-1224.885)<.01,str(position)
    assert hud_yaw_error(t,player.get_actor_rotation().yaw)<.5,'PIE actual aim differs from HUD time rounding interval'
    offset=unreal.GameplayStatics.get_time_seconds(world)-t
    if clock_offset is None:clock_offset=offset
    assert abs(offset-clock_offset)<.02,'PIE route/world clock offset changed'
def tick(dt):
    global stage,mode,deadline,checked,clock_offset
    now=time.monotonic()
    if now-start>260:raise RuntimeError('PIE timeout')
    if stage==0 and now-start>2:
        levels.editor_request_begin_play();stage=1;deadline=now+5
    elif stage==1 and now>deadline:
        assert levels.is_in_play_in_editor();assert_defaults()
        unreal.log('STALE_PIE_DEFAULT_PASS enemy=0 health=100 torch=100')
        cmd('Darkwell.PropLab stale 0 C');stage=2;deadline=now+2
    elif stage==2 and now>deadline:
        assert_defaults()
        status=experiment().get_status()
        assert_route_pose(status)
        if not checked:
            assert unreal.SystemLibrary.get_console_variable_int_value('r.Darkwell.ProjectFogVisual.LabRoute')==14,'Cursor input must be suppressed by existing route guard'
            cmd('Darkwell.PropLab policy 1');cmd('Darkwell.PropLab enemy 1')
            assert len(actors(unreal.DarkwellStalkerCharacter))==0
            assert unreal.SystemLibrary.get_console_variable_int_value('r.Darkwell.ProjectFogVisual.PropRelocationPolicy')==0
            checked=True
        if 'ROUND FINISHED' in status:
            assert f'MODE {mode} POLICY 0 CASE C' in status and '100.0% empty=1 ghost=0' in status,status
            assert len(actors(unreal.DarkwellPropLabFurniture))==25,'Transient experiment not cleaned up'
            unreal.log(f'STALE_PIE_MODE_PASS mode={mode} policy=0 whole=1 ghost=0 restoredFurniture=25 cameraRotation=(-65,90,0) routeWorldClockStable=1')
            mode+=1;checked=False;clock_offset=None
            if mode<3:cmd(f'Darkwell.PropLab stale {mode} C');deadline=now+2
            else:
                player=unreal.GameplayStatics.get_player_pawn(editor.get_game_world(),0)
                unreal.GameplayStatics.apply_damage(player,35,None,None,unreal.DamageType)
                assert player.get_editor_property('health')==65
                cmd('Darkwell.PropLab dark');cmd('Darkwell.PropLab reset');stage=3;deadline=now+6
    elif stage==3 and now>deadline:
        assert_defaults()
        for name in ('PropPresentationMode','PropRelocationPolicy','LabRoute'):
            assert unreal.SystemLibrary.get_console_variable_int_value('r.Darkwell.ProjectFogVisual.'+name)==0
        assert len(actors(unreal.DarkwellPropLabFurniture))==25
        unreal.log('STALE_PIE_RESET_PASS defaults=0/0 enemy=0 health=100 torch=100')
        levels.editor_request_end_play();stage=4;deadline=now+2
    elif stage==4 and now>deadline:
        assert not levels.is_in_play_in_editor()
        unreal.log('STALE_PIE_PASS')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
def guarded(dt):
    try:tick(dt)
    except Exception as error:
        unreal.log_error('STALE_PIE_FAIL '+str(error));unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
handle=unreal.register_slate_post_tick_callback(guarded)

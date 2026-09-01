"""Real PIE check of defaults, explicit enemy controls, route replay and reset."""
import time
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
start=time.monotonic();stage=0;handle=None
def cmd(s): unreal.SystemLibrary.execute_console_command(editor.get_game_world(),s)
def enemies(): return unreal.GameplayStatics.get_all_actors_of_class(editor.get_game_world(),unreal.DarkwellStalkerCharacter)
def state():
    player=unreal.GameplayStatics.get_player_pawn(editor.get_game_world(),0)
    return player,player.get_component_by_class(unreal.DarkwellLoadoutComponent)
def tick(dt):
    global stage
    t=time.monotonic()-start
    if stage==0 and t>2:
        levels.editor_request_begin_play();stage=1
    elif stage==1 and t>9:
        assert levels.is_in_play_in_editor() and len(enemies())==0
        player,tool=state()
        assert player.get_editor_property('health')==100 and tool.get_editor_property('torch_charge')==100
        unreal.log('PROP_RETEST_PIE_DEFAULT_PASS enemy=0 health=100 torch=100')
        cmd('Darkwell.PropLab mode 1');cmd('Darkwell.PropLab route 1');cmd('Darkwell.PropLab enemy 1')
        assert len(enemies())==0, 'Route command must reject enemy immediately, before the next tick'
        stage=2
    elif stage==2 and t>13:
        assert len(enemies())==0
        cmd('Darkwell.PropLab mode 2');cmd('Darkwell.PropLab route 1')
        unreal.log('PROP_RETEST_PIE_ROUTE_PASS rule=SpatialEvidenceOnly enemy=0 replay=1')
        stage=3
    elif stage==3 and t>17:
        cmd('Darkwell.PropLab route 0');stage=4
    elif stage==4 and t>18:
        cmd('Darkwell.PropLab enemy 1');stage=5
    elif stage==5 and t>20:
        assert len(enemies())==1
        cmd('Darkwell.PropLab enemy 0')
        stage=6
    elif stage==6 and t>22:
        assert len(enemies())==0
        player,tool=state()
        unreal.GameplayStatics.apply_damage(player,60,None,None,unreal.DamageType)
        assert player.get_editor_property('health')==40
        cmd('Darkwell.PropLab dark')
        cmd('Darkwell.PropLab reset');stage=7
    elif stage==7 and t>28:
        assert len(enemies())==0
        player,tool=state()
        assert player.get_editor_property('health')==100 and tool.get_editor_property('torch_charge')==100
        for name in ('PropPresentationMode','LabRoute'):
            assert unreal.SystemLibrary.get_console_variable_int_value('r.Darkwell.ProjectFogVisual.'+name)==0
        unreal.log('PROP_RETEST_PIE_RESET_PASS enemy=0 health=100 torch=100 mode=0 rule=SpatialEvidenceOnly route=0')
        levels.editor_request_end_play();stage=8
    elif stage==8 and t>30:
        assert not levels.is_in_play_in_editor()
        unreal.log('PROP_RETEST_PIE_PASS')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
def guarded(dt):
    try:tick(dt)
    except Exception as error:
        unreal.log_error('PROP_RETEST_PIE_FAIL '+str(error));unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
handle=unreal.register_slate_post_tick_callback(guarded)

"""OldHistory64FewDemand: 64 aged Whole/Partial histories and a fixed camera route.

Standalone runs measure complete wall frames; PIE visual runs use viewport
readback separately. Original SyntheticCold184Stress inputs remain unchanged.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

root = Path(os.environ['DARKWELL_STABILIZATION_OUTPUT']).resolve()
run_mode = os.environ['DARKWELL_STABILIZATION_MODE']
protocol = os.environ['DARKWELL_STABILIZATION_PROTOCOL']
standalone = run_mode == 'Standalone'
MAP = '/Game/Maps/L_SightWeaveGrayPolicyLab'
root.mkdir(parents=True, exist_ok=True)
levels = editor = None
if not standalone:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    assert levels.load_level(MAP)

started = time.perf_counter()
last_tick = None
latest_wall_ms = 0

def world():
    return unreal.find_object(None, MAP + '.L_SightWeaveGrayPolicyLab') if standalone else editor.get_game_world()

def run():
    if not standalone:
        unreal.DarkwellEditorDiagnostics.start_performance_pie()
    while world() is None:
        yield 1
    yield 1
    w = world()
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DarkwellSightWeaveGrayPolicyLabDirector)[0]
    room = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DarkwellMovingPropLabRoom)[0]
    player = unreal.GameplayStatics.get_player_character(w, 0)
    controller = unreal.GameplayStatics.get_player_controller(w, 0)
    player.set_actor_tick_enabled(False)
    controller.set_actor_tick_enabled(False)
    assert director.set_audit_viewport_size_for_testing(1920,1080)
    for command in ['t.MaxFPS 0', 'r.VSync 0', 'r.ScreenPercentage 100', 'r.SecondaryScreenPercentage.GameViewport 100', 'r.AntiAliasingMethod 4', 'r.DynamicRes.OperationMode 0']:
        unreal.SystemLibrary.execute_console_command(w, command)
    if not standalone:
        unreal.DarkwellEditorDiagnostics.focus_performance_pie()
    startup=[]
    (root/'viewport-ready.json').write_text(director.get_frame_environment_for_testing(),encoding='utf-8')
    foreground_deadline=time.perf_counter()+90
    while not json.loads(director.get_frame_environment_for_testing())['foreground']:
        assert time.perf_counter()<foreground_deadline, 'OS foreground was not established; sample is invalid'
        startup.append(dict(phase='waiting_for_foreground',wall_ms=latest_wall_ms,engine=json.loads(director.get_frame_environment_for_testing())))
        yield 1
    for startup_index in range(12):
        yield 1
        startup.append(dict(index=startup_index,wall_ms=latest_wall_ms,engine=json.loads(director.get_frame_environment_for_testing()),telemetry=json.loads(room.get_history_runtime_telemetry())['frame_data']))
    (root/'startup-frames.json').write_text(json.dumps(startup,indent=2),encoding='utf-8')
    quality_names = ['sg.'+q+'Quality' for q in ['ViewDistance','AntiAliasing','Shadow','GlobalIllumination','Reflection','PostProcess','Texture','Effects','Foliage','Shading']]
    quality_names += ['r.RayTracing','r.Lumen.HardwareRayTracing','r.Shadow.Virtual.Enable','r.TemporalAA.Upsampling','r.Editor.Viewport.OverridePIEScreenPercentage','Slate.bAllowThrottling','t.IdleWhenNotForeground','r.GTSyncType','r.OneFrameThreadLag']
    settings = {key:unreal.SystemLibrary.get_console_variable_float_value(key) for key in quality_names}
    settings.update(mode=run_mode, protocol=protocol, editor_realtime=False if not standalone else None, initial=json.loads(director.get_frame_environment_for_testing()), seed=0, screenshots=os.environ.get('DARKWELL_A1_VISUAL')=='1')
    (root/'quality.json').write_text(json.dumps(settings,indent=2), encoding='utf-8')
    (root/'render_settings.json').write_text(json.dumps(dict(
        viewport=list(controller.get_viewport_size()),
        screen_percentage=unreal.SystemLibrary.get_console_variable_float_value('r.ScreenPercentage'),
        anti_aliasing_method=unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod'))), encoding='utf-8')
    camera = player.get_component_by_class(unreal.CameraComponent)
    player.get_component_by_class(unreal.SpringArmComponent).set_component_tick_enabled(False)
    camera.set_absolute(True,True,True)
    player.set_actor_rotation(unreal.Rotator(yaw=-90),False)
    camera.set_world_location(unreal.Vector(0,0,400),False,True)
    camera.set_world_rotation(unreal.Rotator(yaw=0),False,True)
    mode=unreal.SystemLibrary.get_console_variable_int_value('r.Darkwell.ObjectMemory.HistoryResidency')
    visual=os.environ.get('DARKWELL_A1_VISUAL')=='1'
    all_rows=[]
    def sample(phase,index):
        r=json.loads(room.get_history_runtime_telemetry())['frame_data']
        r.update(phase=phase,index=index,wall_ms=latest_wall_ms,
                 residency=json.loads(room.get_presentation_residency_telemetry()),
                 engine=json.loads(director.get_frame_environment_for_testing()))
        r['camera']=[camera.get_world_location().x,camera.get_world_location().y,camera.get_world_location().z,
                     camera.get_world_rotation().yaw]
        all_rows.append(r)
        return r
    begin=time.perf_counter()
    assert room.configure_old_history_demand_for_testing()
    setup_ms=(time.perf_counter()-begin)*1000
    created=unreal.GameplayStatics.get_time_seconds(w)
    # Real capture pin expires naturally; no test-only backdating. Both modes
    # have the same stationary preparation input and six-second age contract.
    while unreal.GameplayStatics.get_time_seconds(w)-created<6.0:
        yield 1
        sample('creation_and_pin',len(all_rows))
    initial_hash=room.get_old_history_evidence_hash_for_testing()
    route_rows=[]
    phases=[('few_needed',60),('slow_move',40),('boundary',40),('turn180',60),('teleport',60),('multiple_reentry',60)]
    for phase,count in phases:
        for i in range(count):
            pos=unreal.Vector(0,0,400);yaw=0
            if phase=='slow_move':pos.y=i*5
            if phase=='boundary':yaw=43 if i%2 else 47
            if phase=='turn180':yaw=180
            if phase=='teleport':pos=unreal.Vector(0,-11000,400);yaw=-90
            camera.set_world_location(pos,False,True)
            camera.set_world_rotation(unreal.Rotator(yaw=yaw),False,True)
            yield 1
            r=sample(phase,i);route_rows.append(r)
            assert r['residency']['missing']==0, 'Needed historical presentation missing on this frame'
            if i in (0,count-1):
                r['evidence_hash']=room.get_old_history_evidence_hash_for_testing()
            if visual and i<3:
                assert director.capture_game_viewport_for_testing(str(root/f'{phase}_{i:02}.png'))
    final_hash=room.get_old_history_evidence_hash_for_testing()
    with (root/'frames.jsonl').open('w',encoding='utf-8') as f:
        for row in all_rows:f.write(json.dumps(row)+'\n')
    result=dict(protocol='OldHistory64FewDemand',mode=mode,visual=visual,setup_ms=setup_ms,
                initial_hash=initial_hash,final_hash=final_hash,
                max_full_window_ms=max(r['wall_ms'] for r in all_rows),
                max_aged_route_ms=max(r['wall_ms'] for r in route_rows),
                max_residency_ms=max(r['residency']['frame_ms'] for r in route_rows),
                foreground_waits=sum(r.get('phase')=='waiting_for_foreground' for r in startup),
                foreground_bad=sum(not r['engine']['foreground'] for r in all_rows),
                phases={name:dict(max_frame_ms=max(r['wall_ms'] for r in route_rows if r['phase']==name),
                                 first=next(r['residency'] for r in route_rows if r['phase']==name),
                                 last=[r['residency'] for r in route_rows if r['phase']==name][-1]) for name,_ in phases})
    (root/'performance.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal.SystemLibrary.collect_garbage()
    yield 2
    (root/'post-gc.json').write_text(json.dumps(dict(residency=json.loads(room.get_presentation_residency_telemetry()),engine=json.loads(director.get_frame_environment_for_testing()))),encoding='utf-8')
    assert director.set_audit_viewport_size_for_testing(0,0)
    (root/'complete.json').write_text(json.dumps(dict(protocol_complete=True,cases=1)),encoding='utf-8')

sequence = run()
left = 0
last_game_time = None
def tick(_delta):
    global left, last_tick, latest_wall_ms, last_game_time
    try:
        assert time.perf_counter()-started < 900
        current_game = unreal.GameplayStatics.get_time_seconds(world()) if world() else None
        if current_game is not None and current_game == last_game_time:
            return
        last_game_time = current_game
        now = time.perf_counter()
        latest_wall_ms = (now-last_tick)*1000 if last_tick else 0
        last_tick = now
        if left:
            left -= 1
        else:
            left = max(0, next(sequence)-1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.log('MEMORY_TRANSITION_STOPPED')
        unreal.SystemLibrary.execute_console_command(world(), 'QUIT') if standalone else unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        (root/'failed.txt').write_text(traceback.format_exc(), encoding='utf-8')
        unreal.log_error(traceback.format_exc())
        if not standalone:
            levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(world(), 'QUIT') if standalone else unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle = unreal.register_slate_post_tick_callback(tick)

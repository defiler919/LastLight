"""Ordinary-map reference and render-size proof, separate from the frozen gray Matrix."""
import json
import math
import os
import time
import traceback
import sys
from pathlib import Path
import unreal

root = Path(os.environ['DARKWELL_STABILIZATION_OUTPUT'])
sys.path.insert(0, str(root))
from gray_benchmark_foreground import await_foreground, require_foreground
MAP = os.environ['DARKWELL_STABILIZATION_MAP']
standalone = os.environ['DARKWELL_STABILIZATION_MODE'] == 'Standalone'
editor = levels = None
if not standalone:
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    assert levels.load_level(MAP)
diagnostic = unreal.get_default_object(unreal.DarkwellSightWeaveGrayPolicyLabDirector)
started = last_tick = time.perf_counter()
last_game = None
wall_ms = 0

def world():
    return unreal.find_object(None, MAP+'.'+MAP.rsplit('/',1)[1]) if standalone else editor.get_game_world()

def environment():
    data = json.loads(diagnostic.get_frame_environment_for_testing())
    data['editor_realtime_count'] = 0 if standalone else unreal.DarkwellEditorDiagnostics.get_realtime_editor_viewport_count()
    return data

def distribution(values):
    a = sorted(values)
    streak = longest = 0
    for value in values:
        streak = streak+1 if value > 33 else 0
        longest = max(longest,streak)
    return dict(count=len(a),p50=a[int(len(a)*.5)],p95=a[int(len(a)*.95)],p99=a[int(len(a)*.99)],max=a[-1],
                over33=sum(v>33 for v in a),over100=sum(v>100 for v in a),longest_over33=longest)

def run():
    if not standalone:
        unreal.DarkwellEditorDiagnostics.start_performance_pie()
    while world() is None:
        yield
    w = world()
    if not standalone:
        assert unreal.DarkwellEditorDiagnostics.set_performance_viewport_size(1920,1080)
        yield
    controller = unreal.GameplayStatics.get_player_controller(w,0)
    assert controller is not None
    # The normal map starts at its paused main menu. Resume the already loaded
    # world through the native menu action, without loading or writing a save.
    paused_at_entry = unreal.GameplayStatics.is_game_paused(w)
    for command in ['t.MaxFPS 0','r.VSync 0','r.ScreenPercentage 100','r.SecondaryScreenPercentage.GameViewport 100','r.AntiAliasingMethod 4','r.DynamicRes.OperationMode 0']:
        unreal.SystemLibrary.execute_console_command(w,command)
    yield from await_foreground(root, environment)
    if paused_at_entry:
        controller.execute_menu_action(unreal.DarkwellMenuAction.RESUME_GAME)
    (root/'ordinary-entry.json').write_text(json.dumps(dict(paused_at_entry=paused_at_entry,paused_after_action=unreal.GameplayStatics.is_game_paused(w))))
    player = unreal.GameplayStatics.get_player_character(w,0)
    initial = controller.get_control_rotation()
    rows = []
    for index in range(240):
        yield
        e = environment()
        require_foreground(root, e)
        health = float(player.get_editor_property('Health'))
        assert health > 0, 'Ordinary reference entered the death presentation'
        assert not unreal.GameplayStatics.is_game_paused(w)
        assert e['viewport']==[1920,1080] and e['foreground']==1 and e['screen_percentage']==100
        rows.append(dict(index=index,wall_ms=wall_ms,health=health,engine=e))
        controller.set_control_rotation(unreal.Rotator(pitch=initial.pitch,yaw=initial.yaw+45*math.sin(index/60)))
    (root/'reference-frames.json').write_text(json.dumps(rows))
    names = ['sg.'+q+'Quality' for q in ['ViewDistance','AntiAliasing','Shadow','GlobalIllumination','Reflection','PostProcess','Texture','Effects','Foliage','Shading']]
    names += ['r.RayTracing','r.Lumen.HardwareRayTracing','r.Shadow.Virtual.Enable','r.TemporalAA.Upsampling','r.GTSyncType','r.OneFrameThreadLag']
    result = dict(map=MAP,mode='Standalone' if standalone else 'PIE',wall_ms=distribution([r['wall_ms'] for r in rows]),
                  engine_ms={key:distribution([r['engine'][key] for r in rows]) for key in ['game_ms','render_ms','rhi_ms','gpu_ms','game_wait_ms','render_wait_ms','present_ms']},
                  quality={key:unreal.SystemLibrary.get_console_variable_float_value(key) for key in names},first=rows[0]['engine'],last=rows[-1]['engine'],
                  note='Ordinary map, native gameplay remains active. The following GPU profile and screenshot are outside these timing samples.')
    (root/'reference.json').write_text(json.dumps(result,indent=2))
    # Explicit diagnostic frames: formatted GPU events prove actual TSR input/output sizes.
    unreal.SystemLibrary.execute_console_command(w,'r.ProfileGPU.ShowUI 0')
    unreal.SystemLibrary.execute_console_command(w,'profilegpu')
    for _ in range(20):
        yield
    unreal.SystemLibrary.execute_console_command(w,'Shot -nosuffix -showui filename='+(root/'viewport.png').as_posix())
    for _ in range(20):
        yield
    assert (root/'viewport.png').exists(), 'Ordinary viewport screenshot is missing'
    if not standalone:
        assert unreal.DarkwellEditorDiagnostics.set_performance_viewport_size(0,0)
        levels.editor_request_end_play()
        while world() is not None:
            yield
    (root/'complete.json').write_text(json.dumps(dict(protocol_complete=True,frames=len(rows))))

sequence = run()
def tick(_delta):
    global last_tick,last_game,wall_ms
    try:
        assert time.perf_counter()-started<300
        # Real world time continues while the native main menu is paused, so
        # the foreground wait can complete before any enemy simulation begins.
        game = unreal.GameplayStatics.get_real_time_seconds(world()) if world() else None
        if game is not None and game==last_game:
            return
        last_game=game
        now=time.perf_counter()
        wall_ms=(now-last_tick)*1000
        last_tick=now
        next(sequence)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(world(),'QUIT') if standalone else unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    except Exception:
        (root/'failed.txt').write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
        if not standalone:
            levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(world(),'QUIT') if standalone else unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)

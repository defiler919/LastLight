"""Normal-quality D3D12 matrix: raw wall frames include setup and entry/exit.

The older matrix remains unchanged for like-for-like comparisons. This protocol
records all transition frames and also reports steady slices explicitly.
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
rows, results = [], []
last_tick = None
latest_wall_ms = 0

def world():
    return unreal.find_object(None, MAP + '.L_SightWeaveGrayPolicyLab') if standalone else editor.get_game_world()

def stats(values):
    a = sorted(values)
    return dict(p50=a[min(len(a)-1, int(len(a)*.50))],
                p95=a[min(len(a)-1, int(len(a)*.95))],
                p99=a[min(len(a)-1, int(len(a)*.99))], maximum=max(a))

def summarize(name, samples, setup_ms, setup_resources):
    resource_keys = ['records', 'proxies', 'caps', 'textures', 'mids', 'fine_bytes', 'working_set', 'uobjects']
    def window(items):
        return dict(samples=len(items), wall_frame_ms=stats([r['wall_ms'] for r in items]),
                    game_delta_ms=stats([r['game_delta_ms'] for r in items]),
                    memory_system_ms=stats([r['game_thread_us']/1000 for r in items]))
    result = dict(case=name, setup_ms=setup_ms, setup_resources=setup_resources, all_frames=window(samples),
                  steady_after_90=window(samples[90:]) if len(samples)>90 else None,
                  resources={k:dict(first=samples[0][k], last=samples[-1][k], maximum=max(r[k] for r in samples)) for k in resource_keys},
                  total_work={k:sum(r[k] for r in samples) for k in ['texture_creations', 'mid_creations', 'texture_uploads', 'cap_rebuilds', 'samples_scanned', 'coverage_queries', 'occupancy_tests', 'ownership_tests']},
                  stages_us={k:stats([r[k] for r in samples]) for k in ['current_reveal_us', 'historical_us', 'coverage_us', 'occupancy_us', 'ownership_us', 'texture_us', 'cap_us', 'refresh_us']})
    results.append(result)
    (root/'performance.json').write_text(json.dumps(dict(protocol_complete=False, cases=results), indent=2), encoding='utf-8')
    unreal.log('MEMORY_TRANSITION_CASE '+json.dumps(result, separators=(',', ':')))

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
    settings.update(mode=run_mode, protocol=protocol, editor_realtime=False if not standalone else None, initial=json.loads(director.get_frame_environment_for_testing()), seed=0, screenshots=False)
    (root/'quality.json').write_text(json.dumps(settings,indent=2), encoding='utf-8')
    (root/'render_settings.json').write_text(json.dumps(dict(
        viewport=list(controller.get_viewport_size()),
        screen_percentage=unreal.SystemLibrary.get_console_variable_float_value('r.ScreenPercentage'),
        anti_aliasing_method=unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod'))), encoding='utf-8')
    cases = [('Empty',0,240), ('OneWhole',1,300), ('EightWhole',2,300),
             ('ThirtyTwoWhole',3,360), ('PartialNewThenRepeat',0,360),
             ('Overlap64',4,360), ('SameIdentity64',5,360), ('Distributed184',6,360),
             ('FastSweep90',4,180), ('FastSweep160',4,180), ('StationaryStop',0,360),
             ('LongRepeatDistributed',6,1800), ('ActualNewKnowledge',0,360)]
    if protocol == 'Smoke':
        cases = [('Empty',0,180), ('OneWhole',1,180)]
    elif protocol == 'Knowledge':
        cases = [('ActualNewKnowledge',0,360)]
    elif protocol == 'Attribution':
        cases = [(name,0,300) for name in ['Empty','NoGuidance','NoWorldLabels','NoUi','NoCoverageDraw','Restored']]
    elif protocol == 'LongRun':
        cases = [('ActualNewKnowledge',0,360), ('LongInteraction',0,1000000)]
    raw = (root/'frames.jsonl').open('w', encoding='utf-8')
    for name, mode, count in cases:
        unreal.SystemLibrary.execute_console_command(w, 'Trace.RegionBegin '+name)
        case_start = time.perf_counter()
        setup_start = time.perf_counter()
        if name in ('PartialNewThenRepeat','LongInteraction'):
            assert director.set_stress_mode_for_testing(0)
            assert director.teleport_to_room_for_testing(2, player)
            assert director.reset_current_room_for_testing(player)
        elif name in ('StationaryStop', 'ActualNewKnowledge'):
            assert director.set_stress_mode_for_testing(0)
            assert director.teleport_to_room_for_testing(3, player)
            assert director.reset_current_room_for_testing(player)
            if name == 'ActualNewKnowledge':
                # Only physical pose and authored moving state are driven. The
                # real coverage/policy/runtime must acquire each of six poses.
                # Native property spelling also works in -game, where editor
                # Python's snake-case property aliases are not registered.
                mover = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DarkwellPropLabFurniture)
                    if str(a.get_editor_property('StableId')) == 'Lab.V2.MoveWhole')
                policy = mover.get_component_by_class(unreal.SightWeaveObjectPolicyComponent)
                policy.set_sight_weave_moving(True)
                mover.set_actor_location(unreal.Vector(5300, -4100, 0), False, True)
                player.set_actor_location(unreal.Vector(5300, -4340, 92), False, True)
                policy.set_sight_weave_moving(False)
        else:
            assert director.teleport_to_room_for_testing(6, player)
            assert director.set_stress_mode_for_testing(mode)
        setup_ms = (time.perf_counter()-setup_start)*1000
        setup_resources = dict(records=room.get_total_spatial_record_count(), proxies=room.get_total_proxy_count(),
            tracked_identities=room.get_tracked_identity_count(), stress_mode=mode)
        if protocol == 'Attribution':
            director.set_performance_ui_visible_for_testing(name not in ('NoGuidance','NoUi'), name not in ('NoWorldLabels','NoUi'))
            unreal.SystemLibrary.execute_console_command(w, 'r.Darkwell.FogVisual.Diagnostic.SkipCoverageDraw '+('1' if name=='NoCoverageDraw' else '0'))
        room.reset_history_runtime_telemetry_for_testing()
        # Sweeps start immediately before the first recorded frame. No warm-up exclusion.
        if name not in ('PartialNewThenRepeat', 'StationaryStop', 'ActualNewKnowledge','LongInteraction'):
            director.start_sweep_for_testing(90 if name=='FastSweep90' else 160, not name.startswith('FastSweep'))
        else:
            player.set_actor_rotation(unreal.Rotator(yaw=18 if name=='PartialNewThenRepeat' else 90), False)
        samples = []
        last_game = unreal.GameplayStatics.get_time_seconds(w)
        for index in range(count):
            yield 1
            now_game = unreal.GameplayStatics.get_time_seconds(w)
            r = json.loads(room.get_history_runtime_telemetry())['frame_data']
            r['engine'] = json.loads(director.get_frame_environment_for_testing())
            r['engine']['editor_realtime_count'] = unreal.DarkwellEditorDiagnostics.get_realtime_editor_viewport_count() if not standalone else 0
            assert r['engine']['viewport'] == [1920,1080], r['engine']
            r.update(case=name, index=index, elapsed_seconds=time.perf_counter()-started, wall_ms=latest_wall_ms,
                     game_delta_ms=(now_game-last_game)*1000,
                     yaw=player.get_actor_rotation().yaw)
            last_game = now_game
            samples.append(r)
            raw.write(json.dumps(r, separators=(',', ':'))+'\n')
            if name == 'LongInteraction':
                elapsed = time.perf_counter()-case_start
                section = int(elapsed // 10)
                if index == 0 or section != last_section:
                    # Alternate Partial, Whole, real moving-room observation and Reset/re-entry.
                    target = [2,1,3,5,2,1][section % 6]
                    assert director.teleport_to_room_for_testing(target, player)
                    if section % 6 == 0:
                        assert director.reset_current_room_for_testing(player)
                    if target == 3:
                        assert room.start_gray_policy_motion(False), 'Long-run physical motion did not start'
                    with (root/'long-events.jsonl').open('a',encoding='utf-8') as events:
                        events.write(json.dumps(dict(elapsed_seconds=elapsed,section=section,room=target,
                            reset=section%6==0,motion=target==3,records=room.get_total_spatial_record_count(),
                            proxies=room.get_total_proxy_count()))+'\n')
                    last_section = section
                angles = [18,28,40,52,90,40,-90]
                player.set_actor_rotation(unreal.Rotator(yaw=angles[int(elapsed*2)%len(angles)]), False)
                if elapsed >= 610:
                    break
            if name == 'PartialNewThenRepeat' and index%20==0:
                angles=[18,-90,28,-90,40,-90,52,-90,90,-90,52,-90,40,-90,28,-90]
                player.set_actor_rotation(unreal.Rotator(yaw=angles[(index//20)%len(angles)]), False)
            if name == 'StationaryStop' and index==60:
                assert room.start_gray_policy_motion(False)
            if name == 'ActualNewKnowledge':
                phase = index % 60
                episode = index // 60
                if phase == 24:
                    player.set_actor_rotation(unreal.Rotator(yaw=0), False)
                if episode < 5 and phase == 30:
                    policy.set_sight_weave_moving(True)
                if episode < 5 and 30 <= phase < 40:
                    progress = episode + (phase-29)/10
                    mover.set_actor_location(unreal.Vector(5300+250*progress, -4100+250*progress, 0), False, True)
                if episode < 5 and phase == 40:
                    policy.set_sight_weave_moving(False)
                    player.set_actor_location(unreal.Vector(5300+250*(episode+1), -4340+250*(episode+1), 92), False, True)
                if phase == 59 and episode < 5:
                    player.set_actor_rotation(unreal.Rotator(yaw=90), False)
        if name == 'ActualNewKnowledge':
            # Old positions are behind/alongside the observer. No record injection
            # and no reset occurs between legally observed poses.
            assert samples[-1]['records'] >= samples[23]['records']+5, 'New unresolved captures were not retained'
        raw.flush()
        summarize(name, samples, setup_ms, setup_resources)
        unreal.SystemLibrary.execute_console_command(w, 'Trace.RegionEnd '+name)
        director.start_sweep_for_testing(1, False)
        yield 1
    raw.close()
    if protocol == 'Attribution':
        director.set_performance_ui_visible_for_testing(True,True)
        unreal.SystemLibrary.execute_console_command(w, 'r.Darkwell.FogVisual.Diagnostic.SkipCoverageDraw 0')
    assert director.set_stress_mode_for_testing(0)
    yield 1
    if protocol == 'LongRun':
        # Keep cleanup separate from timed interaction and release the Python
        # row buffer before comparing engine resources after native collection.
        import gc
        samples.clear()
        gc.collect()
        unreal.SystemLibrary.collect_garbage()
        for _ in range(60):
            yield 1
        (root/'long-cleanup.json').write_text(json.dumps(dict(engine=json.loads(director.get_frame_environment_for_testing()),
            telemetry=json.loads(room.get_history_runtime_telemetry())['frame_data']),indent=2),encoding='utf-8')
    assert director.set_audit_viewport_size_for_testing(0,0)
    if not standalone:
        levels.editor_request_end_play()
        while world() is not None:
            yield 1
    (root/'performance.json').write_text(json.dumps(dict(protocol_complete=True, cases=results), indent=2), encoding='utf-8')
    (root/'complete.json').write_text(json.dumps(dict(protocol_complete=True, cases=len(results))), encoding='utf-8')

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

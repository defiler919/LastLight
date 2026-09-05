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

root = Path(os.environ['DARKWELL_AUDIT_OUTPUT']).resolve()
root.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
assert levels.load_level('/Game/Maps/L_SightWeaveGrayPolicyLab')
started = time.perf_counter()
rows, results = [], []
last_tick = None
latest_wall_ms = 0

def world():
    return editor.get_game_world()

def stats(values):
    a = sorted(values)
    return dict(p50=a[min(len(a)-1, int(len(a)*.50))],
                p95=a[min(len(a)-1, int(len(a)*.95))],
                p99=a[min(len(a)-1, int(len(a)*.99))], maximum=max(a))

def summarize(name, samples, setup_ms):
    resource_keys = ['records', 'proxies', 'caps', 'textures', 'mids', 'fine_bytes', 'working_set', 'uobjects']
    def window(items):
        return dict(samples=len(items), wall_frame_ms=stats([r['wall_ms'] for r in items]),
                    game_delta_ms=stats([r['game_delta_ms'] for r in items]),
                    memory_system_ms=stats([r['game_thread_us']/1000 for r in items]))
    result = dict(case=name, setup_ms=setup_ms, all_frames=window(samples),
                  steady_after_90=window(samples[90:]) if len(samples)>90 else None,
                  resources={k:dict(first=samples[0][k], last=samples[-1][k], maximum=max(r[k] for r in samples)) for k in resource_keys},
                  total_work={k:sum(r[k] for r in samples) for k in ['texture_creations', 'mid_creations', 'texture_uploads', 'cap_rebuilds', 'samples_scanned', 'coverage_queries', 'occupancy_tests', 'ownership_tests']},
                  stages_us={k:stats([r[k] for r in samples]) for k in ['current_reveal_us', 'historical_us', 'coverage_us', 'occupancy_us', 'ownership_us', 'texture_us', 'cap_us', 'refresh_us']})
    results.append(result)
    (root/'performance.json').write_text(json.dumps(dict(protocol_complete=False, cases=results), indent=2), encoding='utf-8')
    unreal.log('MEMORY_TRANSITION_CASE '+json.dumps(result, separators=(',', ':')))

def run():
    levels.editor_request_begin_play()
    yield 180
    w = world()
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DarkwellSightWeaveGrayPolicyLabDirector)[0]
    room = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DarkwellMovingPropLabRoom)[0]
    player = unreal.GameplayStatics.get_player_character(w, 0)
    controller = unreal.GameplayStatics.get_player_controller(w, 0)
    player.set_actor_tick_enabled(False)
    controller.set_actor_tick_enabled(False)
    for command in ['t.MaxFPS 0', 'r.VSync 0', 'r.ScreenPercentage 100', 'r.AntiAliasingMethod 4']:
        unreal.SystemLibrary.execute_console_command(w, command)
    (root/'render_settings.json').write_text(json.dumps(dict(
        viewport=list(controller.get_viewport_size()),
        screen_percentage=unreal.SystemLibrary.get_console_variable_float_value('r.ScreenPercentage'),
        anti_aliasing_method=unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod'))), encoding='utf-8')
    cases = [('Empty',0,240), ('OneWhole',1,300), ('EightWhole',2,300),
             ('ThirtyTwoWhole',3,360), ('PartialNewThenRepeat',0,360),
             ('Overlap64',4,360), ('SameIdentity64',5,360), ('Distributed184',6,360),
             ('FastSweep90',4,180), ('FastSweep160',4,180), ('StationaryStop',0,360),
             ('LongRepeatDistributed',6,1800), ('ActualNewKnowledge',0,360)]
    raw = (root/'frames.jsonl').open('w', encoding='utf-8')
    for name, mode, count in cases:
        setup_start = time.perf_counter()
        if name == 'PartialNewThenRepeat':
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
                mover = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)
                    if (m := a.get_component_by_class(unreal.DarkwellRememberablePropComponent))
                    and str(m.get_editor_property('stable_id')) == 'Lab.V2.MoveWhole')
                policy = mover.get_component_by_class(unreal.SightWeaveObjectPolicyComponent)
                policy.set_sight_weave_moving(True)
                mover.set_actor_location(unreal.Vector(5300, -4100, 0), False, True)
                player.set_actor_location(unreal.Vector(5300, -4340, 92), False, True)
                policy.set_sight_weave_moving(False)
        else:
            assert director.teleport_to_room_for_testing(6, player)
            assert director.set_stress_mode_for_testing(mode)
        setup_ms = (time.perf_counter()-setup_start)*1000
        room.reset_history_runtime_telemetry_for_testing()
        # Sweeps start immediately before the first recorded frame. No warm-up exclusion.
        if name not in ('PartialNewThenRepeat', 'StationaryStop', 'ActualNewKnowledge'):
            director.start_sweep_for_testing(90 if name=='FastSweep90' else 160, not name.startswith('FastSweep'))
        else:
            player.set_actor_rotation(unreal.Rotator(yaw=18 if name=='PartialNewThenRepeat' else 90), False)
        samples = []
        last_game = unreal.GameplayStatics.get_time_seconds(w)
        for index in range(count):
            yield 1
            now_game = unreal.GameplayStatics.get_time_seconds(w)
            r = json.loads(room.get_history_runtime_telemetry())['frame_data']
            r.update(case=name, index=index, wall_ms=latest_wall_ms,
                     game_delta_ms=(now_game-last_game)*1000,
                     yaw=player.get_actor_rotation().yaw)
            last_game = now_game
            samples.append(r)
            raw.write(json.dumps(r, separators=(',', ':'))+'\n')
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
        summarize(name, samples, setup_ms)
        director.start_sweep_for_testing(1, False)
        yield 20
    raw.close()
    assert director.set_stress_mode_for_testing(0)
    yield 90
    levels.editor_request_end_play()
    yield 90
    assert world() is None
    unreal.SystemLibrary.collect_garbage()
    yield 30
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
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'CLOSE_SLATE_MAINFRAME')
    except Exception:
        (root/'failed.txt').write_text(traceback.format_exc(), encoding='utf-8')
        unreal.log_error(traceback.format_exc())
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'CLOSE_SLATE_MAINFRAME')
handle = unreal.register_slate_post_tick_callback(tick)

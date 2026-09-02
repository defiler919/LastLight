"""D3D12 fixed-tick pass-through evidence, not a held final reacquisition.

Only drives the observer and existing Lab controls. C++ tests cover real F focus
and exact per-sample state comparisons; this records the actual rendered route.
"""
import json
import re
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
command_line = unreal.SystemLibrary.get_command_line()
mode_match = re.search(r'-CapResidualMode=([12])', command_line)
mode = int(mode_match.group(1)) if mode_match else 2
root = Path(unreal.Paths.project_saved_dir()) / 'PropGameplayLab/MovingMulti/FastSweep' / time.strftime('GPU_%Y%m%d_%H%M%S')
root.mkdir(parents=True, exist_ok=False)
rows = []
start = time.monotonic()
player = controller = room = None
sid = unreal.Name('Lab.InWorld.Rotate.Cabinet')


def world():
    return editor.get_game_world()


def pose(yaw, x=-300, y=100):
    player.set_actor_location(unreal.Vector(x, y, 92), False, True)
    player.set_actor_rotation(unreal.Rotator(yaw=yaw), True)


def trigger(x, y):
    controls = unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellMovingPropLabControl)
    found = [a for a in controls if abs(a.get_actor_location().x-x) < 2 and abs(a.get_actor_location().y-y) < 2]
    assert len(found) == 1 and found[0].trigger_for_lab_evidence(player)


def sample(label):
    fine = room.get_fine_history_telemetry(sid)
    row = dict(label=label, mode=mode, time=unreal.GameplayStatics.get_time_seconds(world()),
               yaw=room.get_tracked_transform(sid).rotation.rotator().yaw,
               observer_yaw=player.get_actor_rotation().yaw,
               coverage=room.get_last_legal_coverage_ratio_for_testing(sid),
               epochs=room.get_spatial_record_count(sid),
               caps=room.get_visible_historical_cap_count_for_testing(sid),
               proxies=room.get_visible_historical_proxy_count_for_testing(sid),
               surface_contact=room.get_current_render_contact_stale_surface_for_testing(sid),
               cap_contact=room.get_current_render_contact_stale_cap_for_testing(sid),
               filter_leak=room.get_hard_ownership_filter_leak_for_testing(sid),
               fine_history=fine, state_hashes=re.findall(r'state_hash=(\d+)', fine),
               runtime=json.loads(room.get_history_runtime_telemetry()),
               viewport=list(controller.get_viewport_size()))
    row['capture_note'] = 'Request-time telemetry; Shot renders on the following engine frame, which can have the next observer pose.'
    row['file'] = f'{len(rows):04}_{label}.png'
    rows.append(row)
    unreal.SystemLibrary.execute_console_command(world(), f'Shot SHOWUI filename="{(root / row["file"]).as_posix()}"')
    unreal.log('FAST_SWEEP_GPU_FRAME ' + json.dumps(row))
    return row


def run():
    global player, controller, room
    assert '-UseFixedTimeStep' in command_line and '-FPS=60' in command_line
    levels.editor_request_begin_play()
    yield 120
    controller = unreal.GameplayStatics.get_player_controller(world(), 0)
    player = unreal.GameplayStatics.get_player_pawn(world(), 0)
    room = unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellMovingPropLabRoom)[0]
    controller.set_actor_tick_enabled(False)
    player.set_actor_tick_enabled(False)
    if '-FastSweepCloseView' in command_line:
        # Presentation-only forensic camera: legal sight still comes from the
        # unmoved player. Same resolution, timing and history rules.
        pose(90)
        boom = player.get_component_by_class(unreal.SpringArmComponent)
        camera = player.get_component_by_class(unreal.CameraComponent)
        boom.set_component_tick_enabled(False)
        origin = unreal.Vector(-550, 850, 300)
        camera.set_world_location(origin, False, True)
        camera.set_world_rotation(unreal.MathLibrary.find_look_at_rotation(origin, unreal.Vector(-300, 650, 75)), False, True)
    unreal.SystemLibrary.execute_console_command(world(), f'r.Darkwell.ProjectFogVisual.PropPresentationMode {mode}')
    assert not unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellStalkerCharacter)
    finals = []
    for sweep in ('fast', 'slow'):
        if sweep == 'slow':
            trigger(1500, -1050)
            yield 2
        pose(90)
        yield 90
        assert sample(sweep + '_remember_0')['coverage'] == 1
        yield 1
        trigger(-300, 260)
        pose(-90)
        yield 190
        pose(146)
        yield 6
        partial = sample(sweep + '_partial_middle')
        assert 0 < partial['coverage'] < .5, partial
        yield 1
        pose(-90)
        yield 303
        sealed = sample(sweep + '_middle_gray_before')
        assert round(abs(sealed['yaw'])) == 180 and sealed['proxies'] > 0
        yield 3
        pose(160)
        yield 1
        sample(sweep + '_start_160')
        # Exactly one fully facing frame, immediately followed by the exit.
        # There is no settle/wait at yaw 90 on the fast route.
        if sweep == 'fast':
            pose(90)
            yield 1
            live = sample('fast_cross_90_one_frame')
            assert live['coverage'] == 1, live
            pose(20)
            for i in range(36):
                yield 1
                sample(f'fast_exit_adjacent_{i:02}')
        else:
            for frame in range(1, 281):
                pose(160 - 140 * frame / 280)
                yield 1
                if frame % 20 == 0:
                    sample(f'slow_cross_{frame:03}')
        yield 90
        final = sample(sweep + '_180_gray_final')
        finals.append(final)
        assert final['coverage'] == 0 and final['proxies'] == 0 and final['caps'] == 0, final
        assert final['epochs'] >= 3, 'Final 180 must be observed and retained; no identity clear'
        for i in range(12):
            yield 1
            sample(f'{sweep}_gray_stationary_{i:02}')
        yield 3
    yield 30
    landed = [p for row in rows for p in root.glob(Path(row['file']).stem + '*.png')]
    checks = dict(rows=rows, finals=finals, screenshots=len(landed), fixed_tick=60, mode=mode,
                  equal_ordered_states=all(len(r['state_hashes'])==2 for r in finals)
                    and finals[0]['state_hashes']==finals[1]['state_hashes'],
                  no_resolved_old_resources=all(r['proxies']==0 and r['caps']==0 for r in finals),
                  no_render_contact=all(r['surface_contact']==0 and r['cap_contact']==0 and r['filter_leak']==0 for r in rows))
    (root / 'checks.json').write_text(json.dumps(checks, indent=2))
    assert checks['equal_ordered_states'] and checks['no_resolved_old_resources'] and checks['no_render_contact']
    assert len(landed) == len(rows), 'Deferred screenshots did not all land'
    unreal.log('FAST_SWEEP_GPU_PASS ' + str(root))
    levels.editor_request_end_play()
    yield 30


sequence = run()
frames_left = 0


def tick(_delta):
    global frames_left
    if frames_left > 0:
        frames_left -= 1
        return
    try:
        assert time.monotonic()-start < 900, 'GPU route exceeded 15 minutes'
        frames_left = max(0, next(sequence)-1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')
    except Exception as error:
        (root / 'failed.json').write_text(json.dumps(dict(error=repr(error), rows=rows), indent=2))
        unreal.log_error('FAST_SWEEP_GPU_FAIL ' + traceback.format_exc())
        levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')


handle = unreal.register_slate_post_tick_callback(tick)

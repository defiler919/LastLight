"""D3D12 PIE evidence for the moving-prop Lab's in-world mechanisms.

The delivered player path uses F through UDarkwellInteractionComponent.  This
driver invokes the same visible mechanism actors directly so evidence capture
does not open the console, change window focus, or call the legacy scenario / 
advance commands.  All output remains under Saved/PropGameplayLab.
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

stamp = time.strftime('%Y%m%d_%H%M%S')
root = Path(unreal.Paths.project_saved_dir()) / 'PropGameplayLab' / 'MovingMulti' / f'InWorldPIE_{stamp}'
root.mkdir(parents=True, exist_ok=False)
rows = []
deadline = 0.0
start = time.monotonic()
handle = None
sequence = None
controller = player = room = None
failure = None


def world():
    return editor.get_game_world()


def actors(cls):
    return unreal.GameplayStatics.get_all_actors_of_class(world(), cls)


def pose(x, y, yaw):
    player.set_actor_location(unreal.Vector(x, y, 92), False, True)
    player.set_actor_rotation(unreal.Rotator(pitch=0, yaw=yaw, roll=0), True)


def mechanism(x, y):
    found = []
    for actor in actors(unreal.DarkwellMovingPropLabControl):
        p = actor.get_actor_location()
        if abs(p.x - x) < 2 and abs(p.y - y) < 2:
            found.append(actor)
    assert len(found) == 1, f'mechanism {x},{y}: {len(found)} matches'
    return found[0]


def trigger(x, y):
    control = mechanism(x, y)
    assert control.trigger_for_lab_evidence(player)
    return control


def tracked(stable_id):
    transform = room.get_tracked_transform(unreal.Name(stable_id))
    p = transform.translation
    r = transform.rotation.rotator()
    return dict(x=p.x, y=p.y, z=p.z, yaw=r.yaw)


def sample(label, stable_id, capture=True):
    row = dict(
        label=label,
        stable_id=stable_id,
        transform=tracked(stable_id),
        scenario=room.get_scenario(),
        phase=room.get_scenario_phase(),
        motion=room.get_motion_state(),
        position=room.get_object_position_label(),
        interaction=room.get_current_interaction(),
        records=room.get_spatial_record_count(unreal.Name(stable_id)),
        viewport=list(controller.get_viewport_size()),
        time=unreal.GameplayStatics.get_time_seconds(world()),
    )
    rows.append(row)
    if capture:
        filename = f'{len(rows):04}_{label}.png'
        row['file'] = filename
        unreal.SystemLibrary.execute_console_command(
            world(), f'Shot SHOWUI filename="{(root / filename).as_posix()}"')
    unreal.log('IN_WORLD_MOVING_EVIDENCE ' + json.dumps(row))
    return row


def capture_motion(prefix, stable_id, seconds, interval=0.25):
    count = int(seconds / interval) + 1
    for index in range(count):
        sample(f'{prefix}_{index:02}', stable_id)
        yield interval


def reset_current():
    assert mechanism(1500, -1050).trigger_for_lab_evidence(player)


def run():
    global controller, player, room
    levels.editor_request_begin_play()
    yield 6.0
    assert levels.is_in_play_in_editor()
    controller = unreal.GameplayStatics.get_player_controller(world(), 0)
    player = unreal.GameplayStatics.get_player_pawn(world(), 0)
    found = actors(unreal.DarkwellMovingPropLabRoom)
    assert len(found) == 1
    room = found[0]
    assert room.is_in_world_control_mode()
    assert len(actors(unreal.DarkwellMovingPropLabControl)) == 7
    assert len(actors(unreal.DarkwellStalkerCharacter)) == 0
    controller.set_actor_tick_enabled(False)
    player.set_actor_tick_enabled(False)
    unreal.SystemLibrary.execute_console_command(world(), 'r.ScreenPercentage 100')
    unreal.SystemLibrary.execute_console_command(world(), 'r.AntiAliasingMethod 4')
    actual_viewport = list(controller.get_viewport_size())
    unreal.log('IN_WORLD_MOVING_GPU viewport=' + json.dumps(actual_viewport)
               + ' rhi=D3D12 feature=SM6 tsr=1 screenPercentage=100')

    # Visible translation: one-second hold, then four seconds of continuous A->B.
    pose(-1100, 100, 90)
    trigger(-1100, 260)
    yield from capture_motion('translate', 'Lab.Moving.Cabinet', 5.4, 0.25)
    assert room.get_motion_state() == 'FINISHED'
    reset_current()
    yield 0.5

    # Visible rotation: one-second hold, then continuous 0->180 over four seconds.
    pose(-300, 100, 90)
    trigger(-300, 260)
    yield from capture_motion('rotate', 'Lab.InWorld.Rotate.Cabinet', 5.4, 0.25)
    assert room.get_motion_state() == 'FINISHED'
    reset_current()
    yield 0.5

    # The cabinet continuously crosses the legal coverage boundary for eight seconds.
    pose(1400, 100, 90)
    trigger(1400, 260)
    yield from capture_motion('coverage_edge', 'Lab.InWorld.Edge.Cabinet', 9.4, 0.5)
    assert room.get_motion_state() == 'FINISHED'
    reset_current()
    yield 0.5

    # Offscreen A->B is armed in front and begins only on the rear pressure plate.
    pose(500, 250, 90)
    sample('hidden_a_observed', 'Lab.InWorld.Hidden.Cabinet')
    trigger(500, 260)
    pose(900, -850, -90)
    yield 0.5
    sample('hidden_motion_rear_plate', 'Lab.InWorld.Hidden.Cabinet')
    yield 4.5
    sample('hidden_motion_finished', 'Lab.InWorld.Hidden.Cabinet')
    pose(900, 250, 90)
    yield 1.0
    sample('hidden_b_seen_a_retained', 'Lab.InWorld.Hidden.Cabinet')
    pose(500, 250, 90)
    yield 1.0
    sample('hidden_a_legally_rechecked', 'Lab.InWorld.Hidden.Cabinet')
    reset_current()
    yield 0.5

    # Four shapes: two move continuously, one becomes absent, one stays fixed.
    pose(-1200, -900, 90)
    trigger(-1200, -1050)
    yield from capture_motion('multi_high', 'Lab.InWorld.Multi.HighCabinet', 5.4, 0.35)
    sample('multi_low_final', 'Lab.InWorld.Multi.LowCabinet')
    sample('multi_table_static', 'Lab.InWorld.Multi.LongTable')
    assert not room.is_actual_present(unreal.Name('Lab.InWorld.Multi.SmallBox'))
    assert room.get_motion_state() == 'FINISHED'
    # Let the final queued screenshot reach disk before recording the file count.
    yield 1.0

    xs = [row['transform']['x'] for row in rows if row['label'].startswith('translate_')]
    yaws = [row['transform']['yaw'] for row in rows if row['label'].startswith('rotate_')]
    assert len({round(value, 2) for value in xs}) >= 12
    assert len({round(value, 2) for value in yaws}) >= 12
    actual_screenshots = len(list(root.glob('*.png')))
    (root / 'checks.json').write_text(json.dumps(dict(
        failure=None,
        viewport=actual_viewport,
        screenshots=actual_screenshots,
        rows=rows), indent=2))
    unreal.log(f'IN_WORLD_MOVING_GPU_PASS screenshots={actual_screenshots} '
               f'viewport={actual_viewport[0]}x{actual_viewport[1]}')
    controller.set_actor_tick_enabled(True)
    player.set_actor_tick_enabled(True)
    levels.editor_request_end_play()
    yield 2.0
    unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')


sequence = run()


def tick(_delta):
    global deadline, sequence, failure
    now = time.monotonic()
    if now < deadline:
        return
    try:
        assert now - start < 180, 'moving evidence timeout'
        deadline = now + next(sequence)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
    except Exception as error:
        failure = repr(error)
        unreal.log_error('IN_WORLD_MOVING_GPU_FAIL ' + failure + '\n' + traceback.format_exc())
        (root / 'failed_checks.json').write_text(json.dumps(dict(failure=failure, rows=rows), indent=2))
        if controller:
            controller.set_actor_tick_enabled(True)
        if player:
            player.set_actor_tick_enabled(True)
        levels.editor_request_end_play()
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')
        unreal.unregister_slate_post_tick_callback(handle)


handle = unreal.register_slate_post_tick_callback(tick)

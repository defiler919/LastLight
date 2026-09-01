"""D3D12 PIE evidence for the moving-prop Lab's in-world mechanisms.

The delivered player path uses F through UDarkwellInteractionComponent. This
GPU-only driver invokes the same visible mechanism actors directly; native
automation and a separate real PIE pass exercise proximity trace + F. The
driver never opens the console or calls legacy scenario / advance commands.
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


def diagnostic(stable_name, *fragments):
    """Call a diagnostic whose reflected Python spelling contains a digit/acronym."""
    normalized_fragments = tuple(fragment.lower().replace('_', '')
                                 for fragment in fragments)
    matches = []
    for attribute in dir(room):
        normalized_attribute = attribute.lower().replace('_', '')
        if all(fragment in normalized_attribute for fragment in normalized_fragments):
            candidate = getattr(room, attribute)
            if callable(candidate):
                matches.append((attribute, candidate))
    assert len(matches) == 1, f'diagnostic {fragments}: {[name for name, _ in matches]}'
    return matches[0][1](stable_name)


def tracked(stable_id):
    transform = room.get_tracked_transform(unreal.Name(stable_id))
    p = transform.translation
    r = transform.rotation.rotator()
    return dict(x=p.x, y=p.y, z=p.z, yaw=r.yaw)


def sample(label, stable_id, capture=True):
    stable_name = unreal.Name(stable_id)
    row = dict(
        label=label,
        stable_id=stable_id,
        transform=tracked(stable_id),
        scenario=room.get_scenario(),
        phase=room.get_scenario_phase(),
        motion=room.get_motion_state(),
        position=room.get_object_position_label(),
        interaction=room.get_current_interaction(),
        records=room.get_spatial_record_count(stable_name),
        live_epochs=room.get_current_epoch_count_for_testing(stable_name),
        stale_epochs=room.get_stale_epoch_count_for_testing(stable_name),
        visible_proxies=room.get_visible_historical_proxy_count_for_testing(stable_name),
        visible_caps=room.get_visible_historical_cap_count_for_testing(stable_name),
        surface_contributors=room.get_max_surface_contributors_for_testing(stable_name),
        cap_contributors=room.get_max_cap_contributors_for_testing(stable_name),
        total_contributors=room.get_max_total_contributors_for_testing(stable_name),
        overlap_contributors=room.get_max_overlap_contributors_for_testing(stable_name),
        current_3d_overlap_stale_surface=diagnostic(
            stable_name, 'current', '3d', 'overlap', 'stale', 'surface'),
        current_3d_overlap_stale_cap=diagnostic(
            stable_name, 'current', '3d', 'overlap', 'stale', 'cap'),
        max_3d_render_ownership=diagnostic(
            stable_name, 'max', '3d', 'render', 'ownership', 'contributors'),
        legal_coverage=room.get_last_legal_coverage_ratio_for_testing(stable_name),
        coverage_valid=room.is_last_coverage_valid_for_testing(stable_name),
        coverage_zero_reason=room.get_last_coverage_zero_reason_for_testing(stable_name),
        transform_revision=room.get_transform_revision_for_testing(stable_name),
        coverage_revision=room.get_coverage_revision_for_testing(stable_name),
        coverage_transform_revision=room.get_coverage_transform_revision_for_testing(stable_name),
        coverage_grid_revision=room.get_coverage_grid_revision_for_testing(stable_name),
        seal_count=room.get_seal_count_for_testing(stable_name),
        observation_episode=room.get_observation_episode_for_testing(stable_name),
        observation_state=room.get_observation_state_for_testing(stable_name),
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
    # Visible rotation: one-second hold, then continuous 0->180 over four seconds.
    pose(-300, 100, 90)
    trigger(-300, 260)
    yield from capture_motion('rotate', 'Lab.InWorld.Rotate.Cabinet', 5.4, 0.25)
    assert room.get_motion_state() == 'FINISHED'
    assert max(row['overlap_contributors'] for row in rows
               if row['label'].startswith('rotate_')) <= 1
    assert all(row['overlap_contributors'] == 1 for row in rows
               if row['label'].startswith('rotate_') and abs(row['transform']['yaw']) > 0.1)
    # User-reported route: fully observe 0 degrees, turn away before hidden
    # rotation starts, briefly reacquire only an edge at an intermediate angle,
    # turn away again, finish at 180 hidden, then slowly reacquire the final pose.
    reset_current()
    pose(-300, 100, 90)
    yield 1.5
    initial = sample('partial_rotation_full_0', 'Lab.InWorld.Rotate.Cabinet')
    assert initial['legal_coverage'] == 1.0
    trigger(-300, 260)
    pose(-300, 100, -90)
    yield 2.2
    hidden_mid = sample('partial_rotation_hidden_mid', 'Lab.InWorld.Rotate.Cabinet')
    assert 40 <= abs(hidden_mid['transform']['yaw']) <= 120
    partial_mid = None
    for yaw in range(150, 129, -1):
        pose(-300, 100, yaw)
        yield 0.08
        candidate = sample(f'partial_rotation_mid_edge_{yaw}',
                           'Lab.InWorld.Rotate.Cabinet')
        if 0.06 <= candidate['legal_coverage'] <= 0.30:
            partial_mid = candidate
            break
    assert partial_mid is not None
    pose(-300, 100, -90)
    yield 0.25
    sealed_mid = sample('partial_rotation_mid_sealed',
                        'Lab.InWorld.Rotate.Cabinet')
    assert sealed_mid['seal_count'] >= 2
    yield 4.5
    hidden_final = sample('partial_rotation_hidden_final_180',
                          'Lab.InWorld.Rotate.Cabinet')
    assert round(abs(hidden_final['transform']['yaw'])) == 180
    partial_final_rows = []
    for yaw in range(155, 89, -1):
        pose(-300, 100, yaw)
        yield 0.08
        partial_final_rows.append(sample(
            f'partial_rotation_final_reacquire_{yaw}',
            'Lab.InWorld.Rotate.Cabinet'))
    pose(-300, 100, 90)
    yield 2.0
    final_live = sample('partial_rotation_final_full_live',
                        'Lab.InWorld.Rotate.Cabinet')
    assert final_live['legal_coverage'] == 1.0
    assert any(row['stale_epochs'] > 0 for row in partial_final_rows)
    assert max(row['current_3d_overlap_stale_surface']
               for row in partial_final_rows) == 0
    assert max(row['current_3d_overlap_stale_cap']
               for row in partial_final_rows) == 0
    assert max(row['max_3d_render_ownership']
               for row in partial_final_rows) <= 1
    assert final_live['current_3d_overlap_stale_surface'] == 0
    assert final_live['current_3d_overlap_stale_cap'] == 0
    # Repeat Scenario 2 while deliberately leaving legal view mid-rotation.
    # This reproduces the user's old/new-pose overlap case without a console.
    reset_current()
    pose(-300, 100, 90)
    trigger(-300, 260)
    yield from capture_motion('rotate_loss_visible', 'Lab.InWorld.Rotate.Cabinet', 2.0, 0.2)
    pose(900, -850, -90)
    yield from capture_motion('rotate_loss_hidden', 'Lab.InWorld.Rotate.Cabinet', 3.6, 0.2)
    pose(-300, 300, 90)
    yield from capture_motion('rotate_reacquired', 'Lab.InWorld.Rotate.Cabinet', 2.0, 0.1)
    yield from capture_motion('rotate_reacquired_fixed_10s',
                              'Lab.InWorld.Rotate.Cabinet', 10.0, 0.1)
    rotation_loss = [row for row in rows
                     if row['label'].startswith(('rotate_loss_', 'rotate_reacquired'))]
    assert any(row['stale_epochs'] == 1 for row in rotation_loss)
    assert max(row['overlap_contributors'] for row in rotation_loss) <= 1
    # The cabinet continuously crosses the legal coverage boundary for eight seconds.
    pose(1400, 100, 90)
    trigger(1400, 260)
    yield from capture_motion('coverage_edge', 'Lab.InWorld.Edge.Cabinet', 9.4, 0.5)
    assert room.get_motion_state() == 'FINISHED'
    # Offscreen A->B is armed in front and begins only on the rear pressure plate.
    pose(500, 250, 90)
    sample('hidden_a_observed', 'Lab.InWorld.Hidden.Cabinet')
    trigger(500, 260)
    pose(900, -850, -90)
    yield 0.5
    sample('hidden_motion_rear_plate', 'Lab.InWorld.Hidden.Cabinet')
    yield from capture_motion('hidden_transit', 'Lab.InWorld.Hidden.Cabinet', 4.5, 0.25)
    sample('hidden_motion_finished', 'Lab.InWorld.Hidden.Cabinet')
    # Ten seconds at the fixed rear camera. Native automation samples all 600
    # 60-Hz states; these denser rendered frames expose whole-proxy flashes.
    yield from capture_motion('hidden_fixed_10s', 'Lab.InWorld.Hidden.Cabinet', 10.0, 0.1)
    for index in range(21):
        pose(900, -850, -110 + 2 * index)
        sample(f'hidden_slow_sweep_{index:02}', 'Lab.InWorld.Hidden.Cabinet')
        yield 0.1
    pose(900, 250, 90)
    yield 1.0
    sample('hidden_b_seen_a_retained', 'Lab.InWorld.Hidden.Cabinet')
    pose(500, 250, 90)
    yield 1.0
    sample('hidden_a_legally_rechecked', 'Lab.InWorld.Hidden.Cabinet')
    # A->B->C: two distinct hidden epochs, each sealed only at the plate.
    pose(-1500, 700, 90)
    sample('abc_a_observed', 'Lab.InWorld.ABC.Cabinet')
    trigger(-1500, 700)
    pose(900, -850, -90)
    yield from capture_motion('abc_a_to_b', 'Lab.InWorld.ABC.Cabinet', 4.5, 0.25)
    pose(-1050, 700, 90)
    yield 1.0
    sample('abc_b_seen_a_retained', 'Lab.InWorld.ABC.Cabinet')
    pose(900, -850, -90)
    yield from capture_motion('abc_b_to_c', 'Lab.InWorld.ABC.Cabinet', 4.5, 0.25)
    pose(-600, 700, 90)
    yield 1.0
    sample('abc_c_seen_ab_retained', 'Lab.InWorld.ABC.Cabinet')

    # Four shapes: two move continuously, one becomes absent, one stays fixed.
    pose(-1200, -900, 90)
    trigger(-1200, -1050)
    yield from capture_motion('multi_high', 'Lab.InWorld.Multi.HighCabinet', 5.4, 0.35)
    sample('multi_low_final', 'Lab.InWorld.Multi.LowCabinet')
    sample('multi_table_static', 'Lab.InWorld.Multi.LongTable')
    assert not room.is_actual_present(unreal.Name('Lab.InWorld.Multi.SmallBox'))
    assert room.get_motion_state() == 'FINISHED'
    reset_current()
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
        assert now - start < 240, 'moving evidence timeout'
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

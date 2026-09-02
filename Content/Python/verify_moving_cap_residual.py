"""Focused real-PIE cap-positive and residual-negative evidence; no authority writes.

The prior comprehensive route remains unchanged. Native tests drive the F
interaction trace; this GPU driver activates those same in-world controls.
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
match = re.search(r'-CapResidualMode=(\d)', command_line)
mode = int(match.group(1)) if match else 2
root = Path(unreal.Paths.project_saved_dir()) / 'PropGameplayLab/MovingMulti/CapResidual' / time.strftime(f'GPU_Mode{mode}_%Y%m%d_%H%M%S')
root.mkdir(parents=True, exist_ok=False)
rows = []
shader_report = []
deadline = 0
start = time.monotonic()
player = controller = room = None


def world():
    return editor.get_game_world()


def pose(x, y, yaw):
    player.set_actor_location(unreal.Vector(x, y, 92), False, True)
    player.set_actor_rotation(unreal.Rotator(yaw=yaw), True)


def trigger(x, y):
    controls = unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellMovingPropLabControl)
    found = [a for a in controls if abs(a.get_actor_location().x-x) < 2 and abs(a.get_actor_location().y-y) < 2]
    assert len(found) == 1 and found[0].trigger_for_lab_evidence(player)


def sample(label, stable='Lab.InWorld.Rotate.Cabinet'):
    sid = unreal.Name(stable)
    transform = room.get_tracked_transform(sid)
    row = dict(label=label, mode=mode, time=unreal.GameplayStatics.get_time_seconds(world()),
               yaw=transform.rotation.rotator().yaw, coverage=room.get_last_legal_coverage_ratio_for_testing(sid),
               epochs=room.get_spatial_record_count(sid), caps=room.get_visible_historical_cap_count_for_testing(sid),
               proxies=room.get_visible_historical_proxy_count_for_testing(sid),
               surface_contact=room.get_current_render_contact_stale_surface_for_testing(sid),
               cap_contact=room.get_current_render_contact_stale_cap_for_testing(sid),
               filter_leak=room.get_hard_ownership_filter_leak_for_testing(sid),
               lifecycle=room.get_cap_lifecycle_telemetry_for_testing(sid),
               false_occupied=room.get_false_occupied_history_telemetry_for_testing(sid),
               residual=room.get_residual_fragment_telemetry_for_testing(sid),
               viewport=list(controller.get_viewport_size()),
               camera=str(controller.get_player_view_point()) if hasattr(controller, 'get_player_view_point') else '',
               player=str(player.get_actor_transform()))
    row['file'] = f'{len(rows):04}_{label}.png'
    rows.append(row)
    unreal.SystemLibrary.execute_console_command(world(), f'Shot SHOWUI filename="{(root / row["file"]).as_posix()}"')
    unreal.log('CAP_RESIDUAL_FRAME ' + json.dumps(row))
    return row


def close_view(x, y, z, target_x, target_y):
    """Forensic camera only: does not move the player or alter legal coverage."""
    camera = player.get_component_by_class(unreal.CameraComponent)
    boom = player.get_component_by_class(unreal.SpringArmComponent)
    saved = camera.get_relative_transform()
    boom.set_component_tick_enabled(False)
    camera.set_world_location(unreal.Vector(x,y,z), False, True)
    camera.set_world_rotation(unreal.MathLibrary.find_look_at_rotation(
        unreal.Vector(x,y,z), unreal.Vector(target_x,target_y,75)), False, True)
    return camera, boom, saved


def restore_view(view):
    camera, boom, saved = view
    camera.set_relative_transform(saved, False, True)
    boom.set_component_tick_enabled(True)


def audit_materials():
    lib = unreal.MaterialEditingLibrary
    result = []
    for name in ('M_ManualFixedReveal', 'M_ManualAccumulatedMemory', 'M_ManualStaleCutCap'):
        material = unreal.load_asset('/Game/Darkwell/Vision/PropLab/' + name)
        todo = [lib.get_material_property_input_node(material, p) for p in
                (unreal.MaterialProperty.MP_OPACITY, unreal.MaterialProperty.MP_OPACITY_MASK,
                 unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
        seen = []
        while todo:
            node = todo.pop()
            if not node or node in seen:
                continue
            seen.append(node)
            todo.extend(lib.get_inputs_for_material_expression(material, node))
        result.append(dict(name=name, blend=str(material.get_editor_property('blend_mode')),
                           code=[n.get_editor_property('code') for n in seen if isinstance(n, unreal.MaterialExpressionCustom)]))
    (root / 'material_graph.json').write_text(json.dumps(result, indent=2))


def run():
    global player, controller, room
    audit_materials()
    levels.editor_request_begin_play()
    yield 6
    controller = unreal.GameplayStatics.get_player_controller(world(), 0)
    player = unreal.GameplayStatics.get_player_pawn(world(), 0)
    room = unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellMovingPropLabRoom)[0]
    controller.set_actor_tick_enabled(False)
    player.set_actor_tick_enabled(False)
    unreal.SystemLibrary.execute_console_command(world(), f'r.Darkwell.ProjectFogVisual.PropPresentationMode {mode}')
    assert not unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellStalkerCharacter)
    # Scene A: same spatial route in each selector. Do not alter geometry or AA.
    pose(-300, 100, 90)
    yield 1.5
    assert sample('A_full_0')['coverage'] == 1
    yield .25
    trigger(-300, 260)
    pose(-300, 100, -90)
    yield 2.2
    sample('A_hidden_mid')
    for yaw in range(150, 129, -1):
        pose(-300, 100, yaw)
        yield .08
        row = sample(f'A_partial_{yaw}')
        if .06 <= row['coverage'] <= .30:
            break
    else:
        raise AssertionError('No partial intermediate observation')
    yield .08
    pose(-300, 100, -90)
    yield .4
    sample('A_partial_sealed')
    yield 4.5
    assert round(abs(sample('A_hidden_180')['yaw'])) == 180
    camera = close_view(-550,850,300,-300,650)
    yield .6
    sample('A_partial_history_cap')
    yield .3
    restore_view(camera)
    from verify_moving_history_shader import probe
    yield from probe(world(), shader_report)
    for yaw in range(155, 89, -1):
        pose(-300, 100, yaw)
        yield .08
        sample(f'A_reacquire_{yaw}')
    yield 2
    final = sample('A_full_current')
    assert final['coverage'] == 1
    final_resources_zero = final['caps'] == 0 and final['proxies'] == 0
    yield .25
    camera = close_view(-40, 280, 400, -300, 650)
    yield 1
    sample('A_forensic_original')
    yield .3
    # Attribution only. Always restore these resources before validation resumes.
    stale_proxies = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.Actor)
                     if 'SpatialMemory_Lab.InWorld.Rotate.Cabinet' in a.get_name()]
    for proxy in stale_proxies:
        proxy.set_actor_hidden_in_game(True)
    yield .5
    sample('A_attribution_no_stale_surfaces')
    yield .3
    for proxy in stale_proxies:
        proxy.set_actor_hidden_in_game(False)
    yield .5
    sample('A_forensic_restored')
    yield .3
    restore_view(camera)
    for label,x,y in (('south',-300,250),('east',100,650),('north',-300,950),('west',-700,650)):
        camera = close_view(x,y,420,-300,650)
        yield .6
        sample('A_camera_' + label)
        yield .3
        restore_view(camera)
    for label, x, y, yaw in (('south', -300, 70, 90), ('east', 300, 650, 180),
                              ('north', -300, 1230, -90), ('west', -900, 650, 0)):
        pose(x, y, yaw)
        yield 2
        sample('A_view_' + label)
        yield .3
    for i in range(20):
        sample(f'A_adjacent_{i:02}')
        yield .1
    # Scene B: real offscreen movement, then real partial empty verification at A.
    hidden = 'Lab.InWorld.Hidden.Cabinet'
    pose(500, 100, 90)
    yield 1.5
    sample('B_remember_A', hidden)
    yield .25
    trigger(500, 260)
    pose(900, -850, -90)
    yield 5.5
    sample('B_hidden_moved', hidden)
    yield .25
    found_cap = False
    for yaw in range(155, 89, -1):
        pose(500, 100, yaw)
        yield .08
        row = sample(f'B_empty_sweep_{yaw}', hidden)
        if row['caps'] > 0:
            found_cap = True
            camera = close_view(750, 270, 390, 500, 650)
            yield .5
            # Preserve this legitimate partial-empty boundary for original frames.
            for i in range(5):
                yield .15
                sample(f'B_positive_cap_{i}', hidden)
            restore_view(camera)
            # Inspect the real cut from unobstructed camera angles as well;
            # the original southeast view includes the unchanged shelving.
            for label,x,y in (('west',200,650),('northwest',250,850),('north',500,950)):
                camera = close_view(x,y,280,500,650)
                yield .5
                sample('B_cap_camera_' + label, hidden)
                yield .3
                restore_view(camera)
            break
    assert found_cap, 'Real non-overlap VerifiedEmpty route produced no visible cap'
    assert all(r['surface_contact']==0 and r['cap_contact']==0 and r['filter_leak']==0 for r in rows), 'Actual rendered ownership violation'
    yield 1
    for row in rows:
        matches = list(root.glob(Path(row['file']).stem + '*.png'))
        row['landed_files'] = [p.name for p in matches]
    (root / 'checks.json').write_text(json.dumps(dict(mode=mode, rows=rows, shader=shader_report,
        screenshots=len(list(root.glob('*.png'))), positive_cap=found_cap,
        final_resources_zero=final_resources_zero), indent=2))
    assert final_resources_zero, 'Final historical resource assertion failed; original views and positive scene retained for attribution'
    unreal.log('CAP_RESIDUAL_GPU_FINISHED root=' + str(root))
    levels.editor_request_end_play()
    yield 2
    unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')


sequence = run()


def tick(_delta):
    global deadline
    now = time.monotonic()
    if now < deadline:
        return
    try:
        assert now-start < 180, 'Focused cap route timeout'
        deadline = now + next(sequence)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
    except Exception as error:
        (root / 'failed.json').write_text(json.dumps(dict(error=repr(error), rows=rows), indent=2))
        unreal.log_error('CAP_RESIDUAL_GPU_FAIL ' + traceback.format_exc())
        levels.editor_request_end_play()
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')
        unreal.unregister_slate_post_tick_callback(handle)


handle = unreal.register_slate_post_tick_callback(tick)

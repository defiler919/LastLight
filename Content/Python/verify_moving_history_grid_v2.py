"""HistoryGridV2 fixed-tick fast/slow user-route evidence; no authority writes.

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
root = Path(unreal.Paths.project_saved_dir()) / 'PropGameplayLab/MovingMulti/HistoryGridV2' / time.strftime(f'GPU_Mode{mode}_%Y%m%d_%H%M%S')
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
    row['fine_history'] = room.get_fine_history_telemetry(sid)
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
    assert '-UseFixedTimeStep' in command_line and '-FPS=60' in command_line
    audit_materials()
    levels.editor_request_begin_play()
    yield 120
    controller = unreal.GameplayStatics.get_player_controller(world(), 0)
    player = unreal.GameplayStatics.get_player_pawn(world(), 0)
    room = unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellMovingPropLabRoom)[0]
    controller.set_actor_tick_enabled(False)
    player.set_actor_tick_enabled(False)
    unreal.SystemLibrary.execute_console_command(world(), f'r.Darkwell.ProjectFogVisual.PropPresentationMode {mode}')
    assert not unreal.GameplayStatics.get_all_actors_of_class(world(), unreal.DarkwellStalkerCharacter)
    finals = []
    for sweep in ('fast', 'slow'):
        if sweep == 'slow':
            trigger(1500, -1050)  # Explicit current-zone reset mechanism; never during a route.
            yield 2
        pose(-300, 100, 90)
        yield 90
        assert sample(sweep + '_remember_0')['coverage'] == 1
        yield 1
        pose(-300, 100, -90)
        yield 4
        trigger(-300, 260)  # Same in-world mechanism entry; native tests cover F focus/trace.
        for _ in range(420):
            angle = abs(room.get_tracked_transform(unreal.Name('Lab.InWorld.Rotate.Cabinet')).rotation.rotator().yaw)
            if angle >= 151:
                break
            yield 1
        else:
            raise AssertionError('Rotation never reached late intermediate pose')
        pose(-300, 100, 146)
        yield 3
        partial = sample(sweep + '_partial_late')
        assert 0 < partial['coverage'] < .50, partial
        yield 1
        pose(-300, 100, -90)
        yield 180
        sealed = sample(sweep + '_existing_residual_start')
        assert round(abs(sealed['yaw'])) == 180 and sealed['proxies'] > 0 and sealed['caps'] > 0
        yield 1
        view = close_view(-550, 850, 300, -300, 650)
        yield 12
        sample(sweep + '_positive_partial_cap')
        yield 1
        restore_view(view)
        if sweep == 'fast':
            from verify_moving_history_shader import probe
            for seconds in probe(world(), shader_report):
                yield max(1, round(seconds * 60))
        if sweep == 'fast':
            pose(-300, 100, 90)
            for i in range(30):
                yield 1
                sample(f'{sweep}_entry_{i:02}')
        else:
            for yaw in range(155, 89, -1):
                pose(-300, 100, yaw)
                yield 2
                if yaw % 3 == 0:
                    sample(f'{sweep}_entry_{yaw}')
        yield 60
        final = sample(sweep + '_full_final')
        finals.append(final)
        yield 3  # Shot is deferred; do not move the camera before it is rendered.
        for label,x,y in (('south',-300,250),('east',100,650),('north',-300,950),('west',-700,650)):
            view = close_view(x,y,420,-300,650)
            yield 12
            sample(sweep + '_camera_' + label)
            yield 3
            restore_view(view)
        yield 12  # Let the restored camera settle before the stationary burst.
        for i in range(12):
            yield 1
            sample(f'{sweep}_stationary_adjacent_{i:02}')
        yield 3  # Keep the final burst frame before reset or the next scenario.
    # Independent positive control: old A has a real empty cut, with current B
    # outside this view. No overlapping historical pose can hide its valid cap.
    hidden = 'Lab.InWorld.Hidden.Cabinet'
    pose(500, 100, 90)
    yield 90
    sample('positive_remember_A', hidden)
    trigger(500, 260)
    pose(900, -850, -90)
    yield 330
    sample('positive_offscreen_moved', hidden)
    found_cap = False
    for yaw in range(155, 89, -1):
        pose(500, 100, yaw)
        yield 3
        row = sample(f'positive_empty_sweep_{yaw}', hidden)
        if row['caps'] > 0:
            found_cap = True
            for label,x,y in (('west',200,650),('northwest',250,850),('north',500,950)):
                view = close_view(x,y,280,500,650)
                yield 15
                sample('positive_empty_cap_' + label, hidden)
                yield 1
                restore_view(view)
            break
    yield 3
    landed = [p for row in rows for p in root.glob(Path(row['file']).stem + '*.png')]
    checks = dict(mode=mode, fixed_tick=60, rows=rows, finals=finals, shader=shader_report,
                  screenshots=len(landed), positive_empty_cap=found_cap,
                  final_resources_zero=all(r['caps']==0 and r['proxies']==0 for r in finals),
                  equal_fine_history=finals[0]['fine_history']==finals[1]['fine_history'])
    (root / 'checks.json').write_text(json.dumps(checks, indent=2))
    assert checks['final_resources_zero'], 'Residual surface/cap remains after complete reacquisition'
    assert found_cap, 'Independent non-overlapping empty-cut cap missing'
    assert checks['equal_fine_history'], 'Fast/slow terminal fine evidence differs'
    assert all(r['surface_contact']==0 and r['cap_contact']==0 and r['filter_leak']==0 for r in rows), 'Render ownership violation'
    levels.editor_request_end_play()
    yield 3
    unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')


sequence = run()
frames_left = 0


def tick(_delta):
    global frames_left
    if frames_left > 0:
        frames_left -= 1
        return
    try:
        assert time.monotonic()-start < 900, 'Fixed tick GPU route exceeded 15 minutes'
        frames_left = max(0, next(sequence)-1)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
    except Exception as error:
        (root / 'failed.json').write_text(json.dumps(dict(error=repr(error), rows=rows), indent=2))
        unreal.log_error('HISTORY_V2_GPU_FAIL ' + traceback.format_exc())
        levels.editor_request_end_play()
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')
        unreal.unregister_slate_post_tick_callback(handle)


handle = unreal.register_slate_post_tick_callback(tick)

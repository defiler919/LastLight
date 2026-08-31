"""Fixed original geometry, Mode 2 material reveal: bounded real GPU PIE driver.
Only player input ticks are paused; world/authority/rendering continue. No actor
or mesh modifications, other than the player pose. No material test overrides on
furniture. Two complete empty-verification/respawn cycles; three slow scans.
"""
import json, time, traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
root=Path(unreal.Paths.project_saved_dir())/'PropGameplayLab'/'FixedReveal'/time.strftime('PIE_%Y%m%d_%H%M%S')
root.mkdir(parents=True,exist_ok=False)
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
saved={k:settings.get_editor_property(k) for k in ('NewWindowWidth','NewWindowHeight')}
settings.set_editor_property('NewWindowWidth',1920)
settings.set_editor_property('NewWindowHeight',1080)
rows=[]; deadline=0; start=time.monotonic(); handle=None; controller=player=room=None
baseline=None; current_parts=None; current_actor=None; failure=None
shader_rows=[]; geometry_checks=0; audit=False

def world():return editor.get_game_world()
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(world(),cls)
def cmd(s):unreal.SystemLibrary.execute_console_command(world(),s)
def pose(x,y,yaw):
    player.set_actor_location(unreal.Vector(x,y,92),False,True)
    player.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),True)
def xyz(v):return [v.x,v.y,v.z]
def transform(t):return [xyz(t.translation),[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],xyz(t.scale3d)]
def cabinet():
    found=[a for a in actors(unreal.DarkwellPropLabFurniture) if str(a.get_editor_property('stable_id'))=='Lab.ManualStale.Cabinet']
    assert len(found)<=1
    return found[0] if found else None
def geometry(a):
    return dict(actor=transform(a.get_actor_transform()),dimensions=xyz(a.get_editor_property('dimensions')),
        parts=[dict(name=p.get_name(),mesh=p.static_mesh.get_path_name(),transform=transform(p.get_world_transform()),
                    relative=[xyz(p.get_editor_property('relative_location')),[p.get_editor_property('relative_rotation').pitch,p.get_editor_property('relative_rotation').yaw,p.get_editor_property('relative_rotation').roll],xyz(p.get_editor_property('relative_scale3d'))],
                    bounds=[xyz(v) for v in p.get_local_bounds()],world_bounds=[xyz(v) if hasattr(v,"x") else v for v in unreal.SystemLibrary.get_component_bounds(p)],
                    corners=[xyz(unreal.MathLibrary.transform_location(p.get_world_transform(),unreal.Vector(x,y,z))) for x in (p.get_local_bounds()[0].x,p.get_local_bounds()[1].x) for y in (p.get_local_bounds()[0].y,p.get_local_bounds()[1].y) for z in (p.get_local_bounds()[0].z,p.get_local_bounds()[1].z)]) for p in a.get_components_by_class(unreal.StaticMeshComponent)])
def sample(label,shot=True):
    global baseline,current_parts,current_actor,geometry_checks
    a=cabinet(); parts=a.get_components_by_class(unreal.StaticMeshComponent) if a else []
    casters=[p for p in parts if p.get_editor_property('cast_shadow') and (p.is_visible() or p.get_editor_property('cast_hidden_shadow'))]
    shown=[p for p in parts if p.is_visible() and not p.get_editor_property('hidden_in_game')]
    if a:
        g=geometry(a)
        if baseline is None:baseline=g
        if g!=baseline:
            (root/'geometry_failure.json').write_text(json.dumps(dict(before=baseline,after=g),indent=2))
            raise AssertionError('Cabinet geometry changed: see geometry_failure.json')
        assert len(parts)==12 and len(casters)==3,'Added/duplicate caster geometry'
        # SpawnActualCabinet deliberately hides a new actor until the next
        # authority update. Permit ONLY that first, unbound spawn frame; all
        # subsequent frames and all captured checkpoints require submission.
        if len(shown)!=3:
            assert len(shown)==0 and label=='invariant_tick' and current_actor!=a.get_path_name(),'Original Mode 2 meshes must be submitted for pixel masking'
            assert all(p.get_material(0).get_scalar_parameter_value('FixedRevealReady')==0 for p in casters),'Only unbound initialization can be hidden'
        assert len(a.get_components_by_class(unreal.PrimitiveComponent))==12,'Auxiliary geometry added'
        geometry_checks+=1
        if current_actor==a.get_path_name():assert current_parts==[p.get_path_name() for p in parts],'Replaced component during visibility transition'
        current_actor=a.get_path_name();current_parts=[p.get_path_name() for p in parts]
    else:assert not casters
    proxies=[]
    for actor in actors(unreal.Actor):
        if not actor.get_name().startswith('Remembered_'):continue
        for p in actor.get_components_by_class(unreal.StaticMeshComponent):
            assert not p.get_editor_property('cast_shadow'),'Memory proxy casts real shadow'
            proxies.append(p.get_path_name())
    p=player.get_actor_location()
    row=dict(label=label,t=unreal.GameplayStatics.get_time_seconds(world()),actual=bool(a),
        source=a.get_path_name() if a else None,casters=[p.get_path_name() for p in casters],shown=len(shown),
        proxy_parts=proxies,status=room.get_status(),coverage=room.get_cabinet_coverage(),verified=room.get_verified_fraction(),
        opacity=room.get_remaining_opacity(),x=p.x,y=p.y,yaw=player.get_actor_rotation().yaw,
        viewport=list(controller.get_viewport_size()))
    assert unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')==4
    assert unreal.SystemLibrary.get_console_variable_int_value('r.ScreenPercentage')==100
    assert unreal.SystemLibrary.get_console_variable_int_value('r.Darkwell.ProjectFogVisual.LabRoute')==0
    if shot:rows.append(row)
    if shot:
        filename=f'{len(rows):04}_{label}.png';row['file']=filename
        cmd(f'Shot filename="{(root/filename).as_posix()}"')
    if shot:unreal.log('FIXED_REVEAL_CHECK '+json.dumps(row))
    return row

def resize_viewport():
    # Resize only this process's PIE client by its measured viewport delta.
    # This is native backbuffer sizing, not screenshot resampling.
    import ctypes, os
    from ctypes import wintypes
    api=ctypes.WinDLL('user32',use_last_error=True)
    api.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
    api.GetWindowTextW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
    api.GetWindowRect.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.RECT)]
    api.SetWindowPos.argtypes=[wintypes.HWND,wintypes.HWND,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,wintypes.UINT]
    windows=[]
    @ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
    def visit(hwnd,param):
        pid=wintypes.DWORD();api.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
        name=ctypes.create_unicode_buffer(1024);api.GetWindowTextW(hwnd,name,1024)
        if pid.value==os.getpid() and 'NetMode:' in name.value:windows.append(hwnd)
        return True
    api.EnumWindows(visit,0);assert len(windows)==1
    rect=wintypes.RECT();assert api.GetWindowRect(windows[0],ctypes.byref(rect))
    vx,vy=controller.get_viewport_size()
    assert api.SetWindowPos(windows[0],None,0,0,rect.right-rect.left+1920-vx,rect.bottom-rect.top+1080-vy,0x0416)
    yield .3
    assert list(controller.get_viewport_size())==[1920,1080]
    cmd('r.ScreenPercentage 100')

def scan(label,x,y,start_yaw,end_yaw):
    pose(x,y,start_yaw);yield .6
    sample(label+'_before');yield .15
    elapsed=0; last=unreal.GameplayStatics.get_time_seconds(world());next_shot=0
    # Six seconds, capture a bounded selection plus four truly adjacent frames
    # halfway through. Geometry assertions run on EVERY Slate/render tick.
    adjacent=0
    while elapsed<6:
        now=unreal.GameplayStatics.get_time_seconds(world());elapsed+=now-last;last=now
        t=min(1,elapsed/6);pose(x,y,start_yaw+(end_yaw-start_yaw)*t)
        shot=False
        if t>=next_shot and next_shot<=1:
            shot=True;next_shot+=.20
        if t>=.48 and adjacent<4:shot=True;adjacent+=1
        sample(label+f'_sweep_{t:.3f}',shot)
        yield 0
    pose(x,y,end_yaw);yield .6
    sample(label+'_full');yield .2

def run():
    global controller,player,room,audit
    unreal.log('FIXED_REVEAL_READY '+root.as_posix())
    while not levels.is_in_play_in_editor():yield .2
    yield 3
    controller=unreal.GameplayStatics.get_player_controller(world(),0)
    player=unreal.GameplayStatics.get_player_pawn(world(),0)
    room=actors(unreal.DarkwellManualStaleRoom)[0]
    controller.set_actor_tick_enabled(False);player.set_actor_tick_enabled(False)
    cmd('Darkwell.PropLab stalemanual reset');cmd('Darkwell.PropLab stalemanual mode 2')
    pose(4500,150,90);yield .8
    audit=True
    yield from resize_viewport()
    from verify_fixed_reveal_shader import probe
    yield from probe(world(),shader_rows)
    sample('initial_observed');yield .2
    for cycle in range(2):
        pose(4500,-450,90);yield .5
        assert not cabinet(),'Pressure failed to remove actual'
        # Fully verify the old empty position before returning to the switch.
        pose(4500,150,90);yield 1
        r=sample(f'c{cycle}_empty_verified')
        assert not r['actual'] and r['verified']==1 and r['opacity']==0
        yield .2
        pose(4840,330,270);yield .5
        sample(f'c{cycle}_absent_shadow_receiver');yield .2
        pose(4720,-450,90);yield .2
        pose(4500,-450,90);yield .5
        assert cabinet(),'Pressure failed to respawn actual'
        # Return back-facing; NEVER teleport top (it would auto-observe).
        pose(4840,330,270);yield .6
        r=sample(f'c{cycle}_hidden_shadow_receiver')
        assert r['coverage']==0 and len(r['casters'])==3 and 'Snapshot: EMPTY' in r['status']
        yield .2
        if cycle==0:
            # world +X is screen-left with the fixed yaw=90 camera.
            yield from scan('left_to_right',4500,150,-5,90)
            yield from scan('right_to_left',4500,150,185,90)
        else:
            yield from scan('diagonal',4840,330,270,160)
        pose(4840,330,270);yield .6
        sample(f'c{cycle}_out_of_view_again');yield .2
        pose(4500,150,90);yield .6
    (root/'geometry.json').write_text(json.dumps(baseline,indent=2))
    unreal.log(f'FIXED_REVEAL_PASS cycles=2 scans=3 geometryChecks={geometry_checks} shots={len(rows)} addedGeometry=0')
    yield from cleanup()

def cleanup():
    if levels.is_in_play_in_editor():
        if controller:controller.set_actor_tick_enabled(True)
        if player:player.set_actor_tick_enabled(True)
        levels.editor_request_end_play()
    yield 2
    for k,v in saved.items():settings.set_editor_property(k,v)
    (root/'checks.json').write_text(json.dumps(dict(failure=failure,geometry_checks=geometry_checks,shader=shader_rows,rows=rows),indent=2))

sequence=run()
def tick(dt):
    global deadline,sequence,failure
    now=time.monotonic()
    try:
        if audit and not failure and player and levels.is_in_play_in_editor():sample('invariant_tick',False)
        if now<deadline:return
        assert now-start<300,'PIE deadline exceeded'
        deadline=now+next(sequence)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
    except Exception:
        failure=traceback.format_exc();unreal.log_error('FIXED_REVEAL_FAIL '+failure)
        (root/'failed_checks.json').write_text(json.dumps(dict(failure=failure,shader=shader_rows,rows=rows),indent=2))
        sequence=cleanup();deadline=now
handle=unreal.register_slate_post_tick_callback(tick)

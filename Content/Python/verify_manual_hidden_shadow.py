"""Stage 1 only: native hidden shadows, same original components and geometry.

Run a D3D12/SM6 editor with -PropLabAsyncCapture and -ExecutePythonScript=this.
Start floating PIE with the official EditorAppToolset after HIDDEN_SHADOW_READY.
This driver changes player poses only; it never changes a cabinet transform,
mesh, material, vertex, shadow flag, visibility flag or memory state directly.
All screenshots/failures remain in Saved. No content assets are saved.
"""
import json, time, traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
root=Path(unreal.Paths.project_saved_dir())/'PropGameplayLab'/'HiddenShadow'/time.strftime('PIE_%Y%m%d_%H%M%S')
root.mkdir(parents=True,exist_ok=False)
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
saved={k:settings.get_editor_property(k) for k in ('NewWindowWidth','NewWindowHeight')}
settings.set_editor_property('NewWindowWidth',1920)
settings.set_editor_property('NewWindowHeight',1080)
rows=[]; deadline=0; start=time.monotonic(); handle=None; controller=player=room=None
baseline=None; current_parts=None; current_actor=None; failure=None

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
                    bounds=[xyz(v) for v in p.get_local_bounds()]) for p in a.get_components_by_class(unreal.StaticMeshComponent)])
def sample(label,shot=True):
    global baseline,current_parts,current_actor
    a=cabinet(); parts=a.get_components_by_class(unreal.StaticMeshComponent) if a else []
    casters=[p for p in parts if p.get_editor_property('cast_shadow') and (p.is_visible() or p.get_editor_property('cast_hidden_shadow'))]
    shown=[p for p in parts if p.is_visible() and not p.get_editor_property('hidden_in_game')]
    if a:
        g=geometry(a)
        if baseline is None:baseline=g
        assert g==baseline,'Cabinet geometry changed'
        assert len(parts)==12 and len(casters)==3,'Added/duplicate caster geometry'
        assert len(shown) in (0,3),'Changed existing whole-object visibility'
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
    rows.append(row)
    if shot:
        filename=f'{len(rows):04}_{label}.png';row['file']=filename
        cmd(f'Shot filename="{(root/filename).as_posix()}"')
    unreal.log('HIDDEN_SHADOW_CHECK '+json.dumps(row))
    return row

def run():
    global controller,player,room
    unreal.log('HIDDEN_SHADOW_READY '+root.as_posix())
    while not levels.is_in_play_in_editor():yield .2
    yield 3
    controller=unreal.GameplayStatics.get_player_controller(world(),0)
    player=unreal.GameplayStatics.get_player_pawn(world(),0)
    room=actors(unreal.DarkwellManualStaleRoom)[0]
    controller.set_actor_tick_enabled(False)
    # Character tick also reapplies cached cursor aim. Pause only these player
    # input ticks for deterministic poses; the world, authority, room, meshes,
    # lights and component ticks keep running. Restore both before leaving PIE.
    player.set_actor_tick_enabled(False)
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
    cmd('Darkwell.PropLab stalemanual reset');cmd('Darkwell.PropLab stalemanual mode 2')
    pose(4500,150,90);yield .7
    assert sample('initial_observed')['shown']==3
    yield .3
    for cycle in range(2):
        pose(4500,-450,90);yield .5
        assert not sample(f'c{cycle}_absent_memory_retained')['actual']
        yield .3
        # Fixed camera/player pose: compare exactly the same receiver and light
        # placement with ABSENT and hidden PRESENT. Only the pressure switch
        # changes actual existence; no visibility/material test override.
        for index,(x,y) in enumerate(((4840,330),(4860,500),(4820,300))):
            pose(x,y,270);yield .6
            assert not sample(f'c{cycle}_absent_pose{index}')['actual']
            yield .25
        pose(4720,-450,90);yield .3
        pose(4500,-450,90);yield .5
        r=sample(f'c{cycle}_present_unseen')
        assert r['actual'] and r['shown']==0 and len(r['casters'])==3
        yield .3
        for index,(x,y) in enumerate(((4840,330),(4860,500),(4820,300))):
            pose(x,y,270);yield .6
            r=sample(f'c{cycle}_hidden_pose{index}')
            assert r['shown']==0 and r['coverage']==0,'Pose directly reveals cabinet'
            yield .25
        pose(4840,330,270);yield .5
        hidden_source=cabinet().get_path_name()
        # Adjacent rendered frames across the ORIGINAL whole-body transition.
        # No gradual reveal is implemented or asserted by this stage.
        t0=unreal.GameplayStatics.get_time_seconds(world())
        while True:
            elapsed=unreal.GameplayStatics.get_time_seconds(world())-t0
            if elapsed>4:break
            pose(4840,330,270-110*min(elapsed/3,1))
            r=sample(f'c{cycle}_transition')
            assert r['source']==hidden_source and len(r['casters'])==3
            yield 0
        yield .25
        assert sample(f'c{cycle}_visible_same_source')['shown']==3
        yield .3
        pose(4840,330,270);yield .6
        assert sample(f'c{cycle}_hidden_again')['shown']==0
        yield .3
        pose(4500,150,90);yield .6
    (root/'geometry.json').write_text(json.dumps(baseline,indent=2))
    unreal.log('HIDDEN_SHADOW_PASS cycles=2 originalComponents=3 addedGeometry=0')
    yield from cleanup()

def cleanup():
    if controller:controller.set_actor_tick_enabled(True)
    if player:player.set_actor_tick_enabled(True)
    levels.editor_request_end_play()
    yield 2
    for k,v in saved.items():settings.set_editor_property(k,v)
    (root/'checks.json').write_text(json.dumps(dict(failure=failure,rows=rows),indent=2))

sequence=run()
def tick(dt):
    global deadline,sequence,failure
    now=time.monotonic()
    if now<deadline:return
    try:
        assert now-start<240,'PIE deadline exceeded'
        deadline=now+next(sequence)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
    except Exception:
        failure=traceback.format_exc();unreal.log_error('HIDDEN_SHADOW_FAIL '+failure)
        (root/'failed_checks.json').write_text(json.dumps(dict(failure=failure,rows=rows),indent=2))
        sequence=cleanup();deadline=now
handle=unreal.register_slate_post_tick_callback(tick)

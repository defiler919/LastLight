"""Bounded real GPU PIE validation, not a replacement for the user's video.
Only player input ticks/pose are controlled. No source geometry or material
overrides. Per-cell knowledge is checked every tick, not inferred from averages.
"""
import json,time,traceback,ctypes,os
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
root=Path(unreal.Paths.project_saved_dir())/'PropGameplayLab'/'AccumulatedMemory'/time.strftime('PIE_%Y%m%d_%H%M%S')
root.mkdir(parents=True,exist_ok=False)
resolution=(2560,1440) if '-Accumulated1440' in unreal.SystemLibrary.get_command_line() else (1920,1080)
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
saved={k:settings.get_editor_property(k) for k in ('NewWindowWidth','NewWindowHeight')}
for k,v in zip(saved,resolution):settings.set_editor_property(k,v)
rows=[];telemetry=[];calibration={};deadline=0;start=time.monotonic();handle=None
controller=player=room=None;baseline=current_parts=current_actor=previous=None
geometry_checks=cell_checks=0;failure=None;audit=False

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
    assert len(found)<=1,'Second actual cabinet'
    return found[0] if found else None
def geometry(a):
    return dict(actor=transform(a.get_actor_transform()),dimensions=xyz(a.get_editor_property('dimensions')),
        parts=[dict(name=p.get_name(),mesh=p.static_mesh.get_path_name(),transform=transform(p.get_world_transform()),
                    relative=[xyz(p.get_editor_property('relative_location')),[p.get_editor_property('relative_rotation').pitch,p.get_editor_property('relative_rotation').yaw,p.get_editor_property('relative_rotation').roll],xyz(p.get_editor_property('relative_scale3d'))],
                    bounds=[xyz(v) for v in p.get_local_bounds()],world_bounds=[xyz(v) if hasattr(v,'x') else v for v in unreal.SystemLibrary.get_component_bounds(p)],
                    corners=[xyz(unreal.MathLibrary.transform_location(p.get_world_transform(),unreal.Vector(x,y,z))) for x in (p.get_local_bounds()[0].x,p.get_local_bounds()[1].x) for y in (p.get_local_bounds()[0].y,p.get_local_bounds()[1].y) for z in (p.get_local_bounds()[0].z,p.get_local_bounds()[1].z)]) for p in a.get_components_by_class(unreal.StaticMeshComponent)])
def sample(label,shot=False):
    global baseline,current_parts,current_actor,geometry_checks,cell_checks,previous
    a=cabinet();parts=a.get_components_by_class(unreal.StaticMeshComponent) if a else []
    casters=[p for p in parts if p.get_editor_property('cast_shadow') and (p.is_visible() or p.get_editor_property('cast_hidden_shadow'))]
    shown=[p for p in parts if p.is_visible() and not p.get_editor_property('hidden_in_game')]
    if a:
        g=geometry(a)
        if baseline is None:baseline=g
        if g!=baseline:
            (root/'geometry_failure.json').write_text(json.dumps(dict(before=baseline,after=g),indent=2))
            raise AssertionError('Fixed geometry changed')
        assert len(parts)==12 and len(casters)==3 and len(a.get_components_by_class(unreal.PrimitiveComponent))==12
        if len(shown)!=3:
            assert len(shown)==0 and not shot and current_actor!=a.get_path_name(),'Original submission changed'
            # Room initializes this generation's texture in the spawn tick;
            # source submission is still deferred to the next authority tick.
            first=json.loads(room.get_spatial_telemetry())
            assert first['discovered']==0 and first['sourceOpacity']==0 and first['live']==0
        geometry_checks+=1
        if current_actor==a.get_path_name():assert current_parts==[p.get_path_name() for p in parts]
        current_actor=a.get_path_name();current_parts=[p.get_path_name() for p in parts]
    proxy_parts=[];other_proxies=[]
    for actor in actors(unreal.Actor):
        if not actor.get_name().startswith('Remembered_'):continue
        for p in actor.get_components_by_class(unreal.StaticMeshComponent):
            assert not p.get_editor_property('cast_shadow'),'Proxy shadow leak'
            if actor.get_name().startswith('Remembered_Lab_ManualStale_Cabinet') or 'Lab.ManualStale.Cabinet' in actor.get_name():
                proxy_parts.append(p.get_path_name())
            else:other_proxies.append(p.get_path_name())
    if len(proxy_parts)>3:
        (root/'proxy_failure.json').write_text(json.dumps(dict(manual=proxy_parts,other=other_proxies),indent=2))
        raise AssertionError('Extra manual cabinet proxy/duplicate geometry')
    s=json.loads(room.get_spatial_telemetry());bits=list(room.get_spatial_knowledge_bits())
    assert len(proxy_parts)==(3 if s['snapshot'] else 0),'Manual snapshot component count differs'
    assert s['actual']==('PRESENT' if a else 'ABSENT')
    if previous and previous[0]['generation']==s['generation']:
        old_s,old=previous
        assert len(old)==len(bits)
        for i,(p,n) in enumerate(zip(old,bits)):
            if a:
                assert not (p&1) or n&1,('D shrank',i,p,n)
                assert not (n&1 and not p&1) or n&8,('New D outside legal sight',i,p,n)
            else:
                assert not (p&2) or n&2,('V shrank',i,p,n)
                assert not (n&4) or p&4,('Stale resurrected',i,p,n)
                assert not (n&2 and n&4),('Verified cell retains stale',i,n)
        cell_checks+=len(bits)
    previous=(s,bits)
    assert abs(s['discovered']-sum(bool(b&1) for b in bits)/len(bits))<1e-6
    if not a:assert s['sourceOpacity']==0 and len(casters)==0
    assert unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')==4
    assert unreal.SystemLibrary.get_console_variable_int_value('r.ScreenPercentage')==100
    assert unreal.SystemLibrary.get_console_variable_int_value('r.Darkwell.ProjectFogVisual.LabRoute')==0
    p=player.get_actor_location()
    row=dict(label=label,t=unreal.GameplayStatics.get_time_seconds(world()),state=s,
        casters=[p.get_path_name() for p in casters],shown=len(shown),proxy_parts=proxy_parts,other_proxies=other_proxies,
        x=p.x,y=p.y,yaw=player.get_actor_rotation().yaw,viewport=list(controller.get_viewport_size()))
    telemetry.append(row)
    if shot:
        row['file']=f'{len(rows):04}_{label}.png';rows.append(row)
        cmd(f'Shot filename="{(root/row["file"]).as_posix()}" -nosuffix')
        (root/f'{len(rows):04}_cells.json').write_text(json.dumps(bits))
        unreal.log('ACCUMULATED_CHECK '+json.dumps(row))
    return s

def resize_viewport():
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
    assert api.SetWindowPos(windows[0],None,0,0,rect.right-rect.left+resolution[0]-vx,rect.bottom-rect.top+resolution[1]-vy,0x0416)
    yield .3
    assert tuple(controller.get_viewport_size())==resolution
    cmd('r.ScreenPercentage 100')

directions=[('left_to_right',4500,150,-5,90),('right_to_left',4500,150,185,90),('diagonal',4840,330,270,160)]
# Primary 1080p covers three full cycles. Optional 1440p is a bounded one-cycle
# supplement, not another large screenshot matrix.
if resolution==(2560,1440):directions=directions[:1]
def calibrate(spec):
    # Calibrate only against fully verified EMPTY floor. It cannot discover a
    # present generation. Use actual conservative per-cell legal area, not MAX.
    name,x,y,away,full=spec;angles=[]
    for target in (.10,.25,.50,.75):
        lo,hi=0.,1.
        for _ in range(11):
            t=(lo+hi)/2;angle=away+(full-away)*t;pose(x,y,angle);yield .05
            current=json.loads(room.get_spatial_telemetry())['current']
            if current<target:lo=t
            else:hi=t
        angle=away+(full-away)*(lo+hi)/2
        pose(x,y,angle);yield .1
        value=json.loads(room.get_spatial_telemetry())['current']
        assert abs(value-target)<.01,('Calibration',name,target,value)
        angles.append(angle)
    calibration[name]=angles

def sweep_to(x,y,begin,end,duration=1.2,entry_label=None):
    first=last=unreal.GameplayStatics.get_time_seconds(world())
    entry_frames=0
    while last-first<duration:
        t=min(1,(last-first)/duration);pose(x,y,begin+(end-begin)*t)
        if entry_label and entry_frames<5 and json.loads(room.get_spatial_telemetry())['discovered']>0:
            sample(entry_label+f'_entry_adjacent_{entry_frames}',True);entry_frames+=1
        yield 0;last=unreal.GameplayStatics.get_time_seconds(world())
    pose(x,y,end);yield .35

def checkpoints(spec,absent=False):
    name,x,y,away,full=spec;prefix=name+('_erase' if absent else '_discover')
    last_angle=away
    for index,(target,angle) in enumerate(zip((.10,.25,.50,.75),calibration[name])):
        if absent and index==0:continue
        pose(x,y,last_angle);yield .12
        yield from sweep_to(x,y,last_angle,angle,entry_label=prefix if index==0 and not absent else None)
        s=sample(prefix+f'_{int(target*100)}_held',True);yield .08
        assert abs(s['current']-target)<.012
        if absent:assert abs(s['verified']-target)<.015 and abs(s['remaining']-(1-target))<.015
        else:assert abs(s['discovered']-target)<.015 and abs(s['sourceOpacity']-s['discovered'])<1e-6
        pose(x,y,away)
        if not absent and index==1:
            for k in range(5):
                yield 0;sample(prefix+f'_25_exit_adjacent_{k}',True)
        yield .4
        after=sample(prefix+f'_{int(target*100)}_away_gray_or_empty',True);yield .08
        assert after['current']==0 and after['live']==0
        if absent:assert after['verified']==s['verified'] and after['remaining']==s['remaining']
        else:assert after['discovered']==s['discovered'] and after['sourceOpacity']==s['sourceOpacity']
        last_angle=angle
    pose(x,y,last_angle);yield .12
    yield from sweep_to(x,y,last_angle,full)
    s=sample(prefix+'_full',True);yield .08
    if absent:assert s['verified']==1 and s['remaining']==0 and s['proxyOpacity']==0
    else:assert s['discovered']==1 and s['sourceOpacity']==1
    pose(x,y,away);yield .4
    s=sample(prefix+'_full_away',True);yield .08
    assert s['current']==0 and s['live']==0
    if absent:assert s['remaining']==0 and s['proxyOpacity']==0
    else:assert s['sourceOpacity']==1

def run():
    global controller,player,room,audit
    unreal.log('ACCUMULATED_READY '+root.as_posix())
    while not levels.is_in_play_in_editor():yield .2
    yield 3
    controller=unreal.GameplayStatics.get_player_controller(world(),0)
    player=unreal.GameplayStatics.get_player_pawn(world(),0)
    room=actors(unreal.DarkwellManualStaleRoom)[0]
    controller.set_actor_tick_enabled(False);player.set_actor_tick_enabled(False)
    cmd('Darkwell.PropLab stalemanual reset');cmd('Darkwell.PropLab stalemanual mode 2')
    pose(4500,150,90);yield .8
    yield from resize_viewport();audit=True
    sample('initial_full',True);yield .1
    pose(4500,-450,90);yield .4
    pose(4500,150,90);yield .8
    s=sample('initial_empty',True);yield .1
    assert s['actual']=='ABSENT' and s['verified']==1 and s['remaining']==0
    for spec in directions:yield from calibrate(spec)
    for cycle,spec in enumerate(directions):
        name,x,y,away,full=spec
        pose(4840,330,270);yield .4;sample(f'c{cycle}_absent_shadow',True);yield .1
        pose(4720,-450,90);yield .2;pose(4500,-450,90);yield .4
        assert cabinet()
        # Back-facing return, never teleport top after respawn.
        pose(4840,330,270);yield .5
        s=sample(f'c{cycle}_present_hidden_shadow',True);yield .1
        assert s['discovered']==0 and s['sourceOpacity']==0 and s['proxyOpacity']==0 and not s['snapshot']
        yield from checkpoints(spec)
        if cycle==0:
            angle=calibration[name][1]
            pose(x,y,angle);yield .4
            for k in range(24):
                pose(x,y,angle+(.25 if k%2 else -.25))
                yield 0
                if k<6:sample(f'boundary_oscillation_adjacent_{k}',True)
            yield .3
        pose(4720,-450,90);yield .2;pose(4500,-450,90);yield .4
        assert not cabinet()
        yield from checkpoints(spec,True)
    unreal.log(f'ACCUMULATED_PASS cycles={len(directions)} directions={len(directions)} geometryChecks={geometry_checks} perCellChecks={cell_checks} shots={len(rows)}')
    yield from cleanup()

def cleanup():
    global audit
    audit=False
    if levels.is_in_play_in_editor():
        if controller:controller.set_actor_tick_enabled(True)
        if player:player.set_actor_tick_enabled(True)
        levels.editor_request_end_play()
    yield 2
    for k,v in saved.items():settings.set_editor_property(k,v)
    (root/'checks.json').write_text(json.dumps(dict(failure=failure,resolution=resolution,geometry_checks=geometry_checks,cell_checks=cell_checks,calibration=calibration,rows=rows),indent=2))
    (root/'telemetry.json').write_text(json.dumps(telemetry))
    (root/'geometry.json').write_text(json.dumps(baseline,indent=2))

sequence=run()
def tick(dt):
    global deadline,sequence,failure
    now=time.monotonic()
    try:
        if audit and not failure and player and levels.is_in_play_in_editor():sample('invariant_tick')
        if now<deadline:return
        assert now-start<600,'PIE deadline exceeded'
        deadline=now+next(sequence)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
    except Exception:
        failure=traceback.format_exc();unreal.log_error('ACCUMULATED_FAIL '+failure)
        (root/'failed_checks.json').write_text(json.dumps(dict(failure=failure,rows=rows),indent=2))
        sequence=cleanup();deadline=now
handle=unreal.register_slate_post_tick_callback(tick)

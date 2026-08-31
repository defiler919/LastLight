"""Real D3D12/SM6 Editor PIE driver. No production runtime route or saved-map edits.

Run separately with -Mode2Width=1920 and 2560, then start a floating Editor PIE
using EditorAppToolset.StartPIE after MODE2_SOLID_READY. Every frame has its real
world time/pose/state; failures and partial output are kept, never overwritten.
"""
import json, math, re, time, traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
args=unreal.SystemLibrary.get_command_line()
width=int(re.search(r'-Mode2Width=(\d+)',args).group(1))
height=width*9//16
root=Path(unreal.Paths.project_saved_dir())/'PropGameplayLab'/'Mode2Solid'/('PIE_'+str(width)+'_'+time.strftime('%Y%m%d_%H%M%S'))
root.mkdir(parents=True,exist_ok=False)
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
saved_settings={key:settings.get_editor_property(key) for key in ('NewWindowWidth','NewWindowHeight')}
settings.set_editor_property('NewWindowWidth',width)
settings.set_editor_property('NewWindowHeight',height)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
rows=[]; handle=None; deadline=0; start=time.monotonic(); controller=None; failed=False
player=room=solid=None

def world():return editor.get_game_world()
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(world(),cls)
def cmd(s):unreal.SystemLibrary.execute_console_command(world(),s)
def pose(x,y,yaw):
    player.set_actor_location(unreal.Vector(x,y,92),False,True)
    player.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),True)
def cabinet():
    return next((a for a in actors(unreal.DarkwellPropLabFurniture) if str(a.get_editor_property('stable_id'))=='Lab.ManualStale.Cabinet'),None)

def resize_pie_window():
    # Slate's initial sane placement clamps 1440p to the desktop work area.
    # Resize only this process's PIE window by the measured *viewport* delta;
    # capture its genuine D3D12 backbuffer, never an upscaled desktop image.
    import ctypes, os
    from ctypes import wintypes
    api=ctypes.WinDLL('user32',use_last_error=True)
    api.GetWindowThreadProcessId.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.DWORD)]
    api.GetWindowTextW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
    api.GetWindowRect.argtypes=[wintypes.HWND,ctypes.POINTER(wintypes.RECT)]
    api.SetWindowPos.argtypes=[wintypes.HWND,wintypes.HWND,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,wintypes.UINT]
    found=[]
    @ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
    def visit(hwnd,param):
        pid=wintypes.DWORD();api.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
        name=ctypes.create_unicode_buffer(1024);api.GetWindowTextW(hwnd,name,1024)
        if pid.value==os.getpid() and 'NetMode:' in name.value:found.append(hwnd)
        return True
    api.EnumWindows(visit,0)
    assert len(found)==1,found
    rect=wintypes.RECT();assert api.GetWindowRect(found[0],ctypes.byref(rect))
    vx,vy=controller.get_viewport_size()
    # SWP_NOSENDCHANGING is also used by UE's WindowsWindow resize path to
    # prevent the OS limiting the requested size to the monitor resolution.
    assert api.SetWindowPos(found[0],None,0,0,rect.right-rect.left+width-vx,rect.bottom-rect.top+height-vy,0x0416)
    unreal.log(f'MODE2_VIEWPORT_RESIZE from={vx}x{vy} requested={width}x{height}')
def sample(label,shot=False):
    p=player.get_actor_location()
    a=cabinet()
    body_drawn=bool(a and any(m.is_visible() and not m.get_editor_property('hidden_in_game') for m in a.get_components_by_class(unreal.StaticMeshComponent)))
    row=dict(label=label,t=unreal.GameplayStatics.get_time_seconds(world()),x=p.x,y=p.y,yaw=player.get_actor_rotation().yaw,
        actual=room.has_actual_cabinet(),verified=room.get_verified_fraction(),remaining=room.get_remaining_opacity(),
        coverage=room.get_cabinet_coverage(),reveal=solid.get_reveal_fraction(),caps=solid.get_cap_triangles(),
        live_triangles=solid.get_live_triangles(),shadow_sources=solid.get_shadow_sources(),source_body_drawn=body_drawn,
        toggles=room.get_toggle_count(),status=room.get_status())
    assert not body_drawn,'Unclipped real body leaked'
    assert row['actual'] or row['shadow_sources']==0,'ABSENT cast a real shadow'
    assert row['shadow_sources'] in (0,3),'Duplicate shadow casters'
    assert not actors(unreal.DarkwellStalkerCharacter)
    assert unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')==4
    assert unreal.SystemLibrary.get_console_variable_int_value('r.Darkwell.ProjectFogVisual.LabRoute')==0
    if shot:
        name=f'{len(rows):04d}_{label}.png'
        row['image']=name
        cmd('Shot filename="'+(root/name).as_posix()+'"')
    rows.append(row)
    return row

def sweep(label,moving=False):
    begin=unreal.GameplayStatics.get_time_seconds(world()); last=-1; partial=0
    while True:
        elapsed=unreal.GameplayStatics.get_time_seconds(world())-begin
        if elapsed>7:break
        yaw=10+160*elapsed/7
        x=4500+55*math.sin(elapsed*.8) if moving else 4500
        y=150+20*math.sin(elapsed*1.2) if moving else 150
        # Continuous player translation plus aiming, using actual world delta.
        pose(x,y,yaw)
        yield 0
        t=unreal.GameplayStatics.get_time_seconds(world())-begin
        capture=t-last>=1/15
        s=sample(label,capture)
        if capture:last=t
        partial+=0<s['reveal']<.85 if 'reveal' in label else 0<s['verified']<1
    assert partial>3,'No spatially partial frames in sweep'
    # Shot is consumed by the next rendered frame. Do not let the caller's
    # deliberate end-of-sweep teleport overwrite the last requested image.
    yield .15

def run():
    global player,controller,room,solid
    yield 2
    unreal.log('MODE2_SOLID_READY floatingPIE=1 width='+str(width))
    while not levels.is_in_play_in_editor():yield .1
    yield 8
    assert levels.is_in_play_in_editor()
    player=unreal.GameplayStatics.get_player_pawn(world(),0)
    controller=unreal.GameplayStatics.get_player_controller(world(),0)
    controller.set_actor_tick_enabled(False)
    room=actors(unreal.DarkwellManualStaleRoom)[0];solid=room.get_mode2_presentation()
    cmd('r.AntiAliasingMethod 4');cmd('r.ScreenPercentage 100');cmd('r.VSync 0')
    cmd(f'r.SetRes {width}x{height}w')
    for attempt in range(4):
        if tuple(controller.get_viewport_size())==(width,height):break
        resize_pie_window()
        yield .5 # allow Slate/DPI and RHI resize to settle before measuring again
    assert tuple(controller.get_viewport_size())==(width,height),controller.get_viewport_size()
    cmd('Darkwell.PropLab stalemanual mode 2');cmd('Darkwell.PropLab stalemanual reset')
    pose(4500,150,90)
    yield 3
    s=sample('initial_full',True)
    assert s['actual'] and s['reveal']>.9 and 'Snapshot: VALID' in s['status']
    yield .3
    import struct
    # PNG compression runs on the existing async writer; wait for a complete
    # file without stopping PIE ticks or changing visual fade timing.
    write_deadline=time.monotonic()+5
    first=[]
    while time.monotonic()<write_deadline:
        first=list(root.glob('0000_initial_full*.png'))
        if len(first)==1 and first[0].stat().st_size>24:break
        yield .1
    assert len(first)==1 and first[0].stat().st_size>24,'Initial screenshot was not written'
    size=struct.unpack('>II',first[0].read_bytes()[16:24])
    assert size==(width,height),f'Actual GPU viewport size {size}, expected {(width,height)}'
    # Separate uncaptured moving interval: synchronous GPU readbacks must not
    # be mistaken for ordinary gameplay frame time.
    begin=unreal.GameplayStatics.get_time_seconds(world())
    while unreal.GameplayStatics.get_time_seconds(world())-begin<3:
        t=unreal.GameplayStatics.get_time_seconds(world())-begin
        pose(4500+50*math.sin(t*2),150,55+25*math.sin(t*2))
        yield 0
        sample('uncaptured_moving')
    pose(4500,150,90)
    yield .4
    for cycle in range(2):
        pose(4500,-450,90)
        yield .5
        s=sample(f'cycle{cycle}_absent_unverified',True)
        assert not s['actual'] and s['verified']==0 and s['remaining']==1 and s['shadow_sources']==0
        yield .2
        pose(4500,150,55)
        yield .6
        s=sample(f'cycle{cycle}_half_solid_cap',True)
        assert 0<s['verified']<1 and s['caps']>0
        # Consecutive stationary frames expose jitter/TSR instability.
        for i in range(12):
            yield 1/30
            sample(f'cycle{cycle}_static_cap',True)
        yield from sweep(f'cycle{cycle}_erase',cycle==1)
        pose(4500,150,90)
        yield .5
        s=sample(f'cycle{cycle}_verified_empty',True)
        assert s['verified']==1 and s['remaining']==0 and 'Snapshot: EMPTY' in s['status'] and s['caps']==0
        yield .2
        pose(4500,150,-90)
        yield .7
        sample(f'cycle{cycle}_absent_shadow_control',True)
        yield .2
        pose(4720,-450,90)
        yield .3
        pose(4500,-450,90)
        yield .5
        s=sample(f'cycle{cycle}_respawn_occluded',True)
        assert s['actual'] and s['coverage']==0 and s['live_triangles']==0 and s['shadow_sources']==3
        yield .2
        pose(4500,150,-90)
        yield .7
        s=sample(f'cycle{cycle}_present_hidden_shadow',True)
        assert s['coverage']==0 and s['live_triangles']==0 and 'Snapshot: EMPTY' in s['status']
        yield .2
        yield from sweep(f'cycle{cycle}_reveal',cycle==1)
        pose(4500,150,90)
        yield .5
        s=sample(f'cycle{cycle}_rediscovered_full',True)
        assert s['reveal']>.9 and s['shadow_sources']==3 and 'Snapshot: VALID' in s['status']
        yield .3
        pose(4720,-450,90)
        yield .5
        s=sample(f'cycle{cycle}_leave_remembered',True)
        assert s['live_triangles']==0 and s['remaining']==1 and s['shadow_sources']==3
        yield .2
    controller.set_actor_tick_enabled(True)
    levels.editor_request_end_play()
    yield 2
    assert not levels.is_in_play_in_editor()
    for key,value in saved_settings.items():settings.set_editor_property(key,value)
    (root/'checks.json').write_text(json.dumps(rows,indent=2))
    unreal.log(f'MODE2_SOLID_PIE_PASS width={width} height={height} cycles=2 rows={len(rows)} root={root}')

sequence=run()
def tick(dt):
    global deadline,sequence,failed
    now=time.monotonic()
    if now<deadline:return
    try:
        assert failed or now-start<240,'GPU PIE timeout'
        deadline=now+next(sequence)
    except StopIteration:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
    except Exception as error:
        failed=True
        unreal.log_error('MODE2_SOLID_PIE_FAIL '+repr(error)+'\n'+traceback.format_exc())
        (root/'failed_checks.json').write_text(json.dumps(rows,indent=2))
        if controller:controller.set_actor_tick_enabled(True)
        levels.editor_request_end_play()
        for key,value in saved_settings.items():settings.set_editor_property(key,value)
        def quit_later():yield 2
        sequence=quit_later();deadline=now+2
handle=unreal.register_slate_post_tick_callback(tick)

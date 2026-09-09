"""Bounded real PIE smoke, not human gameplay acceptance. No asset writes."""
import unreal, time, json, traceback, os
from pathlib import Path
root = Path(unreal.Paths.project_saved_dir()) / os.environ.get('DARKWELL_APARTMENT_PIE_RUN','ApartmentPIE')
root.mkdir(exist_ok=False)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_SightWeaveApartmentLab')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
start=time.monotonic(); stamp=start; phase=0; w=p=pc=trigger=None
busy=False
data={'kind':'automated D3D12 PIE smoke; not manual acceptance','stages':[]}
def snapshot(label):
    rows=[]
    for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
        row={'path':a.get_path_name(),'class':a.get_class().get_path_name(),'components':[]}
        for c in a.get_components_by_class(unreal.ActorComponent):
            cr={'class':c.get_class().get_path_name(),'path':c.get_path_name(),'tick':c.is_component_tick_enabled()}
            if isinstance(c,unreal.MeshComponent):
                cr['materials']=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]
            row['components'].append(cr)
        rows.append(row)
        if isinstance(a,unreal.DarkwellHUD):unreal.SystemLibrary.execute_console_command(w,'obj dump '+a.get_path_name())
    for n in ['DarkwellFogVisualSubsystem','DarkwellSightWeaveWorldSubsystem','SightWeaveRenderWorldSubsystem']:
        o=unreal.find_object(None,w.get_path_name()+':'+n+'_0')
        if o:unreal.SystemLibrary.execute_console_command(w,'obj dump '+o.get_path_name())
    data['stages'].append({'label':label,'active':trigger.is_active(),'actors':rows})
    (root/'runtime.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
    unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(root/(label+'.png')))
def pose(x,y,yaw):
    p.set_actor_location(unreal.Vector(x,y,92),False,True)
    p.set_actor_rotation(unreal.Rotator(yaw=yaw),True)
def tick(dt):
    global phase,stamp,w,p,pc,trigger,busy
    if busy:return
    busy=True
    try:
        now=time.monotonic()
        if now-start>240:raise RuntimeError('PIE smoke timeout')
        if phase==0 and now-stamp>2:
            levels.editor_request_begin_play(); phase=1;stamp=now
        elif phase==1 and now-stamp>8:
            w=unreal.EditorLevelLibrary.get_game_world()
            assert w
            p=unreal.GameplayStatics.get_player_pawn(w,0);pc=unreal.GameplayStatics.get_player_controller(w,0)
            # Freeze cursor steering only for repeatable automated captures.
            pc.set_actor_tick_enabled(False)
            trigger=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DarkwellBlackRegionTrigger)[0]
            assert not trigger.is_active()
            snapshot('01_entry_closed');phase=2;stamp=now
        elif phase==2 and now-stamp>4:
            pose(0,100,90);phase=3;stamp=now
        elif phase==3 and now-stamp>6:
            snapshot('02_living_observed');pose(350,170,60);phase=4;stamp=now
        elif phase==4 and now-stamp>6:
            snapshot('03_bedroom_observed');pose(0,100,180);phase=5;stamp=now
        elif phase==5 and now-stamp>5:
            snapshot('04_room_remembered');assert trigger.activate(),trigger.get_last_failure();phase=6;stamp=now
        elif phase==6 and now-stamp>4:
            snapshot('05_blackout');pose(350,170,60);phase=7;stamp=now
        elif phase==7 and now-stamp>5:
            snapshot('06_blocked_live');pose(0,100,180);phase=8;stamp=now
        elif phase==8 and now-stamp>5:
            snapshot('07_blocked_away');trigger.deactivate();phase=9;stamp=now
        elif phase==9 and now-stamp>4:
            assert not trigger.is_active();snapshot('08_deactivated_no_restore');pose(350,170,60);phase=10;stamp=now
        elif phase==10 and now-stamp>5:
            snapshot('09_reobserve');phase=11;stamp=now
        elif phase==11 and now-stamp>4:
            pc.set_actor_tick_enabled(True);levels.editor_request_end_play();phase=12;stamp=now
        elif phase==12 and now-stamp>3:
            (root/'complete.txt').write_text('D3D12 PIE lifecycle and captures completed. Scripted pawn placement and Trigger API; NOT manual F/walking/visual acceptance.',encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
    except Exception:
        (root/'error.txt').write_text(traceback.format_exc(),encoding='utf-8')
        unreal.log_error(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
    finally:
        busy=False
handle=unreal.register_slate_post_tick_callback(tick)

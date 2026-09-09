"""Bounded real D3D12 PIE contract/captures. Scripted poses, not human acceptance."""
import unreal,time,json,os,traceback
from pathlib import Path
root=Path(unreal.Paths.project_saved_dir())/'StaticKnowledge'/os.environ.get('DARKWELL_STATIC_PIE_RUN','PIE1')
root.mkdir(parents=True,exist_ok=False)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/Maps/L_SightWeaveApartmentLab')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
start=stamp=time.monotonic();phase=0;busy=False;w=p=pc=lab=k=trigger=None
pending=None;pending_file=None
data={'kind':'automated D3D12 PIE contract, not manual acceptance','stages':[]}
target=unreal.Vector2D(450.3125,170.3125)
def pose(x,y,yaw):
    lab.set_observer_pose_for_testing(unreal.Vector(x,y,92),yaw)
def capture(name, after=None):
    global pending,pending_file
    data['stages'].append({'name':name,'stored':k.has_stored_memory(target),'legal':k.get_legal_coverage(target),'door_probe_legal':k.get_legal_coverage(unreal.Vector2D(.3125,50.3125)),'yaw':p.get_actor_rotation().yaw,'active':trigger.is_active(),'telemetry':json.loads(k.get_telemetry())})
    (root/'results.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
    unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(root/(name+'.png')))
    pending=after or (lambda:None);pending_file=root/(name+'.png')
def tick(dt):
    global stamp,phase,busy,w,p,pc,lab,k,trigger,pending
    if busy:return
    busy=True
    try:
        now=time.monotonic()
        if now-start>220:raise RuntimeError('PIE deadline')
        # Screenshot capture is asynchronous. Hold the exact stage until the
        # file exists, then mutate pose/event and start the next settling period.
        if pending is not None:
            if pending_file.exists() and now-stamp>1:
                action=pending;pending=None;action();stamp=now
            return
        if phase==0 and now-stamp>2:
            levels.editor_request_begin_play();phase=1;stamp=now
        elif phase==1 and now-stamp>8:
            w=unreal.EditorLevelLibrary.get_game_world();p=unreal.GameplayStatics.get_player_pawn(w,0);pc=unreal.GameplayStatics.get_player_controller(w,0)
            pc.set_actor_tick_enabled(False)
            lab=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DarkwellApartmentLab)[0]
            trigger=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DarkwellBlackRegionTrigger)[0]
            k=lab.get_static_knowledge()
            assert json.loads(k.get_telemetry())['objects']==29
            assert not k.has_stored_memory(target),'unexplored bedroom preknown'
            capture('01_initial_unknown',lambda:pose(0,-250,90));phase=2;stamp=now
        elif phase==2 and now-stamp>3:
            assert not k.has_stored_memory(unreal.Vector2D(.3125,50.3125)),'closed door leaked'
            capture('02_closed_door',lambda:lab.set_doors_open_for_testing(True));phase=3;stamp=now
        elif phase==3 and now-stamp>4:
            assert k.has_stored_memory(unreal.Vector2D(.3125,50.3125)),'open doorway not observed'
            assert not k.has_stored_memory(unreal.Vector2D(300.3125,-49.6875)),'solid side wall leaked'
            capture('03_open_door_wall_blocks',lambda:pose(350,170,60));phase=4;stamp=now
        elif phase==4 and now-stamp>4:
            assert k.has_stored_memory(target),'legal bedroom observation missing'
            capture('04_bedroom_live',lambda:pose(0,100,180));phase=5;stamp=now
        elif phase==5 and now-stamp>3:
            assert k.has_stored_memory(target) and k.get_legal_coverage(target)<.99
            def activate_checked():
                assert trigger.activate(),trigger.get_last_failure()
                assert not k.has_stored_memory(target),'Clear not same-call'
            capture('05_remembered',activate_checked)
            phase=6;stamp=now
        elif phase==6 and now-stamp>3:
            assert not k.has_stored_memory(target);capture('06_clear_block_unknown',lambda:pose(350,170,60));phase=7;stamp=now
        elif phase==7 and now-stamp>3:
            assert k.get_legal_coverage(target)>=.99 and not k.has_stored_memory(target),'Block changed live or allowed writes'
            capture('07_blocked_live',lambda:pose(0,100,180));phase=8;stamp=now
        elif phase==8 and now-stamp>3:
            def release_checked():
                trigger.deactivate()
                assert not k.has_stored_memory(target),'release revived old memory'
            assert not k.has_stored_memory(target);capture('08_blocked_away',release_checked)
            phase=9;stamp=now
        elif phase==9 and now-stamp>3:
            assert not k.has_stored_memory(target);capture('09_released_no_restore',lambda:pose(350,170,60));phase=10;stamp=now
        elif phase==10 and now-stamp>3:
            assert k.has_stored_memory(target);capture('10_reobserved');phase=11;stamp=now
        elif phase==11 and now-stamp>3:
            # Repeated real Trigger transactions, no replacement Clear/Block logic.
            for i in range(20):
                assert trigger.activate();assert trigger.activate();assert not k.has_stored_memory(target)
                trigger.deactivate();trigger.deactivate();assert not k.has_stored_memory(target)
            assert trigger.activate();levels.editor_request_end_play();phase=12;stamp=now
        elif phase==12 and now-stamp>4:
            (root/'complete.txt').write_text('Static authority / doors / Clear+Block / 20 transitions / active teardown completed. Automated, not manual.',encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
    except Exception:
        (root/'error.txt').write_text(traceback.format_exc(),encoding='utf-8');unreal.log_error(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
    finally:busy=False
handle=unreal.register_slate_post_tick_callback(tick)

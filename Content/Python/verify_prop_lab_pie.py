"""Actual PIE lifecycle/control smoke test; finishes in the lab editor, without saving maps."""
import time
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/Maps/L_ProjectFogPropGameplayLab')
started = time.monotonic()
stage = 0
handle = None

def command(world, text):
    unreal.SystemLibrary.execute_console_command(world, text)

def tick(delta):
    global stage
    elapsed = time.monotonic()-started
    if stage == 0 and elapsed > 2:
        levels.editor_request_begin_play()
        stage = 1
    elif stage == 1 and elapsed > 9:
        assert levels.is_in_play_in_editor(), 'Actual PIE failed to start'
        world = editor.get_game_world()
        assert world
        command(world, 'Darkwell.PropLab mode 1')
        command(world, 'Darkwell.PropLab policy 1')
        command(world, 'Darkwell.PropLab help')
        command(world, 'Shot')
        unreal.log('LAB_PIE_ACTIVE_HARD_POLICY1')
        stage = 2
    elif stage == 2 and elapsed > 12:
        world = editor.get_game_world()
        command(world, 'Darkwell.PropLab mode 2')
        command(world, 'Darkwell.PropLab lantern')
        command(world, 'Shot')
        stage = 3
    elif stage == 3 and elapsed > 15:
        command(editor.get_game_world(), 'Darkwell.PropLab torch')
        levels.editor_request_end_play()
        stage = 4
    elif stage == 4 and elapsed > 19:
        assert not levels.is_in_play_in_editor()
        world = editor.get_editor_world()
        # Console reads and rejected out-of-map commands remain in the log.
        command(world, 'r.Darkwell.ProjectFogVisual.PropPresentationMode')
        command(world, 'r.Darkwell.ProjectFogVisual.PropRelocationPolicy')
        command(world, 'Darkwell.PropLab mode 2')
        unreal.log('LAB_PIE_LIFECYCLE_PASS: stopped PIE; inspect CVar readback=0 and command rejection')
        unreal.unregister_slate_post_tick_callback(handle)
        stage = 5

handle = unreal.register_slate_post_tick_callback(tick)

"""Bounded lifecycle reduction. No screenshots, stress, forced GC or exit delay.

The main-window variant is driven externally through its actual close button.
This variant deliberately records that Python/Slate is part of its reproduction.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal

root = Path(os.environ['DARKWELL_EXIT_OUTPUT'])
cycles = int(os.environ.get('DARKWELL_EXIT_CYCLES', '0'))
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
events = []
started = time.monotonic()

def record(event):
    events.append(dict(event=event, wall_seconds=time.monotonic()-started,
                       game_world=editor.get_game_world() is not None))
    (root/'lifecycle.json').write_text(json.dumps(events, indent=2), encoding='utf-8')
    unreal.log('GRAY_EXIT ' + event)

def sequence():
    record('script_started')
    # Establish a drawn editor; subsequent waits are predicates on actual PIE state.
    yield 30
    for index in range(cycles):
        record(f'play_requested_{index}')
        levels.editor_request_begin_play()
        while editor.get_game_world() is None:
            yield 1
        record(f'play_started_{index}')
        yield 60
        levels.editor_request_end_play()
        while editor.get_game_world() is not None:
            yield 1
        record(f'pie_stopped_{index}')
    record('protocol_complete')
    (root/'complete.json').write_text(json.dumps(dict(cycles=cycles)), encoding='utf-8')

steps = sequence()
left = 0
def tick(delta):
    global left
    try:
        if time.monotonic()-started > 180:
            raise TimeoutError('Lifecycle did not complete in 180 seconds')
        if left:
            left -= 1
        else:
            left = max(0, next(steps)-1)
    except (StopIteration, Exception) as error:
        if not isinstance(error, StopIteration):
            (root/'failed.txt').write_text(traceback.format_exc(), encoding='utf-8')
            unreal.log_error(traceback.format_exc())
            levels.editor_request_end_play()
        unreal.unregister_slate_post_tick_callback(handle)
        record('callback_unregistered_close_requested')
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle = unreal.register_slate_post_tick_callback(tick)

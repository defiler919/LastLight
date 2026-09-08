"""Foreground handshake for the bounded Unknown Partial functional tests."""
import json
import os
import sys
import time
import traceback
from pathlib import Path
import unreal

root = Path(os.environ['DARKWELL_UNKNOWN_TEST_OUTPUT'])
sys.path.insert(0, str(Path(__file__).parent))
from gray_benchmark_foreground import await_foreground, require_foreground

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
diagnostic = unreal.get_default_object(unreal.DarkwellSightWeaveGrayPolicyLabDirector)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
started = time.perf_counter()
timeout_seconds = int(os.environ.get('DARKWELL_UNKNOWN_TEST_TIMEOUT', '180'))


def environment():
    return json.loads(diagnostic.get_frame_environment_for_testing())


def run():
    yield from await_foreground(root, environment)
    unreal.SystemLibrary.execute_console_command(
        editor.get_editor_world(), 'Automation RunTests ' + os.environ['DARKWELL_UNKNOWN_TEST_SELECTOR'])
    # TestExit owns normal termination; no input, gameplay or performance run.
    while True:
        require_foreground(root, environment())
        yield


sequence = run()


def tick(_delta):
    try:
        if time.perf_counter() - started > timeout_seconds:
            raise RuntimeError('Bounded Unknown functional test timeout')
        next(sequence)
    except Exception:
        (root / 'failed.txt').write_text(traceback.format_exc(), encoding='utf-8')
        unreal.log_error(traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)


handle = unreal.register_slate_post_tick_callback(tick)

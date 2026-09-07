"""No UE/window needed: deterministic gate, cancellation and invalid-sample checks."""
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'Content/Python'))
import gray_benchmark_foreground as gate


class ForegroundGateTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.engine = dict(frame=0, foreground=1, minimized=0)
        self.sequence = gate.await_foreground(self.root, lambda: dict(self.engine))

    def step(self):
        self.engine['frame'] += 1
        return next(self.sequence)

    def test_stable_frames_still_need_runner_approval(self):
        for _ in range(12): self.step()
        self.assertTrue(json.loads((self.root/'foreground-live.json').read_text())['ready'])
        self.assertFalse((self.root/'foreground-confirmed.json').exists())
        (self.root/'foreground-approved.json').write_text('{}')
        with self.assertRaises(StopIteration): self.step()
        self.assertTrue((self.root/'foreground-confirmed.json').exists())

    def test_duplicate_frames_do_not_count_and_loss_resets(self):
        self.step()
        for _ in range(20): next(self.sequence)
        self.assertEqual(json.loads((self.root/'foreground-live.json').read_text())['consecutive'], 1)
        self.engine['foreground'] = 0
        self.step()
        self.assertEqual(json.loads((self.root/'foreground-live.json').read_text())['consecutive'], 0)

    def test_runner_abort_is_terminal(self):
        (self.root/'foreground-abort.json').write_text('{}')
        with self.assertRaisesRegex(RuntimeError, 'Runner aborted'): self.step()

    def test_deadline_is_bounded(self):
        with patch.object(gate.time, 'perf_counter', side_effect=[0, 46]):
            with self.assertRaisesRegex(RuntimeError, 'timed out'): self.step()

    def test_loss_during_approval_or_measurement_is_invalid(self):
        for _ in range(12): self.step()
        (self.root/'foreground-approved.json').write_text('{}')
        self.engine['foreground'] = 0
        with self.assertRaisesRegex(RuntimeError, 'Foreground lost'): self.step()
        self.assertTrue((self.root/'foreground-lost.json').exists())
        with self.assertRaisesRegex(RuntimeError, 'Foreground lost'):
            gate.require_foreground(self.root, dict(foreground=1, minimized=1))


if __name__ == '__main__': unittest.main()

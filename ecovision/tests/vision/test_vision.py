"""Host-only behavioral tests. These do not exercise a camera or KPU."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'firmware' / 'k230'))


class FakeBackend:
    def __init__(self, frames):
        self.frames = iter(frames)
        self.closed = False

    def detect(self):
        value = next(self.frames)
        if isinstance(value, Exception):
            raise value
        return value

    def close(self):
        self.closed = True


def boxes(count):
    return [[0, 0.8, 10, 20, 100, 200] for _ in range(count)]


class VisionTests(unittest.TestCase):
    def session(self, frames):
        import vision
        backend = FakeBackend(frames)
        return vision.VisionSession(lambda: backend), backend

    def test_zero_to_five_are_valid_without_debounce(self):
        session, _ = self.session([boxes(i) for i in range(6)])
        for count in range(6):
            state = session.tick(count * 1000)
            self.assertEqual((state['count'], state['valid']), (count, True))

    def test_failure_replaces_previous_count_and_recovery_is_immediate(self):
        session, _ = self.session([boxes(2), RuntimeError('camera'), boxes(0)])
        self.assertEqual(session.tick(0)['count'], 2)
        fault = session.tick(1000)
        self.assertFalse(fault['valid'])
        self.assertIsNone(fault['count'])
        self.assertEqual(session.tick(2000)['count'], 0)

    def test_none_result_is_invalid_but_empty_list_is_valid_zero(self):
        session, _ = self.session([None, []])
        self.assertFalse(session.tick(0)['valid'])
        self.assertTrue(session.tick(1000)['valid'])

    def test_over_capacity_is_not_clamped_to_five(self):
        session, _ = self.session([boxes(6)])
        state = session.tick(0)
        self.assertEqual(state['reason'], 'scene_capacity_exceeded')
        self.assertIsNone(state['count'])

    def test_malformed_results_are_invalid(self):
        for detections in ([[]], [[0, float('nan'), 0, 0, 1, 1]],
                           [[0, .8, 20, 0, 10, 5]], [[1, .8, 0, 0, 1, 1]]):
            with self.subTest(detections=detections):
                session, _ = self.session([detections])
                self.assertFalse(session.tick(0)['valid'])

    def test_heartbeat_includes_fault_without_reinitializing_forever(self):
        import vision
        calls = []
        def fail():
            calls.append(1)
            raise OSError('model missing')
        session = vision.VisionSession(fail)
        first = session.tick(0)
        self.assertIsNone(session.tick(500))
        second = session.tick(1000)
        self.assertEqual((first['seq'], second['seq']), (0, 1))
        self.assertFalse(second['valid'])
        self.assertEqual(len(calls), 1)

    def test_fault_between_heartbeats_is_not_hidden_by_old_success(self):
        session, _ = self.session([boxes(2), OSError('sensor'), OSError('sensor')])
        session.tick(0)
        self.assertIsNone(session.tick(100))
        self.assertFalse(session.tick(1000)['valid'])

    def test_resources_are_closed(self):
        session, backend = self.session([[]])
        session.tick(0)
        session.close()
        self.assertTrue(backend.closed)

    def test_unconfirmed_configuration_cannot_initialize_hardware(self):
        import canmv_backend
        with self.assertRaisesRegex(ValueError, 'board'):
            canmv_backend.validate_config({})

    def test_frame_quality_rejects_missing_wrong_shape_and_saturation(self):
        from canmv_backend import validate_frame
        class Frame:
            def __init__(self, shape, low, high):
                self.shape, self.low, self.high = shape, low, high
            def min(self):
                return self.low
            def max(self):
                return self.high
        class ArrayModule:
            @staticmethod
            def min(frame):
                return frame.low
            @staticmethod
            def max(frame):
                return frame.high
        for frame in (None, Frame((720, 1280, 3), 0, 100),
                      Frame((3, 720, 1280), 0, 0),
                      Frame((3, 720, 1280), 255, 255)):
            with self.subTest(frame=frame):
                with self.assertRaises(ValueError):
                    validate_frame(frame, [1280, 720], ArrayModule)
        validate_frame(Frame((3, 720, 1280), 2, 200), [1280, 720], ArrayModule)

    def test_sequence_wrap_does_not_stop_heartbeat(self):
        session, _ = self.session([[], []])
        session.seq = 0xffffffff
        self.assertEqual(session.tick(0)['seq'], 0xffffffff)
        self.assertEqual(session.tick(1000)['seq'], 0)


if __name__ == '__main__':
    unittest.main()

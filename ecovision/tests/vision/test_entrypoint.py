import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'firmware' / 'k230'))
import main


class EntryTests(unittest.TestCase):
    def test_uart_disabled_does_not_import_machine(self):
        self.assertIsNone(main.open_uart({'enabled': False}))

    def test_uart_unconfirmed_fails_before_pinmux(self):
        with self.assertRaisesRegex(ValueError, 'pinmap'):
            main.open_uart({'enabled': True})

    def test_short_uart_writes_are_completed(self):
        class UART:
            sent = b''
            def write(self, data):
                self.sent += data[:2]
                return len(data[:2])
        uart = UART()
        main.write_frame(uart, b'$frame\r\n')
        self.assertEqual(uart.sent, b'$frame\r\n')

    def test_stalled_uart_write_does_not_loop(self):
        class UART:
            def write(self, data):
                return None
        with self.assertRaises(OSError):
            main.write_frame(UART(), b'abc')

    def test_boot_id_does_not_repeat_across_runs(self):
        import boot_identity
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
            path = str(Path(directory) / 'boot_id')
            first = boot_identity.next_boot_id(path, sync=lambda: None)
            second = boot_identity.next_boot_id(path, sync=lambda: None)
            self.assertEqual(int(second, 16), int(first, 16) + 1)
            self.assertEqual(len(second), 16)

    def test_corrupt_boot_id_file_fails_closed(self):
        import boot_identity
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
            path = Path(directory) / 'boot_id'
            path.write_text('broken')
            with self.assertRaises(ValueError):
                boot_identity.next_boot_id(str(path), sync=lambda: None)

    def test_main_loop_handshake_real_fault_and_recovery_frames(self):
        from protocol import frame
        boot = 'fedcba9876543210'
        link = '0123456789abcdef'
        class UART:
            pending = frame('EVQ,' + link)
            sent = b''
            closed = False
            def readinto(self, buf):
                count = len(self.pending)
                buf[:count] = self.pending
                self.pending = b''
                return count
            def write(self, data):
                self.sent += data
                return len(data)
            def deinit(self):
                self.closed = True
        class Backend:
            index = 0
            closed = False
            def detect(self):
                self.index += 1
                if self.index == 1:
                    return [[0, .9, 0, 0, 100, 200]]
                if self.index == 2:
                    raise OSError('camera frame failed')
                if self.index == 3:
                    return []
                raise KeyboardInterrupt
            def close(self):
                self.closed = True
        uart, backend = UART(), Backend()
        clock = [0]
        def sleep_ms(unused):
            clock[0] += 1000
        fake_time = SimpleNamespace(ticks_ms=lambda: clock[0],
                                    ticks_diff=lambda a, b: a - b, sleep_ms=sleep_ms)
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent) as directory:
            config = Path(directory) / 'config.json'
            config.write_text('{}')
            with patch.object(main, 'time', fake_time), patch.object(main, 'open_uart', return_value=uart), patch.object(main, 'next_boot_id', return_value=boot), patch.object(main, 'CanMVBackend', return_value=backend), patch('builtins.print'):
                main.run(str(config))
        expected = (frame('EVH,' + link + ',' + boot)
                    + frame('EV1,' + link + ',' + boot + ',0,0,1,1')
                    + frame('EV1,' + link + ',' + boot + ',1,1000,0,0')
                    + frame('EV1,' + link + ',' + boot + ',2,2000,0,1'))
        self.assertEqual(uart.sent, expected)
        self.assertTrue(uart.closed)
        self.assertTrue(backend.closed)


if __name__ == '__main__':
    unittest.main()

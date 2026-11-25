import sys
from pathlib import Path
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'firmware/k230'))
from runtime_identity import reserve_random_id

class IdentityTests(unittest.TestCase):
    def test_cold_boot_entropy_repetition_preserves_the_ledger(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'ids'
            first = reserve_random_id(str(path), lambda size: b'\x01' * size)
            second = reserve_random_id(str(path), lambda size: b'\x01' * size)
            third = reserve_random_id(str(path), lambda size: b'\x01' * size)
            fresh = reserve_random_id(str(path), lambda size: b'\x02' * size)

            self.assertEqual(first, '01' * 8)
            self.assertEqual(fresh, '02' * 8)
            self.assertEqual(len({first, second, third, fresh}), 4)
            self.assertEqual(path.read_text().splitlines(), [first, second, third, fresh])

    def test_repeated_entropy_skips_used_ids_after_wrapping(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'ids'
            path.write_text('0000000000000000\n0000000000000001\nffffffffffffffff\n')
            reserved = reserve_random_id(str(path), lambda size: b'\xff' * size)
            self.assertEqual(reserved, '0000000000000002')

    def test_corrupt_history_is_not_reset(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'ids';p.write_text('broken\n')
            with self.assertRaises(ValueError):reserve_random_id(str(p),lambda n:b'\x01'*n)

if __name__=='__main__':unittest.main()

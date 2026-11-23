import sys
from pathlib import Path
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'firmware/k230'))
from runtime_identity import reserve_random_id

class IdentityTests(unittest.TestCase):
    def test_repeated_entropy_is_rejected_against_history(self):
        with tempfile.TemporaryDirectory() as d:
            p=str(Path(d)/'ids')
            a=reserve_random_id(p,lambda n:b'\x01'*n)
            self.assertEqual(a,'01'*8)
            with self.assertRaises(ValueError):reserve_random_id(p,lambda n:b'\x01'*n)
            self.assertEqual(reserve_random_id(p,lambda n:b'\x02'*n),'02'*8)
    def test_corrupt_history_is_not_reset(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'ids';p.write_text('broken\n')
            with self.assertRaises(ValueError):reserve_random_id(str(p),lambda n:b'\x01'*n)

if __name__=='__main__':unittest.main()

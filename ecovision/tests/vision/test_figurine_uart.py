import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'firmware/k230'))
from protocol import frame
from figurine_uart_link import UartPublisher

class Port:
    def __init__(self): self.rx = b''; self.sent=[]
    def readinto(self, buf):
        n=min(len(buf),len(self.rx));buf[:n]=self.rx[:n];self.rx=self.rx[n:];return n
    def write(self, data): self.sent.append(bytes(data));return len(data)

class PublisherTests(unittest.TestCase):
    def test_handshake_and_one_second_latest_sample(self):
        p=Port(); u=UartPublisher(p,'000000000000000b')
        u.service(0,12,True);self.assertEqual(p.sent,[])
        p.rx=frame('EVQ,000000000000000a')
        u.service(10,12,True)
        self.assertTrue(p.sent[0].startswith(b'$EVH,'))
        self.assertTrue(p.sent[1].endswith(frame('EV1,000000000000000a,000000000000000b,0,10,12,1')))
        u.service(500,3,True);self.assertEqual(len(p.sent),2)
        u.service(1010,0,True);self.assertIn(b',0,1*',p.sent[-1])
        u.service(2010,22,True);self.assertIn(b',0,0*',p.sent[-1])
    def test_fault_recovery_and_no_sequence_reset(self):
        p=Port();u=UartPublisher(p,'000000000000000b')
        p.rx=frame('EVQ,000000000000000a');u.service(0,None,False)
        self.assertIn(b',0,0*',p.sent[-1])
        p.rx=frame('EVQ,000000000000000a');u.service(1000,15,True)
        self.assertIn(b',1,1000,15,1*',p.sent[-1])
    def test_short_writes_and_stall(self):
        p=Port();u=UartPublisher(p,'000000000000000b')
        p.rx=frame('EVQ,000000000000000a');p.write=lambda data:0
        with self.assertRaises(OSError):u.service(0,0,True)
    def test_fault_between_heartbeats_is_reported(self):
        p=Port();u=UartPublisher(p,'000000000000000b')
        p.rx=frame('EVQ,000000000000000a');u.service(0,12,True)
        u.service(100,None,False);u.service(200,12,True)
        u.service(1000,12,True)
        self.assertIn(b',0,0*',p.sent[-1])
        u.service(2000,12,True)
        self.assertIn(b',12,1*',p.sent[-1])

if __name__=='__main__':unittest.main()

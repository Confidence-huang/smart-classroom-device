import sys
import unittest
import json
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'firmware' / 'k230'))
import protocol

LINK = '0123456789abcdef'
BOOT = 'fedcba9876543210'


class ProtocolTests(unittest.TestCase):
    def test_frozen_public_vectors_match_byte_for_byte(self):
        path = Path(__file__).resolve().parents[2] / 'contracts/examples/uart.json'
        public = json.loads(path.read_text(encoding='utf-8'))
        vectors = {entry['name']: entry['wire'].encode('ascii') for entry in public['frames']}
        link = protocol.VisionLink('000000000000000b')
        self.assertEqual(protocol.frame('EVQ,000000000000000a'), vectors['query'])
        self.assertEqual(link.receive(vectors['query'], 0), [vectors['hello']])
        link.seq = 1
        self.assertEqual(link.sample(2, True, 1000), vectors['normal'])
        self.assertEqual(link.sample(None, False, 2000), vectors['invalid_detection'])

    def test_crc_standard_vector(self):
        self.assertEqual(protocol.crc16(b'123456789'), 0x29b1)

    def test_no_data_before_query_then_exact_handshake(self):
        link = protocol.VisionLink(BOOT)
        self.assertIsNone(link.sample(None, False, 123))
        query = protocol.frame('EVQ,' + LINK)
        self.assertEqual(link.receive(query[:10], 0), [])
        self.assertEqual(link.receive(query[10:], 1), [protocol.frame('EVH,' + LINK + ',' + BOOT)])
        self.assertEqual(link.sample(2, True, 123), protocol.frame('EV1,' + LINK + ',' + BOOT + ',0,123,2,1'))

    def test_duplicate_query_and_new_link_do_not_reset_seq(self):
        link = protocol.VisionLink(BOOT)
        query = protocol.frame('EVQ,' + LINK)
        link.receive(query, 0)
        link.sample(1, True, 0)
        link.receive(query, 1)
        self.assertIn(b',1,1000,0,0*', link.sample(None, False, 1000))
        new = '1111111111111111'
        link.receive(protocol.frame('EVQ,' + new), 2)
        self.assertIn((new + ',' + BOOT + ',2,2000,0,1').encode(), link.sample(0, True, 2000))

    def test_invalid_count_uses_zero_placeholder_with_valid_zero(self):
        link = protocol.VisionLink(BOOT)
        link.receive(protocol.frame('EVQ,' + LINK), 0)
        self.assertIn(b',0,0,0,0*', link.sample(None, False, 0))

    def test_crc_errors_malformed_non_ascii_and_oversize_resynchronize(self):
        link = protocol.VisionLink(BOOT)
        query = protocol.frame('EVQ,' + LINK)
        for bad in (query.replace(b'EVQ', b'EVZ'), b'$' + b'x' * 130,
                    b'$\xff*0000\r\n', protocol.frame('EVQ,' + LINK.upper()),
                    protocol.frame('EVQ,' + LINK + ',extra')):
            self.assertEqual(link.receive(bad, 0), [])
        self.assertEqual(len(link.receive(query + query, 1)), 2)

    def test_incomplete_frame_expires_at_200ms(self):
        link = protocol.VisionLink(BOOT)
        query = protocol.frame('EVQ,' + LINK)
        self.assertEqual(link.receive(query[:10], 0), [])
        self.assertEqual(link.receive(query[10:], 200), [])
        self.assertEqual(len(link.receive(query, 201)), 1)

    def test_wrap_and_boot_id_validation(self):
        with self.assertRaises(ValueError):
            protocol.VisionLink('guess')
        link = protocol.VisionLink(BOOT)
        link.receive(protocol.frame('EVQ,' + LINK), 0)
        link.seq = 0xffffffff
        self.assertIn(b',4294967295,0,1,1*', link.sample(1, True, 1 << 32))
        self.assertIn(b',0,1,1,1*', link.sample(1, True, (1 << 32) + 1))


if __name__ == '__main__':
    unittest.main()

"""Build per-device data from existing private settings; never embed it in C/HEX."""
from pathlib import Path
import argparse
import binascii
import json
import re
import ssl
import struct


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--legacy-compat', action='store_true')
    args = parser.parse_args()
    private = Path(__file__).resolve().parents[1] / '.private'
    settings = json.loads((private / 'recovered-settings.json').read_text(encoding='utf-8'))
    ca = (private / 'recovered-ca-0.pem').read_bytes()
    ssl.PEM_cert_to_DER_cert(ca.decode('ascii'))
    if not 0 < len(ca) <= 4096 or settings['MQTT_PORT'] != 8883:
        raise ValueError('Invalid CA size or TLS port')

    def field(value, size):
        data = str(value).encode('ascii')
        if not 0 < len(data) < size or any(x < 32 or x > 126 or x in (34, 92) for x in data):
            raise ValueError('Unsupported configuration field; value omitted')
        return data + bytes(size - len(data))

    if not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,31}', settings['DEVICE_ID']):
        raise ValueError('Invalid device ID')
    payload = (field(settings['MQTT_HOST'], 128) + field(settings['MQTT_USER'], 128)
               + field(settings['MQTT_PASSWORD'], 128) + field(settings['DEVICE_ID'], 33)
               + struct.pack('<HH', settings['MQTT_PORT'], len(ca)) + ca)
    if args.legacy_compat:
        topic = settings['MQTT_TOPIC']
        if not topic.startswith('classroom/') or '+' in topic or '#' in topic:
            raise ValueError('Unexpected compatibility topic')
        if not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,31}', settings['ROOM_ID']):
            raise ValueError('Invalid room ID')
        payload += field(topic, 128) + field(settings['ROOM_ID'], 33)
    record = b'EVNET01\0' + struct.pack('<II', len(payload), binascii.crc32(payload)) + payload
    (private / 'network-config.bin').write_bytes(record)
    print(json.dumps({'private_record_bytes': len(record), 'legacy_compatibility': args.legacy_compat,
                      'values_logged': False, 'flash_written': False}))


if __name__ == '__main__':
    main()

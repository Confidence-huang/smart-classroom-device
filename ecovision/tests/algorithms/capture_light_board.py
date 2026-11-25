"""Read-only ST-LINK VCP capture for the EcoVision daylight baseline."""

import argparse
import json
from pathlib import Path
import re
import time

import serial
from serial.tools import list_ports


LIGHT = re.compile(r'^\[LIGHT\] source=BH1750 addr=0x[0-9a-fA-F]{2} '
                   r'valid=([01]) raw=(\d+) lux=(\d+\.\d{2}) status=(-?\d+)$')
LED = re.compile(r'^\[LED\] OK level=([0-4])(?: |$)')
VISION = re.compile(r'^\[VISION\] RX seq=(\d+) count=(\d+) valid=([01])(?: |$)')
SHADOW = re.compile(r'^\[LUX_SHADOW\] ms=(\d+) valid=([01]) stable=([01]) lux_x100=(-?\d+) '
                    r'occ=([0-2]) actual=([01]) virtual=([01]) advice=([0-2])$')


def parse(line):
    if match := LIGHT.match(line):
        valid, raw, lux, status = match.groups()
        return dict(kind='bh1750', valid=valid == '1', raw=int(raw),
                    lux=float(lux) if valid == '1' else None, status=int(status))
    if match := LED.match(line):
        return dict(kind='led_change', level=int(match[1]))
    if match := VISION.match(line):
        return dict(kind='vision', seq=int(match[1]), count=int(match[2]), valid=match[3] == '1')
    if match := SHADOW.match(line):
        ms, valid, stable, lux, occupancy, actual, virtual, advice = map(int, match.groups())
        return dict(kind='shadow', sample_tick=ms, valid=bool(valid), stable=bool(stable),
                    lux_x100=lux if valid else None, occupancy=occupancy, actual=actual,
                    virtual=virtual, advice=advice)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=45)
    parser.add_argument('--port')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error('--seconds must be between 1 and 300')
    ports = [p for p in list_ports.comports() if p.vid == 0x0483 and
             ('STLINK' in (p.description or '').upper().replace('-', '') or
              'STLINK' in (p.product or '').upper().replace('-', ''))]
    if args.port:
        ports = [p for p in ports if p.device.casefold() == args.port.casefold()]
    if len(ports) != 1:
        parser.error(f'expected one ST-LINK virtual serial port, found {len(ports)}')

    args.output.parent.mkdir(parents=True, exist_ok=True)
    counts = {'lux_valid': 0, 'lux_invalid': 0, 'led_change': 0, 'vision': 0, 'shadow': 0}
    # Open only the identified VCP. Leave modem control lines deasserted.
    link = serial.Serial(baudrate=115200, timeout=0.2)
    link.dtr = link.rts = False
    link.port = ports[0].device
    with link as port, args.output.open('x', encoding='utf-8') as out:
        start = time.monotonic()
        pending = bytearray()
        while time.monotonic() - start < args.seconds:
            pending.extend(port.read(port.in_waiting or 1))
            while b'\n' in pending:
                line, _, remainder = pending.partition(b'\n')
                pending = remainder
                event = parse(line.decode('ascii', errors='replace').strip())
                if event is None:
                    continue
                key = event['kind']
                if key == 'bh1750':
                    key = 'lux_valid' if event['valid'] else 'lux_invalid'
                counts[key] += 1
                event['receipt_ms'] = round((time.monotonic() - start) * 1000)
                out.write(json.dumps(event) + '\n')
            if len(pending) > 4096:
                pending.clear()
    print(json.dumps(counts))
    return 0 if counts['lux_valid'] else 2


if __name__ == '__main__':
    raise SystemExit(main())

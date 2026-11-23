"""CanMV entry point. Upload module to /sdcard/ecovision, then run this file.

EV_DIAG stdout is local debugging, never the STM32 protocol UART.
"""
import gc
import os
import sys
import time
try:
    import ujson as json
except ImportError:
    import json
sys.path.insert(0, '/sdcard/ecovision')
for path in ('/sdcard/examples', '/sdcard/app'):
    if path not in sys.path:
        sys.path.append(path)
from canmv_backend import CanMVBackend
from vision import VisionSession
from protocol import VisionLink
from boot_identity import next_boot_id


def open_uart(config):
    if config.get('enabled') is not True:
        return None
    if config.get('pinmap_confirmed') is not True or config.get('electrical_confirmed') is not True:
        raise ValueError('confirm actual UART pinmap and electrical levels')
    number, tx, rx = config.get('id'), config.get('tx_pin'), config.get('rx_pin')
    if type(number) is not int or number not in (1, 2, 4) or type(tx) is not int or type(rx) is not int or tx == rx:
        raise ValueError('confirm UART id and distinct pins')
    from machine import FPIOA, UART
    fpioa = FPIOA()
    fpioa.set_function(tx, getattr(FPIOA, 'UART%d_TXD' % number))
    fpioa.set_function(rx, getattr(FPIOA, 'UART%d_RXD' % number))
    return UART(getattr(UART, 'UART%d' % number), baudrate=115200,
                bits=UART.EIGHTBITS, parity=UART.PARITY_NONE,
                stop=UART.STOPBITS_ONE, timeout=0)


def write_frame(uart, data):
    # Handle short writes; never treat a partial write as a full protocol frame.
    offset = 0
    while offset < len(data):
        sent = uart.write(data[offset:])
        if sent is None or sent <= 0:
            raise OSError('UART short write stalled')
        offset += sent


def run(config_path='/sdcard/ecovision/config.json'):
    try:
        with open(config_path) as stream:
            config = json.load(stream)
    except (OSError, ValueError) as exc:
        config = {}
        print('EV_DIAG config_unavailable', str(exc))
    session = VisionSession(lambda: CanMVBackend(config))
    uart = None
    clock_last = time.ticks_ms()
    uptime = 0
    def now_ms():
        nonlocal clock_last, uptime
        current = time.ticks_ms()
        uptime += time.ticks_diff(current, clock_last)
        clock_last = current
        return uptime
    try:
        uart = open_uart(config.get('uart', {}))
        link = VisionLink(next_boot_id('/sdcard/ecovision/.vision_boot_id')) if uart else None
        rx = bytearray(512)
        while True:
            if hasattr(os, 'exitpoint'):
                os.exitpoint()
            if uart:
                received = uart.readinto(rx)
                for reply in link.receive(bytes(rx[:received]) if received else b'', now_ms()):
                    write_frame(uart, reply)
            state = session.tick(now_ms())
            if state is not None:
                print('EV_DIAG', json.dumps(state))
                if uart:
                    packet = link.sample(state['count'], state['valid'], now_ms())
                    if packet is not None:
                        write_frame(uart, packet)
            gc.collect()
            time.sleep_ms(10)
    except KeyboardInterrupt:
        print('EV_DIAG stopped_by_operator')
    finally:
        try:
            session.close()
        finally:
            if uart:
                uart.deinit()


if __name__ == '__main__':
    run()

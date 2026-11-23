"""Reserve a new ID durably before it can appear on the UART.

Keep the counter file across normal deployments. Do not clone it to another
active board. Power-cut durability still requires a real SD-card test.
"""
import os
from protocol import valid_id


def next_boot_id(path, sync=None):
    if sync is None:
        sync = getattr(os, 'sync', None)
    if sync is None:
        raise ValueError('filesystem sync unavailable; cannot reserve boot ID')
    try:
        with open(path) as stream:
            value = stream.read().strip()
    except OSError as exc:
        if exc.args[0] != 2:  # Only a missing counter may be initialized.
            raise
        try:
            import ubinascii as binascii
        except ImportError:
            import binascii
        value = binascii.hexlify(os.urandom(8)).decode()
    if not valid_id(value):
        raise ValueError('corrupt boot ID counter; do not silently reset')
    number = int(value, 16)
    if number == 0xffffffffffffffff:
        raise ValueError('boot ID exhausted')
    temporary = path + '.next'
    with open(temporary, 'w') as stream:
        stream.write('%016x' % (number + 1))
        stream.flush()
    sync()
    getattr(os, 'replace', os.rename)(temporary, path)
    sync()
    return value

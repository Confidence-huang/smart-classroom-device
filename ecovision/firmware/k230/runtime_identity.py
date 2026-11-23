"""Random boot IDs for CanMV builds without os.sync.

The persisted ledger rejects previously observed IDs. Since this firmware
cannot explicitly sync the filesystem, crash durability of the ledger is
not asserted; fresh OS entropy is also required on every launch. Power-cut
validation remains separate from normal soft-restart verification.
"""
import os
try:
    import ubinascii as binascii
except ImportError:
    import binascii
from protocol import valid_id

def reserve_random_id(path, random_bytes=None):
    if random_bytes is None:
        random_bytes=os.urandom
    try:
        with open(path) as stream:
            history=stream.read().splitlines()
    except OSError as exc:
        if exc.args[0]!=2:
            raise
        history=[]
    if any(not valid_id(value) for value in history):
        raise ValueError('corrupt boot ID ledger')
    for _ in range(16):
        value=binascii.hexlify(random_bytes(8)).decode()
        if not valid_id(value):
            raise ValueError('OS entropy did not return 8 bytes')
        if value in history:
            continue
        with open(path,'a') as stream:
            stream.write(value+'\n')
            stream.flush()
        return value
    raise ValueError('repeated boot entropy; UART disabled')

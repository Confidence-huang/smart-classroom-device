"""Reserve a fresh boot ID in the CanMV vision UART ledger.

Prefer OS randomness. When the cold-boot sequence repeats, continue from the
last recorded ID. Validate and keep the ledger even when randomness repeats.
CanMV v1.8-66 has no os.sync, so sudden power-loss durability is not guaranteed.
"""
import os

try:
    import ubinascii as binascii
except ImportError:
    import binascii

from protocol import valid_id


def reserve_random_id(path, random_bytes=None):
    if random_bytes is None:
        random_bytes = os.urandom

    # Existing cards already contain IDs used in EVH and EV1 frames.
    try:
        with open(path) as stream:
            history = stream.read().splitlines()
    except OSError as exc:
        if exc.args[0] != 2:
            raise
        history = []

    if any(not valid_id(value) for value in history):
        raise ValueError('corrupt boot ID ledger')
    used_ids = set(history)

    for _ in range(16):
        value = binascii.hexlify(random_bytes(8)).decode()
        if not valid_id(value):
            raise ValueError('OS entropy did not return 8 bytes')
        if value not in used_ids:
            break
    else:
        # Identical startup entropy must not stop UART or reuse any past ID.
        number = int(history[-1], 16)
        while value in used_ids:
            number = (number + 1) & 0xffffffffffffffff
            value = '%016x' % number

    # Make the reservation before the publisher can send this ID.
    with open(path, 'a') as stream:
        record = value + '\n'
        if stream.write(record) != len(record):
            raise OSError('incomplete boot ID ledger write')
        stream.flush()
    return value

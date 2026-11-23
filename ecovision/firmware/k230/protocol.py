"""EcoVision INTERFACES.md 1.0.0 UART. Pure MicroPython/CPython code."""


def crc16(data):
    crc = 0xffff
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xffff if crc & 0x8000 else (crc << 1) & 0xffff
    return crc


def frame(payload):
    body = payload.encode('ascii')
    result = b'$' + body + ('*%04X\r\n' % crc16(body)).encode('ascii')
    if len(result) > 128:
        raise ValueError('frame_too_long')
    return result


def valid_id(value):
    return isinstance(value, str) and len(value) == 16 and all(c in '0123456789abcdef' for c in value)


class VisionLink:
    def __init__(self, boot_id):
        if not valid_id(boot_id):
            raise ValueError('invalid vision_boot_id')
        self.boot_id = boot_id
        self.link_id = None
        self.seq = 0
        self.buffer = bytearray()
        self.start_ms = None
        self.errors = 0

    def receive(self, data, now_ms):
        replies = []
        if self.start_ms is not None and now_ms - self.start_ms >= 200:
            self.buffer = bytearray()
            self.start_ms = None
            self.errors += 1
        for byte in data:
            if byte == 36:  # '$' always resynchronizes.
                self.buffer = bytearray(b'$')
                self.start_ms = now_ms
                continue
            if self.start_ms is None:
                continue
            self.buffer.append(byte)
            if byte > 127 or len(self.buffer) > 128:
                self.buffer = bytearray()
                self.start_ms = None
                self.errors += 1
            elif byte == 10:
                raw = bytes(self.buffer)
                self.buffer = bytearray()
                self.start_ms = None
                try:
                    if not raw.endswith(b'\r\n') or raw[-7:-6] != b'*':
                        raise ValueError('ending')
                    body, checksum = raw[1:-7], raw[-6:-2]
                    if checksum != ('%04X' % crc16(body)).encode('ascii'):
                        raise ValueError('crc')
                    fields = body.decode('ascii').split(',')
                    if len(fields) != 2 or fields[0] != 'EVQ' or not valid_id(fields[1]):
                        raise ValueError('query')
                    self.link_id = fields[1]
                    replies.append(frame('EVH,' + self.link_id + ',' + self.boot_id))
                except (ValueError, UnicodeError):
                    self.errors += 1
        return replies

    def sample(self, count, valid, uptime_ms):
        if self.link_id is None:
            return None
        if valid and (type(count) is not int or not 0 <= count <= 20):
            raise ValueError('count_out_of_range')
        result = frame('EV1,%s,%s,%d,%d,%d,%d' % (
            self.link_id, self.boot_id, self.seq, uptime_ms & 0xffffffff,
            count if valid else 0, 1 if valid else 0))
        self.seq = (self.seq + 1) & 0xffffffff
        return result

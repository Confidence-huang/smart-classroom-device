"""EV1 publisher; monotonic now_ms must be extended across ticks_ms wrap."""
from protocol import VisionLink

class UartPublisher:
    def __init__(self, uart, boot_id):
        self.uart = uart
        self.link = VisionLink(boot_id)
        self.rx = bytearray(512)
        self.last_sent = None
        self.sent = 0
        self.fault_pending = False

    def write(self, data):
        offset = 0
        while offset < len(data):
            n = self.uart.write(data[offset:])
            if n is None or n <= 0:
                raise OSError('UART write stalled')
            offset += n

    def service(self, now_ms, count, valid):
        valid = bool(valid and type(count) is int and 0 <= count <= 20)
        if not valid:
            self.fault_pending = True
        n = self.uart.readinto(self.rx)
        for reply in self.link.receive(bytes(self.rx[:n]) if n else b'', now_ms):
            self.write(reply)
            print('UART EVH', self.link.link_id)
        if self.last_sent is not None and now_ms-self.last_sent < 1000:
            return None
        valid = valid and not self.fault_pending
        packet = self.link.sample(count if valid else 0, valid, now_ms)
        if packet is not None:
            self.write(packet)
            self.last_sent = now_ms
            self.sent += 1
            self.fault_pending = False
            print('UART TX', packet.decode().strip())
        return packet

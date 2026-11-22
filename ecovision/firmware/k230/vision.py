"""Portable counting and diagnostic heartbeat. No control policy or wire protocol."""
import math


def count_people(detections):
    # Official PersonDetectionApp returns post-NMS [class, score, x1,y1,x2,y2].
    # None means no usable inference result; only [] means a valid empty scene.
    if not isinstance(detections, (list, tuple)):
        raise ValueError('missing_detection_result')
    for row in detections:
        if len(row) != 6 or any(not math.isfinite(float(v)) for v in row):
            raise ValueError('malformed_detection')
        if row[0] != 0 or not 0 <= row[1] <= 1 or row[4] <= row[2] or row[5] <= row[3]:
            raise ValueError('malformed_detection')
    if len(detections) > 5:
        raise ValueError('scene_capacity_exceeded')
    return len(detections)


class VisionSession:
    def __init__(self, factory):
        self.factory = factory
        self.backend = None
        self.started = False
        self.init_error = None
        self.last_emit_ms = None
        self.seq = 0

    def tick(self, uptime_ms):
        count, valid, reason = None, False, None
        if not self.started:
            self.started = True
            try:
                self.backend = self.factory()
            except Exception as exc:
                self.init_error = 'init_failed: ' + str(exc)
        if self.backend is None:
            reason = self.init_error
        else:
            try:
                count = count_people(self.backend.detect())
                valid = True
            except Exception as exc:
                reason = str(exc) or type(exc).__name__
        if self.last_emit_ms is not None and uptime_ms - self.last_emit_ms < 1000:
            return None
        self.last_emit_ms = uptime_ms
        result = dict(seq=self.seq, uptime_ms=uptime_ms, count=count,
                      valid=valid, reason=reason)
        self.seq = (self.seq + 1) & 0xffffffff
        return result

    def close(self):
        if self.backend is not None:
            self.backend.close()
            self.backend = None

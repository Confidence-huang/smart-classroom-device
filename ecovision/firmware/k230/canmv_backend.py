"""Adapter for the confirmed official CanMV PersonDetectionApp API only.

No camera pins, UART pins, network imports, or model conversion here.
"""
import sys


def validate_config(config):
    for key in ('board', 'camera', 'firmware', 'example_source', 'example_version',
                'model_source', 'model_version', 'official_test_record'):
        if not config.get(key):
            raise ValueError('confirm ' + key)
    if config.get('official_example_passed') is not True:
        raise ValueError('run original official example first')
    for key in ('example_sha256', 'model_sha256'):
        value = config.get(key, '') or ''
        if len(value) != 64 or any(c not in '0123456789abcdef' for c in value):
            raise ValueError('confirm ' + key)
    for key in ('example_dir', 'model_path', 'display_mode'):
        if not config.get(key):
            raise ValueError('confirm ' + key)
    for key in ('rgb888p_size', 'model_input_size'):
        size = config.get(key)
        if not isinstance(size, list) or len(size) != 2 or any(type(v) is not int or v <= 0 for v in size):
            raise ValueError('confirm ' + key)
    if config['rgb888p_size'][0] % 16:
        raise ValueError('rgb888p_size width must be aligned to 16')
    if config.get('labels') != ['person'] or len(config.get('anchors', [])) != 18:
        raise ValueError('requires confirmed single-person-class anchor model')
    for key in ('confidence_threshold', 'nms_threshold'):
        if not 0 < config.get(key, 0) <= 1:
            raise ValueError('confirm ' + key)


def sha256_file(path):
    try:
        import uhashlib as hashlib
        import ubinascii as binascii
    except ImportError:
        import hashlib
        import binascii
    digest = hashlib.sha256()
    with open(path, 'rb') as stream:
        while True:
            chunk = stream.read(16384)
            if not chunk:
                break
            digest.update(chunk)
    return binascii.hexlify(digest.digest()).decode()


def validate_frame(frame, size, array_module=None):
    if frame is None or tuple(frame.shape) != (3, size[1], size[0]):
        raise ValueError('camera_frame_missing_or_wrong_shape')
    # Basic black/saturated-frame guard, not a general occlusion detector.
    if array_module is None:
        from ulab import numpy as array_module
    if int(array_module.max(frame)) <= 1 or int(array_module.min(frame)) >= 254:
        raise ValueError('camera_frame_black_or_saturated')


class CanMVBackend:
    def __init__(self, config):
        self.pl = None
        self.detector = None
        self.config = config
        validate_config(config)  # Before importing or touching any hardware.
        example_path = config['example_dir'].rstrip('/') + '/person_detection.py'
        for path, expected in ((example_path, config['example_sha256']),
                               (config['model_path'], config['model_sha256'])):
            if sha256_file(path) != expected:
                raise ValueError('asset_sha256_mismatch: ' + path)
        sys.path.insert(0, config['example_dir'])
        from libs.PipeLine import PipeLine
        from person_detection import PersonDetectionApp
        try:
            self.pl = PipeLine(rgb888p_size=config['rgb888p_size'],
                               display_mode=config['display_mode'],
                               display_size=config.get('display_size'))
            self.pl.create(**config.get('camera_create', {}))
            self.detector = PersonDetectionApp(
                config['model_path'], model_input_size=config['model_input_size'],
                labels=config['labels'], anchors=config['anchors'],
                confidence_threshold=config['confidence_threshold'],
                nms_threshold=config['nms_threshold'], nms_option=False,
                strides=[8, 16, 32], rgb888p_size=config['rgb888p_size'],
                display_size=self.pl.get_display_size(), debug_mode=0)
            self.detector.config_preprocess()
        except BaseException:
            self.close()
            raise

    def detect(self):
        frame = self.pl.get_frame()
        validate_frame(frame, self.config['rgb888p_size'])
        result = self.detector.run(frame)
        # Use all post-NMS boxes for count and display. The reference demo's
        # draw_result has extra edge/size filters that could hide counted boxes.
        self.pl.osd_img.clear()
        if result is not None:
            from vision import count_people
            count_people(result)  # Validate before drawing any coordinates.
            dw, dh = self.pl.get_display_size()
            sw, sh = self.config['rgb888p_size']
            for row in result:
                x, y = int(row[2] * dw / sw), int(row[3] * dh / sh)
                w, h = max(1, int((row[4] - row[2]) * dw / sw)), max(1, int((row[5] - row[3]) * dh / sh))
                self.pl.osd_img.draw_rectangle(x, y, w, h, color=(255, 0, 255, 0), thickness=2)
        self.pl.show_image()
        return result

    def close(self):
        try:
            if self.detector is not None:
                detector, self.detector = self.detector, None
                detector.deinit()
        finally:
            if self.pl is not None:
                pl, self.pl = self.pl, None
                pl.destroy()

"""Run on CanMV without enabling a camera, UART, or network."""
import os
import sys
sys.path.insert(0, '/sdcard/ecovision')
from canmv_backend import sha256_file


def run():
    print('EV_DIAG preflight; not hardware acceptance')
    print('uname:', os.uname() if hasattr(os, 'uname') else 'unavailable')
    print('python:', sys.version)
    try:
        import nncase_runtime as nn
        print('nncase:', getattr(nn, '__version__', 'unavailable'))
    except ImportError as exc:
        print('nncase unavailable:', str(exc))
    for path in ('/sdcard/revision.txt', '/sdcard/ecovision/official/firmware_revision.txt'):
        try:
            with open(path) as stream:
                print(path, stream.read())
        except OSError:
            print('missing:', path)
    for path in ('/sdcard/examples/05-AI-Demo/person_detection.py',
                 '/sdcard/examples/kmodel/person_detect_yolov5n.kmodel',
                 '/sdcard/examples/kmodel/yolov8n_224.kmodel',
                 '/sdcard/examples/kmodel/yolov8n_320.kmodel',
                 '/sdcard/ecovision/official/person_detection.py',
                 '/sdcard/ecovision/official/person_detect_yolov5n.kmodel'):
        try:
            print('asset:', path, 'size:', os.stat(path)[6], 'sha256:', sha256_file(path))
        except OSError:
            print('missing:', path)


if __name__ == '__main__':
    run()

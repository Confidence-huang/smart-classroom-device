"""30 live frames, local display only. Run after the original camera example."""
import gc
import sys
sys.path.insert(0, '/sdcard/ecovision')
for path in ('/sdcard/examples', '/sdcard/app'):
    if path not in sys.path:
        sys.path.append(path)
from canmv_backend import validate_frame


def run():
    from libs.PipeLine import PipeLine
    # sensor_id=2 comes from the user's successful GC2093 script, not pin guesses.
    pl = PipeLine(rgb888p_size=[640, 480], display_mode='auto', display_size=None)
    try:
        pl.create(sensor_id=2)
        for index in range(30):
            image = pl.get_frame()
            validate_frame(image, [640, 480])
            pl.osd_img.clear()
            pl.show_image()
            print('EV_DIAG camera_frame', index, 'shape', image.shape)
            gc.collect()
        print('EV_DIAG camera_smoke_completed; inference not tested')
    finally:
        pl.destroy()


if __name__ == '__main__':
    run()

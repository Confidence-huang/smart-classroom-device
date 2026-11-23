import gc
import os
import sys
import time

for module_path in ("/sdcard/examples", "/sdcard/app", "/sdcard"):
    if module_path not in sys.path:
        sys.path.insert(0, module_path)

import aidemo
import image
import nncase_runtime as nn
import ulab.numpy as np
from libs.AI2D import Ai2d
from libs.AIBase import AIBase
from libs.PipeLine import PipeLine
from libs.Utils import ScopedTiming, get_colors, letterbox_pad_param
from media.media import *


def align_up(value, alignment=16):
    return (value + alignment - 1) // alignment * alignment


class ObjectDetectionApp(AIBase):
    def __init__(
        self,
        kmodel_path,
        labels,
        model_input_size,
        max_boxes_num,
        confidence_threshold=0.5,
        nms_threshold=0.2,
        rgb888p_size=None,
        display_size=None,
        debug_mode=0,
    ):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
        self.kmodel_path = kmodel_path
        self.labels = labels
        self.model_input_size = model_input_size
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        self.max_boxes_num = max_boxes_num
        self.rgb888p_size = [align_up(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [align_up(display_size[0], 16), display_size[1]]
        self.debug_mode = debug_mode
        self.color_four = get_colors(len(self.labels))
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(
            nn.ai2d_format.NCHW_FMT,
            nn.ai2d_format.NCHW_FMT,
            np.uint8,
            np.uint8,
        )

    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            top, bottom, left, right, self.scale = letterbox_pad_param(
                self.rgb888p_size,
                self.model_input_size,
            )
            self.ai2d.pad(
                [0, 0, 0, 0, top, bottom, left, right],
                0,
                [128, 128, 128],
            )
            self.ai2d.resize(
                nn.interp_method.tf_bilinear,
                nn.interp_mode.half_pixel,
            )
            self.ai2d.build(
                [1, 3, ai2d_input_size[1], ai2d_input_size[0]],
                [1, 3, self.model_input_size[1], self.model_input_size[0]],
            )

    def preprocess(self, input_np):
        with ScopedTiming("preprocess", self.debug_mode > 0):
            resized = self.ai2d.run(input_np).to_numpy()
            return [nn.from_numpy(resized * (1.0 / 255.0))]

    def postprocess(self, results):
        with ScopedTiming("postprocess", self.debug_mode > 0):
            new_result = results[0][0].transpose()
            return aidemo.yolov8_det_postprocess(
                new_result.copy(),
                [self.rgb888p_size[1], self.rgb888p_size[0]],
                [self.model_input_size[1], self.model_input_size[0]],
                [self.display_size[1], self.display_size[0]],
                len(self.labels),
                self.confidence_threshold,
                self.nms_threshold,
                self.max_boxes_num,
            )

    def draw_result(self, pipeline, detections):
        with ScopedTiming("display_draw", self.debug_mode > 0):
            pipeline.osd_img.clear()
            count = len(detections[0]) if detections else 0
            pipeline.osd_img.draw_string_advanced(
                12, 12, 36, "Count: %d" % count, color=(255, 255, 255, 255)
            )
            if not count:
                return

            for index in range(len(detections[0])):
                x, y, width, height = (
                    int(round(value, 0)) for value in detections[0][index]
                )
                class_id = detections[1][index]
                confidence = round(detections[2][index], 2)
                color = self.color_four[class_id]
                pipeline.osd_img.draw_rectangle(
                    x,
                    y,
                    width,
                    height,
                    color=color,
                    thickness=4,
                )
                pipeline.osd_img.draw_string_advanced(
                    x,
                    max(0, y - 28),
                    22,
                    " " + self.labels[class_id] + " " + str(confidence),
                    color=color,
                )




import sys as _python_sys
_python_sys.path.insert(0, '/sdcard/ecovision_uart')
from uart_board import BoardUart

pipeline = None
detector = None
board = None
try:
    board = BoardUart()
    try:
        pipeline = PipeLine(rgb888p_size=[640,480], display_mode="auto", display_size=None)
        pipeline.create(sensor_id=2)
        detector = ObjectDetectionApp(
            "/sdcard/kmodel/figurine_multi.kmodel", labels=["figurine"],
            model_input_size=[320,320], max_boxes_num=30,
            confidence_threshold=0.35, nms_threshold=0.3,
            rgb888p_size=[640,480], display_size=pipeline.get_display_size())
        detector.config_preprocess()
    except Exception as exc:
        print("VISION INIT FAILED", str(exc), "sending invalid heartbeat")
        while True:
            os.exitpoint()
            board.publisher.service(board.now(), None, False)
            time.sleep_ms(10)
    print("Model: figurine_multi | conf 0.35 | NMS 0.3 | EV1 UART2 GPIO5/6")
    frames = 0
    started = time.ticks_ms()
    last_error = None
    while True:
        os.exitpoint()
        try:
            frame = pipeline.get_frame()
            detections = detector.run(frame)
            valid = detections is not None
            count = len(detections[0]) if valid and detections else 0
            if not 0 <= count <= 20:
                valid = False
            if not valid:
                raise ValueError("missing result or count outside protocol range")
            detector.draw_result(pipeline, detections)
            last_error = None
        except Exception as exc:
            if str(exc) == "IDE interrupt":
                raise
            count = None
            valid = False
            if str(exc) != last_error:
                print("VISION INVALID", str(exc))
                last_error = str(exc)
            pipeline.osd_img.clear()
            pipeline.osd_img.draw_string_advanced(12,12,36,"VISION INVALID",color=(255,255,0,0))
        pipeline.show_image()
        board.publisher.service(board.now(), count, valid)
        gc.collect()
        frames += 1
        if frames == 30:
            elapsed = time.ticks_diff(time.ticks_ms(), started)
            print("figurine count:", count, "valid:", valid, "fps:",round(1000*frames/max(1,elapsed),1))
            frames = 0
            started = time.ticks_ms()
except KeyboardInterrupt:
    print("Stopped by user")
except Exception as exc:
    if str(exc) != "IDE interrupt":
        raise
finally:
    try:
        if detector is not None:
            detector.deinit()
    finally:
        try:
            if pipeline is not None:
                pipeline.destroy()
        finally:
            if board is not None:
                board.close()

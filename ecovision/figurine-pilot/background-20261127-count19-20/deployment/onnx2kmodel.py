"""把 ONNX 模型量化编译成 K230 可用的 kmodel（nncase 2.11.0）。

用法:
    python pc_tools/onnx2kmodel.py <model.onnx> <calib_dir> <out.kmodel> [imgsz]

说明:
- 预处理与 K230 板端 AI2D 保持一致：RGB 通道、letterbox 等比缩放 + 128 灰边填充、
  float32 / 255 归一化到 0~1。
- 版本必须与板子固件内置的 nncase 版本一致（本机为 2.11.0）。
- 校准图建议用真实部署场景帧 100~200 张。
"""

import importlib.metadata as metadata
import os
import sys
import sysconfig

import numpy as np
from PIL import Image

# Windows 下 nncase 需要 NNCASE_PLUGIN_PATH 才能找到 KPU 插件
_plugin_dir = os.path.join(sysconfig.get_paths()["purelib"], "nncase")
if os.path.isdir(_plugin_dir):
    os.environ.setdefault("NNCASE_PLUGIN_PATH", _plugin_dir)

import nncase  # noqa: E402


def letterbox(img: Image.Image, size: int, color=(128, 128, 128)) -> Image.Image:
    """等比缩放并居中填充，与板端 AI2D letterbox 保持一致。"""
    w, h = img.size
    scale = min(size / w, size / h)
    new_w, new_h = int(w * scale), int(h * scale)
    canvas = Image.new("RGB", (size, size), color)
    canvas.paste(img.resize((new_w, new_h), Image.BILINEAR), ((size - new_w) // 2, (size - new_h) // 2))
    return canvas


def load_calibration(calib_dir: str, imgsz: int, max_samples: int = 200):
    samples = []
    for name in sorted(os.listdir(calib_dir)):
        if not name.lower().endswith((".jpg", ".jpeg", ".png", ".bmp")):
            continue
        img = Image.open(os.path.join(calib_dir, name)).convert("RGB")
        if img.size != (imgsz, imgsz):
            img = letterbox(img, imgsz)
        arr = np.asarray(img, dtype=np.float32) / 255.0
        samples.append(arr.transpose(2, 0, 1)[np.newaxis, ...])  # [1,3,H,W]
        if len(samples) >= max_samples:
            break
    return samples


def main() -> int:
    if len(sys.argv) < 4:
        print(__doc__)
        return 1

    onnx_path, calib_dir, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    imgsz = int(sys.argv[4]) if len(sys.argv) > 4 else 320
    shape = [1, 3, imgsz, imgsz]

    samples = load_calibration(calib_dir, imgsz)
    if not samples:
        print(f"校准集为空: {calib_dir}")
        return 1
    print(f"nncase {metadata.version('nncase')} / kpu {metadata.version('nncase-kpu')}")
    print(f"校准样本数: {len(samples)}，输入尺寸: {imgsz}")

    compile_options = nncase.CompileOptions()
    compile_options.target = "k230"
    compile_options.preprocess = False
    compile_options.input_type = "float32"
    compile_options.input_shape = shape
    compile_options.input_layout = "NCHW"
    compile_options.output_layout = "NCHW"

    compiler = nncase.Compiler(compile_options)
    with open(onnx_path, "rb") as f:
        compiler.import_onnx(f.read(), nncase.ImportOptions())

    ptq = nncase.PTQTensorOptions()
    ptq.samples_count = len(samples)
    ptq.quant_type = "uint8"
    ptq.w_quant_type = "uint8"
    ptq.calibrate_method = "Kld"  # nncase 2.11 默认 Kld，可换 NoClip 对比
    ptq.set_tensor_data([samples])
    compiler.use_ptq(ptq)

    print("编译中...")
    compiler.compile()
    kmodel = compiler.gencode_tobytes()
    with open(out_path, "wb") as f:
        f.write(kmodel)
    print(f"kmodel: {out_path}  {len(kmodel) / 1024 / 1024:.2f} MB")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

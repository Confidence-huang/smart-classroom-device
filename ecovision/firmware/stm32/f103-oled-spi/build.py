"""Build F103 SPI OLED/IR receiver; never connects to or flashes hardware."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parent
arguments = argparse.ArgumentParser(description=__doc__)
arguments.add_argument('--toolchain', type=Path, default=Path('D:/k51/ARM/ARMCLANG/bin'),
                       help='directory containing the Arm Compiler 6 executables')
arguments.add_argument('--output', type=Path, default=root / 'build',
                       help='directory for the HEX, map, and build log')
options = arguments.parse_args()
tools = options.toolchain.resolve()
repo = Path.home() / 'STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7'
cmsis = repo / 'Drivers/CMSIS'
device = cmsis / 'Device/ST/STM32F1xx'
shared = root.parent / 'EcoVision_F413ZH/App'
# Compile against the tracked font.h so the build can use a clean checkout.
out = options.output.resolve()
out.mkdir(parents=True, exist_ok=True)
scatter = out / 'f103.sct'
scatter.write_text('LR_IROM1 0x08000000 0x10000 {\n ER_IROM1 0x08000000 0x10000 {\n *.o (RESET, +First)\n *(InRoot$$Sections)\n .ANY (+RO)\n }\n RW_IRAM1 0x20000000 0x5000 { .ANY (+RW +ZI) }\n}\n')

def run(tool, args):
    p = subprocess.run([str(tools / (tool + '.exe')), *map(str, args)], capture_output=True, text=True, errors='replace')
    log.write(p.stdout + p.stderr)
    if p.returncode:
        raise RuntimeError(p.stdout + p.stderr)

images = {}
with (out / 'build.log').open('w', encoding='utf-8') as log:
    startup = out / 'startup.o'
    run('armasm', ['--cpu=Cortex-M3', '-o', startup, device / 'Source/Templates/arm/startup_stm32f103xb.s'])
    for profile in ('ssd1306', 'sh1106'):
        objects = [startup]
        for src in (root / 'main.c', root / 'oled_spi.c', root / 'ir_receiver.c', shared / 'Src/ir_pulse_codec.c', device / 'Source/Templates/system_stm32f1xx.c'):
            obj = out / (profile + '-' + src.stem + '.o')
            run('armclang', ['--target=arm-arm-none-eabi', '-mcpu=cortex-m3', '-mthumb', '-std=c99', '-Oz', '-Wall', '-Wextra', '-Werror',
                             '-ffunction-sections', '-fdata-sections', '-DSTM32F103xB', '-DOLED_SH1106=' + str(int(profile == 'sh1106')),
                             '-I' + str(root), '-I' + str(shared / 'Inc'), '-I' + str(cmsis / 'Include'), '-I' + str(device / 'Include'), '-c', src, '-o', obj])
            objects.append(obj)
        elf = out / ('f103-ir-' + profile + '.axf')
        run('armlink', [*objects, '--strict', '--scatter', scatter, '--entry=Reset_Handler', '--info=summarysizes', '--map', '--symbols', '--list=' + str(out / (profile + '.map')), '-o', elf])
        image = out / ('f103-ir-' + profile + '.hex')
        run('fromelf', ['--i32combined', '--output=' + str(image), elf])
        base = 0
        lowest, highest = 0xffffffff, 0
        for line in image.read_text().splitlines():
            record = bytes.fromhex(line.removeprefix(':'))
            if len(record) != record[0] + 5 or sum(record) & 255:
                raise RuntimeError('Bad HEX checksum/length')
            if record[3] == 4:
                base = int.from_bytes(record[4:6], 'big') << 16
            elif record[3] == 0 and record[0]:
                address = base + int.from_bytes(record[1:3], 'big')
                if not 0x08000000 <= address < address + record[0] <= 0x08010000:
                    raise RuntimeError('Image exceeds F103C8 64 KiB flash')
                lowest, highest = min(lowest, address), max(highest, address + record[0])
        images[profile] = dict(sha256=hashlib.sha256(image.read_bytes()).hexdigest(), start=hex(lowest), end=hex(highest))
        print(profile, images[profile])
(out / 'images.json').write_text(json.dumps(images, indent=2) + '\n', encoding='utf-8')

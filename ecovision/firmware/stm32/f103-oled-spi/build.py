"""Build F103 SPI OLED/IR receiver; never connects to or flashes hardware."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

root = Path(__file__).resolve().parent
tools = Path('D:/k51/ARM/ARMCLANG/bin')
repo = Path.home() / 'STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7'
cmsis = repo / 'Drivers/CMSIS'
device = cmsis / 'Device/ST/STM32F1xx'
shared = root.parent / 'EcoVision_F413ZH/App'
donor = Path('D:/移远杯项目文档/wzy works/红外模块_整合包(2)/02_F103_接收显示代码/Core/Src/f103_oled.c')
font = re.search(r'static const uint8_t font5x7\[40\]\[5\] = \{.*?\n\};', donor.read_text(encoding='utf-8', errors='replace'), re.S)
if not font:
    raise SystemExit('Donor font table not found')
(root / 'font.h').write_text('/* Font from user-supplied wzy f103_oled.c. */\n' + font[0] + '\n', encoding='utf-8')
out = root / 'build'
out.mkdir(exist_ok=True)
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

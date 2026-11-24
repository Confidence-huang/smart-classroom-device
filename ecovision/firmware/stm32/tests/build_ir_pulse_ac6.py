"""Build the current Keil project sources with AC6 tools outside the IDE."""
import hashlib
import argparse
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--generated-core', type=Path)
parser.add_argument('--toolchain', type=Path, default=Path('D:/k51/ARM/ARMCLANG/bin'))
parser.add_argument('--output', type=Path)
parser.add_argument('--name', default='EcoVision-dma-watchdog-20261124.1')
args = parser.parse_args()
project = root / 'firmware/stm32/EcoVision_F413ZH/MDK-ARM'
output = args.output or root / 'firmware/stm32/ir-merge-evidence/2026-11-23'
if args.generated_core:
    output = output / 'regenerated'
build = output / 'obj'
build.mkdir(parents=True, exist_ok=True)
tools = args.toolchain
target = ET.parse(project / 'EcoVision_F413ZH.uvprojx').find('./Targets/Target')
settings = target.find('./TargetOption/TargetArmAds/Cads/VariousControls')
includes = ['-I' + str((project / path).resolve())
            for path in settings.findtext('IncludePath').split(';') if path]
if args.generated_core:
    includes.insert(0, '-I' + str(args.generated_core.resolve() / 'Inc'))
defines = ['-D' + name for name in settings.findtext('Define').split(',') if name]


def run(tool, args, log):
    result = subprocess.run([str(tools / (tool + '.exe')), *map(str, args)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, encoding='utf-8', errors='replace')
    log.write(result.stdout)
    if result.returncode:
        print(result.stdout)
        raise SystemExit(f'{tool} failed with exit {result.returncode}')
    return result.stdout


with (output / 'ac6-build.log').open('w', encoding='utf-8') as log:
    objects = []
    for index, entry in enumerate(target.findall('./Groups/Group/Files/File')):
        source = (project / entry.findtext('FilePath')).resolve(strict=True)
        if args.generated_core and source.name in ('main.c', 'stm32f4xx_it.c', 'stm32f4xx_hal_msp.c'):
            source = (args.generated_core / 'Src' / source.name).resolve(strict=True)
        obj = build / f'{index:03d}-{source.stem}.o'
        log.write(f'{source}\n')
        if source.suffix.lower() == '.c':
            run('armclang', ['--target=arm-arm-none-eabi', '-mcpu=cortex-m4', '-mthumb',
                             '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard', '-std=c99', '-Oz',
                             '-ffunction-sections', '-fdata-sections',
                             *(['-Wall','-Wextra'] if 'App' in source.parts else []), *defines, *includes,
                             '-c', source, '-o', obj], log)
        elif source.suffix.lower() == '.s':
            run('armasm', ['--cpu=Cortex-M4.fp', '--apcs=/hardfp', '-o', obj, source], log)
        else:
            raise SystemExit(f'Unexpected source type {source}')
        objects.append(obj)

    # Keep the last 128 KiB out of application HEX. Existing board config lives there.
    scatter = project / 'application.sct'
    if 'LR_IROM1 0x08000000 0x00160000' not in scatter.read_text():
        raise SystemExit('Application scatter file does not preserve configuration sector')
    elf = build / (args.name + '.axf')
    print(run('armlink', [*objects, '--strict', '--scatter', scatter, '--info=summarysizes',
                          '--entry=Reset_Handler', '-o', elf], log).strip())
    image = output / (args.name + '.hex')
    run('fromelf', ['--i32combined', '--output=' + str(image), elf], log)
    upper, lowest, highest = 0, None, 0
    for line in image.read_text().splitlines():
        if not line.startswith(':'):
            raise SystemExit('Invalid Intel HEX line')
        record = bytes.fromhex(line[1:])
        if len(record) != record[0] + 5 or sum(record) & 255:
            raise SystemExit('Invalid Intel HEX length/checksum')
        kind, address = record[3], int.from_bytes(record[1:3], 'big')
        if kind in (2, 4):
            upper = int.from_bytes(record[4:6], 'big') << (4 if kind == 2 else 16)
        elif kind == 0 and record[0]:
            start, end = upper + address, upper + address + record[0]
            if start < 0x08000000 or end > 0x08160000:
                raise SystemExit('HEX overlaps non-application memory/configuration')
            lowest = start if lowest is None else min(lowest, start)
            highest = max(highest, end)
    if lowest is None:
        raise SystemExit('HEX has no application data')
    digest = hashlib.sha256(image.read_bytes()).hexdigest()
    (output / 'image-check.json').write_text(json.dumps({
        'sha256': digest, 'lowest': hex(lowest), 'end_exclusive': hex(highest),
        'configuration_start': '0x08160000', 'configuration_preserved': True,
        'generated_core': bool(args.generated_core)
    }, indent=2), encoding='utf-8')
    print(f'{len(objects)} objects compiled and linked. SHA-256 {digest}')
    log.write(f'{len(objects)} objects compiled and linked. SHA-256 {digest}\n')

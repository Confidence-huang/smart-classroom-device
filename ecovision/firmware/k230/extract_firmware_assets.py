"""PC-only read-only FAT16 extraction from the user's matching firmware image.

Usage: python -B firmware/k230/extract_firmware_assets.py IMAGE
Writes only the fixed official/ directory next to this tool. No flashing.
"""
import hashlib
import json
import struct
import sys
from pathlib import Path


class Fat16:
    def __init__(self, stream):
        self.stream = stream
        stream.seek(0)
        mbr = stream.read(512)
        if mbr[510:] != b'\x55\xaa':
            raise ValueError('not an MBR image')
        # Firmware layout: second MBR partition is SDCARD. Verify its label.
        sector = struct.unpack_from('<I', mbr, 446 + 16 + 8)[0]
        self.base = sector * 512
        stream.seek(self.base)
        b = stream.read(512)
        if b[43:54].strip() != b'SDCARD' or b[54:62].strip() != b'FAT16':
            raise ValueError('expected SDCARD FAT16 partition')
        bps = struct.unpack_from('<H', b, 11)[0]
        reserved = struct.unpack_from('<H', b, 14)[0]
        roots = struct.unpack_from('<H', b, 17)[0]
        spf = struct.unpack_from('<H', b, 22)[0]
        self.cluster_bytes = bps * b[13]
        fat_offset = self.base + reserved * bps
        self.root_offset = fat_offset + b[16] * spf * bps
        self.root_bytes = roots * 32
        self.data_offset = self.root_offset + ((self.root_bytes + bps - 1) // bps) * bps
        stream.seek(fat_offset)
        self.fat = stream.read(spf * bps)

    def chain(self, cluster):
        output = bytearray()
        seen = set()
        while 2 <= cluster < 0xfff8:
            if cluster in seen or cluster * 2 + 2 > len(self.fat):
                raise ValueError('invalid FAT chain')
            seen.add(cluster)
            self.stream.seek(self.data_offset + (cluster - 2) * self.cluster_bytes)
            output.extend(self.stream.read(self.cluster_bytes))
            cluster = struct.unpack_from('<H', self.fat, cluster * 2)[0]
        return bytes(output)

    def entries(self, cluster=None):
        if cluster is None:
            self.stream.seek(self.root_offset)
            data = self.stream.read(self.root_bytes)
        else:
            data = self.chain(cluster)
        names = {}
        for offset in range(0, len(data), 32):
            e = data[offset:offset + 32]
            if not e or e[0] == 0:
                break
            if e[0] == 0xe5:
                names = {}
                continue
            if e[11] == 15:
                names[e[0] & 31] = (e[1:11] + e[14:26] + e[28:32]).decode('utf-16le').split('\0')[0].replace('\uffff', '')
                continue
            name = ''.join(names[k] for k in sorted(names)) if names else e[:8].decode('ascii').strip() + ('.' + e[8:11].decode('ascii').strip() if e[8:11].strip() else '')
            names = {}
            if e[11] & 8 or name in ('.', '..'):
                continue
            yield name, e[11], struct.unpack_from('<H', e, 26)[0], struct.unpack_from('<I', e, 28)[0]

    def read(self, path):
        cluster = None
        for component in path.strip('/').split('/'):
            matches = [entry for entry in self.entries(cluster) if entry[0].lower() == component.lower()]
            if len(matches) != 1:
                raise ValueError('missing/ambiguous image path: ' + path)
            _, attr, cluster, size = matches[0]
        if attr & 16:
            raise ValueError('expected file')
        return self.chain(cluster)[:size]


def extract(image_path):
    output = Path(__file__).resolve().parent / 'official'
    output.mkdir(exist_ok=True)
    paths = {
        'person_detection.py': '/examples/05-AI-Demo/person_detection.py',
        'person_detect_yolov5n.kmodel': '/examples/kmodel/person_detect_yolov5n.kmodel',
        'firmware_revision.txt': '/revision.txt',
    }
    records = []
    with image_path.open('rb') as stream:
        filesystem = Fat16(stream)
        for filename, source in paths.items():
            data = filesystem.read(source)
            target = output / filename
            if target.exists() and target.read_bytes() != data:
                raise ValueError('refuse to overwrite different asset: ' + filename)
            target.write_bytes(data)
            records.append(dict(file=filename, image_path=source, size=len(data), sha256=hashlib.sha256(data).hexdigest()))
        stream.seek(0)
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    manifest = dict(image_name=image_path.name, image_sha256=digest, assets=records,
                    origin='user-provided local firmware image; no model training or conversion')
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    extract(Path(sys.argv[1]))

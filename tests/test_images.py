"""Offline checks only. This script never invokes a USB operation."""
from pathlib import Path
import json
import subprocess
import tempfile
import zipfile
import os
import hashlib
def crc_normal(data):
    table = []
    for byte in range(256):
        value = byte << 24
        for _ in range(8):
            value = ((value << 1) ^ (0x04c11db7 if value & 0x80000000 else 0)) & 0xffffffff
        table.append(value)
    value = 0
    for byte in data:
        value = ((value << 8) & 0xffffffff) ^ table[(value >> 24) ^ byte]
    return value
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'tests/fixtures'
fixtures = ROOT / 'build' / 'fixtures'
fixtures.mkdir(parents=True, exist_ok=True)
cli = ROOT / 'build' / 'spectrum-updater'
reports = []
for name in ('v105', 'v106', 'v108', 'beta03', 'dc9-v101', 'd02-v101', 'd02-beta01'):
    with zipfile.ZipFile(SOURCE / f'{name}.zip') as archive:
        entries = [entry for entry in archive.namelist() if entry.lower().endswith('.bin')]
        assert len(entries) == 1
        data = archive.read(entries[0])
    path = fixtures / f'{name}.bin'
    path.write_bytes(data)
    result = subprocess.run([str(cli), 'inspect', str(path)], text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    reports.append({'name': name, 'accepted': True, 'output': result.stdout})
    # Reject corruptions in boot data, compressed data, table and CRC footer.
    for offset in (0, 0xff00, 0x10000, 0x32080, 0x33880, len(data)-1):
        bad = bytearray(data)
        bad[offset] ^= 1
        with tempfile.NamedTemporaryFile() as f:
            f.write(bad)
            f.flush()
            check = subprocess.run([str(cli), 'inspect', f.name], capture_output=True)
            assert check.returncode != 0, (name, offset)
    # A flash invocation missing hardware identity must reject before opening it.
    check = subprocess.run([str(cli), 'flash', str(path)], capture_output=True)
    assert check.returncode == 2
    # Recalculate outer/app CRCs after corrupting only the staged main CRC.
    # This proves the added integrity layer rejects a superficially repaired file.
    bad = bytearray(data)
    app_start = int.from_bytes(bad[0x10000:0x10004], 'little')
    app_end = int.from_bytes(bad[0x1000c:0x10010], 'little')
    bad[app_end-1] ^= 1
    bad[0x10014:0x10018] = crc_normal(bad[app_start:app_end]).to_bytes(4, 'little')
    bad[-4:] = crc_normal(bad[:-4]).to_bytes(4, 'big')
    with tempfile.NamedTemporaryFile() as f:
        f.write(bad); f.flush()
        check = subprocess.run([str(cli), 'inspect', f.name], text=True, capture_output=True)
        assert check.returncode != 0 and 'Staged main-build CRC16' in check.stderr
    raw = data + b'\xa5' * (0x400000-len(data))
    with tempfile.NamedTemporaryFile() as f:
        f.write(raw); f.flush()
        check = subprocess.run([str(cli), 'inspect-backup', f.name], capture_output=True)
        assert check.returncode == 0
        check = subprocess.run([str(cli), 'restore', f.name], capture_output=True)
        assert check.returncode == 2
(ROOT / 'build' / 'image-tests.json').write_text(json.dumps(reports, indent=2))
print('Offline image tests: 7 vendor releases and raw backups accepted; corruptions and incomplete operations rejected before USB access.')

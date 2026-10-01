"""Download pinned developer fixtures. No USB access; firmware is never bundled."""
from pathlib import Path
from urllib.request import urlopen
import hashlib
import io
import json
import lzma
import zipfile
ROOT = Path(__file__).resolve().parents[1]
profiles = json.loads(Path(__file__).with_name('test-fixture-sources.json').read_text())
fixtures = ROOT / 'tests/fixtures'
fixtures.mkdir(parents=True, exist_ok=True)
usb = {
    'usb1902': [('hub06a4', 0x5044b, 18113), ('pd1902', 0x54b13, 15988)],
    'usb1702': [('hub06a4', 0x4fe05, 18113), ('pd1702', 0x544cd, 16079)]
}
for name, profile in profiles.items():
    archive_path = fixtures / (name + '.zip')
    data = archive_path.read_bytes() if archive_path.exists() else b''
    if len(data) != profile['bytes'] or hashlib.sha256(data).hexdigest() != profile['sha256']:
        with urlopen(profile['source_url'], timeout=30) as response:
            data = response.read(16 * 1024 * 1024 + 1)
    if len(data) != profile['bytes'] or hashlib.sha256(data).hexdigest() != profile['sha256']:
        raise ValueError('Vendor archive differs from pinned fixture: ' + name)
    archive_path.write_bytes(data)
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        payload = archive.read(profile['member'])
    if name in usb:
        for stem, offset, count in usb[name]:
            decoded = lzma.decompress(payload[offset:offset+count], format=lzma.FORMAT_RAW,
                                      filters=[{'id': lzma.FILTER_LZMA2, 'dict_size': 8 * 1024 * 1024}])
            pin = profile['extracted_payloads'][stem + '.bin']['sha256']
            if hashlib.sha256(decoded).hexdigest() != pin:
                raise ValueError('USB payload differs from pinned fixture: ' + stem)
            (fixtures / (stem + '.bin')).write_bytes(decoded)
    else:
        (fixtures / (name + '.bin')).write_bytes(payload)
    if name == 'beta03':
        target = ROOT / 'resources/Firmware/STOCK_ES07D03_Beta03.bin'
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(payload)
print('Nine original vendor package fixtures prepared. No firmware is bundled by build.sh.')

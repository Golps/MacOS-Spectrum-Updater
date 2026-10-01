"""Create a GitHub app asset without firmware, private data, or build caches."""
from pathlib import Path
import argparse, hashlib, json, plistlib, shutil, subprocess, tempfile, zipfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--version', required=True, help='App marketing version; must match Info.plist')
parser.add_argument('--app', type=Path, default=ROOT / 'build/Spectrum Updater.app')
parser.add_argument('--output', type=Path, default=ROOT / 'build/release')
args = parser.parse_args()
app = args.app.resolve()
info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
if info['CFBundleShortVersionString'] != args.version:
    raise ValueError('Requested package version differs from the app')
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
for name in ['SpectrumUpdater', 'spectrum-updater']:
    archs = subprocess.check_output(['lipo', '-archs', str(app / 'Contents/MacOS' / name)], text=True).split()
    if set(archs) != {'arm64', 'x86_64'}:
        raise ValueError('Both Apple Silicon and Intel executables are required')
if (app / 'Contents/Resources/Firmware').exists():
    raise ValueError('Public app must not bundle firmware')
args.output.mkdir(parents=True, exist_ok=True)
archive = args.output / ('Spectrum-Updater-' + args.version + '-macOS.zip')
if archive.exists():
    raise ValueError('Package already exists; use an empty output directory')
with tempfile.TemporaryDirectory(prefix='spectrum-release-') as temp:
    package = Path(temp) / 'Spectrum Updater'
    package.mkdir()
    shutil.copytree(app, package / 'Spectrum Updater.app')
    shutil.copy2(ROOT / 'Start-here.txt', package / 'Start Here.txt')
    shutil.copy2(ROOT / 'LICENSE', package / 'LICENSE.txt')
    (package / 'Compatibility.txt').write_text(
        'Spectrum Updater ' + args.version + ' — build ' + info['CFBundleVersion'] + '\n\n'
        'macOS 13+, Apple Silicon and Intel. Ad-hoc signed, not Apple-notarized.\n'
        'Profiles: ES07D03, ES07DC9, ES07E30, ES07D02. Exact stock catalog only.\n'
        'ES07D03 scaler: prior reported physical success on one unit.\n'
        'Hub/PD and other model units: offline tests; physical validation outstanding.\n'
        'Excluded: D01, DCA, OLED, arbitrary/custom images, unknown installed releases.\n'
        'No vendor firmware is included. USB raw whole-chip automatic restore is not offered.\n\n'
        'Guide: https://github.com/Golps/MacOS-Spectrum-Updater/blob/main/docs/UPDATE-GUIDE.md\n'
        'Support: https://github.com/Golps/MacOS-Spectrum-Updater/issues\n')
    identity = Path.home().name.lower().encode()
    checked = 0
    for file in package.rglob('*'):
        if not file.is_file():
            continue
        data = file.read_bytes()
        if b'/Users/' in data or b'/private/var/folders/' in data or identity in data.lower():
            raise ValueError('Private workstation information detected in package')
        if file.suffix.lower() in {'.bin', '.exe', '.zip'}:
            raise ValueError('Unexpected firmware/installer/archive included')
        checked += 1
    subprocess.run(['xattr', '-cr', str(package)], check=True)
    subprocess.run(['ditto', '-c', '-k', '--keepParent', '--norsrc', '--noextattr', str(package), str(archive.resolve())], check=True)
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None:
            raise ValueError('Archive integrity check failed')
        if any('/Firmware/' in n or '__MACOSX/' in n for n in z.namelist()):
            raise ValueError('Unexpected firmware or metadata inside archive')
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
(args.output / 'SHA256SUMS.txt').write_text(digest + '  ' + archive.name + '\n')
report = {
    'version': args.version, 'build': info['CFBundleVersion'], 'archive': archive.name,
    'bytes': archive.stat().st_size, 'sha256': digest, 'architectures': ['arm64', 'x86_64'],
    'signature': 'ad-hoc verified', 'apple_notarized': False, 'firmware_bundled': False,
    'files_privacy_scanned': checked, 'hardware_operations_performed': False,
    'hub_PD_hardware_validated': False,
}
(args.output / 'package-report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))

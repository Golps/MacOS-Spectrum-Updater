from pathlib import Path
import hashlib, json, re
root=Path(__file__).resolve().parent.parent
log=(root/'build/protocol-test-log.txt').read_text()
count=int(re.search(r'Protocol simulator: (\d+) assertions passed',log)[1])
files=['src/macos.c','src/macos.h','src/protocol.c','src/image.c','src/files.c','src/spectrum.h','tests/test_protocol.c']
(root/'build/protocol-tests.json').write_text(json.dumps({'assertions_passed':count,'physical_device_operations':False,'files':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in files}},indent=2)+'\n')

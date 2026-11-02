import argparse
import hashlib
import json
from pathlib import Path
import zipfile
root = Path('platform/ps5/dist')
parser = argparse.ArgumentParser()
parser.add_argument('--bundle', type=Path, default=root / 'YataiDON-PS5-experimental.zip')
args = parser.parse_args()
receipt = json.loads((root / 'build-receipt.json').read_text())
app = root / receipt['title_id']
for name, expected in receipt['files'].items():
    actual = hashlib.sha256((app / name).read_bytes()).hexdigest()
    assert actual == expected, name
param = json.loads((app / 'sce_sys/param.json').read_text())
assert param['titleId'] == receipt['title_id']
assert param.get('attribute3', 0) & 0x80040 == 0x80040, 'Missing 120 Hz title capability flags'
assert len(param['contentId'].rsplit('-', 1)[1]) == 16
assert int.from_bytes((app / 'eboot.bin').read_bytes()[:4], 'little') == 0x1D3D154F
with zipfile.ZipFile(args.bundle) as bundle:
    assert bundle.testzip() is None
    assert 'PPSA99329/eboot.bin' in bundle.namelist()
    assert 'PPSA99329/sce_module/libc.prx' in bundle.namelist()
    for name, expected in receipt['files'].items():
        assert hashlib.sha256(bundle.read(receipt['title_id'] + '/' + name)).hexdigest() == expected, name
    bundled_param = json.loads(bundle.read('PPSA99329/sce_sys/param.json'))
    assert bundled_param['attribute3'] & 0x80040 == 0x80040
    assert any(name.startswith('data/YataiDON/shader/') for name in bundle.namelist())
    print(f'PASS: native hashes, metadata, SELF header and ZIP integrity ({len(bundle.namelist())} files)')

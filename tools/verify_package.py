"""校验交付文件哈希、工程引用和正式配置。"""
from hashlib import sha256
from pathlib import Path
from zipfile import ZipFile
import re
import xml.etree.ElementTree as ET


root = Path(__file__).resolve().parents[1]
count = 0
for line in (root / 'SHA256SUMS.txt').read_text(encoding='utf-8-sig').splitlines():
    expected, relative = line.split('  ', 1)
    path = root / relative
    assert path.is_file(), relative
    assert sha256(path.read_bytes()).hexdigest() == expected.lower(), relative
    count += 1
assert count > 100, '校验清单不完整'
for board in ('A', 'B'):
    assert (root / f'{board}.hex').read_bytes() == (root / f'fw/{board}/{board}.hex').read_bytes()
    project = root / f'Firmware/{board}/MDK-ARM/{board}_Firmware.uvprojx'
    tree = ET.parse(project)
    for item in tree.findall('.//FilePath'):
        assert (project.parent / item.text.replace('\\', '/')).is_file(), item.text
    source = (root / f'Firmware/{board}/Core/Src/main.c').read_text(encoding='utf-8-sig')
    assert re.search(r'#define DEBUG_DISPLAY\s+0\b', source)
    if board == 'A':
        assert re.search(r'#define ACCEPTANCE_DEMO\s+0\b', source)
with ZipFile(root / 'Proteus/Dual_Board_Sim.pdsprj') as archive:
    assert archive.testzip() is None
    for name in ('ROOT.CDB', 'ROOT.DSN'):
        data = archive.read(name)
        assert data.count(b'330R') == 4, name
        assert data.count(b'32.768kHz') == 2, name
        assert b'220k' not in data, name
    data = archive.read('ROOT.DSN')
    paths = re.findall(rb'\{PROGRAM=([^}]*)\}', data)
    assert len(paths) == 2
    for relative in paths:
        path = relative.decode('gbk').strip().replace('\\', '/')
        assert path in ('../fw/A/A.hex', '../fw/B/B.hex'), path
        assert (root / 'Proteus' / path).is_file(), path
print(f'PASS: {count} SHA-256 entries; Keil references, HEX mirrors and circuit properties verified.')

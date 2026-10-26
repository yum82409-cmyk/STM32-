"""在英文临时目录重编译两机，并更新仿真实际加载的 HEX。"""
from argparse import ArgumentParser
from hashlib import sha256
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument('--keil', type=Path, default=Path(r'E:\Keil 5\UV4\UV4.exe'))
    args = parser.parse_args()
    if not args.keil.is_file():
        parser.error('未找到 Keil UV4.exe，请用 --keil 指定已安装路径。')
    root = Path(__file__).resolve().parents[1]
    logs = root / 'docs' / 'build_logs'
    logs.mkdir(parents=True, exist_ok=True)
    products = {}
    with tempfile.TemporaryDirectory(prefix='stm32-build-') as temporary:
        stage = Path(temporary)
        shutil.copytree(root / 'Firmware', stage / 'Firmware')
        for board in ('A', 'B'):
            project = stage / f'Firmware/{board}/MDK-ARM/{board}_Firmware.uvprojx'
            log = stage / f'{board}_build.log'
            result = subprocess.run([str(args.keil), '-r', str(project), '-j0', '-o', str(log)],
                                    cwd=project.parent, timeout=180, check=False)
            data = log.read_bytes() if log.exists() else b''
            (logs / f'{board}_build.log').write_bytes(data)
            text = data.decode('utf-8', errors='replace')
            product = project.parent / f'{board}_Firmware/{board}_Firmware.hex'
            if result.returncode != 0 or not re.search(r'0 Error\(s\), 0 Warning\(s\)', text) or not product.is_file():
                raise RuntimeError(f'{board} 编译失败或存在警告，详见 {logs}')
            products[board] = product.read_bytes()
    # 两机都成功后才更新发布固件。
    for board, data in products.items():
        for target in (root / f'{board}.hex', root / f'fw/{board}/{board}.hex'):
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        print(board, '0 errors, 0 warnings', sha256(data).hexdigest())


if __name__ == '__main__':
    main()

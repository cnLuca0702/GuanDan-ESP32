#!/usr/bin/env python3
"""Build and render the real Guandan LVGL UI on the host, without display hardware.

Requires CMake, a C/C++ compiler, and LVGL v9.5.0 source. Reuses
managed_components/lvgl__lvgl when installed by ESP-IDF, otherwise pass --lvgl.
The renderer uses 24 KB of LVGL heap and the same 240x20 RGB565 draw-buffer size
and 30 px corner mask as the BSP. This is not physical-display acceptance.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data) & 0xffffffff)


def ppm_to_png(path: Path) -> None:
    data = path.read_bytes()
    magic, dimensions, maximum, pixels = data.split(b'\n', 3)
    assert magic == b'P6' and maximum == b'255'
    width, height = map(int, dimensions.split())
    assert len(pixels) == width * height * 3
    rows = b''.join(b'\0' + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    output = b'\x89PNG\r\n\x1a\n'
    output += png_chunk(b'IHDR', struct.pack('!2I5B', width, height, 8, 2, 0, 0, 0))
    output += png_chunk(b'IDAT', zlib.compress(rows))
    output += png_chunk(b'IEND', b'')
    path.with_suffix('.png').write_bytes(output)


def run_logged(command: list[str], log: Path) -> None:
    result = subprocess.run(command, capture_output=True, text=True)
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        print(result.stdout, end='')
        print(result.stderr, end='')
        raise SystemExit(result.returncode)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl', type=Path)
    args = parser.parse_args()
    candidates = [ROOT / 'managed_components/lvgl__lvgl', ROOT / 'build/host/lvgl-minimal', ROOT / 'build/host/lvgl']
    source = args.lvgl or next((p for p in candidates if (p / 'src').is_dir()), None)
    if source is None:
        parser.error('LVGL source missing; run the ESP-IDF build first or pass --lvgl /path/to/lvgl-v9.5.0')
    build = ROOT / 'build/guandan-preview'
    output = build / 'screenshots'
    output.mkdir(parents=True, exist_ok=True)
    run_logged(['cmake', '-S', str(ROOT / 'tests/guandan_preview'), '-B', str(build),
                f'-DLVGL_SOURCE_DIR={source.resolve()}', '-DCMAKE_BUILD_TYPE=Debug'], build / 'configure.log')
    run_logged(['cmake', '--build', str(build), '--parallel', '8'], build / 'compile.log')
    print('Host LVGL 9.5.0 configure/build: PASS')
    result = subprocess.run([str(build / 'guandan_preview'), str(output), str(ROOT / 'assets/fonts/guandan_characters.txt')], capture_output=True, text=True)
    (build / 'validation.log').write_text(result.stdout + result.stderr)
    print(result.stdout, end='')
    print(result.stderr, end='')
    for ppm in output.glob('*.ppm'):
        ppm_to_png(ppm)
    print(f'Rendered PNGs: {output}')
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Create source or binary release archives from explicitly selected files."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import re
import stat
import tarfile
from collections.abc import Mapping
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXCLUDED = {'.git', '.github', '__pycache__', 'autom4te.cache', '.DS_Store'}
GENERATED = {'.o', '.d', '.a', '.pyc', '.orig', '.rej'}
BOOT_FILES = ('BOOTX64.EFI', 'BOOTIA32.EFI', 'BOOTAA64.EFI', 'BOOTRISCV64.EFI',
              'BOOTLOONGARCH64.EFI', 'limine-bios.sys', 'limine-bios-cd.bin',
              'limine-bios-pxe.bin', 'limine-uefi-cd.bin')


def add_tree(files: dict[str, Path | bytes], source: Path, destination: str) -> None:
    if not source.exists():
        raise ValueError(f'Required release input is missing: {source}')
    candidates = sorted(source.rglob('*')) if source.is_dir() else [source]
    for path in candidates:
        if not path.is_file():
            continue
        relative = path.relative_to(source) if source.is_dir() else Path()
        if any(part in EXCLUDED for part in relative.parts) or path.suffix in GENERATED or path.name.endswith('~'):
            continue
        if path.is_symlink() and not path.resolve().is_relative_to(ROOT):
            raise ValueError(f'Release input links outside the source tree: {path}')
        name = (Path(destination) / relative).as_posix()
        if name in files:
            raise ValueError(f'Duplicate release path: {name}')
        files[name] = path


def source_files(version: str) -> dict[str, Path | bytes]:
    files: dict[str, Path | bytes] = {}
    for line in (ROOT / 'packaging/source-roots.txt').read_text().splitlines():
        name = line.strip()
        if not name or name.startswith('#'):
            continue
        if Path(name).is_absolute() or '..' in Path(name).parts:
            raise ValueError(f'Invalid source manifest entry: {name}')
        add_tree(files, ROOT / name, name)
    files['version'] = (version + '\n').encode()
    return files


def binary_files(build: Path) -> dict[str, Path | bytes]:
    files: dict[str, Path | bytes] = {}
    firmware = [name for name in BOOT_FILES if (build / 'bin' / name).is_file()]
    if not firmware:
        raise ValueError('Build at least one firmware target before packaging binaries')
    for name in firmware:
        add_tree(files, build / 'bin' / name, name)
    for name in ['limine.c', 'Makefile']:
        add_tree(files, build / 'bin' / name, name)
    if 'limine-bios.sys' in firmware:
        add_tree(files, build / 'bin/limine-bios-hdd.h', 'limine-bios-hdd.h')
    else:
        # The portable host tool must compile without the BIOS installation header.
        files['Makefile'] = (build / 'bin/Makefile').read_bytes().replace(b'CPPFLAGS=', b'CPPFLAGS=-DLIMINE_NO_BIOS')
    for name in ['COPYING', '3RDPARTY.md', 'CONFIG.md', 'USAGE.md', 'FAQ.md', 'INSTALL.md']:
        add_tree(files, ROOT / name, name)
    add_tree(files, ROOT / 'LICENSES', 'LICENSES')
    add_tree(files, ROOT / 'freetype/docs/FTL.TXT', 'LICENSES/FTL.TXT')
    add_tree(files, ROOT / 'docs/CONFIGURATION.md', 'docs/CONFIGURATION.md')
    files['docs/CONFIGURATION.md'] = (ROOT / 'docs/CONFIGURATION.md').read_bytes().replace(
        b'../themes/classical/icons/README.md', b'../theme/icons/README.md')
    add_tree(files, ROOT / 'docs/FONTS.md', 'docs/FONTS.md')
    add_tree(files, ROOT / 'docs/RELEASE.md', 'docs/RELEASE.md')
    add_tree(files, ROOT / 'docs/GUI.md', 'docs/GUI.md')
    add_tree(files, ROOT / 'docs/COMPATIBILITY.md', 'docs/COMPATIBILITY.md')
    add_tree(files, ROOT / 'docs/DEPLOYMENT.md', 'README.md')
    add_tree(files, ROOT / 'docs/DEPLOYMENT.md', 'docs/DEPLOYMENT.md')
    for name in ['minimal', 'hints-grid', 'hints-custom', 'hints-vertical']:
        add_tree(files, ROOT / f'examples/{name}.conf', f'examples/{name}.conf')
    add_tree(files, build / 'theme/eucleia.eui', 'theme/boot/eucleia.eui')
    add_tree(files, ROOT / 'themes/classical/eucleia.conf', 'theme/eucleia.conf')
    add_tree(files, ROOT / 'themes/classical/limine.conf', 'theme/limine.conf.example')
    add_tree(files, ROOT / 'themes/classical/icons', 'theme/icons')
    add_tree(files, ROOT / 'themes/classical/components', 'theme/components')
    for role in ['cinzel', 'ebgaramond']:
        add_tree(files, ROOT / f'themes/classical/fonts/{role}/OFL.txt', f'theme/fonts/{role}/OFL.txt')
    add_tree(files, ROOT / 'themes/classical/README.md', 'theme/README.md')
    add_tree(files, ROOT / 'themes/classical/SOURCES.md', 'theme/SOURCES.md')
    return files


def content(value: Path | bytes) -> bytes:
    return value.read_bytes() if isinstance(value, Path) else value


def archive(files: Mapping[str, Path | bytes], target: Path, prefix: str, epoch: int) -> None:
    with (target.open('wb') as raw,
          gzip.GzipFile(fileobj=raw, mode='wb', filename='', mtime=0) as compressed,
          tarfile.open(fileobj=compressed, mode='w', format=tarfile.PAX_FORMAT) as tar):
        for name, value in sorted(files.items()):
            data = content(value)
            info = tarfile.TarInfo(prefix + '/' + name)
            info.size = len(data)
            executable = isinstance(value, Path) and value.stat().st_mode & stat.S_IXUSR
            info.mode = 0o755 if executable else 0o644
            info.mtime = epoch
            info.uid = info.gid = 0
            info.uname = info.gname = ''
            tar.addfile(info, io.BytesIO(data))
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    target.with_suffix(target.suffix + '.sha256').write_text(f'{digest}  {target.name}\n')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=['source', 'binary'])
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--epoch', type=int, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.+_-]{0,95}', args.version) or args.epoch < 0:
        parser.error('Invalid version or source timestamp')
    args.build = args.build.resolve()
    args.build.mkdir(parents=True, exist_ok=True)
    files = source_files(args.version) if args.kind == 'source' else binary_files(args.build)
    manifest = {name: hashlib.sha256(content(value)).hexdigest() for name, value in sorted(files.items())}
    files['RELEASE-MANIFEST.json'] = (json.dumps({'project': 'Limine-Eucleia', 'version': args.version,
        'kind': args.kind, 'files': manifest}, indent=2) + '\n').encode()
    suffix = '-binary' if args.kind == 'binary' else ''
    name = f'limine-eucleia-{args.version}{suffix}'
    target = args.build / (name + '.tar.gz')
    archive(files, target, name, args.epoch)
    print(f'Created {target} ({len(files)} files)')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError) as error:
        raise SystemExit(str(error))

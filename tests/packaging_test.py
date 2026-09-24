#!/usr/bin/env python3
"""Verify release contents, integrity and exclusion of local development files."""

from __future__ import annotations

import hashlib
import json
import posixpath
import re
import sys
import tarfile
from pathlib import Path, PurePosixPath


def check(path: Path) -> None:
    expected = path.with_suffix(path.suffix + '.sha256').read_text().split()[0]
    assert hashlib.sha256(path.read_bytes()).hexdigest() == expected, 'Archive checksum mismatch'
    with tarfile.open(path, 'r:gz') as archive:
        members = archive.getmembers()
        prefixes = {PurePosixPath(member.name).parts[0] for member in members}
        assert len(prefixes) == 1
        files: dict[str, bytes] = {}
        for member in members:
            parts = PurePosixPath(member.name).parts
            assert member.isfile() and not member.name.startswith('/') and '..' not in parts
            assert parts[1] not in {'Concepts', 'eucleia', 'work', '.git', '.github', 'dist'}
            assert '.git' not in parts and '__pycache__' not in parts
            name = '/'.join(parts[1:])
            assert name not in files, name
            stream = archive.extractfile(member)
            assert stream is not None, member.name
            with stream:
                files[name] = stream.read()
        manifest = json.loads(files.pop('RELEASE-MANIFEST.json'))
        assert manifest['project'] == 'Limine-Eucleia'
        assert set(files) == set(manifest['files'])
        for name, data in files.items():
            assert hashlib.sha256(data).hexdigest() == manifest['files'][name], name
        if manifest['kind'] == 'source':
            required = {'common/menu_gui.c', 'common/ui/config.c', 'common/ui/font.c',
                'common/ui/support.c', 'tests/ui/hints_vm_test.py',
                'configure', 'GNUmakefile.in', 'tools/package.py', 'tools/build-theme.py',
                'tests/ui/authoring_vm_test.py', 'docs/CONFIGURATION.md', 'version',
                'tests/ui/theme_vm_test.py', 'docs/images/classical.png',
                'dev', 'dev.py', 'pyproject.toml', '.vscode/c_cpp_properties.json',
                'themes/classical/limine.conf', 'themes/classical/SOURCES.md',
                'themes/classical/tokens.json',
                'typings/fontTools/ttLib/__init__.pyi',
                'typings/fontTools/varLib/instancer.pyi',
                'freetype/docs/FTL.TXT', 'themes/classical/fonts/cinzel/OFL.txt',
                'themes/classical/fonts/ebgaramond/OFL.txt'}
            assert b'./version.sh' not in files['version']
            assert archive.getmember(next(iter(prefixes)) + '/dev').mode & 0o111
        else:
            required = {'limine.c', 'Makefile', 'COPYING', 'LICENSES/FTL.TXT',
                'LICENSES/FreeType-third-party.txt', 'theme/boot/eucleia.eui',
                'theme/eucleia.conf', 'theme/icons/windows.png', 'theme/icons/linux.png',
                'theme/icons/README.md', 'theme/icons/entries.conf', 'theme/icons/LICENSE',
                'theme/limine.conf.example', 'theme/SOURCES.md',
                'examples/hints-grid.conf', 'examples/hints-custom.conf', 'examples/hints-vertical.conf',
                'theme/components/eucleia-header.png',
                'theme/components/cursor.png', 'theme/fonts/cinzel/OFL.txt',
                'theme/fonts/ebgaramond/OFL.txt', 'docs/CONFIGURATION.md'}
            assert files['theme/boot/eucleia.eui'].startswith(b'EUCTHM02')
            assert any(name.endswith('.EFI') or name == 'limine-bios.sys' for name in files)
            if 'limine-bios.sys' in files:
                assert 'limine-bios-hdd.h' in files
            else:
                assert b'-DLIMINE_NO_BIOS' in files['Makefile']
            for name, data in files.items():
                if not name.endswith('.md'):
                    continue
                for target in re.findall(r'!?\[[^\]]*\]\(([^\s)]+)', data.decode()):
                    if '://' in target or target.startswith('#'):
                        continue
                    target = target.split('#')[0]
                    linked = posixpath.normpath(str(PurePosixPath(name).parent / target))
                    assert linked in files, (name, target)
        assert required <= set(files), sorted(required - set(files))
        theme = 'themes/classical/' if manifest['kind'] == 'source' else 'theme/'
        assert not any(name.endswith(('.svg', '.pb')) for name in files if name.startswith(theme))
        assert {name for name in files if name.startswith(theme) and name.endswith('.json')} == (
            {theme + 'tokens.json'} if manifest['kind'] == 'source' else set())
        for line in files[theme + 'eucleia.conf'].decode().splitlines():
            key, colon, value = line.partition(':')
            if colon and key.strip() in {'file', 'icon', 'font'}:
                assert theme + value.strip() in files, value
    print(f'Passed {manifest["kind"]} archive: {len(files)} files, hashes, notices and exclusions')


if __name__ == '__main__':
    if len(sys.argv) < 2:
        raise SystemExit('Usage: python3 tests/packaging_test.py archive.tar.gz [...]')
    for argument in sys.argv[1:]:
        check(Path(argument))

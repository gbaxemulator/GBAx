#!/usr/bin/env python3
"""Shared packaging helpers. Never include a user's ROMs or configuration."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[2]


def version():
    text = (ROOT / 'version.cmake').read_text()
    return '.'.join(re.search(r'set\(LIB_VERSION_' + key + r' (\d+)\)', text)[1]
                    for key in ('MAJOR', 'MINOR', 'PATCH'))


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def resources(destination, notice_roots):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for name in ('nointro.dat', 'gba_catalog_us.json'):
        shutil.copy2(ROOT / 'res' / name, destination / name)
    shutil.copytree(ROOT / 'res/shaders', destination / 'shaders', dirs_exist_ok=True)
    notices = destination / 'licenses'
    shutil.copytree(ROOT / 'res/licenses', notices, dirs_exist_ok=True)
    shutil.copy2(ROOT / 'LICENSE', notices / 'GBAx-MPL-2.0.txt')
    for name in ('zlib', 'libpng'):
        p = ROOT / 'src/third-party' / name / 'LICENSE'
        if p.exists():
            shutil.copy2(p, notices / (name + '-LICENSE.txt'))
    for root in notice_roots:
        root = Path(root)
        if not root.is_dir():
            raise SystemExit(f'Notice directory missing: {root}')
        for pattern in ('**/copyright', '**/LICENSE*', '**/COPYING*', '**/Copyright*'):
            for p in root.glob(pattern):
                if p.is_file() and p.stat().st_size < 2 * 1024 * 1024:
                    target = notices / 'dependencies' / root.name / p.relative_to(root)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(p, target)
    (destination / 'SOURCE.txt').write_text(
        f'GBAx {version()}\nCorresponding source: GBAx-{version()}-source.zip, '
        'distributed alongside the application downloads.\n'
        'GBAx source retains its upstream copyright notices and MPL-2.0 license.\n'
        'Dependency notices are in licenses/. Qt and SDL are dynamically linked.\n'
        'This package does not include game ROMs or Nintendo BIOS files.\n', encoding='utf-8')


def tools(destination):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    entries = json.loads((Path(__file__).parent / 'appimage-tools.json').read_text())
    for name, entry in entries.items():
        target = destination / name
        if not target.exists() or digest(target) != entry['sha256']:
            with urllib.request.urlopen(entry['url'], timeout=120) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != entry['sha256']:
                raise SystemExit(f'{name}: upstream changed; review and update the tool lock before continuing')
            target.write_bytes(data)
        target.chmod(0o755)


def check_cache(path):
    cache = {}
    for line in Path(path).read_text().splitlines():
        if '=' in line and ':' in line and not line.startswith(('#', '//')):
            key, value = line.split('=', 1)
            cache[key.split(':')[0]] = value
    # CMake can silently disable dependencies; catch that before distributing.
    for key in ('BUILD_QT', 'BUILD_SDL', 'USE_PNG', 'USE_ZLIB', 'USE_SQLITE3', 'USE_LIBZIP'):
        if cache.get(key) not in ('1', 'ON', 'TRUE'):
            raise SystemExit(f'Required release feature missing: {key}={cache.get(key)}')
    if cache.get('BUILD_SHARED') not in ('0', 'OFF', 'FALSE'):
        raise SystemExit('The release profile requires a static emulator core')
    if cache.get('BUILD_STATIC') not in ('1', 'ON', 'TRUE'):
        raise SystemExit('BUILD_STATIC must be ON; both OFF enables the shared-core fallback')
    flags = Path(path).parent / 'include/gbax/flags.h'
    if not flags.is_file():
        raise SystemExit('Missing generated feature definitions')
    text = flags.read_text()
    for feature in ('USE_PNG', 'USE_ZLIB', 'USE_SQLITE3', 'USE_LIBZIP'):
        if not re.search(r'^#define\s+' + feature + r'\b', text, re.M):
            raise SystemExit(f'CMake disabled required feature: {feature}')


def manifest(directory):
    directory = Path(directory)
    paths = sorted(p for p in directory.iterdir() if p.suffix in ('.zip', '.dmg', '.AppImage'))
    if not paths:
        raise SystemExit('No release packages found')
    files = [{'name': p.name, 'bytes': p.stat().st_size, 'sha256': digest(p)} for p in paths]
    (directory / 'SHA256SUMS.txt').write_text(''.join(f"{p['sha256']}  {p['name']}\n" for p in files))
    (directory / 'downloads.json').write_text(json.dumps({'version': version(), 'status': 'candidate', 'files': files}, indent=2) + '\n')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='action', required=True)
    sub.add_parser('version')
    r = sub.add_parser('resources')
    r.add_argument('destination')
    r.add_argument('--notices', action='append', default=[])
    for name in ('tools', 'manifest', 'check-cache'):
        sub.add_parser(name).add_argument('path')
    a = p.parse_args()
    if a.action == 'version': print(version())
    elif a.action == 'resources': resources(a.destination, a.notices)
    elif a.action == 'tools': tools(a.path)
    elif a.action == 'check-cache': check_cache(a.path)
    elif a.action == 'manifest': manifest(a.path)


if __name__ == '__main__':
    main()

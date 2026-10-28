#!/usr/bin/env python3
"""Fail assembly if any promised OS/architecture, source or checksum is missing."""
import json
from pathlib import Path
import sys
from release import digest, version

root = Path(sys.argv[1])
v = version()
for name in ('linux-package-check.json', 'windows-package-check.json',
             'macos-arm64-package-check.json', 'macos-x86_64-package-check.json'):
    try:
        report = json.loads((root / name).read_text(encoding='utf-8-sig'))
        keys = ('tls', 'png', 'jpeg', 'catalog', 'database', 'shaders', 'writableArtwork', 'controllerSupport')
        if report.get('passed') is not True or not all(report.get('checks', {}).get(k) is True for k in keys):
            raise ValueError('Runtime checks failed')
    except (OSError, ValueError) as error:
        raise SystemExit(f'Missing or failing runtime report {name}: {error}')
required = [f'GBAx-{v}-windows-x64.zip', f'GBAx-{v}-linux-x86_64.AppImage', f'GBAx-{v}-source.zip']
for arch in ('arm64', 'x86_64'):
    candidates = [root / f'GBAx-{v}-macos-{arch}{suffix}.dmg' for suffix in ('', '-unsigned')]
    present = [p for p in candidates if p.is_file()]
    if len(present) != 1:
        raise SystemExit(f'Expected exactly one Mac {arch} package')
    required.append(present[0].name)
manifest = json.loads((root / 'downloads.json').read_text())
entries = {e['name']: e for e in manifest['files']}
for name in required:
    p = root / name
    if not p.is_file() or p.stat().st_size == 0:
        raise SystemExit(f'Missing release package: {name}')
    e = entries.get(name)
    if not e or digest(p) != e['sha256'] or p.stat().st_size != e['bytes']:
        raise SystemExit(f'Checksum/size mismatch: {name}')
print('All four desktop packages and matching source are present; checksums verified.')

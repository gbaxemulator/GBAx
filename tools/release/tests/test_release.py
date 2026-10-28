import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('gbax_release', TOOLS / 'release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def test_missing_platform_or_failed_runtime_blocks_assembly(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            checks = {k: True for k in ('tls', 'png', 'jpeg', 'catalog', 'database', 'shaders', 'writableArtwork', 'controllerSupport')}
            reports = ['linux-package-check.json', 'windows-package-check.json', 'macos-arm64-package-check.json', 'macos-x86_64-package-check.json']
            for name in reports:
                (root / name).write_text(json.dumps({'passed': True, 'checks': checks}))
            v = release.version()
            files = [f'GBAx-{v}-windows-x64.zip', f'GBAx-{v}-linux-x86_64.AppImage', f'GBAx-{v}-source.zip',
                     f'GBAx-{v}-macos-arm64-unsigned.dmg', f'GBAx-{v}-macos-x86_64-unsigned.dmg']
            # These are checksum fixtures, not runnable executable fixtures.
            for name in files: (root / name).write_bytes(name.encode())
            release.manifest(root)
            command = [sys.executable, str(TOOLS / 'verify-release.py'), d]
            def run(): return subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(run().returncode, 0)
            (root / reports[0]).write_text(json.dumps({'passed': False, 'checks': checks}))
            self.assertNotEqual(run().returncode, 0)
            (root / reports[0]).write_text(json.dumps({'passed': True, 'checks': checks}))
            (root / files[-1]).unlink()
            self.assertNotEqual(run().returncode, 0)

    def test_checksum_detects_changed_binary(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'test.zip'
            p.write_bytes(b'original')
            release.manifest(d)
            checksum = json.loads((Path(d) / 'downloads.json').read_text())['files'][0]['sha256']
            p.write_bytes(b'modified')
            self.assertNotEqual(checksum, release.digest(p))

    def test_cache_does_not_hide_disabled_feature(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root / 'include/gbax').mkdir(parents=True)
            cache = root / 'CMakeCache.txt'
            keys = ('BUILD_QT', 'BUILD_SDL', 'BUILD_STATIC', 'USE_PNG', 'USE_ZLIB', 'USE_SQLITE3', 'USE_LIBZIP')
            cache.write_text('\n'.join(f'{k}:BOOL=ON' for k in keys) + '\nBUILD_SHARED:BOOL=OFF\n')
            flags = root / 'include/gbax/flags.h'
            features = ('USE_PNG', 'USE_ZLIB', 'USE_SQLITE3', 'USE_LIBZIP')
            flags.write_text('\n'.join('#define ' + k for k in features))
            release.check_cache(cache)
            flags.write_text('#define USE_PNG\n')
            with self.assertRaises(SystemExit): release.check_cache(cache)


if __name__ == '__main__': unittest.main()

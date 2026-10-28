#!/usr/bin/env python3
"""Check the deployed app with Cocoa; capture a stack sample if startup hangs."""
import json
import os
from pathlib import Path
import subprocess
import sys

executable, report_name = sys.argv[1:]
report = Path(report_name)
env = os.environ.copy()
for name in ('QT_PLUGIN_PATH', 'DYLD_LIBRARY_PATH'):
    env.pop(name, None)
env['QT_QPA_PLATFORM'] = 'cocoa'
process = subprocess.Popen([executable, '--package-check'], env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
try:
    stdout, stderr = process.communicate(timeout=120)
except subprocess.TimeoutExpired:
    sample_path = report.with_suffix('.sample.txt')
    try:
        subprocess.run(['/usr/bin/sample', str(process.pid), '3', '-file', str(sample_path)],
                       timeout=20, check=False)
        if sample_path.exists():
            print('\n'.join(sample_path.read_text(errors='replace').splitlines()[:200]), file=sys.stderr)
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f'Could not sample stalled application: {error}', file=sys.stderr)
    finally:
        process.kill()
        stdout, stderr = process.communicate()
        report.write_text(stdout)
        report.with_suffix('.stderr.txt').write_text(stderr)
        print(stderr, file=sys.stderr)
    raise SystemExit('Packaged Mac application did not finish its Cocoa runtime check within two minutes')
report.write_text(stdout)
report.with_suffix('.stderr.txt').write_text(stderr)
print(stdout, end='')
print(stderr, end='', file=sys.stderr)
if process.returncode:
    raise SystemExit(process.returncode)
if json.loads(stdout).get('passed') is not True:
    raise SystemExit('Packaged Mac runtime checks failed')

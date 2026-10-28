#!/usr/bin/env python3
"""Unix smoke test for a built GBAx Qt executable, using an original test ROM.

Run: python3 tools/qa_launcher_smoke.py /absolute/path/to/gbax-qt
All settings and ROM files are isolated in a temporary directory.
"""
import hashlib
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def main():
    if len(sys.argv) != 2 or os.name != "posix":
        raise SystemExit("Usage (Unix): qa_launcher_smoke.py /path/to/gbax-qt")
    executable = Path(sys.argv[1]).resolve(strict=True)
    try:
        probe = socket.socket(socket.AF_UNIX)
        probe.close()
    except PermissionError:
        print("SKIP: this environment blocks local Unix sockets; run on a normal Linux desktop.")
        return 77
    endpoint = Path(tempfile.gettempdir()) / (
        "GBAxLauncherBridge-" + hashlib.sha1(str(executable).encode()).hexdigest()
    )
    if endpoint.exists():
        raise SystemExit("Close any running copy of this GBAx executable first.")
    with tempfile.TemporaryDirectory(prefix="gbax-smoke-") as temp:
        root = Path(temp)
        config = root / "gbax"
        config.mkdir()
        (config / "qt.ini").write_text(
            "[General]\ndisplayDriver=0\n"
            "[gbax]\nfirstRunComplete=true\nshowLauncherSplash=false\n"
            "[gbaxLauncher]\nautoDownloadCovers=false\n"
        )
        rom = bytearray(1024)
        rom[:4] = bytes.fromhex("2e0000ea")
        rom[0xA0:0xAC] = b"GBAX QA ROM "
        rom[0xB2] = 0x96
        rom[0xC0:0xC4] = bytes.fromhex("feffffea")
        rom_path = root / "smoke.gba"
        rom_path.write_bytes(rom)
        env = dict(os.environ, XDG_CONFIG_HOME=temp, XDG_DATA_HOME=temp,
                   QT_QPA_PLATFORM="offscreen", SDL_AUDIODRIVER="dummy")
        for run in range(3):
            command = [str(executable), "--launcher", "-C", "audioSync=0",
                       "-C", "videoSync=0", "-C", "useBios=0"]
            if run == 1:
                command += ["-f"]
            command += [str(rom_path)]
            with tempfile.TemporaryFile() as log:
                process = subprocess.Popen(command, env=env, stdout=log, stderr=log)
                try:
                    deadline = time.monotonic() + 15
                    while not endpoint.exists():
                        if process.poll() is not None or time.monotonic() > deadline:
                            log.seek(0)
                            raise RuntimeError(f"Bridge did not become ready (process status {process.poll()}):\n" + log.read().decode(errors="replace"))
                        time.sleep(0.02)
                    # Let the original ARM program run and initialize its window.
                    time.sleep(0.25)
                    with socket.socket(socket.AF_UNIX) as client:
                        client.settimeout(5)
                        client.connect(str(endpoint))
                        client.sendall(b"open_settings\n")
                    time.sleep(0.1)
                    with socket.socket(socket.AF_UNIX) as client:
                        client.settimeout(5)
                        client.connect(str(endpoint))
                        client.sendall(b"close_")
                        time.sleep(0.1)
                        if process.poll() is not None:
                            raise RuntimeError("Player exited before the command was complete")
                        client.sendall(b"game\n")
                    status = process.wait(timeout=10)
                    log.seek(0)
                    output = log.read().decode(errors="replace")
                    if status != 0 or "QThread: Destroyed while thread is still running" in output:
                        raise RuntimeError(f"Player exit status {status}:\n{output}")
                    print(f"PASS {run + 1}/3: start, settings, fragmented close command, clean exit")
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()
    print("PASS: repeated player-process launch/close")


if __name__ == "__main__":
    sys.exit(main() or 0)

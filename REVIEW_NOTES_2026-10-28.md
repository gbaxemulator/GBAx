# GBAx source review — 28 October 2026

Reviewed input: `gbax_patch3_8_release_notes_website_links_REPACKED.zip`.

The code identifies itself as **GBAx 2.0.0**. It already includes the custom frameless player, quick-menu overlay, launcher, cover catalogue, and separate game processes. This is beyond the earlier title-bar checkpoint. The upload alone cannot establish whether a newer private copy exists.

## Result

The reviewed source builds on Linux with GCC 13.3, Qt 6.4.2 and SDL 2. **All 28 CTest groups pass**, including the new reset and player/launcher lifecycle regressions. This is a source update, not a Windows binary release or a guarantee of compatibility with every game.

## Fixes

| Area | Finding and change |
| --- | --- |
| Reopening a player | A regression test reproduced a segmentation fault in `ActionMapper.cpp`: a retained action called a lambda holding a deleted `QAction`. The connection now has the menu action as its lifetime context and checks the weak pointer. The same test passes after the fix. |
| Reset / portable mode | Reset recursively deleted the entire configuration directory, which can also contain the portable executable, ROMs and saves. Reset now removes only named settings and launcher-index files; keeps other files and directories; refuses reset while a game is running; checks that the old process exits; and reports removal failures. |
| ROM scanning | Scan completion and thread deletion could race, and shutdown stopped waiting after 2.5 seconds. Completion now runs after the worker finishes, the thread is owned and guarded, shutdown waits for cancellation to finish, and progress updates are batched. |
| Cover downloads | Catalogue artwork used the application's GUI-thread network manager and pixmaps from a worker thread. It now uses the existing asynchronous GUI-thread request path, with a 15-second timeout per request. Live remote-service behavior has not been verified. |
| Save preview jobs | Background workers no longer access launcher widgets. A parented future watcher delivers results on the GUI thread and discards outdated selections. |
| Starting games | Removed the artificial 900 ms delay and the blocking five-second `waitForStarted`. QProcess signals manage completion and errors; duplicate launches are guarded; missing ROMs are reported; recent-game history changes only after process start. |
| D-pad navigation | Fixed tests of mapped hat values: they are bitmasks, not key numbers. Both launcher navigation and the quick menu now use shifted key bits. |
| Player window | Added a bottom-right resize grip, native title-bar dragging where supported, fullscreen restoration to the previous maximized state, deferred fullscreen requests before showing, and video-size calculations that account for custom chrome. |
| Pause and keyboard | Closing the quick menu preserves a previously paused game. Unmapped key releases are forwarded as releases. Settings no longer dereferences an absent display. |
| OpenGL | Removed a graphics effect from the ancestor of the OpenGL display. The styled border and header remain. Qt documents that graphics effects apply to children and are unsupported on OpenGL widgets: https://doc.qt.io/qt-6/qwidget.html#setGraphicsEffect |
| Local command bridge | Receivers wait for a complete newline-terminated command and use guarded window references. The socket integration script remains unverified here because this environment blocks Unix sockets. |
| Build | Declared C++17, matching the existing `std::clamp` / `std::as_const` usage; fixed the no-Git-tags CMake error; and corrected SDL OpenGL source selection when libepoxy enables a renderer. Mouse-position access works with both Qt 5 and Qt 6. A full Qt 5 build was not run. |
| Archive consistency | CMake uses `src/`. The redundant root `platform/` tree contained different Qt files. It is now synchronized with `src/platform/`, including the new helpers and tests, so either copy contains the reviewed code. |

No emulator CPU, memory, sound or graphics-core algorithms were changed. Existing source credits and licenses remain.

## Verified

- Complete Linux Debug build: core library, Qt application, SDL frontend and test targets.
- Original 26 test groups plus two new groups: **28/28 passed**.
- Deferred fullscreen before showing a window.
- Three repeated player creation / emulation / teardown cycles using a minimal original ARM test program, with no commercial ROM or BIOS data.
- Emulated frame signals, save-state creation and save-state reload.
- Windowed / maximized / fullscreen header and resize-grip states.
- Quick-menu pause restoration and mapped D-pad-down navigation.
- Five launcher destruction cycles during scans of 401 test ROM files.
- Reset removes the expected settings/index files while preserving executable, portable marker, ROM, save, savestate, artwork and unrelated-file fixtures. It does not recursively delete an unexpected directory named like a settings file.

The offscreen Qt platform cannot verify physical dragging, desktop focus, window masks or GPU rendering. Optional controller-mapping and offscreen-platform warnings are present; they are not test failures.

## Reproduce the automated checks

With the project's development dependencies installed:

```sh
cmake -S . -B build-review -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_SUITE=ON -DBUILD_QT_UI_TESTS=ON \
  -DBUILD_LTO=OFF -DFORCE_QT_VERSION=6 \
  -DUSE_FFMPEG=OFF -DUSE_DISCORD_RPC=OFF -DUSE_LUA=OFF -DUSE_FREETYPE=OFF
cmake --build build-review --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build-review --output-on-failure
```

`BUILD_QT_UI_TESTS` is optional and restricted to non-macOS Unix so the test can isolate preferences using `XDG_CONFIG_HOME`. The portable-reset test is included in the normal Qt test suite. Optional FFmpeg, Lua, Discord RPC and font-rendering features were disabled in the reviewed configuration and have not been validated.

On a normal Linux desktop with local Unix sockets enabled, an additional process/IPC test is available:

```sh
python3 tools/qa_launcher_smoke.py /absolute/path/to/build-review/qt/gbax-qt
```

This tests three separate process launches, opening settings, and closing the game via a command sent in fragments. It exits with code 77 when local sockets are blocked. It was **skipped here**, not counted as a pass.

## Windows build and acceptance checks

Use the same MSVC / Qt / dependency setup as the previously working build. Configure a fresh build directory with the Qt MSVC kit path and any existing dependency toolchain settings; do not reuse Linux build files. C++17 is now required explicitly. Then build the Qt target:

```powershell
cmake --build build --config Release --target gbax-qt --parallel 2
```

The Windows executable is `GBAx.exe`. Package Qt's runtime and plugins with the `windeployqt` from the matching Qt kit, and retain the matching core/SDL/dependency DLLs. No Windows EXE or DLL package is included in this source archive.

Before marking a Windows release ready:

1. Launch Unbound, close it, then launch Emerald; repeat with another game. Confirm in-game saves and savestates persist.
2. Test custom title-bar dragging, resizing, minimize/restore, maximize, Alt+Tab and fullscreen transitions, including a second monitor and display scaling.
3. Test both Software (Qt) and OpenGL display modes, audio output, and shader changes.
4. Test a real controller's D-pad, Start/Select, shoulder/stick shortcuts, quick menu and game input.
5. Open emulator settings from the launcher, switch between windows, and return to the launcher from the overlay.
6. Scan and cancel a large ROM folder, close during scanning, and test slow or disconnected paths.
7. Verify online covers, website/help links, and update behavior with network access.
8. In a disposable portable copy, verify Reset preserves ROMs, saves, artwork and the executable and returns to first-run setup.

Real-game compatibility, Windows/MSVC compilation, hardware controller behavior, Windows shell focus/DWM behavior and live network services remain unverified in this environment.

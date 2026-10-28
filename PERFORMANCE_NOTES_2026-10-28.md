# GBAx launcher performance update

## Follow-up: visual polish

- Artwork placeholders now use explicit Queued, Downloading, No cover found, Download failed, Paused and No artwork states. Queue/start/completion notifications refresh both the installed and catalogue aliases. A network failure is not reported as a missing cover; missing-cover results follow exhausted HTTP 404 candidates. Retry reasons are stored with cooldowns.
- Selection uses a crisp accent-colour border without zooming or shading the cover. Selected icon variants are provided explicitly so the native Qt style does not tint the artwork. Keyboard/controller focus uses a slightly thicker border.
- The launcher and welcome dialog share a resolved UI font. Windows prefers Segoe UI when installed; other environments use their resolved system font. The grid delegate applies the same family and matching font metrics, including after stylesheet processing.
- Quick Save Slots defaults to a compact collapsed heading and explanation when empty. It expands automatically for a selected game with existing saves, supports manual expansion/collapse, and skips hidden/disabled slots during navigation. Linked catalogue games resolve to their installed ROM for previewing and launching a slot. Dates and times use compact separate lines, and save files without screenshots do not display a misleading Empty preview.
- The welcome dialog hides automatic update checking when BUILD_UPDATER is disabled. Visible USB/Bluetooth guidance explains why the Controller button is disabled, and live detection enables it when a gamepad is reported. Builds without controller support explain that separately.

The deterministic checks cover queue/download/404/offline states, unchanged selected-cover pixel colours, matching grid/UI font families, keyboard expansion/collapse, empty and populated save panels, and welcome visibility/guidance. Hardware hot-plugging and Windows native font rendering still require local verification. This continues the existing background artwork/preload workflow; no emulator-core algorithms or save formats were changed.

All 28 CTest groups pass for this update, including the 16 checks in the Qt player/launcher test executable (which includes setup and cleanup). The 110-game fixture populated in 0.66 seconds including its 300 ms settling delay. Validation logs are in validation/visual-polish/.

## Follow-up: smooth catalogue browsing and complete resume text

Opening Browse Games now starts background artwork preparation for the entire catalogue, independent of scrolling, search results or the current page. Installed-library artwork gets priority. The automatic-download preference, cancellation, cached-cover checks, four-cover download limit and retry cooldowns remain in effect. Opening Browse does not force another round of failed downloads on every visit.

Catalogue rows are added in short background batches until all rows are available, rather than waiting for scrolling near the bottom. Thumbnail processing prioritises the viewport and nearby screens, then fills the remaining rows in the background with a bounded number of decode jobs. Completed thumbnails are installed in offscreen rows. Existing row artwork stays visible while a replacement size is being prepared. This intentionally uses more thumbnail memory as the catalogue warms; it does not decode every full-resolution cover at startup.

The Browse summary now says artwork downloads in the background. Placeholders distinguish pending/loading artwork, disabled/paused downloads and unsuccessful attempts. A first download still needs time and an available server; this change removes scroll-dependent triggering rather than promising every cover exists.

Continue Playing now calculates its height from the actual text layout after resizing. The stats label uses the available width, supports wrapping and displays a compact date such as 28 Oct 2026, with the longer wording retained in its tooltip. The title and stats were checked together at 1000 x 704 and 1246 x 880 logical window sizes.

Validation: all 28 CTest groups pass. In the controlled HTTP-fixture test, all 947 catalogue thumbnail icons were ready without scrolling after about 19.4 seconds, while Settings was open; the maximum measured GUI heartbeat gap was 33 ms. A jump to the last row already had its artwork. This is simulated-network Linux testing, not a prediction of real download speed. The 110-game startup benchmark remains under one second including its settling delay. Logs are under validation/smooth-browsing/.

## Follow-up: automatic background artwork

After selecting a ROM folder, the completed scan now schedules missing artwork for the entire installed library, including games outside the viewport or excluded by the current search. It also handles new games on later scans. The queue continues while Settings, Favorites or another page is open. Images themselves are still decoded lazily for visible rows.

The automatic download option is enabled by default; an existing disabled preference is respected. Re-enabling it starts processing the whole library. Up to four cover downloads run together, with small queue-feeding batches, cache reuse and retry cooldowns. Cancelling pauses automatic artwork for the current launcher session; selecting a folder, toggling the option back on or restarting can resume it. The manual catalogue-download button remains available.

The Continue Playing card now allows enough height for wrapped titles. The completion label says Artwork finished instead of implying every game has a cover.

Validation: a 72-game fixture fetched all 70 missing covers while Settings was open and a search excluded all grid rows. Two local covers were skipped; a rescan made no new HTTP requests. Disabling automatic downloads prevented a newly added game from being fetched, and re-enabling fetched it. Cancellation remained effective after returning to the library. All 28 CTest groups passed. These tests use injected HTTP fixtures; Windows and live artwork providers need the usual local check. Logs are in validation/background-artwork/.

This update builds on the reviewed patch 3.8 source. It preserves the existing emulator core, credits and licenses. The earlier REVIEW_NOTES document describes that earlier checkpoint; this document supersedes its launcher/artwork behavior and Windows build instructions.

## Why the launcher froze

Unknown ROM names repeatedly triggered normalization, regular-expression construction and fuzzy comparisons against the 947-game catalogue. Sorting repeated much of this work. The startup flow also held the launcher behind scanning and artwork completion, and cover decoding and thumbnail work competed with UI painting.

The new matcher uses precompiled expressions, cached results and exact normalized lookup tables. It deliberately leaves ambiguous games and ROM hacks unmatched rather than assigning unrelated metadata. Explicit user links remain supported. GB/GBC files are not automatically matched to the GBA catalogue.

## Changes

- The launcher opens immediately. Background scanning displays inline progress and cancellation; games already listed remain accessible. Startup refresh detects added/deleted ROMs. Archive validation is cached by size and modification time.
- Artwork loads for visible games, with two image-processing workers, four concurrent downloads, timeouts, payload limits and atomic cache writes. Installed and catalogue views reuse matching artwork. Downloaded covers appear immediately without restarting. Missing covers and network failures have retry cooldowns; manual requests can retry.
- Catalogue artwork indexes are loaded asynchronously and cached. Bulk downloads feed a bounded queue. Settings can start/cancel downloads inside the launcher.
- Thumbnail and backdrop work is reduced and cached. Placeholder backdrops are no longer repeatedly blurred. Previously visited list icons can still retain memory; this is not a fully virtualized library model.
- Game/launcher commands no longer synchronously wait for sockets on the UI thread. Settings are written before launching a game and reloaded when its process exits.
- Installed, Favorites and Recently Played use section-specific headings. The smaller resume card appears only when there is a recent game on Installed. The grid responds to available width, uses the system UI font, and supports double-click to play.
- Browse selects and displays its first game correctly. Unlinked entries offer Locate ROM. Full details titles wrap. The details pane uses one scroll area, including save slots.
- Context menus and scrollbars match the theme. Hide from Library replaces the misleading Uninstall label. Clearing launcher caches preserves downloaded/manual artwork.
- Welcome correctly saves an empty ROM folder, describes recursive scanning accurately, offers Original pixels as the fastest default, and disables update checks when the updater was not built. Existing graphics settings are preserved.
- Emulator settings can open before any game is running. Portable-mode settings migration refreshes its destination correctly.

## Measurements and validation

Controlled Linux Debug test: GCC 13.3, Qt 6.4.2, offscreen Qt platform, 110 synthetic ROM files, 55 generated 600x800 covers, 947 catalogue entries, automatic network downloads disabled. No commercial game or BIOS data was used.

| Measurement | Before | After (representative run) |
| --- | ---: | ---: |
| Launcher ready signal | 47.37 s | 0.13 s |
| All 110 rows populated, including 300 ms settling time | 47.85 s | 0.67 s |
| Browse transition, including 300 ms settling time | 1.55 s | 0.33 s |
| Longest observed GUI heartbeat gap | 44.22 s | 0.09 s |

These are test-harness observations, not Windows guarantees or gameplay FPS measurements. Large archives, slow disks and network providers may behave differently. Original and final logs are included under validation/performance/.

The complete Linux application/test build and 28 CTest groups pass. Regression coverage includes player lifecycle, save/reload, scan shutdown, title matching, add/delete rescanning, delayed artwork without startup blocking, cancellation, shared artwork, bounded concurrency, offline cooldown, welcome without a folder, and settings without a game. HTTP tests use deterministic injected replies, not live artwork services. Artwork test caches are isolated from the normal application cache.

Remaining validation: Windows MSVC/Qt 6.10.2 compilation and desktop interactions; actual Internet artwork providers; controller hardware; real-game launch times and rendering. Local socket integration is blocked by this environment and is not counted as a pass. Existing unused-helper/compiler and offscreen-platform warnings remain. The legacy command-line artwork-cache helper remains available; the launcher now uses its new in-process queue instead.

## Windows build (your existing toolchain)

Close GBAx. Extract the archive and copy the contents of its gbax folder into C:\Users\shawn\GBAx. Preserve your ROMs, saves, artwork and dependency DLLs. Use CMD or the x64 Native Tools Prompt. A working build directory does not need to be deleted.

```bat
cd /d C:\Users\shawn\GBAx
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DBUILD_QT=ON -DBUILD_SDL=ON -DBUILD_UPDATER=OFF ^
  "-DCMAKE_TOOLCHAIN_FILE=C:\Users\shawn\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  "-DQt6_DIR=C:\Qt\6.10.2\msvc2022_64\lib\cmake\Qt6"
cmake --build build-msvc --config Release --target gbax-qt --parallel 4
copy /Y res\nointro.dat build-msvc\Release\
copy /Y res\gba_catalog_us.json build-msvc\Release\
cd build-msvc\Release
"C:\Qt\6.10.2\msvc2022_64\bin\windeployqt.exe" --release GBAx.exe
```

This archive contains source, not a prebuilt Windows executable. Keep BUILD_SDL=ON for the Qt frontend's existing setup, while building only gbax-qt.

After rebuilding, check startup with your 110-game folder, switching sections during downloads, cancelling a scan/download, adding/removing a ROM, downloading a previously missing cover, and launching/closing/relaunching games. Filename matching is intentionally conservative; use explicit linking or metadata editing for ambiguous titles.

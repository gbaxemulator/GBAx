# Desktop release validation — 28 October 2026

This delivery contains the release source, native build scripts and a tested Linux AppImage candidate. Windows and both Mac architectures have not been built in this environment. No GitHub workflow has been run and no public release or website download has been published.

## Completed

- Native Linux Release compilation: Qt 6.4.2, GCC 13.3, Ubuntu 24.04 libraries; static emulator core with dynamic Qt/SDL dependencies.
- CTest: **25/25 test groups passed**, including **18/18 Qt player/launcher checks**. The final run completed in 39.21 seconds; see `ctest-linux.txt`.
- Packaging helper tests: **3/3 passed**. These cover required feature detection, changed-package checksums and blocking assembly when an OS package or runtime check is missing/failing.
- `build-linux.sh` completed end to end, producing `GBAx-2.0.0-linux-x86_64.AppImage` (43,583,992 bytes).
- The packaged AppImage passed all eight `--package-check` checks with development library/plugin paths removed: TLS backend availability, PNG/JPEG format support, catalogue data, ROM identification database, shader resources, writable artwork storage and compiled SDL controller support. See `linux-package-check.json` and `linux-SHA256SUMS.txt`.
- Both shell scripts passed Bash syntax checks; Python helpers compiled; the workflow YAML parsed and its OS matrix/action pins were checked. PowerShell and macOS scripts still require their native runs.

## Changes needed for distribution

- New covers and thumbnails use the writable user configuration directory. Existing artwork beside older installations remains readable. Signed Mac bundles and read-only Linux AppImages no longer need to be modified by artwork downloads.
- Mac file-open events create a player when none exists, instead of indexing an empty window list. The regression test verifies actual emulated frames before closing the player.
- SDL support can be enabled for the Qt frontend without building the separate SDL executable.
- Mac deployment targets are consistent across the build; the bundle icon case and resource installation paths are corrected. The Linux desktop entry points to the packaged executable.
- Packaging normalizes executable permissions after dependency deployment, bundles resources/notices and validates the deployed runtime before generating checksums.
- The narrow-window layout test waits for the existing resize debounce timer instead of using a fixed sleep. Its layout assertions remain in place.

## Still required before website publication

1. Run the hosted Windows, Linux, Apple Silicon and Intel Mac jobs. The assembly job refuses incomplete sets.
2. Test gameplay, audio, physical controllers, graphics/fullscreen, save/load and first-run behaviour on each actual desktop OS. Headless tests do not establish hardware compatibility or game-library accuracy.
3. Produce signed/notarized Mac packages for the public download page. The first workflow run may use unsigned Mac packages for testing. Windows Authenticode signing is not included in this initial workflow.
4. Publish matching source, dependency notices and checksums alongside the verified binaries.

The initial baseline is Windows x64; Ubuntu 24.04/glibc 2.39+ x86_64; and macOS 15+ on Apple Silicon or Intel. See the root `RELEASE_BUILD.md` for commands, credentials setup and the manual test checklist.

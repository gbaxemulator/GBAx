# GBAx desktop downloads

This source includes a release workflow for Windows x64, Linux x86_64, macOS Apple Silicon and macOS Intel. Run all native jobs before treating the four packages as website-ready. A local Linux check is not a Windows or Mac test.

## Run the hosted builds

1. Put this source tree at the root of your GitHub repository. Include `.github/workflows/release.yml`, `src`, `include`, `res` and `tools`; do not include your build folders, ROM library, saves or personal configuration.
2. Open **Actions → Build desktop downloads → Run workflow**.
3. For the first run, leave Mac signing and draft-release creation unchecked. This builds unsigned Mac test packages without needing an Apple account.
4. When every job succeeds, download **GBAx-website-downloads** from the workflow artifacts. The assembly job checks that all four packages, matching source and passing runtime reports are present.
5. Configure Apple signing as described below, then run again with **Sign and notarize both Mac packages** enabled. Optionally create a **draft** GitHub release. The workflow never publishes a release automatically.
6. Test the packages on their actual desktop systems before publishing the draft or copying the downloads to your website.

Changes to source, resources or build scripts on `main` also run the unsigned builds automatically. Signing and draft-release creation are explicit options in **Run workflow**. Hosted runner usage is subject to your GitHub plan.

For the results of the checks actually run on this source delivery, see `tools/release/validation/STATUS.md`. Build scripts for a platform are not a substitute for a passing native build and a desktop test on that platform.

## Expected downloads and support baseline

| File | Intended system |
| --- | --- |
| `GBAx-2.0.0-windows-x64.zip` | Windows 10/11, 64-bit Intel/AMD |
| `GBAx-2.0.0-linux-x86_64.AppImage` | Ubuntu 24.04 or a compatible glibc 2.39+ desktop |
| `GBAx-2.0.0-macos-arm64.dmg` | macOS 15+, Apple Silicon |
| `GBAx-2.0.0-macos-x86_64.dmg` | macOS 15+, Intel |
| `GBAx-2.0.0-source.zip` | Matching source for this release |
| `SHA256SUMS.txt`, `downloads.json` | Integrity checks and website download metadata |

Unsigned Mac builds have `-unsigned` before `.dmg`. Those are for testing; they do not provide the normal notarized public installation experience. The Windows ZIP is also unsigned; Authenticode signing can be added separately.

Qt itself can target macOS 13, but this initial workflow deliberately uses macOS 15 as its baseline because its native Homebrew dependencies are built on macOS 15 runners. Lowering a CMake flag alone does not make those dependencies compatible with older Macs. Linux uses Ubuntu's Qt 6.4.2 packages, while Windows and Mac use Qt 6.10.2; these are explicit platform choices, not a claim that the same binary runs everywhere.

## Local builds

Use a fresh release build directory. The scripts refuse to reuse an existing `package/AppDir` or `package/dmg` staging directory so stale dependencies cannot silently ship. Remove only that generated staging directory before repackaging. Your normal Windows `build-msvc` directory is unaffected.

### Windows

With Visual Studio 2022 C++ tools, CMake, Python, Qt and vcpkg installed, install the runtime dependencies:

```powershell
C:\Users\shawn\vcpkg\vcpkg.exe install sdl2:x64-windows libzip:x64-windows libpng:x64-windows sqlite3:x64-windows zlib:x64-windows libepoxy:x64-windows
```

Then, from the source directory in PowerShell:

```powershell
./tools/release/build-windows.ps1 -QtRoot 'C:\Qt\6.10.2\msvc2022_64' -VcpkgRoot 'C:\Users\shawn\vcpkg'
```

The script builds the Qt frontend, copies runtime DLLs and resources, runs `windeployqt`, checks the deployed program and produces the ZIP in `dist`. Extract the entire ZIP when testing it; copying only GBAx.exe is insufficient. It includes SDL support without building the separate SDL frontend.

### Linux

On Ubuntu 24.04, install the packages listed in the workflow's **Install Linux dependencies** step, then run:

```bash
bash tools/release/build-linux.sh
chmod +x dist/GBAx-2.0.0-linux-x86_64.AppImage
./dist/GBAx-2.0.0-linux-x86_64.AppImage
```

The packager uses pinned linuxdeploy and Qt-plugin releases with SHA-256 verification. It does not require FUSE while building. Users whose system lacks FUSE can use `APPIMAGE_EXTRACT_AND_RUN=1 ./GBAx-2.0.0-linux-x86_64.AppImage`.

### macOS

Install Xcode, CMake/Ninja, Python, Qt 6.10.2 and the Homebrew packages listed in the workflow. Set `QT_ROOT_DIR` to your Qt `macos` directory, then run:

```bash
export QT_ROOT_DIR="$HOME/Qt/6.10.2/macos"
bash tools/release/build-macos.sh
```

Build on each architecture, or use the two hosted Mac runners. Dependencies are architecture-specific; this workflow does not pretend two independent builds are a universal binary. The script deploys the app bundle, checks it, then produces a drag-to-Applications DMG.

## Apple signing

Add these **GitHub Actions repository secrets**; do not paste certificates or passwords into source files or chat:

- `APPLE_CERTIFICATE_P12_BASE64`: base64-encoded export of your Developer ID Application certificate and private key.
- `APPLE_CERTIFICATE_PASSWORD`: password protecting that export.
- `APPLE_SIGNING_IDENTITY`: full `Developer ID Application: ... (TEAMID)` identity.
- `APPLE_ID`: Apple developer account email.
- `APPLE_APP_PASSWORD`: Apple app-specific password for notarization.
- `APPLE_TEAM_ID`: developer team ID.

The signed job uses a temporary keychain, signs the deployed bundle and DMG, submits the DMG with `notarytool`, staples the ticket and validates it. Missing secrets or a failed signing/notarization step fail the job. Cleanup restores the original keychain search list and removes the temporary signing material.

## What is included

GBA/GB/GBC emulation, SDL audio/controllers, ZIP/7z ROM loading, save states, screenshots, library scanning and background artwork are included. The initial portable profile disables automatic updates, FFmpeg recording, Lua scripting and Discord presence. Those optional integrations require their own dependency/packaging checks before enabling them. Normal local builds retain their existing defaults.

New artwork is stored under the user's GBAx configuration directory, outside the installed bundle. Existing covers from older builds remain readable in their old location. Game/save paths are not migrated or deleted by this update. Do not enable portable mode inside a mounted read-only AppImage or signed application bundle.

## Website handoff

Host the exact files from **GBAx-website-downloads**. Provide Windows, Linux and macOS download buttons; under macOS provide separate Apple Silicon and Intel choices. Link matching source and SHA256SUMS.txt alongside the binaries. Keep included license and dependency notices with the application.

The build adds no games or Nintendo BIOS files. It does not prefill a user's ROM folder or upload any saves. The `downloads.json` status remains `candidate` until the release is reviewed; it contains filenames, byte sizes and SHA-256 hashes, not invented website URLs.

Before the first public release, check on each system: first-run setup; a ROM folder with subfolders and ZIP/7z archives; covers downloading without scrolling; one game with audio; save/load; controller reconnect; fullscreen; closing and reopening the launcher. On Linux check X11 and Wayland. On Mac test a browser-downloaded notarized DMG, not just the unsigned build directory. Test on a machine without Qt/vcpkg/Homebrew development libraries as well.

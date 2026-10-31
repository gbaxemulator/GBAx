<p align="center">
  <a href="https://gbax-emulator.com/">
    <img src="https://gbax-emulator.com/wp-content/uploads/2026/10/cropped-gbax-logo-official-2.webp" alt="GBAx — Game Boy Advance Emulator" width="500">
  </a>
</p>

<h1 align="center">GBAx — Game Boy Advance Emulator</h1>

<p align="center">
  <strong>Classic Handheld. Modern Experience.</strong>
</p>

<p align="center">
  A modern desktop emulator with a visual game library, controller-friendly navigation and convenient tools for enjoying your collection.
</p>

<p align="center">
  <a href="https://gbax-emulator.com/">Website</a> ·
  <a href="https://github.com/emucores/GBAx/releases">Downloads</a> ·
  <a href="https://gbax-emulator.com/docs/">Documentation</a> ·
  <a href="https://gbax-emulator.com/play-online/">Play Online</a> ·
  <a href="https://github.com/emucores/GBAx/issues">Report an Issue</a>
</p>

<p align="center">
  <a href="https://github.com/emucores/GBAx/releases">
    <img src="https://img.shields.io/github/v/release/emucores/GBAx?include_prereleases&label=Release&color=9333ea" alt="Latest release">
  </a>
  <a href="LICENSE">
    <img src="https://img.shields.io/badge/License-MPL--2.0-9333ea" alt="License: MPL 2.0">
  </a>
  <img src="https://img.shields.io/badge/Platforms-Windows%20%7C%20macOS%20%7C%20Linux-272032" alt="Windows, macOS and Linux">
</p>

---

## Overview

**GBAx** brings Game Boy Advance, Game Boy and Game Boy Color emulation into a desktop experience designed around your collection.

Browse cover artwork, organise favourites, revisit recently played games and launch directly from your library. Controller navigation and an in-game quick menu keep common actions within easy reach.

GBAx is developed under **[EmuCores](https://github.com/emucores)**.

> **Preview Release**
>
> GBAx is currently available as preview software. Features and platform requirements depend on the release. Please read the release notes and back up your progress before changing builds.

## Screenshots

<p align="center">
  <img src="https://gbax-emulator.com/wp-content/plugins/gbax-website/assets/img/library.webp" alt="GBAx desktop launcher showing its visual game library" width="1000">
</p>

## Features

### Your Collection, Organised

- **Visual Game Library** — Browse games with cover artwork and searchable titles.
- **Background Artwork Downloads** — Continue browsing while missing covers load.
- **Favourites** — Keep frequently played games close at hand.
- **Recently Played** — Find your recent sessions quickly.
- **Folder Scanning** — Choose your ROM directory and optionally include subfolders.

### Built Around Playing

- **Controller-Friendly Navigation** — Browse the launcher and select games with a controller.
- **In-Game Quick Menu** — Access common actions during gameplay.
- **Save States** — Save and restore snapshots of your game.
- **Fullscreen Play** — Give your games more room.
- **Configurable Controls** — Adjust keyboard and controller mappings.
- **Graphics and Audio Settings** — Configure presentation and sound.
- **Screenshot Capture** — Capture moments from your games.

### Make It Yours

- Accent themes for the launcher.
- Performance Mode to reduce interface effects.
- Artwork management and library maintenance tools.
- Separate launcher and emulator settings.

Feature availability varies by build. The release notes describe the configuration included in each package.

## Downloads

Get desktop builds from **[GitHub Releases](https://github.com/emucores/GBAx/releases)** or visit the **[GBAx Downloads page](https://gbax-emulator.com/downloads/)**.

### Preview 1 Packages

| Platform | Package | Requirements |
| --- | --- | --- |
| Windows | ZIP | Windows 10/11, x64 |
| Linux | AppImage | x86_64; Ubuntu 24.04 or compatible glibc 2.39+ |
| macOS — Apple Silicon | DMG | macOS 15+, ARM64 |
| macOS — Intel | DMG | macOS 15+, x86_64 |

**Windows:** Extract the complete archive and launch `GBAx.exe`. Keep the included DLLs and folders alongside the executable.

**Linux:** Make the AppImage executable, then launch it. If FUSE is unavailable, use `APPIMAGE_EXTRACT_AND_RUN=1`.

**macOS:** Download the package matching your processor and move GBAx into Applications. Preview packages are ad-hoc signed and are not Developer ID signed or notarized.

Release assets include matching source archives, dependency notices, package-check reports and checksums.

**Game ROMs and BIOS files are not included in the desktop packages.**

## Getting Started

1. Download the package for your operating system.
2. Extract or install it, then launch GBAx.
3. Select the folder containing your game files.
4. Scan your library and allow cover artwork to load.
5. Configure your keyboard or controller in Settings.
6. Select a game and start playing.

See the **[Getting Started Guide](https://gbax-emulator.com/docs/getting-started/)** for more detail.

## Saves and Progress

Use the game’s own save system for regular progress and save states for convenient snapshots.

Keep backups before updating, moving between devices or changing emulators. Save states can depend on the emulator version and should not be assumed interchangeable.

The desktop application and browser player maintain separate storage. Moving progress between them requires an explicit export/import workflow using compatible save files.

## GBAxJS — Browser Play

**GBAxJS** brings the GBAx experience to your browser, with quick game loading, a personal browser library and downloadable save backups.

The current preview includes:

- Local game-file loading.
- Keyboard, touch and controller mapping, subject to device support.
- Pause, sound and fullscreen or expanded-view controls.
- A compact mobile toolbar.
- Hide Menus mode for a clearer gameplay view.
- My Library with favourites, recent games and collections.
- In-game save backups and compatible save-state backups.
- Backup import/export and safety copies before supported restores.

Browser progress is stored locally on the device. Download backups regularly, particularly before clearing browser data or switching devices.

GBAxJS remains in preview, with mobile compatibility and background-session recovery under active testing.

**[Explore Browser Play →](https://gbax-emulator.com/play-online/)**

## Documentation

The **[GBAx Documentation](https://gbax-emulator.com/docs/)** covers everyday use and the implementation behind the emulator.

| Topic | What You Will Find |
| --- | --- |
| Getting Started | Installation, setup and launching your first game |
| Launcher and Library | Folder scanning, artwork and interface behaviour |
| Controllers | Launcher navigation and in-game mappings |
| Saves and Backups | Game saves, save states and progress management |
| Hardware Emulation | CPU execution, memory, rendering, audio and timing |
| Building From Source | Platform-specific development instructions |
| Troubleshooting | Common setup and gameplay problems |

## Building From Source

Clone the repository:

```bash
git clone https://github.com/emucores/GBAx.git
cd GBAx
```

Follow the instructions for your platform:

- **[Windows Build Guide](https://gbax-emulator.com/docs/build-windows/)**
- **[macOS Build Guide](https://gbax-emulator.com/docs/build-macos/)**
- **[Linux Build Guide](https://gbax-emulator.com/docs/build-linux/)**

See [RELEASE_BUILD.md](RELEASE_BUILD.md) for repository-specific release build instructions.

Dependencies and build options vary by platform. Use the guide matching the version you intend to build.

## Reporting Issues

Please report reproducible problems through **[GitHub Issues](https://github.com/emucores/GBAx/issues)**.

Include:

- GBAx version and package name.
- Operating system and processor architecture.
- Game title, region and revision, where relevant.
- Steps to reproduce the problem.
- Expected behaviour and what actually happened.
- Controller model and connection type for input issues.
- Relevant logs or screenshots.

For browser-player issues, also include the browser version, device model and whether the problem occurs in embedded or expanded view.

Do not attach commercial ROMs, BIOS files or personal information.

## Contributing

Contributions are welcome, including bug reports, documentation improvements, compatibility testing and code changes.

Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a pull request. For substantial changes, open an issue first to discuss the approach.

## Technical Foundation and Credits

GBAx builds on the **[mGBA](https://mgba.io/)** emulation foundation and its contributors’ work on Game Boy Advance, Game Boy and Game Boy Color hardware emulation.

The desktop application uses Qt-based frontend components alongside GBAx’s launcher and library features.

The current GBAxJS preview integrates **[EmulatorJS](https://emulatorjs.org/)** with an mGBA WebAssembly core. GBAxJS provides the surrounding player interface, library integration and backup workflow.

We acknowledge the upstream developers, maintainers, researchers and contributors whose work makes these projects possible. Upstream components retain their respective licenses and notices.

## License

This repository is distributed under the **Mozilla Public License 2.0**. See [LICENSE](LICENSE) for the full terms.

Third-party dependencies and browser-runtime components are governed by their own licenses. Consult their accompanying notices and source repositories.

GBAx is an independent project and is not affiliated with or endorsed by Nintendo. Product names and trademarks belong to their respective owners.

---

<p align="center">
  <strong>GBAx — Your Games. Your Library. Your Way to Play.</strong>
  <br>
  Developed under <a href="https://github.com/emucores">EmuCores</a>
</p>

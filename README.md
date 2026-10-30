<p align="center">
  <a href="https://gbax-emulator.com/">
    <img src="https://gbax-emulator.com/wp-content/uploads/2026/10/cropped-gbax-logo-official-2.webp" alt="GBAx — Game Boy Advance Emulator" width="460">
  </a>
</p>

<h1 align="center">GBAx — Game Boy Advance Emulator</h1>

<p align="center">
  <strong>Classic handheld. Modern experience.</strong>
</p>

<p align="center">
  <a href="https://gbax-emulator.com/">Website</a> ·
  <a href="https://github.com/emucores/GBAx/releases">Downloads</a> ·
  <a href="https://gbax-emulator.com/docs/">Documentation</a> ·
  <a href="https://github.com/emucores/GBAx/issues">Report a bug</a>
</p>

---

## About GBAx

GBAx combines Game Boy Advance emulation with a modern desktop launcher, a visual game library and controller-friendly navigation.

Built on the **mGBA emulation foundation**, GBAx brings established Game Boy Advance, Game Boy and Game Boy Color emulation together with tools for organising your collection, downloading cover artwork and getting back into your favourite games.

Developed under **[EmuCores](https://github.com/emucores)**.

> **GBAx 2.0.0 Preview 1 is available now.**
> Download desktop packages for Windows, Linux and macOS. This is a preview release, and feedback from real-world gameplay and controller testing helps guide development.

## Screenshots

<p align="center">
  <img src="https://gbax-emulator.com/wp-content/plugins/gbax-website/assets/img/library.webp" alt="GBAx launcher displaying its visual game library" width="960">
</p>

## Features

### Your collection, organised

- **Visual game library** with cover artwork and searchable game titles.
- **Automatic artwork downloads** that run in the background while you browse.
- **Favourites** for quick access to the games you love.
- **Recently played games** to help you pick up where you left off.
- **ROM folder management** with library scanning and optional subfolder scanning.

### A comfortable desktop experience

- **Controller-friendly navigation** for browsing your library and launching games.
- **In-game quick menu** for common actions without returning to the launcher.
- **Save and load states** to return to a saved moment.
- **Fullscreen support** for an uninterrupted view of your game.
- **Configurable keyboard and controller mappings**.
- **Graphics and audio settings** to adjust your setup.
- **Accent themes and launcher performance mode**.
- **Screenshot capture**.

### An established emulation foundation

- Game Boy Advance, Game Boy and Game Boy Color emulation through the mGBA-based core.
- Hardware emulation covering CPU execution, memory, video, audio and timing.
- ZIP and 7z archive support in the current preview packages.

Feature availability depends on the build. See the release notes for the configuration shipped with each version.

## Downloads

Download GBAx from:

- **[GitHub Releases](https://github.com/emucores/GBAx/releases)**
- **[GBAx Downloads](https://gbax-emulator.com/downloads/)**

### Preview 1 packages

| Platform | Package | Requirements |
| --- | --- | --- |
| Windows | ZIP archive | Windows 10/11, x64 |
| Linux | AppImage | x86_64; Ubuntu 24.04 or compatible glibc 2.39+ |
| macOS — Apple Silicon | DMG | macOS 15+, ARM64 |
| macOS — Intel | DMG | macOS 15+, x86_64 |

### Windows

Extract the complete ZIP archive, then run `GBAx.exe`.

Keep the bundled DLLs and folders alongside the executable.

### Linux

Mark the AppImage executable and launch it.

If FUSE is unavailable, the package supports running with `APPIMAGE_EXTRACT_AND_RUN=1`.

### macOS

Download the package matching your processor and drag GBAx into Applications.

The preview packages are ad-hoc signed test builds. They are not Apple Developer ID signed or notarized, so macOS may block them or display a security warning.

### Release contents

Release assets include matching source code, dependency notices, package-check reports and `SHA256SUMS.txt`.

Games and BIOS files are not included in the executable packages.

## Getting started

1. Download the package for your operating system.
2. Extract or install it and launch GBAx.
3. Choose the folder containing your game files.
4. Scan your library and let cover artwork load.
5. Configure your keyboard or controller in Settings.
6. Select a game and start playing.

For a walkthrough, see the **[Getting Started guide](https://gbax-emulator.com/docs/getting-started/)**.

Back up your save files before changing builds. Save states may not be interchangeable between versions or different emulators.

## Documentation

Explore the **[GBAx Documentation](https://gbax-emulator.com/docs/)** for setup instructions, troubleshooting and explanations of how the emulator works.

Topics include:

- Getting started and managing your library.
- Controller setup and launcher navigation.
- Saves, save states and backups.
- Launcher architecture and artwork management.
- CPU instruction execution.
- Memory and the system bus.
- Video rendering, backgrounds and sprites.
- Audio channels and timing.
- Timers, DMA and interrupts.
- Cartridges, input and peripherals.
- Building GBAx from source.

### How GBAx works

GBAx uses mGBA's hardware emulation implementation to reproduce the behaviour of the original systems.

The core handles instruction execution, memory access, video, audio and hardware timing. GBAx's launcher and library features provide the surrounding desktop experience: finding games, managing artwork, navigating with a controller and moving between the library and gameplay.

Read more in the **[hardware documentation](https://gbax-emulator.com/docs/)**.

## Build from source

Clone the repository:

```bash
git clone https://github.com/emucores/GBAx.git
cd GBAx
```

Follow the guide for your platform:

- **[Windows build guide](https://gbax-emulator.com/docs/build-windows/)**
- **[macOS build guide](https://gbax-emulator.com/docs/build-macos/)**
- **[Linux build guide](https://gbax-emulator.com/docs/build-linux/)**

For a build matching a published package, use its release tag or matching source archive. The default branch may contain changes made after the release.

## Browser play

The **[GBAx website](https://gbax-emulator.com/)** also provides browser play through **EmulatorJS**.

Browser play is separate from the desktop application. Features, controls and save behaviour may differ between the two.

## Preview status

GBAx 2.0.0 Preview 1 provides native desktop packages for Windows, Linux and macOS.

Automated build and package checks do not replace testing gameplay, audio, controllers, save states and fullscreen behaviour on real devices.

The following features are **disabled in the Preview 1 release profile**:

- Automatic updates.
- FFmpeg recording.
- Lua scripting.
- Discord presence.

Check the **[release notes](https://github.com/emucores/GBAx/releases)** for the features and limitations of each build.

## Report a problem

Found a bug? Open an **[issue](https://github.com/emucores/GBAx/issues)** and include:

- GBAx version.
- Operating system and processor architecture.
- Game title and region, if relevant.
- Steps needed to reproduce the issue.
- What you expected and what actually happened.
- Controller model and connection type for input problems.
- Relevant screenshots or logs, with personal information removed.

Please do not attach copyrighted game files or BIOS images.

## Contributing

Bug fixes, documentation improvements and reproducible test reports are welcome.

Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes. For substantial changes, open an issue first to discuss the approach.

## Credits and licence

GBAx is based on **[mGBA](https://mgba.io/)**, created by **Jeffrey Pfau** and developed by the mGBA contributors.

Their work provides the emulation foundation on which GBAx builds its desktop experience.

The inherited code includes copyright © 2013–2026 Jeffrey Pfau. Existing source-file copyright and licence notices remain applicable to their respective contributions.

The repository includes the **[Mozilla Public License 2.0](LICENSE)**. Third-party components retain their own licences and notices, including:

- [inih](https://github.com/benhoyt/inih) — © 2009–2020 Ben Hoyt; BSD 3-clause licence.
- [LZMA SDK](https://www.7-zip.org/sdk.html) — public domain.
- [MurmurHash3](https://github.com/aappleby/smhasher) — Austin Appleby; public domain.
- [getopt for MSVC](https://github.com/skandhurkat/Getopt-for-Visual-Studio/) — public domain.
- [SQLite](https://www.sqlite.org/) — public domain.

See the source and release dependency notices for additional components and their terms.

GBAx is an independent project and is not affiliated with Nintendo. Game titles, artwork and trademarks belong to their respective owners.

---

<p align="center">
  <strong>GBAx — Classic handheld. Modern experience.</strong><br>
  <a href="https://gbax-emulator.com/">gbax-emulator.com</a> ·
  <a href="https://github.com/emucores">EmuCores</a>
</p>

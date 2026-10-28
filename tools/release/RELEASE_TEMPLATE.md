GBAx 2.0.0 desktop release candidate.

- Windows 10/11 x64: extract the complete ZIP and run GBAx.exe.
- Linux x86_64: Ubuntu 24.04 or a compatible glibc 2.39+ desktop; make the AppImage executable and run it.
- macOS 15 or newer: choose arm64 for Apple Silicon or x86_64 for Intel, then drag GBAx into Applications.
- Files ending in -unsigned.dmg are test builds and are not notarized public releases.
- Matching source, dependency notices, SHA-256 checksums and a downloads manifest are included.

Before publishing this draft: confirm the actual Windows/Mac build results and hardware testing, and use notarized Mac packages for the public download buttons.

This portable build includes GBA/GB/GBC emulation, SDL audio/controllers, ZIP/7z support, artwork, screenshots and save states. Automatic updates, FFmpeg recording, Lua scripting and Discord presence are disabled in this release profile.

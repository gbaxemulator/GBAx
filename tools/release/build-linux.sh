#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="${GBAX_BUILD_DIR:-$root/build-release-linux}"
dist="${GBAX_DIST_DIR:-$root/dist}"
cmake -S "$root" -B "$build" -G Ninja -C "$root/tools/release/profile.cmake" \
  -DCMAKE_INSTALL_PREFIX=/usr -DDATADIR=../share/gbax "$@"
cmake --build "$build" --target gbax-qt --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
python3 "$root/tools/release/release.py" check-cache "$build/CMakeCache.txt"
stage="$build/package/AppDir"
if [[ -e "$stage" ]]; then echo "Remove only the old staging directory before repackaging: $stage" >&2; exit 1; fi
mkdir -p "$stage/usr/bin" "$stage/usr/share/applications" "$stage/usr/share/icons/hicolor/256x256/apps" "$dist"
cp "$build/qt/gbax-qt" "$stage/usr/bin/"
chmod 755 "$stage/usr/bin/gbax-qt"
cp "$root/res/gbax-qt.desktop" "$stage/usr/share/applications/io.gbax.GBAx.desktop"
cp "$root/res/gbax-256.png" "$stage/usr/share/icons/hicolor/256x256/apps/io.gbax.GBAx.png"
python3 "$root/tools/release/release.py" resources "$stage/usr/share/gbax" --notices "${GBAX_NOTICE_ROOT:-/usr/share/doc}"
desktop-file-validate "$stage/usr/share/applications/io.gbax.GBAx.desktop"
python3 "$root/tools/release/release.py" tools "$build/package/tools"
export QMAKE="${QMAKE:-$(command -v qmake6)}"
export APPIMAGE_EXTRACT_AND_RUN=1
# Offscreen is used only for packaging checks. X11 and Wayland are deployed.
export EXTRA_PLATFORM_PLUGINS="libqoffscreen.so;libqwayland-egl.so;libqwayland-generic.so"
export DEPLOY_COPYRIGHT_FILES=1
version="$(python3 "$root/tools/release/release.py" version)"
export OUTPUT="$dist/GBAx-$version-linux-x86_64.AppImage"
"$build/package/tools/linuxdeploy-x86_64.AppImage" --appdir "$stage" --plugin qt
# Dependency patching/stripping may replace the executable. Normalize the final
# staged permissions before making the read-only filesystem image.
chmod 755 "$stage/usr/bin/gbax-qt" "$stage/AppRun"
output_tools="$build/package/tools/output"
mkdir -p "$output_tools"
(cd "$output_tools" && ../linuxdeploy-x86_64.AppImage --appimage-extract >/dev/null)
"$output_tools/squashfs-root/plugins/linuxdeploy-plugin-appimage/usr/bin/linuxdeploy-plugin-appimage" --appdir "$stage"
chmod 755 "$OUTPUT"
# Do not accidentally validate against the development machine's Qt plugins.
env -u QT_PLUGIN_PATH -u LD_LIBRARY_PATH -u QML2_IMPORT_PATH \
  QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy \
  "$OUTPUT" --package-check | tee "$dist/linux-package-check.json"
python3 "$root/tools/release/release.py" manifest "$dist"

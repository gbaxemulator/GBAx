#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="${GBAX_BUILD_DIR:-$root/build-release-macos}"
dist="${GBAX_DIST_DIR:-$root/dist}"
arch="$(uname -m)"
case "$arch" in arm64|x86_64) ;; *) echo "Unsupported Mac architecture: $arch" >&2; exit 1;; esac
: "${QT_ROOT_DIR:?Set QT_ROOT_DIR to the Qt 6.10.2 macos installation}"
prefixes="$QT_ROOT_DIR;$(brew --prefix sdl2);$(brew --prefix libzip);$(brew --prefix libpng);$(brew --prefix sqlite);$(brew --prefix zlib)"
cmake -S "$root" -B "$build" -G Ninja -C "$root/tools/release/profile.cmake" \
  -DCMAKE_PREFIX_PATH="$prefixes" -DCMAKE_OSX_ARCHITECTURES="$arch" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DUSE_EPOXY=OFF "$@"
cmake --build "$build" --target gbax-qt --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
python3 "$root/tools/release/release.py" check-cache "$build/CMakeCache.txt"
stage="$build/package/dmg"
if [[ -e "$stage" ]]; then echo "Remove only the old staging directory before repackaging: $stage" >&2; exit 1; fi
mkdir -p "$stage" "$dist"
ditto "$build/qt/GBAx.app" "$stage/GBAx.app"
app="$stage/GBAx.app"
python3 "$root/tools/release/release.py" resources "$app/Contents/Resources" \
  --notices "$(brew --prefix sdl2)" --notices "$(brew --prefix libzip)" \
  --notices "$(brew --prefix libpng)" --notices "$(brew --prefix sqlite)" --notices "$(brew --prefix zlib)"
deploy_options=(-always-overwrite)
suffix='-unsigned'
if [[ "${GBAX_SIGN_MACOS:-false}" == true ]]; then
  : "${APPLE_CERTIFICATE_P12_BASE64:?Missing signing certificate}"
  : "${APPLE_CERTIFICATE_PASSWORD:?Missing certificate password}"
  : "${APPLE_SIGNING_IDENTITY:?Missing Developer ID Application identity}"
  : "${APPLE_ID:?Missing Apple ID}"
  : "${APPLE_APP_PASSWORD:?Missing app-specific password}"
  : "${APPLE_TEAM_ID:?Missing team ID}"
  signing_dir="$(mktemp -d)"
  keychain="$signing_dir/release.keychain-db"
  keychain_password="$(openssl rand -hex 24)"
  # Never log signing secrets; the disposable keychain is deleted on any exit.
  cleanup_signing() { security delete-keychain "$keychain" >/dev/null 2>&1 || true; rm -rf -- "$signing_dir"; }
  trap cleanup_signing EXIT
  printf '%s' "$APPLE_CERTIFICATE_P12_BASE64" | base64 --decode > "$signing_dir/certificate.p12"
  security create-keychain -p "$keychain_password" "$keychain"
  security set-keychain-settings -lut 21600 "$keychain"
  security unlock-keychain -p "$keychain_password" "$keychain"
  security import "$signing_dir/certificate.p12" -k "$keychain" -P "$APPLE_CERTIFICATE_PASSWORD" -T /usr/bin/codesign >/dev/null
  security set-key-partition-list -S apple-tool:,apple:,codesign: -k "$keychain_password" "$keychain" >/dev/null
  # Preserve the user's search list as well as the temporary keychain.
  python3 - "$keychain" "$signing_dir/search-list.json" <<'PY'
import json,shlex,subprocess,sys
original=shlex.split(subprocess.check_output(['security','list-keychains','-d','user'],text=True))
open(sys.argv[2],'w').write(json.dumps(original))
subprocess.run(['security','list-keychains','-d','user','-s',sys.argv[1],*original],check=True)
PY
  cleanup_signing() {
    python3 - "$signing_dir/search-list.json" <<'PY'
import json,subprocess,sys
from pathlib import Path
p=Path(sys.argv[1])
if p.exists(): subprocess.run(['security','list-keychains','-d','user','-s',*json.loads(p.read_text())],check=False)
PY
    security delete-keychain "$keychain" >/dev/null 2>&1 || true
    rm -rf -- "$signing_dir"
  }
  deploy_options+=("-sign-for-notarization=$APPLE_SIGNING_IDENTITY")
  suffix=''
fi
"$QT_ROOT_DIR/bin/macdeployqt" "$app" "${deploy_options[@]}"
if [[ -z "$suffix" ]]; then codesign --verify --deep --strict --verbose=2 "$app"; fi
# Check the actual deployed bundle before turning it into a disk image.
env -u QT_PLUGIN_PATH -u DYLD_LIBRARY_PATH QT_QPA_PLATFORM=offscreen \
  "$app/Contents/MacOS/GBAx" --package-check | tee "$dist/macos-$arch-package-check.json"
lipo -verify_arch "$arch" "$app/Contents/MacOS/GBAx"
ln -s /Applications "$stage/Applications"
version="$(python3 "$root/tools/release/release.py" version)"
dmg="$dist/GBAx-$version-macos-$arch$suffix.dmg"
hdiutil create -volname "GBAx $version" -srcfolder "$stage" -ov -format UDZO "$dmg"
if [[ -z "$suffix" ]]; then
  codesign --sign "$APPLE_SIGNING_IDENTITY" --timestamp "$dmg"
  xcrun notarytool submit "$dmg" --apple-id "$APPLE_ID" --password "$APPLE_APP_PASSWORD" --team-id "$APPLE_TEAM_ID" --wait
  xcrun stapler staple "$dmg"
  xcrun stapler validate "$dmg"
fi
python3 "$root/tools/release/release.py" manifest "$dist"

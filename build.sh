#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
task_sdk_path=$(xcrun --show-sdk-path)
task_sources="src/main.c src/macos.c src/protocol.c src/image.c src/files.c src/vli.c src/vli-cli.c"
for task_arch in arm64 x86_64; do
    xcrun clang -arch "$task_arch" -isysroot "$task_sdk_path" -mmacosx-version-min=12.0 \
      -std=c11 -O2 -Wall -Wextra -Werror -Wno-deprecated-declarations \
      $task_sources -framework IOKit -framework CoreFoundation -lz \
      -o "build/spectrum-updater-$task_arch"
done
xcrun lipo -create build/spectrum-updater-arm64 build/spectrum-updater-x86_64 -output build/spectrum-updater
codesign --force --sign - build/spectrum-updater
mkdir -p "build/Spectrum Updater.app/Contents/MacOS" "build/Spectrum Updater.app/Contents/Resources"
cp Info.plist "build/Spectrum Updater.app/Contents/Info.plist"
cp build/spectrum-updater "build/Spectrum Updater.app/Contents/MacOS/spectrum-updater"
# The public tool imports vendor files; no firmware ships inside the app.
rm -rf "build/Spectrum Updater.app/Contents/Resources/Firmware"
xcrun swiftc -module-cache-path build/module-cache -O -framework AppKit src/SpectrumBrand.swift tools/render_icon.swift -o build/render-icon
build/render-icon build/Spectrum.iconset "build/Spectrum Updater.app/Contents/Resources/Spectrum.icns"
for task_arch in arm64 x86_64; do
    xcrun swiftc -target "$task_arch-apple-macosx13.0" -sdk "$task_sdk_path" -module-cache-path build/module-cache -runtime-compatibility-version none \
      -O -framework AppKit src/App.swift src/SpectrumBrand.swift src/GuidedUI.swift src/UpdateLog.swift src/FirmwareCatalog.swift src/VendorImport.swift src/ManagedBackups.swift -o "build/SpectrumUpdater-$task_arch"
done
sh test-catalog.sh
xcrun lipo -create build/SpectrumUpdater-arm64 build/SpectrumUpdater-x86_64 \
  -output "build/Spectrum Updater.app/Contents/MacOS/SpectrumUpdater"
codesign --force --sign - "build/Spectrum Updater.app/Contents/MacOS/spectrum-updater"
codesign --force --sign - "build/Spectrum Updater.app"
printf 'Built universal macOS app and scaler/hub/PD CLI. Hardware not exercised.\n'

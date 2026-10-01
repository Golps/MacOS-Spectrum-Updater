#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
xcrun clang -std=c11 -g -O1 -Wall -Wextra -Werror -Wno-deprecated-declarations \
 -fsanitize=address,undefined tests/test_vli.c src/vli.c src/image.c src/files.c -lz -o build/test-vli
build/test-vli tests/fixtures/hub06a4.bin tests/fixtures/pd1902.bin tests/fixtures/pd1702.bin > build/usb-protocol-tests.json
xcrun clang -std=c11 -g -O1 -Wall -Wextra -Werror -Wno-deprecated-declarations \
 -fsanitize=address,undefined tests/test_vli_cli.c src/image.c src/files.c -lz -o build/test-vli-cli
build/test-vli-cli resources/Firmware/STOCK_ES07D03_Beta03.bin tests/fixtures/hub06a4.bin \
 tests/fixtures/pd1902.bin tests/fixtures/pd1702.bin build/usb-cli-tests.json > build/usb-cli-test-log.txt 2>&1
xcrun swiftc -module-cache-path build/module-cache -O src/FirmwareCatalog.swift src/VendorImport.swift \
 tests/test_vendor_import.swift -o build/test-vendor-import
build/test-vendor-import > build/vendor-import-tests.json
xcrun clang -std=c11 -Wall -Wextra -Werror tests/test_model_profiles.c -o build/test-model-profiles
build/test-model-profiles > build/model-profile-tests.txt
cat build/usb-protocol-tests.json build/usb-cli-tests.json build/vendor-import-tests.json build/model-profile-tests.txt

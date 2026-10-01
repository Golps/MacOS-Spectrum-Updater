#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
xcrun swiftc -module-cache-path build/module-cache -O src/FirmwareCatalog.swift src/ManagedBackups.swift \
  tests/test_managed_backups.swift -o build/test-managed-backups
build/test-managed-backups > build/managed-backup-tests.json
xcrun clang -std=c11 -g -O1 -Wall -Wextra -Werror -Wno-deprecated-declarations \
  -fsanitize=address,undefined tests/test_auto_model.c src/image.c src/files.c -lz -o build/test-auto-model
build/test-auto-model tests/fixtures/v108.bin \
  resources/Firmware/STOCK_ES07D03_Beta03.bin build/auto-model-tests.json \
  tests/fixtures/d02-v101.bin tests/fixtures/d02-beta01.bin tests/fixtures/dc9-v101.bin > build/auto-model-test-log.txt 2>&1
cat build/managed-backup-tests.json build/auto-model-tests.json

#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build
xcrun swiftc -module-cache-path build/module-cache -O src/FirmwareCatalog.swift tests/test_catalog.swift -o build/test-catalog
build/test-catalog

#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p build/ui-validation
xcrun swiftc -module-cache-path build/module-cache -O -framework AppKit \
  src/SpectrumBrand.swift src/GuidedUI.swift src/UpdateLog.swift tests/test_guided_ui.swift -o build/test-guided-ui
build/test-guided-ui build/ui-validation > build/ui-validation/test-results.txt
cat build/ui-validation/test-results.txt

xcrun swiftc -module-cache-path build/module-cache -O src/UpdateLog.swift tests/test_update_log.swift -o build/test-update-log
build/test-update-log > build/update-log-tests.json
cat build/update-log-tests.json

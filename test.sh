#!/bin/sh
set -eu
cd "$(dirname "$0")"
task_python=${SP_TEST_PYTHON:-python3}
sh test-catalog.sh
sh test-managed.sh
sh test-ui.sh
sh test-usb.sh
"$task_python" tests/test_images.py
xcrun clang -std=c11 -g -O1 -Wall -Wextra -Werror -Wno-deprecated-declarations \
  -fsanitize=address,undefined tests/test_protocol.c src/protocol.c src/image.c src/files.c \
  -lz -o build/test-protocol
build/test-protocol build/fixtures/beta03.bin > build/protocol-test-log.txt
cat build/protocol-test-log.txt
"$task_python" tests/record_protocol_evidence.py

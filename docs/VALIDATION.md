# Validation evidence and current limits

[← README](../README.md)

## Software validation

These results describe the packaged 1.0 build 15 source and prior unchanged protocol implementations. They were produced with offline simulators, test fixtures, stubbed CLI transports, and offscreen views. No monitor operation was performed to produce this publication package.

| Area | Recorded result | Evidence |
| --- | --- | --- |
| MStar scaler protocol | 21,058,882 assertions | [protocol-tests.json](validation/protocol-tests.json) |
| VIA USB/SPI protocol and planner | 1,536,900 assertions | [usb-protocol-tests.json](validation/usb-protocol-tests.json) |
| Scaler CLI automatic model authorization | 22 scenarios, 215 assertions | [auto-model-tests.json](validation/auto-model-tests.json) |
| USB CLI orchestration | 15 scenarios, 128 assertions | [usb-cli-tests.json](validation/usb-cli-tests.json) |
| Original vendor package import | 9 ZIPs, 38 assertions | [vendor-import-tests.json](validation/vendor-import-tests.json) |
| Supported model/profile rules | 16 checks | [model-profile-tests.txt](validation/model-profile-tests.txt) |
| Private backup/receipt behavior | 19 assertions | [managed-backup-tests.json](validation/managed-backup-tests.json) |
| Visible log formatter | 13 assertions | [update-log-tests.json](validation/update-log-tests.json) |
| Stock scaler image validation | 7 original scaler containers | [image-tests.json](validation/image-tests.json) |
| Fixed-page light/dark UI and step progression | 524 checks in the packaged build | [release-checks.json](validation/release-checks.json) |

Assertion counts include repeated low-level checks and fault-injection loops; they are not counts of physical devices or independent real-world update sessions.

The new documentation renderer and release archive are checked separately during repository preparation. The stored release evidence is retained as the original sanitized snapshot rather than presented as newly rerun hardware testing.

## Failure handling covered offline

- Corrupt/truncated files, wrong hashes, unknown releases, and incompatible target models.
- Installed-image authorization before write.
- Unsupported silicon/flash geometry and protected SPI state.
- Failed/short setup and transport operations, bounded busy waits, and readback mismatches.
- Dirty erase results, corrupt programming, stale backup comparisons, and invalid shared headers/layout.
- Writes outside a component's permitted range.
- Component entry-chunk/header commit ordering.
- Backup/receipt collisions and durability before writes.
- Partial log lines, path omission, semantic success/error display, stable scrolling, theme contrast, fixed-page fit, and current-step-only emphasis.

CLI tests replace native USB transports with memory-only stubs. ASan/UBSan applies to the relevant C protocol/CLI suites. The UI harness does not launch AppDelegate or discover a monitor.

## What has been physically observed

| Route / unit | Current evidence |
| --- | --- |
| ES07D03 scaler | Prior user-reported successful physical flash, incorporating corrected chip sizing and sequence |
| ES07DC9, ES07E30, ES07D02 scaler | Exact stock images/profile logic validated offline; real update sessions not yet documented |
| VIA hub/PD | Native backend implemented and offline tests passed; real macOS access/update behavior not yet tested |
| Power-loss factory fallback | Not certified by these tests |
| Custom firmware behavior improvement | Not included in this stock release |

A prior D03 update does not establish coverage for every revision or original factory image. A final flash readback does not prove boot success, application of secondary components, calibration, wake timing, or improved HDR/refresh transitions.

## Outstanding work

1. Confirm normal macOS IOKit access to the directly paired hub without driver seizure.
2. Document hub and PD updates on compatible physical units, including backup/readback/restart outcomes.
3. Document scaler updates on each additional model/controller/flash profile.
4. Establish original firmware evidence before adding D01/DCA or other targets.
5. Investigate recovery behavior with model-specific evidence rather than simulator assumptions.
6. Obtain maintainer Developer ID signing/notarization if desired for future public packages.

Use the [hardware-validation issue template](https://github.com/Golps/MacOS-Spectrum-Updater/issues/new/choose) to contribute precise positive or negative results.

# Changelog

## 1.0 — build 15

### App and workflow

- Native universal macOS app for Apple Silicon and Intel; macOS 13+.
- Four explicit older IPS model profiles: ES07D03, ES07DC9, ES07E30, ES07D02.
- Stock vendor ZIP/BIN/USB-EXE import without bundled firmware or executing Windows installers.
- Independent scaler and VIA hub/PD routes, automatic private backups, and readback verification.
- Automatic registry-only USB discovery with a friendly Spectrum monitor label.
- Single-page layout, permanent diagnostics, readable light/dark styling, and original icon.
- Visible logs omit selected paths/commands, color success/error results, and preserve scroll position.
- Unchanged connection polls no longer redraw the controls.
- Step progression starts at Select monitor; Connect remains current until Continue to Install; only the current step is accented.

### Protocol and validation

- D03 C22017/C22018 geometry and corrected vendor ISP sequence retained.
- Explicit installed-image/model authorization and stock-only target policy.
- Separate VIA shared-SPI planning with preserved factory/other-component partitions and bounded failure handling.
- Offline scaler/VIA protocol, CLI, image, importer, backup, and UI evidence included.

### Known limits

- USB hub/PD physical updates and macOS paired-hub access remain unvalidated.
- Physical validation on DC9, E30, and D02 units is not yet documented.
- ES07D01, ES07DCA, OLED families, arbitrary images, and unknown installed firmware are excluded.
- Raw USB whole-chip automatic restoration is not implemented.
- The binary is ad-hoc signed and not Apple-notarized.
- This version installs stock firmware and includes no custom blackout/wake fix.

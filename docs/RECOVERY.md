# Backups, reverting, and recovery

[← README](../README.md) · [Troubleshooting](TROUBLESHOOTING.md)

## What is saved

Before a write, the engine reads the applicable flash contents twice, confirms consistent reads, saves an exclusive backup, durably syncs it and its receipt, and verifies the saved bytes before erase.

The app keeps its backups privately under:

```text
~/Library/Application Support/Spectrum Updater/Backups/
```

USB shared-flash backups use the separate `USB` subdirectory. Firmware import snapshots are temporary and are different from these retained backups. Replacing/removing the app release folder does not remove private backups.

Do not upload backups to this repository. A flash dump can contain calibration, device identity, board configuration, or other unit-specific data. Keep the original file and associated receipt/index records together.

## Scaler backup restoration

A scaler backup covers the complete 4 MiB FW2 window, not the whole scaler chip. Factory FW1 is outside that window and is preserved by normal update/restore operations.

The GUI offers only backups whose integrity and metadata match the selected monitor with a usable unique USB serial number. Placeholder serials are not adequate identification, and the USB port/location is not used as a substitute. Consequently, some units will have retained backups but no GUI restore choice.

A restore is an actual maintenance/write operation. It performs profile checks, creates a new pre-restore backup, restores the FW2 contents, and verifies the written result. It needs stable power/data and the instructed power cycle.

## Reverting to a vendor release

When the monitor is still recognized and readable, select a supported original vendor file for the component you want to revert, review the model and connection, and install it through the ordinary workflow.

Only explicit release/model combinations are accepted. An older firmware filename does not imply that it is a compatible downgrade. Finish each component's power cycle before changing another component.

## USB hub/PD backups

The engine backs up the entire supported shared USB SPI flash before writing a selected component. This preserves evidence and provides data for a reviewed recovery procedure.

**The app does not implement automatic restoration of an entire USB chip.** It does not offer a USB dump as a scaler restore. Where the shared layout is still valid and recognized, reinstalling a supported original hub or PD component is the intended normal route.

Factory/recovery partitions are preserved by the component planner, but power-loss fallback behavior is not certified on physical monitors by the offline tests. Do not describe the retained factory payload as a guarantee that a failed update will recover itself.

## If an operation fails

### Before erase/programming

A rejected image, model, chip, layout, or backup check prevents writing. Record the precise result and follow any session cleanup/power-cycle instructions. Resolve the identified cause rather than bypassing a validation check.

### After erase/programming has begun

Preserve the backup, receipt, and complete diagnostic result. Determine which component/phase failed and whether final readback succeeded. Let the running process finish its error handling before disconnecting anything, then follow its explicit cleanup/power-cycle instructions.

If normal recognition is no longer possible, this app's strict ordinary write path can refuse further work. A monitor-specific recovery procedure may require vendor tools, known factory recovery behavior, or hardware access outside this app's scope. Avoid guessed model selection or a raw whole-chip rewrite.

## Verification versus booting

A hash/readback success establishes byte integrity at the checked location. It does not prove successful boot, correct calibration, application of every secondary component, or improved display behavior. Complete the restart and confirm the component version/normal functionality afterward.

## Recovery research contributions

A useful recovery proposal explains the original layout, affected chip/partition, failure timing, preserved recovery data, and exact procedure used. Report what was observed on a real device versus inferred from code or a simulator. Read [FIRMWARE-RESEARCH.md](FIRMWARE-RESEARCH.md) before proposing new write routes.

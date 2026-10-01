# Troubleshooting

[← README](../README.md) · [Update guide](UPDATE-GUIDE.md) · [Recovery](RECOVERY.md)

## No monitor connection is detected

1. Confirm a USB **data** cable connects the monitor's upstream USB-B or USB-C port to the Mac. HDMI/DisplayPort video alone cannot carry the update protocol.
2. Match the monitor's **Select USB hub source** setting to that upstream port.
3. Use a direct connection rather than a dock, KVM, or external hub.
4. Try **Refresh Connection** after changing the connection.
5. With multiple monitors connected, disconnect the others to simplify selection.

Detection reads the macOS USB registry. It cannot prove compatibility with a listed model before installation's installed-firmware check.

## macOS shows USB Billboard Device

That is the internal USB controller's product name. The updater presents it as **Spectrum monitor** in the main workflow. Select the actual model from the monitor label; do not interpret the USB product string as a different Spectrum model.

## The app will not open

The release app is ad-hoc signed and is not Apple-notarized. After attempting to open it, check System Settings → Privacy & Security for **Open Anyway**, if offered. Do not disable system security globally. Confirm macOS 13 or later and download the app asset, not only the GitHub source archive. You can also build from source.

## The selected file is not recognized

The importer accepts a fixed catalog of original vendor files, ZIP packages, and USB installers. It validates exact size and SHA256; filenames alone are not trusted.

Possible causes include a different vendor release, an incomplete download, a repacked archive, a renamed unrelated image, or modified firmware. Get the original package and check [FIRMWARE.md](FIRMWARE.md). A supported extracted BIN can be accepted even when an unrecognized archive wrapper cannot.

## Firmware does not match the selected model

Use the model on the physical label. Do not try another model to make the file pass. Shared release compatibility is explicit in the catalog; the app does not infer it from similar names.

If the installed scaler image is an unrecognized older factory release, the engine can reject it even when the selected target file and physical model appear appropriate. Report that exact version and public firmware reference instead of bypassing the installed-profile check.

## Unsupported JEDEC chip or geometry

A chip may have a valid ID but no supported geometry/configuration profile. The operation stops before erase when this is detected. Document the reported ID and exact model; do not substitute a guessed flash size. D03 C22017/C22018 profiles already incorporate the earlier chip-size correction.

## Paired hub/controller/layout could not be validated

The hub/PD engine requires the selected billboard's direct compatible hub ancestor, VL822Q7 silicon, supported SPI, consistent headers, and valid PD identity/checksum. An unrelated hub, OS access denial, ambiguous legacy PD layout, SPI protection, or unsupported hardware causes rejection.

These routes still need physical validation. Passing offline tests is not a reason to force them through an unknown controller or layout.

## IOKit USB error or short transfer

The operation can stop when macOS denies access, the device disconnects, a command fails, or the returned length differs from what the protocol requires. Inspect the surrounding log to determine whether writing had begun.

Do not repeatedly retry a transfer failure mid-update or disconnect power while writes are still active. Follow the cleanup/power-cycle message after the process ends. If erase/programming occurred, preserve the backup and logs and follow [Recovery](RECOVERY.md).

## Processing update never appears

V108 Beta 3 contains 10 components and lacks the trailing secondary-update component. It normally does not show that on-screen stage. Larger supported packages, including V108, carry the trailing component and may show it after restart. The presence or absence of the message by itself is not a success/failure test.

## A flash dump differs from the firmware file at its tail

The updater erases only the rounded target image span, not all remaining FW2 bytes. Installing a smaller package over a larger one leaves data beyond that span in place, matching the observed vendor behavior. Compare the image's intended bytes and erase region; don't classify an expected retained tail as corruption.

## The app succeeds but the monitor still shows the old version

Finish the requested 10-second **DC power** cycle. If a secondary update appears, let it finish and power-cycle again. Programming/readback verifies flash bytes; it does not complete the monitor's restart for you. Different components may expose versions in different OSD fields.

The optional comparison re-enters maintenance and reads the selected component; it is not a passive version display. Follow cleanup instructions afterward.

## HDR/120 Hz transitions still go black, or wake remains slow

This updater installs unchanged vendor firmware. It does not contain a custom timing, frame-retention, HDMI handoff, or wake-delay fix. To research these behaviors, share reproducible measurements in a [firmware research proposal](FIRMWARE-RESEARCH.md), including source, input, modes, and timing.

## Backup restoration is unavailable

Many monitors expose a placeholder serial such as `0000000000000001`. A USB location is not a reliable device identity. The app therefore will not offer device-specific scaler backup restore for those units. Backups are still retained privately; use a recognized compatible vendor scaler file to revert when the current firmware remains readable/recognized.

USB backups are not offered as raw whole-chip automatic restore. See [Recovery](RECOVERY.md) for the limits.

## The log reports an error after a successful-looking write

Treat cleanup, receipt, or verification errors as errors. A completed programming phase is not the whole operation. Preserve the complete visible result and note whether final readback passed, whether the session ended, and what restart instructions appeared.

## Filing a useful issue

Include:

- App version/build, macOS version, and Apple Silicon/Intel.
- Monitor label model and current scaler version.
- Target component and exact firmware release.
- Cable route and monitor USB source setting.
- Whether the failure was import, detection, validation, backup, erase, programming, verification, or cleanup.
- The sanitized **Copy Log** output and steps to reproduce.

Review logs before posting. Remove serial numbers, private file paths, account names, and unrelated device information. Do not upload a raw flash dump, vendor binary, or private backup in a public issue.

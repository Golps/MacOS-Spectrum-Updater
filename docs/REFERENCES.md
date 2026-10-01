# Research references and attribution

[← README](../README.md)

## Spectrum protocol research

[niklas389/dough-monitor-tools](https://github.com/niklas389/dough-monitor-tools) provides Spectrum-specific MStar ISP, flash, and firmware-image research. It informed understanding of the scaler update sequence and container behavior, including the difference between a smaller package's intended erase span and retained tail bytes.

Our updater contains independent implementations. Linking this project does not claim that its author reviewed, tested, approved, or endorsed this Mac app.

## VIA hub and PD protocol

[fwupd's VLI plugin](https://github.com/fwupd/fwupd/tree/main/plugins/vli) is a primary reference for VIA vendor-control behavior, hub firmware headers/relocation, and shared SPI PD layouts. Relevant files include `fu-vli-device.c`, `fu-vli-usbhub-device.c`, `fu-vli-usbhub-pd-device.c`, `fu-vli-usbhub-firmware.c`, `fu-vli-pd-firmware.c`, `fu-vli.rs`, and `vli.quirk`.

Upstream authors include VIA Corporation and Richard Hughes. The upstream code is LGPL-2.1-or-later. The C backend in this repository was independently written from protocol facts; it is not a bundled/copied fwupd implementation and does not ship fwupd.

## Vendor installer import

[innoextract](https://github.com/dscharrer/innoextract), by Daniel Scharrer, documents and implements Inno installer extraction. Stream format references informed the native exact-package importer. Upstream uses the zlib license; this app neither embeds nor executes innoextract.

The native importer frames pinned raw LZMA2 payloads following the [XZ file format](https://tukaani.org/xz/xz-file-format.txt), uses macOS Compression for decoding, and validates exact payload hashes afterward.

## Vendor firmware and model information

- [Dough 4K IPS product/download page](https://dough.tech/products/spectrum-4k-144hz).
- [Dough firmware announcement and QHD links](https://www.reddit.com/r/doughcommunity/comments/15etlh9/firmware/).
- [Spectrum One manufacturer specifications](https://files.bbystatic.com/sZx8dgseD2pOvZWD8uQA0g==/Specifcations), covering D03/DC9/E30 finishes, IPS technology, and 144 Hz range.
- [Spectrum Black 27 OLED](https://dough.tech/products/spectrum-black-27-480hz) and [Spectrum Black 32 OLED](https://dough.tech/products/spectrum-black-32), separate families excluded from this tool.

Links can change or disappear. Exact original ZIP and payload hashes are kept in the catalog and test fixture manifest; an unrecognized replacement download is not silently accepted.

## License boundaries

Original updater source/artwork is [MIT licensed](../LICENSE). Vendor firmware is not included in the repository, app, or release asset and remains governed by its own rights/terms. Third-party projects are references, not bundled dependencies. [PROTOCOL.txt](../PROTOCOL.txt) records additional attribution.

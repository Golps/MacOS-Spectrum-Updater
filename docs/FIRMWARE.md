# Supported firmware files and vendor downloads

[← README](../README.md) · [Model matrix](COMPATIBILITY.md)

## Stock-only import

The app accepts exact known vendor payloads and package wrappers. It does not accept an arbitrary file just because it ends in BIN, ZIP, or EXE. Import validates size and SHA256; engine inspection checks the relevant container/header/component integrity before hardware use.

No firmware binary is included in this repository, the app, or its release ZIP. Obtain originals from the vendor. Windows EXEs in the catalog are data inputs only and are never executed.

## Recognized payloads

| Title | Version display | Bytes | Payload SHA256 |
| --- | --- | ---: | --- |
| V108 Beta 3 | D03.V108T | 1344628 | `d9be1e1c7aad81813faa7490a200fe6cfb83d0f371169f4ff94b829ea64c5ce3` |
| V108 | V108 | 2069060 | `136d982c65f8c7615c6f5dd4adc6f9bfa8226f1f877e6e6dcff3d7bb28591757` |
| V106 | V106 | 2056388 | `199bb51c3d31023ffb37542acc250af4bd66cc7059f9a221697ce9c6c5b27ab1` |
| V105 | V105 | 2056020 | `536d94f761d84fa7d8b616dbf5442b81a2d470f2cb7112cba213bc4fbd10add9` |
| Glossy V101 | V101 | 2069060 | `0b5d8350af9997e35b8591b4d5585ae859988129b72b4e7b8d55a3ab68ea56a9` |
| QHD V101 | D02.V101 | 2014868 | `5f0a7bf92119fcc5965f09985d41645a062e86fabd993de86635a515b6e92162` |
| QHD Beta 1 | D02 Beta 1 | 2014868 | `e319da9490df8cec4ff94677385e57a560899a9ef335b6dc7e9b052bf3bcc9f0` |
| USB hub 06A4 | 06A4 | 42556 | `ec6699214c621671449ec941ab4ccd8c413cb79b6e369d99a654c12beb3a3356` |
| USB-C / PD 19.02 | 0A.89.19.02 | 32768 | `62879652a96b8098cb240c4a4290828304afc6709b3948066ccb0d1de49e0fe1` |
| USB-C / PD 17.02 | 0A.89.17.02 | 32768 | `3e3c4c2224c676b1e4971280f5678f65b37ddb8609c3afdfa1df6f72e14b44b1` |

The target-model assignments are documented in [Compatibility](COMPATIBILITY.md) and implemented in `src/FirmwareCatalog.swift` and `src/known-firmware.h`. A checksum-valid file still needs the correct target profile and supported installed firmware/hardware.

## Vendor source pages

- [Dough 4K IPS product/download page](https://dough.tech/products/spectrum-4k-144hz).
- [Dough firmware announcement, including QHD 280 Hz links](https://www.reddit.com/r/doughcommunity/comments/15etlh9/firmware/).

The direct links below are the original archives recorded in the fixture/import catalog. They can be removed or replaced by the hosting service; the app will reject a replacement whose bytes no longer match.

- [v105 original ZIP](https://cdn.shopify.com/s/files/1/0919/4202/7635/files/spectrum_one_es07d03_scaler_fw_105.zip) — archive SHA256 `d95de1784ceb799e92ea5cffaf7acf1aae0a64011f4915bab61e47b464d07cca`.
- [v106 original ZIP](https://cdn.shopify.com/s/files/1/0919/4202/7635/files/spectrum_one_es07d03_scaler_fw_106.zip) — archive SHA256 `f0040fd38cb1785afbf02ec742f8c424b4aaebdeff9a7bffb97f797df7176f91`.
- [v108 original ZIP](https://cdn.shopify.com/s/files/1/0919/4202/7635/files/spectrum_one_es07d03_scaler_fw_108.zip) — archive SHA256 `06eafe9eb83a117a22002832efce255902ccdccefae07a20e5ed44b2d11a19f8`.
- [beta03 original ZIP](https://cdn.shopify.com/s/files/1/0919/4202/7635/files/spectrum_one_es07d03_scaler_fw_1169-Beta03.zip) — archive SHA256 `b227596f273214f00de3176f40fdbbd97d0d6e5216cb1fd02f6bf63b14772db0`.
- [usb1702 original ZIP](https://cdn.shopify.com/s/files/1/0919/4202/7635/files/spectrum_one_usb_fw_06A4-0A.89.17.02.zip) — archive SHA256 `c734affb14d5ca50756beba91d7c511d76f52e91dbc986d9a1d0cbb0dcbca7fa`.
- [usb1902 original ZIP](https://cdn.shopify.com/s/files/1/0919/4202/7635/files/spectrum_one_usb_fw_06A4-0A.89.19.02.zip) — archive SHA256 `13605e7587f2f4269b9d67c0bc253e159566b20395be5979f8a8b8883d14b19b`.
- [dc9-v101 original ZIP](https://cdn.shopify.com/s/files/1/0919/4202/7635/files/spectrum_one_es07dc9_scaler_fw_101.zip) — archive SHA256 `8a09f42099596e3d5f90451db562c0a9fd7c79ba7fa77a7875eb9462db4a7cd9`.
- [d02-v101 original ZIP](https://cdn.shopify.com/s/files/1/0728/3153/3351/files/spectrum_es07d02_scaler_fw_101.zip?v=1692801994) — archive SHA256 `fcb298863617ec1194a63cf54bbd74d77562ebbbabc343e0a6566442c48b7f63`.
- [d02-beta01 original ZIP](https://cdn.shopify.com/s/files/1/0728/3153/3351/files/spectrum_es07d02_scaler_fw_1167_Beta01.zip?v=1694620704) — archive SHA256 `eb6d1dda4791554fdfbcb93f106e88dd95737f33f215780f4f132520efc8a555`.

## ZIP and USB installer behavior

The nine original ZIP packages are hash-pinned, as are the two supported USB installer executables. Extraction uses a private snapshot and bounded output. The extracted component hash must independently match the payload catalog.

A supported USB package loads both hub and PD. Different vendor USB installer versions may carry the same hub image with different PD releases. Choose the intended component from the loaded menu and update one component at a time.

A repacked or newer ZIP wrapper may be rejected even if it contains a known BIN. If you have the original matching extracted BIN, select that file directly; its exact payload hash remains mandatory. Unknown new payloads require a reviewed catalog change, not renaming a file.

## Beta 3 versus V108

V108 Beta 3 is a separate recognized vendor package, not the final V108 payload. It contains 10 components and no trailing `010f` secondary-update component; supported larger packages contain 11. The missing on-screen `Processing update` stage is expected for Beta 3.

The smaller image's erase span stops before the retained tail of a previously larger package. These differences are part of the known package layout and do not alone indicate corruption.

## Version strings

The app version (1.0/build 15), imported firmware display version, and the monitor's reported scaler/USB version are separate values. Importing or programming a file does not immediately change the OSD before restart.

## Distribution rights

Hashes, package metadata, and links are supplied for identification and reproducibility. Vendor firmware remains subject to its own rights/terms. Do not add vendor BINs, installers, private backups, or downloaded test fixture ZIPs to public commits or release assets.

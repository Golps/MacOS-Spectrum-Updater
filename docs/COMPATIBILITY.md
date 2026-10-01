# Compatibility and support boundaries

[← README](../README.md) · [Firmware catalog](FIRMWARE.md)

## Model profiles

| Model | Panel family | Finish | Accepted scaler releases | Physical validation |
| --- | --- | --- | --- | --- |
| ES07D03 | 27-inch IPS, 3840×2160, 144 Hz | Matte | V105, V106, V108, V108 Beta 3 | Reported successful scaler update on one unit |
| ES07DC9 | 27-inch IPS, 3840×2160, 144 Hz | Glossy | Glossy V101, shared V108 Beta 3 | Not yet documented |
| ES07E30 | 27-inch IPS, 3840×2160, 144 Hz | Gorilla Glass | V108, shared V108 Beta 3 | Not yet documented |
| ES07D02 | 27-inch IPS, 2560×1440, 280 Hz | Matte | QHD V101, QHD Beta 1 | Not yet documented |

The four models are shown in the app's model selector. These are exact software profiles, not a blanket guarantee for every hardware revision.

Dough's [Spectrum One specification](https://files.bbystatic.com/sZx8dgseD2pOvZWD8uQA0g==/Specifcations) documents the D03/DC9/E30 finishes, IPS panel, and 144 Hz range. The [vendor firmware announcement](https://www.reddit.com/r/doughcommunity/comments/15etlh9/firmware/) identifies D02 as QHD 280 Hz. Release acceptance follows the implementation's explicit target catalog; the app still requires compatible recognized installed firmware.

## USB hub and PD profiles

| Component | Accepted payload | Required controller | Status |
| --- | --- | --- | --- |
| USB hub | 06A4 | VIA VL822Q7 | Implemented; offline tests passed; hardware update/access validation outstanding |
| USB-C / PD | 0A.89.17.02, 0A.89.19.02 | VIA VL103, recognized shared VL822 layout | Implemented; offline tests passed; hardware update/access validation outstanding |

The catalog associates these with the four IPS model profiles above. The backend additionally requires a directly paired compatible controller, a recognized installed scaler, supported SPI geometry, valid headers/checksums, and a consistent flash layout. Neither the menu selection nor a VIA vendor ID by itself authorizes a write.

## Flash chips and layout

| Route | Supported IDs | Geometry |
| --- | --- | --- |
| Scaler | Macronix C22017, C22018 | 8 MiB or 16 MiB chip; fixed 4 MiB FW2 window at `0x400000–0x800000` |
| USB shared SPI | Macronix C22012–C22014; Winbond EF3012–EF3014 / EF4012–EF4014 | 256 KiB, 512 KiB, or 1 MiB; supported 4 KiB erase/256-byte page geometry |

The chip ID alone is not the complete authorization check. Configuration, protection status, current image, and layout also matter. Other chip IDs/configurations stop before erase.

## Other Spectrum names

Spectrum is a product family, not a single flashing protocol. The older IPS line, historical announced models, and the newer Spectrum Black OLED products must not be treated as interchangeable.

- **ES07D01:** no supported firmware release profile in this build.
- **ES07DCA:** appears in historical documentation/discussion, but no verified dedicated original firmware or cross-model update authorization was obtained for this build.
- **Spectrum Black 27/32 and other OLEDs:** excluded; this repository does not implement their update process. See the vendor's [27-inch OLED](https://dough.tech/products/spectrum-black-27-480hz) and [32-inch OLED](https://dough.tech/products/spectrum-black-32) pages.
- **240 Hz:** not an additional supported IPS model in this catalog. The supported QHD IPS profile is 280 Hz; Spectrum Black 240 Hz is a separate OLED family.
- **Kit/stand/accessory part numbers:** do not establish a new monitor firmware target.

The selector contains only profiles the engine can actually authorize. It intentionally does not add unsupported product names as flashable choices.

## Installed firmware matters

Automatic compatibility identification validates the installed scaler image against known release hashes/profiles. An older factory image outside the catalog, a damaged image, or an unknown custom build can prevent installation even on a listed model.

Some recognized scaler images are shared across compatible models. Those establish a compatible profile, not a uniquely proven physical label. The user must select the model on the monitor itself. Two legacy development image hashes are recognized only as **installed** D03 images to allow migration back to stock; the public importer does not accept custom firmware for installation.

## USB topology matters

macOS exposes the monitor's internal billboard as VID/PID `2109:8886`. For hub/PD, the engine requires a directly paired `2109:2822` USB2 hub ancestor and validates its VL822Q7 silicon. It does not pick an unrelated VIA hub just because it has a matching vendor ID or serial string.

Normal IOKit access is used without driver seizure, a kernel extension, or a root helper. The macOS USB driver may deny access on a real unit; that remains an unvalidated hardware/software interaction and causes the operation to stop.

## Requesting additional support

Open a [research/compatibility proposal](https://github.com/Golps/MacOS-Spectrum-Updater/issues/new/choose) with the exact label model, public vendor firmware reference, available controller/chip information, and a proposed non-destructive validation route. Do not assume that changing the dropdown or adding a chip size is sufficient support.

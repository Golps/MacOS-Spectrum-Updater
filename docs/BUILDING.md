# Building, testing, and packaging

[← README](../README.md)

## Requirements

- macOS with Apple Command Line Tools (`xcode-select --install` if needed).
- The build targets **macOS 13+** for the Swift app and produces both Apple Silicon and Intel binaries.
- Native frameworks: AppKit, Foundation/CryptoKit, Compression, IOKit, CoreFoundation, and system zlib.
- Python **3.10+ recommended** for developer fixture/validation/release tools. Python is not an app runtime dependency.
- No firmware is required to compile the app or run the basic catalog/UI tests.

## Clone and build

```sh
git clone https://github.com/Golps/MacOS-Spectrum-Updater.git
cd MacOS-Spectrum-Updater
sh build.sh
```

Outputs:

```text
build/Spectrum Updater.app
build/spectrum-updater
```

`build.sh` compiles arm64/x86_64 C and Swift, combines the binaries, generates the original icon from `SpectrumBrand.swift`, runs catalog checks, and applies an ad-hoc signature. It never contacts a monitor.

If no vendor fixtures are present, fixture-dependent catalog checks are explicitly skipped; pure catalog/model-policy checks still run. A successful build with skips is not a full protocol-suite result.

## Tests without firmware fixtures

```sh
sh test-catalog.sh
sh test-ui.sh
```

The UI test constructs views offscreen and validates themes, layout, visible logs, fixed page fit, stable log scrolling, and step highlighting. The log unit test covers partial-line handling, sanitization, and semantic status. No app discovery or hardware backend is invoked.

## Full offline test suite

```sh
python3 tools/fetch_test_fixtures.py
sh build.sh
sh test.sh
```

The explicit fixture downloader fetches nine pinned vendor ZIPs, verifies them, and extracts test inputs. Fixtures are gitignored and are not copied into the app or release. Tests do not execute the vendor Windows installers.

For a specific Python installation:

```sh
SP_TEST_PYTHON=/path/to/python3 sh test.sh
```

| Script | Coverage |
| --- | --- |
| `test-catalog.sh` | Release identity, model compatibility, stock-only policy |
| `test-managed.sh` | Private backup/receipt management and scaler CLI model guards |
| `test-ui.sh` | Offscreen light/dark layout, current-step behavior, log display/scrolling |
| `test-usb.sh` | VIA SPI simulation, USB CLI orchestration, native importer, model profiles |
| `test.sh` | Above checks plus stock image corruption and scaler protocol simulations |

C protocol tests use AddressSanitizer and UndefinedBehaviorSanitizer. CLI tests stub the hardware transport and never link/use the real macOS USB open path. Exact counts and scope are in [VALIDATION.md](VALIDATION.md).

## Documentation images

```sh
xcrun swiftc -O -framework AppKit -module-cache-path build/module-cache \
  src/SpectrumBrand.swift src/GuidedUI.swift src/UpdateLog.swift \
  tools/render_docs.swift -o build/render-docs
build/render-docs docs/assets
```

These images render the real presentation views with simulated data. The generator performs no monitor operations or app discovery. They must be captioned accordingly.

## Package a release

```sh
sh build.sh
python3 tools/package_github_release.py --version 1.0
```

The tool writes the app download ZIP, SHA256 checksum file, and a JSON packaging report under gitignored `build/release/`. It verifies version, universal executables, ad-hoc signature, absence of bundled firmware, and absence of local identity/workstation paths. The archive includes the app plus concise start-here and compatibility notices.

Upload the ZIP and checksum file as release assets. The repository source belongs on the branch; do not commit a generated `.app`, vendor fixtures, private backups, or build cache. Tag the source commit used to create the app.

## CI

The default GitHub Actions job builds the universal app, runs fixture-free catalog/UI checks, and uploads the app package and test results as CI artifacts. It never operates a monitor. Fixture-based full validation is available as a separately dispatched job because it needs explicit vendor downloads.

CI artifacts are not automatically published as public releases. Maintainers can run **Publish macOS release** from the Actions page, select the source branch/commit and a matching tag (for example `v1.0.0`), and publish the freshly built app ZIP, checksum, and package report. That workflow uses the repository-scoped Actions token with contents-write permission; it does not require an Apple signing identity or a personal token in the source.

A release tag must match `Info.plist`, and its notes must exist at `docs/releases/<app-version>.md`. An existing release is not silently overwritten. Hardware validation remains a separate activity, regardless of CI status.

## Signing and notarization

Current packages are locally ad-hoc signed. Public Developer ID signing/notarization requires a maintainer's own Apple developer identity and credentials. Those credentials do not belong in this repository. This version does not claim Apple notarization.

## Maintenance notes

Update `Info.plist` app version/build, release notes, the exact firmware catalog, docs, and validation evidence together when behavior changes. New profiles require reviewed image/protocol evidence; UI support alone is insufficient. Keep [CONTRIBUTING.md](../CONTRIBUTING.md) and the hardware results table honest about what has been tested.

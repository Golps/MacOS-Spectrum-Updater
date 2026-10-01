# Contributing

Thanks for helping make Spectrum updates and research more accessible on macOS. Contributions can improve the interface, importer, protocols, compatibility evidence, tests, documentation, and future firmware understanding.

## Start with the right kind of issue

- **Bug report:** reproducible app/import/connection/update problem with sanitized diagnostics.
- **Hardware validation:** real unit/component results that help establish model/controller support.
- **Research proposal:** AI-assisted firmware analysis, a new model route, a component hypothesis, or a recovery investigation.

Read [Compatibility](docs/COMPATIBILITY.md) and [Validation](docs/VALIDATION.md) before describing a route as supported on hardware.

## Development

Clone the repository and follow [BUILDING.md](docs/BUILDING.md). Keep generated build output, vendor test fixtures, backups, serials, and private logs out of Git. Basic UI/catalog checks need no firmware; full protocol checks use explicitly fetched hash-pinned originals.

Use a focused branch and pull request for ordinary contributions. Explain the concrete problem, resulting behavior, and relevant validation. Avoid changing unrelated firmware or geometry profiles while fixing a UI issue.

## What a pull request should include

- A clear problem and before/after behavior.
- Relevant source/documentation changes.
- Tests or reproducible evidence appropriate to the change.
- Exact model/component scope for protocol changes.
- Any remaining hardware validation gaps.
- Updated compatibility/validation docs when a profile or claimed route changes.

A parser/checksum success is not proof of hardware compatibility. A simulator pass is not proof of boot or power-loss recovery. Preserve those distinctions in PRs and release notes.

## Protocol and model changes

Don't add a model to the selector without an engine profile and original firmware/controller evidence. Don't bypass exact image hashes, chip geometry, installed-image authorization, consistent-read checks, backup durability, or final verification to make an unknown target proceed.

New protocols should have pure planning/parsing layers, explicit permitted flash ranges, fault-injection coverage, and clear failure/cleanup behavior. Document required native macOS access and any uncertainty about driver coexistence.

## Firmware research and AI assistance

We welcome your own AI tools for analysis, explanations, tests, and proposed improvements. Follow [FIRMWARE-RESEARCH.md](docs/FIRMWARE-RESEARCH.md). Share reproducible scripts/patches and measured evidence, distinguish hypotheses from facts, and describe recovery assumptions. Disclose material AI assistance when it helps reviewers understand how a result was produced.

Custom firmware is a separate research area; this release's importer remains stock-only. Do not attach a vendor image or a private flash dump to a public issue/PR. Hashes, public references, source patches, and sanitized observations are preferable.

## Hardware validation reports

Include the exact label model, original/current firmware, target component, macOS/architecture, connection route, controller/chip IDs if reported, backup/verification outcome, restart behavior, and post-update functionality. Report negative results too.

Do not turn a community result into a broad certification claim. Tests across other units, factory releases, or hardware revisions may still be needed.

## Licensing and attribution

Contributions to original project code should be compatible with the MIT license. Preserve third-party attribution, and avoid copying code under incompatible terms. Vendor firmware is not part of the repository's MIT license.

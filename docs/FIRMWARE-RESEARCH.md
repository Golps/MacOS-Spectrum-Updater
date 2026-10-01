# Community firmware research

[← README](../README.md) · [Contributing](../CONTRIBUTING.md) · [References](REFERENCES.md)

## An invitation to explore the Spectrum platform

Spectrum's IPS hardware offers a useful platform for display and USB-C research. Community work can help explain input transitions, wake responsiveness, compatibility, and the update process itself. We welcome contributors who use their own AI assistants for source review, reverse engineering, protocol analysis, test generation, and eventually carefully reviewed firmware experiments.

The goal is evidence-backed improvement: document how the existing system works, measure a behavior, propose a specific change, and show the result. AI can accelerate that process, but a model's confidence is not a substitute for correct chip/partition identity or observed hardware behavior.

## What this repository currently ships

The public updater installs **unchanged stock vendor firmware** for its recognized IPS profiles. It does not distribute custom scaler, bridge, hub, or PD images. An earlier development image is not offered by the public importer.

Keep firmware development distinct from updater development. A better Mac flashing workflow and a better monitor firmware are separate projects with different evidence requirements. We welcome research proposals here; a custom firmware installation route would need a separately reviewed design.

## Useful research areas

| Area | Questions worth answering |
| --- | --- |
| Wake responsiveness | Where is the delay between source readiness, link detection, scaler initialization, backlight/panel enable, and visible image? |
| HDR/refresh-rate transitions | Which mode changes cause retraining, pipeline reset, or loss of the on-screen image? How long does each phase take? |
| HDMI/DisplayPort behavior | Which link events, timeouts, bridge states, or EDID/capability decisions differ between sources? |
| USB hub/PD behavior | How do negotiated power, upstream selection, sleep/wake, and hub firmware interact? |
| Package changes | What changed between vendor releases, including component boundaries, staged updates, checksums, and strings? |
| Recovery | Which fallback paths are actually demonstrated on hardware, and what information is needed to restore a specific partition? |
| Update portability | What new model/controller profiles can be established from original vendor evidence? |

Features such as frame retention must be tied to actual documented/identified capabilities. Don't assume a monitor can preserve a frame through a link transition merely because another display appears seamless.

## A practical research workflow

1. **Define the target.** Record the physical model, controller/flash information where known, source device, cable/input, installed firmware, and one measurable symptom.
2. **Establish a baseline.** Reproduce the behavior and measure timing over multiple runs. Separate normal mode changes from a persistent no-signal state requiring power cycling.
3. **Obtain original evidence.** Keep hashes of vendor images, public source links, and a clean description of the extraction or disassembly method.
4. **Map the package.** Identify containers, components, checksums, entry points, relocation/address assumptions, and secondary update behavior.
5. **Propose the smallest justified change.** Name the code/data being changed, explain the intended effect, and describe compatibility assumptions.
6. **Test without writing first.** Use parsers, simulators, static analysis, diff reports, transport traces, and fault injection where applicable.
7. **Review recovery and validation.** Establish the target partition and a credible device-specific restore path; document what is observed versus inferred.
8. **Report outcomes honestly.** Share the patch/build recipe and measurements, including regressions or uncertainty. Never label a checksum-valid image as proven bootable based only on the checksum.

## Using your own AI tools

Good tasks include explaining disassembly with references to offsets/functions, comparing release components, identifying unchecked assumptions, designing negative tests, and writing reproducible analysis scripts.

Provide the assistant with the exact model, hashes, memory map, protocol references, and measured behavior. Ask it to distinguish facts from hypotheses and to preserve the stock baseline. Review generated code and citations yourself; avoid an unreviewed binary with only a confident narrative attached.

Useful deliverables are a text diff, annotated offsets, reproducible build/extraction steps, a simulator case, and before/after measurements. Avoid publishing private flash dumps, personal diagnostic logs, or vendor binaries whose redistribution rights aren't established.

## Submitting a proposal

Use the **Firmware research / compatibility proposal** issue template. Include:

- Objective and exact target model/component.
- Current behavior and reproduction/measurement method.
- Public references and hashes of relevant originals.
- What is established, what is inferred, and what remains unknown.
- Proposed patch or analysis artifact and how it is reproduced.
- Compatibility and recovery assumptions.
- AI assistance used, if material to understanding or reviewing the result.

This repository does not claim endorsement by Eve/Dough, VIA, or the referenced upstream projects. Independent collaboration can still make the platform easier to understand and maintain.

# Project Vision

## Product Intent

OpenAuto Prodigy is a clean-room, open-source replacement for OpenAuto Pro (BlueWave Studio, defunct). It provides a full-featured Android Auto head unit experience on Raspberry Pi hardware, built on Qt 6, C++17, and its own open-androidauto protocol library.

## Primary User Outcomes

- Connect your phone via wireless and get Android Auto on a touchscreen.
- Reliable audio/video with low latency on Pi 4 hardware.
- Configurable UI (themes, layouts, settings) without recompiling.
- Plugin system for extending functionality (OBD, GPIO, cameras, custom apps).
- Companion app for phone-side control (GPS, battery, time sync, internet sharing).

## Design Principles

- Protocol Optimization: Ensure compliance with the AA protocol to ensure lasting support.
- Provide maximum performance on design hardware - RPi4 4GB
- Plugin isolation: features extend the system without modifying core.
- Pi-first: every feature must work on a Pi 4 with no USB sound card/Bluetooth required. 
- No vendor lock-in: open formats, open protocols, GPLv3.
- Evidence before completion: verification output required for behavior-changing work.

## Constraints

- Development hardware: Raspberry Pi 4, 1024x600 DFRobot touchscreen, Raspberry Pi Official Touch Screen, Samsung S25+ / Moto Edge 5G 2024, Android 16, Android Auto 16.1
- Stack: Qt 6.8 + QML, C++17, CMake, open-androidauto (in-tree AA protocol library), PipeWire.
- Cross-compile via Docker (aarch64), deploy via rsync + SSH.
- Solo-maintained: process must stay lightweight.
- No GPU passthrough on dev VM — GPU-dependent work tested on Pi only.

## Testing Hardware

- Pi 4 at 192.168.1.152 (SSH: matt@ via certificate authentication), device name = prodigy, running Debian Trixie.
- Phone with ADB debug access for companion app testing.
- Dev VM (claude-dev) for local x86 builds and cross-compilation.

## Non-Goals

- Supporting hardware other than Pi 4 (for now).
- Commercial distribution or app store presence.
- Cloud services, accounts, or telemetry.
- Full CarPlay or non-AA projection protocols.
- USB AndroidAuto Support

## Direction Change Log

(Populated as architectural direction shifts occur)

# Documentation Index

## Project Management

- [project-vision.md](project-vision.md) — product intent, design principles, constraints
- [roadmap-current.md](roadmap-current.md) — current priorities (Now/Next/Later)
- [session-handoffs.md](session-handoffs.md) — session continuity log
- [wishlist.md](wishlist.md) — idea parking lot

## Getting Started

- [development.md](development.md) — build, dependencies, cross-compile, dual-platform
- [wireless-setup.md](wireless-setup.md) — WiFi AP + Bluetooth for wireless AA

## Reference

- [config-schema.md](config-schema.md) — YAML config keys and defaults
- [design-decisions.md](design-decisions.md) — architectural choices and rationale
- [design-philosophy.md](design-philosophy.md) — core design principles (detailed)
- [plugin-api.md](plugin-api.md) — IPlugin interface, lifecycle, IHostContext
- [hfp-stack-spike.md](hfp-stack-spike.md) — HFP audio routing research
- [release-packaging.md](release-packaging.md) — prebuilt release naming and archive conventions
- [debugging-notes.md](debugging-notes.md) — common issues and fixes
- [testing-reconnect.md](testing-reconnect.md) — reconnect test procedures

## AA Protocol

Protocol definitions and generic reference docs are in [open-android-auto](https://github.com/mrmees/open-android-auto). The docs below are implementation-specific to this project.

- [aa-phone-side-debug.md](aa-phone-side-debug.md) — phone-side behavior and debugging
- [android-auto-protocol-cross-reference.md](android-auto-protocol-cross-reference.md) — protocol cross-reference (wire formats)
- [aa-apk-deep-dive.md](aa-apk-deep-dive.md) — APK v16.1 analysis
- [aa-display-rendering.md](aa-display-rendering.md) — video pipeline and rendering
- [aa-video-resolution.md](aa-video-resolution.md) — resolution negotiation
- [aa-troubleshooting-runbook.md](aa-troubleshooting-runbook.md) — troubleshooting guide
- [apk-proto-reference.md](apk-proto-reference.md) — protobuf message reference
- [apk-indexing.md](apk-indexing.md) — APK indexing pipeline
- [proto-validation-report.md](proto-validation-report.md) — field migration validation

## Milestone History

- [plans/milestone-01-foundation.md](plans/milestone-01-foundation.md) — Feb 16-18: initial build, architecture, protocol
- [plans/milestone-02-aa-integration.md](plans/milestone-02-aa-integration.md) — Feb 19-21: oaa library, settings, audio, sidebar
- [plans/milestone-03-companion-system.md](plans/milestone-03-companion-system.md) — Feb 22-23: companion app, multi-vehicle, UI
- [plans/milestone-04-protocol-correctness.md](plans/milestone-04-protocol-correctness.md) — Feb 23-25: proto migration, APK analysis
- [plans/milestone-05-av-optimization.md](plans/milestone-05-av-optimization.md) — Feb 25-26: video, HW accel, proxy, system service

## Active Plans

- [plans/active/](plans/active/) — in-progress or not-yet-started design/implementation plans

## Pi Configuration

- [pi-config/](pi-config/) — system config snapshot (hostapd, systemd, BlueZ, labwc, udev, boot)

## Archive

- [OpenAutoPro_archive_information/](OpenAutoPro_archive_information/) — original OpenAuto Pro reverse engineering (from community repo)
- [OpenAutoPro_archive_information/needs-review/](OpenAutoPro_archive_information/needs-review/) — workspace files pending manual triage

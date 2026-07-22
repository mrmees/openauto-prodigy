# OpenAuto Prodigy — Design Philosophy

## Why This Exists

BlueWave Studio created OpenAuto Pro — a closed-source Android Auto head unit app for Raspberry Pi. At some point they stopped developing it. Time moved on, dependencies broke, and users were left stranded with no recourse because the source was never open.

Prodigy is a **fresh start**. Not a fork, not a patch, not a compatibility shim. A clean-room rebuild on current software, for anyone who wants to put Android Auto on a Raspberry Pi.

## Core Principles

### 1. Open Source (GPLv3)

The core application has no required closed-source dependency or proprietary
runtime blob. The AA protocol implementation, QML UI, services, plugins, web
configuration panel, and installer are open. Optional browser content may use a
system-installed Widevine CDM when the operator provides one, but neither the
core shell nor WebEngine support depends on it.

### 2. Raspberry Pi 4 Reference Hardware

The Pi 4 is the target. Every feature must work acceptably on a Pi 4 with a basic USB touchscreen over HDMI. This constraint keeps us honest — no lazy "just throw GPU at it" solutions, no features that require exotic hardware.

Other platforms (Pi 5, other SBCs, x86 dev machines) are welcome as build/test targets, but the Pi 4 is the acceptance test. If it doesn't run well there, it doesn't ship.

### 3. Current Software Stack

We build on what's current, not what was current when the original project started:

| Component | Choice | Why |
|-----------|--------|-----|
| **OS** | RPi OS Trixie (Debian 13) | Latest stable RPi OS |
| **Display** | labwc (Wayland) | Modern compositor, replaces X11 |
| **Audio** | PipeWire | Replaces PulseAudio, handles both playback and Bluetooth audio |
| **UI** | Qt 6 + QML | Current Qt LTS, declarative UI |
| **Language** | C++17 | Modern enough to be pleasant, old enough to be well-supported |
| **Build** | CMake | Standard for Qt/C++ projects |

No backporting to Buster, Bullseye, or Bookworm. No PulseAudio compatibility layers. No X11 fallback paths. Current stack, clean dependencies.

### 4. Android Compatibility

- **Support baseline:** Android 12 (API 31)
- **Primary target:** Android 14+ (API 34)
- **AA protocol:** Request version 1.7

Older phones that support wireless AA may work, but compatibility work is
prioritized around the stated baseline and current test devices.

### 5. Wireless Only

USB Android Auto adds a second transport and device-management stack (AOAP,
libusb, permissions, and hotplug). Prodigy deliberately supports one path: the
Pi's Bluetooth discovery hands the phone WiFi AP credentials, then AA runs over
TCP.

This is a deliberate simplification. One transport path means fewer bugs, less code to maintain, and a simpler user experience.

### 6. Plugin Architecture

The core app is a shell with a plugin system. Everything interesting is a plugin:

- **Android Auto** (`org.openauto.android-auto`) — the killer app, but still a plugin
- **Bluetooth Audio** (`org.openauto.bt-audio`) — A2DP sink + AVRCP
- **Phone** (`org.openauto.phone`) — HFP dialer and incoming call overlay
- **Media Player** (`org.openauto.media-player`) — local and removable-media playback
- **Equalizer** (`org.openauto.equalizer`) — controls for the shared EQ service

Third-party plugins can be loaded from `~/.openauto/plugins/` as shared
libraries with YAML manifests. The plugin API (`IPlugin` + `IHostContext`)
provides interfaces for config, theme, display, audio, Bluetooth, events,
actions, notifications, equalization, overlays, and shared state providers. IPC
and concrete service internals are not part of that contract.

This means Prodigy isn't just an AA app — it's a platform for car Pi projects.

### 7. Installable by Normal Humans

The target user isn't a developer. It's someone who can flash an SD card, SSH into a Pi, and follow a README. The install process is:

```bash
git clone https://github.com/mrmees/openauto-prodigy
cd openauto-prodigy
./install.sh
```

The installer handles dependencies, builds from source, configures hostapd/WiFi, sets up systemd services, and walks the user through Bluetooth pairing. If something fails, the error message should tell them what to do — not send them to Stack Overflow.

### 8. Native Core, Web Extensions

Native QML for anything core, driving-relevant, or latency-sensitive (projection,
phone, BT audio, settings, launcher, media playback). The web runtime
(QtWebEngine) is the extension surface: glanceable dashboard content, optional
add-ons, and third-party/community work that shouldn't require touching C++.

Two consequences:

- **WebEngine stays optional.** The app must build and run fully without
  qt6-webengine installed (`HAS_WEBENGINE`). No core function may depend on a
  browser stack existing.
- **No core plugin migrates to web.** Renderer processes can crash and reload
  (the widget host has retry machinery for exactly that); a weather tile
  tolerates this, an incoming-call overlay does not.

Decided 2026-07-07 after the web-widget ship — full rationale in
`docs/archive/plans/2026-07-07-web-surface-strategy-design.md`.

## What We Don't Do

| Decision | Reason |
|----------|--------|
| USB Android Auto | Wireless is universal, USB adds complexity for diminishing returns |
| X11/fbdev fallback | Wayland (labwc) is the compositor. No X11 compatibility path. |
| PulseAudio support | PipeWire handles everything PulseAudio did, plus Bluetooth audio natively |
| Android < 12 | Outside the supported and tested compatibility baseline |
| Required proprietary codecs | Projection supports the negotiated H.264/H.265 paths and protocol audio formats without making a proprietary browser codec part of the core |
| Accounts or telemetry | Core operation is local and has no account or phone-home requirement; optional content such as weather and web widgets may use network services |
| Multi-platform UI | Qt 6 + QML for everything. No Electron, no web UI for the main app. |

## Contributing

If you're thinking about contributing, these principles should guide your decisions:

- **Will it work on a Pi 4?** If not, reconsider.
- **Does it add a new dependency?** Justify it.
- **Is it a plugin or core?** Default to plugin.
- **Does it require Android < 12?** Probably not worth it.
- **Can a non-developer install it?** If your feature needs manual config file editing, add a UI for it.

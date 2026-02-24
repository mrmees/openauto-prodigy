# OpenAuto Prodigy — Design Philosophy

## Why This Exists

BlueWave Studio created OpenAuto Pro — a closed-source Android Auto head unit app for Raspberry Pi. At some point they stopped developing it. Time moved on, dependencies broke, and users were left stranded with no recourse because the source was never open.

Prodigy is a **fresh start**. Not a fork, not a patch, not a compatibility shim. A clean-room rebuild on current software, for anyone who wants to put Android Auto on a Raspberry Pi.

## Core Principles

### 1. Open Source (GPLv3)

No closed-source dependencies, no proprietary blobs, no "contact us for licensing." The entire stack — from the AA protocol implementation to the QML UI to the installer — is open. Anyone can build it, modify it, ship it in their project car, or use it as a starting point for something better.

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

- **Minimum:** Android 12 (API 31) — this is when Google made wireless Android Auto mandatory on all new devices
- **Primary target:** Android 14+ (API 34) — this is what we optimize for and test against
- **AA Protocol:** Request version 1.7 (the current maximum the phone supports)

We don't chase ancient Android versions. If someone's running Android 10, their phone probably can't do wireless AA anyway. The protocol is backwards-compatible, so older phones that *do* support wireless AA will likely still work — we just don't go out of our way to support their quirks.

### 5. Wireless Only

USB Android Auto adds significant complexity (AOAP protocol, libusb, USB permission management, hotplug handling) for a use case that's increasingly irrelevant. Every phone sold since 2022 supports wireless AA. The Pi 4's built-in WiFi and Bluetooth handle the entire connection natively.

This is a deliberate simplification. One transport path means fewer bugs, less code to maintain, and a simpler user experience.

### 6. Plugin Architecture

The core app is a shell with a plugin system. Everything interesting is a plugin:

- **Android Auto** (`org.openauto.android-auto`) — the killer app, but still a plugin
- **Bluetooth Audio** (`org.openauto.bt-audio`) — A2DP sink + AVRCP
- **Phone** (`org.openauto.phone`) — HFP dialer and incoming call overlay
- **Future:** OBD-II dashboard, backup camera, dashcam, media player, etc.

Third-party plugins can be loaded from `~/.openauto/plugins/` as shared libraries with YAML manifests. The plugin API (`IPlugin` + `IHostContext`) provides access to all core services (config, theme, audio, IPC).

This means Prodigy isn't just an AA app — it's a platform for car Pi projects.

### 7. Installable by Normal Humans

The target user isn't a developer. It's someone who can flash an SD card, SSH into a Pi, and follow a README. The install process is:

```bash
git clone https://github.com/mrmees/openauto-prodigy
cd openauto-prodigy
./install.sh
```

The installer handles dependencies, builds from source, configures hostapd/WiFi, sets up systemd services, and walks the user through Bluetooth pairing. If something fails, the error message should tell them what to do — not send them to Stack Overflow.

## What We Don't Do

| Decision | Reason |
|----------|--------|
| USB Android Auto | Wireless is universal, USB adds complexity for diminishing returns |
| X11/fbdev fallback | Wayland (labwc) is the compositor. No X11 compatibility path. |
| PulseAudio support | PipeWire handles everything PulseAudio did, plus Bluetooth audio natively |
| Android < 12 | No wireless AA support, increasingly rare in the wild |
| Proprietary codecs | Stick to what's in the protocol (H.264 video, PCM audio) |
| Cloud dependencies | Everything runs locally. No accounts, no telemetry, no phone-home |
| Multi-platform UI | Qt 6 + QML for everything. No Electron, no web UI for the main app. |

## Contributing

If you're thinking about contributing, these principles should guide your decisions:

- **Will it work on a Pi 4?** If not, reconsider.
- **Does it add a new dependency?** Justify it.
- **Is it a plugin or core?** Default to plugin.
- **Does it require Android < 12?** Probably not worth it.
- **Can a non-developer install it?** If your feature needs manual config file editing, add a UI for it.

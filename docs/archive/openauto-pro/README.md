> **Archive Notice:** This content was imported from the
> [openauto-pro-community](https://github.com/mrmees/openauto-pro-community)
> repository. It documents reverse engineering of the original OpenAuto Pro
> software and is preserved here as reference material. Active development
> happens in the parent openauto-prodigy repository.

---

# OpenAuto Pro Community

A community-driven effort to revive and maintain [OpenAuto Pro](https://bluewavestudio.io), an Android Auto head unit application for Raspberry Pi. BlueWave Studio shut down without releasing source code or providing updates, leaving paid customers with broken software as Android Auto evolves.

## Background

OpenAuto Pro was a commercial product built on top of the open-source [openauto](https://github.com/f1xpl/openauto) and [aasdk](https://github.com/f1xpl/aasdk) projects by f1xpl. BlueWave Studio added a Qt/QML-based UI, Bluetooth telephony, OBD-II gauges, FM radio, screen mirroring, wireless Android Auto (Autobox) support, and a plugin API on top of the open-source Android Auto protocol implementation.

When BlueWave Studio ceased operations, they did not release the source code or provide any path forward for customers who had paid for licenses. The software has since broken due to Android Auto protocol changes in newer Android versions.

## Project Goals

1. **Fix Android Auto compatibility** with current Android versions
2. **Rebuild from source** using the open-source openauto/aasdk as a foundation
3. **Restore full functionality** including the proprietary features BlueWave added
4. **Release as open source** so this never happens again

## What We Have

### Recovered from the v16.1 binary image:

| Asset | Count | Status |
|-------|-------|--------|
| QML UI files | 162 files | Complete UI layer with full directory structure |
| SVG icons | 88 files | Complete |
| Python scripts | 5 files | Hotspot, cursor, RTC, launcher configs |
| Protobuf API definition | 34 messages | `Api.proto` fully reconstructed |
| C++ binary (autoapp) | 27 MB | Stripped ARM (armhf) ELF — needs Ghidra for decompilation |
| Shared libraries | 2 files | libaasdk.so + libaasdk_proto.so (stripped) |

### Architecture (from binary analysis):

**Namespace structure:** `f1x::openauto::autoapp::`
- `androidauto` — AA connection, handshake, video/audio/input/sensor/bluetooth services
- `autobox` — Wireless Android Auto dongle support
- `bluetooth` — A2DP, telephony, phonebook, device management
- `mirroring` — ADB-based screen mirroring
- `obd` — OBD-II gauge support
- `audio` — Equalizer (LADSPA/PulseAudio), FM radio (RTL-SDR), music library
- `ui` — QML controllers
- `api` — External plugin API (protobuf over socket)
- `system` — Gesture sensor, temperature sensor, day/night mode, WiFi

**Key dependencies:**
- Qt 5.11 (Quick, QML, Multimedia, DBus, X11Extras)
- Boost 1.67 (filesystem, system, log, thread, regex, etc.)
- libusb, protobuf, OpenSSL 1.1
- PulseAudio, ALSA, BlueZ/KF5BluezQt
- Broadcom VideoCore (RPi GPU)
- libgps, libi2c, librtlsdr, libxdo

## Repository Structure

```
original/           # Assets extracted from the v16.1 binary
  qml/              # Complete QML UI source
    applications/   # Per-app UI (android_auto, autobox, music, settings, etc.)
    components/     # Reusable components (gauges, bars, camera views)
    controls/       # Custom controls (buttons, sliders, popups, keyboards)
    widgets/        # Home screen widgets
  images/           # SVG icons
  scripts/          # Python helper scripts
  proto/            # Recovered protobuf API definitions
tools/              # Extraction and analysis tools
docs/               # Project documentation
```

## Upstream References

- [f1xpl/openauto](https://github.com/f1xpl/openauto) — Original open-source Android Auto head unit (frozen since 2018)
- [f1xpl/aasdk](https://github.com/f1xpl/aasdk) — Android Auto SDK library (frozen since 2018)

## Roadmap

### Phase 1: Analysis & Documentation (current)
- [x] Extract QML UI from binary
- [x] Recover protobuf API definitions
- [x] Extract SVG icons and scripts
- [x] Catalog binary dependencies and architecture
- [ ] Decompile core binary with Ghidra (ARM analysis)
- [ ] Map BlueWave modifications vs. upstream openauto
- [ ] Document Android Auto protocol changes since 2018

### Phase 2: Build Foundation
- [ ] Set up ARM cross-compilation environment
- [ ] Get upstream openauto building
- [ ] Integrate recovered QML UI
- [ ] Implement BlueWave's API layer using recovered proto definitions

### Phase 3: Fix Android Auto
- [ ] Identify exact protocol version mismatch
- [ ] Update aasdk for current AA protocol
- [ ] Test with modern Android phones

### Phase 4: Full Rebuild
- [ ] Reimplement proprietary features (Bluetooth telephony, OBD, etc.)
- [ ] Port to newer Qt version
- [ ] Support newer Raspberry Pi models (Pi 4, Pi 5)
- [ ] Modernize build system (CMake)

## Legal Notes

This project is based on reverse engineering of a commercially purchased product from a defunct company (BlueWave Studio) that provided no source release, no refunds, and no continuing support. The original open-source foundation (openauto, aasdk) is GPL-licensed.

All recovered assets in this repository were extracted from a legitimately purchased copy. The Android Auto protocol implementation is not certified by Google.

## Contributing

This project needs help! If you're a former OpenAuto Pro user, a C++ developer familiar with Qt, or someone who knows the Android Auto protocol, please get involved.

## License

TBD — The upstream openauto/aasdk are GPLv3. The recovered QML and protobuf definitions are from BlueWave Studio's proprietary additions. Given the circumstances (abandoned product, no source release, no support), we believe this reverse engineering effort is justified. We welcome legal guidance on the appropriate license.

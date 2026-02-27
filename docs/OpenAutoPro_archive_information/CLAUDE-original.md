# OpenAuto Prodigy — Project Context

## Role

**This is the reference/spec repo.** It holds recovered assets and research from the original OpenAuto Pro. The active implementation lives in the sibling repo `../openauto-prodigy/`.

Use this repo to answer "what did OAP do?" and "how did OAP do it?" — not to write new code.

**GitHub:** https://github.com/mrmees/openauto-pro-community

## What's Useful Here

- **`original/qml/`** — 162 QML files: the complete UI spec (screens, navigation, components, settings)
- **`original/images/`** — 88 SVG icons
- **`original/proto/Api.proto`** — Plugin API definition (Prodigy targets backward compatibility)
- **`original/scripts/`** — System-level scripts (hotspot, cursor, RTC, splash)
- **`docs/plans/`** — Design doc and Phase 1 implementation plan

## Firmware Analysis (AAP Protocol Ground Truth)

- **`docs/alpine-ilx-w650bt-firmware-analysis.md`** — Alpine iLX-W650BT: Google "tamul" AAP SDK v1.4.1 on bare-metal RTOS (Toshiba TMM9200). XML attribute paths, protobuf enums, state machines, keycodes.
- **`docs/kenwood-dnx-firmware-analysis.md`** — Kenwood DNX/DDX: Newer Google `libreceiver-lib.so` on Linux (Telechips TCC8971). Significantly more protocol coverage: radio tuner, notifications, instrument cluster, split-screen, HD Radio, comprehensive vehicle sensors, media browser, VoiceSession, NavigationNextTurn. Also documents JK proxy architecture, AOAP transport, VNC AAPHID SDK, GStreamer audio.
- **`docs/sony-xav-firmware-analysis.md`** — Sony XAV-AX100: Google AAP Receiver Library / GAL Protocol on Linux (Sunplus SPHE8388 "Gemini"). Firmware decrypted via keys from Sony's GPL U-Boot source. Most complete service catalog of all analyzed units: 30+ GAL service types, complete AA config XML, Android Binder IPC on embedded Linux, debug symbols in shared libraries.
- **`docs/pioneer-dmh-firmware-analysis.md`** — Pioneer DMH-WT8600NEX: Fully encrypted `.avh` container (AES + RSA-2048). No extraction possible, but documents firmware format, cross-model comparison, and encryption analysis.

Alpine and Kenwood analyses were created from `strings` analysis only — no decompilation. Sony was decrypted using keys derived from GPL source code. Pioneer firmware remains fully encrypted. The Sony SDK has the most complete service catalog, while the Kenwood SDK has the broadest protobuf message coverage.

## Historical (already distilled into Prodigy's CLAUDE.md)

- `docs/reverse-engineering-notes.md` — Full RE findings
- `docs/abi-compatibility-analysis.md` — Binary patching analysis (abandoned approach)
- `docs/android-auto-fix-research.md` — AA 12.7+ bug research (fixed by SonOfGib's aasdk)
- `tools/` — One-time extraction scripts, already run

## File Locations

### In This Repo
```
original/           # Recovered assets from OAP v16.1 (reference/spec only)
  qml/              # 162 QML files (complete UI specification)
  images/           # 88 SVG icons
  scripts/          # 7 shell/Python utility scripts
  proto/Api.proto   # Complete plugin API (34 messages)
tools/              # Extraction scripts (extract_qrc.py, extract_proto.py)
docs/               # Research, analysis, and design documents
  plans/            # Design and implementation plans
  reverse-engineering-notes.md   # Full RE findings
  android-auto-fix-research.md   # AA 12.7+ bug details
  abi-compatibility-analysis.md  # Library compatibility analysis
```

### Outside This Repo
| Path | Contents |
|------|----------|
| `../bluewavestudio-openauto-pro-release-16.1.img` | Original disk image (7.3GB) |
| `../1.img` | Extracted ext4 root partition (browsable with 7-Zip) |
| `../extracted/autoapp` | Main binary (28MB ARM ELF) |
| `../extracted/controller_service` | Companion service binary (457KB) |
| `../extracted/libaasdk.so` | Android Auto SDK shared library (884KB) |
| `../extracted/libaasdk_proto.so` | AA protobuf definitions library (1.3MB) |
| `../oap_manual_1.pdf` | OpenAuto Pro user manual (43 pages) |
| `../upstream/openauto/` | f1xpl/openauto (original, frozen 2018) |
| `../upstream/aasdk/` | f1xpl/aasdk (original, frozen 2018) |
| `../upstream/sonofgib-openauto/` | SonOfGib's fixed openauto fork |
| `../upstream/sonofgib-aasdk/` | SonOfGib's fixed aasdk fork |
| `../upstream/openauto-pro-api/` | BlueWave's published plugin API (proto + Python examples) |

### Reference Projects (not cloned, use GitHub)
- `github.com/opencardev/openauto` — actively maintained openauto fork (builds on Trixie, has WiFi projection, tests)
- `github.com/openDsh/dash` — Qt-based infotainment UI built on openauto (OBD, camera, BT, theming)

## Reverse Engineering (Completed — Reference Only)

This work informed the design but is no longer the active focus. Key findings preserved for reference:

### Original Binary Architecture
- **Binary:** 28MB stripped ARM armhf ELF, namespace `f1x::openauto::autoapp::`
- **Build:** Qt 5.11, Boost 1.67, protobuf 17, OpenSSL 1.1, Raspbian
- **Build path:** `/home/pi/workdir/openauto/`
- **54 shared library dependencies** (see `docs/reverse-engineering-notes.md`)
- **Sub-namespaces:** androidauto, autobox, bluetooth, mirroring, obd, audio, ui, api, system

### Android Auto Bug (Root Cause)
- **Problem:** AA 12.7+ sends WiFi credentials with message ID `0x8001`. BlueWave's enum has `CREDENTIALS_REQUEST = 0x0002`. Mismatch causes message to be dropped.
- **Ghidra finding:** `WirelessServiceChannel::messageHandler()` in libaasdk.so has NO switch statement at all — every message falls through to "not handled" logging. The actual credential handling may be in the autoapp binary, not the library.
- **Fix in rebuild:** SonOfGib's aasdk uses correct `0x8001` value. Using it as our submodule means the bug is fixed from day one.

### Key Binary Offsets (for reference)
| Binary | Offset | Content |
|--------|--------|---------|
| autoapp | 0x5c1378 | Serialized FileDescriptorProto (Api.proto, 6700 bytes) |
| libaasdk_proto.so | 0x132c74 | `WirelessMessage::WIRELESS_CREDENTIALS_REQUEST` enum (value: 2, should be 0x8001) |

## Workflow Reminder

When starting work on a new section/task, always do a quick web search for:
- New or updated MCP servers/skills/tools relevant to the current work
- Updated documentation for libraries being used (aasdk, Qt 6, Boost.ASIO, etc.)
- Community developments (GitHub issues, forks, PRs) on relevant projects

This prevents building on stale information and catches improvements we might otherwise miss.

## AA Protocol Reference Loading

A comprehensive Android Auto protocol reference (2200+ lines) is available at
`../../reference/android-auto-protocol.md`. Load relevant sections based on current work:

| Work Area | Sections to Load |
|-----------|-----------------|
| Wireless connection | 1-4 (protocol overview, wireless flow, BT profiles, Pi hardware) |
| Channel implementation | 1 + 5 (protocol overview + channel architecture) |
| Audio work | 1 + 5 (audio parts) + 6 (wireless audio pipeline) |
| Debugging connection failures | 1 + 2 + 7 (protocol + wireless flow + breaking changes) |
| Hardware/config | 4 (Pi radio constraints) |
| Checking for updates | 8 (freshness check resources) |
| Production SDK services / protocol gaps | 10 (firmware analysis synthesis) |
| Full context (rare) | All sections |

The reference includes exact message IDs, proto field names, wire format diagrams, and
handshake sequences verified against SonOfGib's aasdk source code. When in doubt, the
reference cites source files — check those for the latest truth.

## Technical Notes

- **WSL2 loop device mounting is broken on this machine** — use 7-Zip to browse ext4 images
- **Binary is stripped but leaky** — C++ template instantiations, Qt meta-object strings, and Boost.Log channels reveal class hierarchies
- **OAP config files:** `openauto_system.ini` (all settings including day/night colors, sensor config, hotspot) and `openauto_applications.ini` (external app launcher, up to 8 entries)
- **Plugin API protocol:** TCP/IP, message format: 32-bit size (LE) + 32-bit message ID (LE) + 32-bit flags (LE) + protobuf payload
- **BlueWave's additions over upstream:** QML UI, BT telephony (HFP/PBAP/A2DP), OBD-II, screen mirroring, music player, FM radio (RTL-SDR), plugin API, Autobox wireless, equalizer, system services (GPIO, I2C sensors, day/night)
- **OAP manual documents:** BMW iDrive/IBus/MMI 2G controller support, keyboard mapping table, TSL2561 light sensor I2C config, DS18B20 1-Wire temp sensor, V4L2 rear camera with GPIO trigger

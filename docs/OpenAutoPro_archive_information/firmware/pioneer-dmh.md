# Pioneer DMH-WT8600NEX Firmware Analysis

> **Source:** Pioneer DMH-WT8600NEX firmware v2.08.03 from `DMH-WT8600NEX.zip`
> **Also examined:** DMH-WT7600NEX (same firmware), DMH-WC6600NEX (same firmware)
> **Date:** July 2025 (firmware file timestamp)
> **Platform code:** KM955 (WT8600NEX), KM950 (WT7600NEX), KM830 (WC6600NEX)
> **Region:** XNUC (North America)
> **Firmware format:** `.avh` — Pioneer proprietary encrypted container
> **Encryption:** AES (or equivalent) with fixed key — bulk data identical across model variants

**No proprietary code was decompiled or disassembled.** This analysis was performed entirely through binary structure analysis, header parsing, and multi-file comparison of publicly available firmware update files.

**Key finding:** The firmware is fully encrypted with a hardware-embedded key. The encryption is competent — no ECB mode, no weak key derivation, no keystream reuse artifacts. However, cross-model comparison reveals that all DMH-series models in this generation share identical encrypted payloads, differentiated only by a plaintext header and an RSA-2048 signature.

---

## Firmware File Format (`.avh`)

### Overall Structure

| Offset | Size | Content |
|--------|------|---------|
| 0x000 | 256 bytes | Plaintext header (model, platform, version, file size) |
| 0x100 | 256 bytes | RSA-2048 signature (per-model, over header) |
| 0x200 | ~270.7 MB | Encrypted firmware payload (fixed key, identical across models) |

Total file size: 283,800,672 bytes (all three models examined are byte-identical in size).

### Plaintext Header (0x000–0x0FF)

```
Offset  Size  Field                     WT8600 Value
------  ----  -----                     ------------
0x00    4     Magic                     "20MD" (0x32304D44)
0x04    2     Format version            0x0001
0x06    2     Reserved                  0x0000
0x08    4     Reserved                  0x00000000
0x0C    4     File size (LE)            0x10EA7460 (283,800,672)
0x10    4     Reserved                  0x00000000
0x14    32    Model name (null-padded)  "DMH-WT8600NEX"
0x34    16    Platform code             "&KM955/XNUC" + "XNUC"
0x44    4     Region code               "XNUC"
0x48    2     Version major?            0x0002
0x4A    2     Version minor?            0x0803
0x4C    2     Version patch?            0x0101
0x4E    2     Padding start             0xFFFF
0x50    126   0xFF padding              (all 0xFF)
0xCE    50    0x00 padding              (all 0x00)
```

### Platform Codes

| Model | Internal Code | Platform | Screen |
|-------|---------------|----------|--------|
| DMH-WT8600NEX | KM955/XNUC | KM955 | 10.1" floating capacitive |
| DMH-WT7600NEX | KM950/XNUC | KM950 | 9" floating capacitive |
| DMH-WC6600NEX | KM830/XNUC | KM830 | 6.8" capacitive |

All three share the same firmware payload — the model name and platform code in the header are the only differences. This confirms that display resolution and panel differences are handled at runtime, not at the firmware level.

### RSA-2048 Signature Block (0x100–0x1FF)

The 256-byte block at offset 0x100 is a per-model digital signature:

- **Size:** 256 bytes = 2048 bits (RSA-2048)
- **Per-model:** Different ciphertext for each model despite identical firmware payload
- **Purpose:** Likely signs the plaintext header to prevent cross-model firmware flashing
- **Not an encryption key:** The bulk data is encrypted independently

### Encrypted Payload (0x200–EOF)

The remaining ~270.7 MB is encrypted firmware data:

- **Byte-identical** across all three models examined (confirmed via SHA-256)
- **SHA-256:** `9df0df25025e31e921d730ff7b071637...`
- **Payload size:** 283,800,160 bytes (multiple of 32, not 512 or 4096)
- **Entropy:** 8.000 bits/byte across every 1MB block (maximum)
- **No ECB patterns:** 17,737,526 unique 16-byte blocks with zero repeats
- **Chi-squared:** 315.1 (first 1MB), 273.4 (last 1MB) — consistent with strong cipher

---

## Encryption Analysis

### What Was Tested

| Method | Result |
|--------|--------|
| AES-ECB detection (repeated blocks) | 0 repeats in 17.7M blocks — not ECB |
| AES-CBC/CTR with keys derived from model name, platform code, header | No hits |
| AES with MD5/SHA1/SHA256 of model strings | No hits |
| AES-256 with header byte ranges as key/IV | No hits |
| RC4 with model strings and hashes | No hits |
| ChaCha20 with derived keys | No hits |
| Blowfish/DES with obvious keys | No hits |
| XOR brute force (1/2/4/16-byte keys) | All converge on 0x00 (no improvement) |
| i.MX6 NXP default test keys | No hits |
| Pioneer AVIC/AVH community known keys | No hits |
| Positional byte frequency analysis | Uniform distribution at every position |
| Raw compression (zlib/lzma/deflate) | No decompression possible |

### What This Tells Us

1. **The encryption is competent.** The key is not derived from any publicly visible header field or model identifier.
2. **Fixed key across models.** The key is the same for all DMH-series units in this generation (since the encrypted payload is identical).
3. **Key is hardware-embedded.** Likely burned into the SoC's OTP (one-time programmable) fuses or stored in the bootloader's secure storage.
4. **Not crackable without physical access.** Would require JTAG/SWD extraction from a live unit, bootloader dump, or finding the key in an unencrypted older firmware that shares the same platform.

---

## Cross-Model Comparison

### File Identity

All three firmware files were downloaded from Pioneer's CDN (images.salsify.com) on the same date:

| File | Size | MD5 |
|------|------|-----|
| DMH-WT8600NEX.avh | 283,800,672 | `789375c8c607aed1011aff3f74301755` |
| DMH-WT7600NEX.avh | 283,800,672 | `3b76068b1a16a4936743e00e95c18ceb` |
| DMH-WC6600NEX.avh | 283,800,672 | `e55d6e81a4cde9a41432c5a802fbb8e5` |

### Byte Differences

**Total differing bytes: 261** (out of 283,800,672 = 0.00009%)

| Range | Content | Difference |
|-------|---------|------------|
| 0x19–0x1A | Model name bytes | `T8` vs `T7` vs `C6` |
| 0x37–0x39 | Platform code bytes | `955` vs `950` vs `830` |
| 0x100–0x1FF | RSA signature | Completely different per model |

Everything from 0x200 onward is **byte-for-byte identical** across all three models.

---

## What We Know About the Platform

### From Header Analysis

- **Magic `20MD`:** Pioneer's firmware container format identifier (likely "20xx Model Data" or "2-series Model Descriptor")
- **Format version 0x0001:** First version of this container format
- **Region `XNUC`:** North American market (X = international, NUC = North/US/Canada?)
- **KM-series platform codes:** Internal Pioneer hardware platform identifiers

### From Public Sources

- **SoC:** Likely NXP i.MX series (Pioneer AVH/AVIC 2018-2019 used i.MX6; the DMH series is the successor generation)
- **OS:** Likely Android-based (per Pioneer community: "nearly all Pioneer head units have a proprietary OS that seems Android-based")
- **Features:** Wireless Android Auto, Wireless Apple CarPlay, Amazon Alexa, HD Radio, Bluetooth, FLAC, SiriusXM
- **OTA updates:** Supported via Pioneer CarAVAssist app (suggests the firmware container is also used for OTA delivery)

### From Older Pioneer Generations (AVH/AVIC 2018-2019)

The [LennartF22/pioneer-firmware](https://github.com/LennartF22/pioneer-firmware) project documents the older generation:

| Aspect | AVH18/AVH19 (older) | DMH-WT8600NEX (current) |
|--------|---------------------|------------------------|
| Firmware format | ZIP with `.PRG` partition images | Single encrypted `.avh` blob |
| Internal structure | `PJ190BOT.PRG`, `PJ190PLT.PRG`, etc. | Unknown (encrypted) |
| Partition layout | boot, recovery, platform, userdata, etc. | Presumably similar (Android) |
| Security | CMD42 SD card lock, BSP CRC32 | Full AES encryption + RSA signatures |
| SoC | NXP i.MX6 | Unknown (likely i.MX successor) |
| OS | Android with custom U-Boot | Presumably Android |

The shift from partition-level `.PRG` files to a single encrypted `.avh` blob represents a significant security upgrade between generations.

---

## Comparison: Pioneer vs Alpine vs Kenwood

| Aspect | Alpine iLX-W650BT | Kenwood DNX/DDX | Pioneer DMH-WT8600NEX |
|--------|-------------------|-----------------|----------------------|
| **Firmware format** | Single `.FIR` file | `.nfu` archive with ext4 images | Encrypted `.avh` container |
| **Extractable?** | Yes (strings analysis) | Yes (ext4 mount + strings) | **No** (fully encrypted) |
| **Platform** | Toshiba TMM9200, bare-metal RTOS | Telechips TCC8971, Linux | NXP i.MX (likely), Android-based |
| **AAP SDK visible?** | Yes — Google "tamul" v1.4.1 | Yes — Google `libreceiver-lib.so` | **No** (encrypted) |
| **SSL library** | MatrixSSL 3.9.5 | OpenSSL | Unknown |
| **Protobuf** | Lite runtime | Full runtime | Unknown |
| **Config format** | XML attribute paths | libconfig (`.cfg`) | Unknown |
| **Security model** | Minimal (plain binary) | Minimal (standard ext4) | **Strong** (AES + RSA-2048) |

### Implications for OpenAuto Prodigy

The Pioneer firmware cannot contribute protocol details to our AA implementation, but the analysis is still valuable:

1. **Security trend:** Newer head units are encrypting firmware. This means fewer open firmware sources will be available over time for protocol research.
2. **Firmware identity:** All DMH-series models share the same firmware image — model-specific behavior is configured at runtime, likely through hardware ID registers or configuration files.
3. **Format documentation:** The `.avh` header format is now documented, which could be useful if a decryption method is found in the future.
4. **For future decryption:** If someone obtains the AES key from physical access to a DMH-series unit, this document provides the exact file structure needed to extract the firmware.

---

## Potential Future Approaches

If the goal remains extracting Pioneer's AA implementation:

1. **Physical key extraction:** JTAG/SWD the bootloader from a live unit to extract the AES key
2. **Older firmware:** Find a pre-encryption Pioneer model that supports AA and has extractable firmware
3. **Bootloader analysis:** If the bootloader itself is accessible (via the older AVH/AVIC root methods), it may contain the decryption key for the current generation
4. **Side-channel:** Monitor the unit's UART/debug output during a firmware update for key material
5. **Wait for community:** The avic411.com and XDA communities may eventually crack the DMH generation

---

## References

- [Pioneer DMH-WT8600NEX product page](https://usa.pioneer/products/dmh-wt8600nex)
- [LennartF22/pioneer-firmware — AVH/AVIC firmware tools](https://github.com/LennartF22/pioneer-firmware)
- [Pioneer AVIC Development Mod (avic411.com)](https://avic411.com/index.php?/topic/81468-the-avic-development-mod/)
- [Pioneer AVIC units rooted (XDA)](https://xdaforums.com/t/pioneer-avic-nex-units-have-been-rooted.3956546/)
- [Pioneer AVIC hacking (Hackaday)](https://hackaday.com/2016/12/13/pioneer-avic-infotainment-units-hacked-to-load-custom-roms/)

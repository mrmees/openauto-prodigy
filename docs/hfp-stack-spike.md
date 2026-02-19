# HFP Stack Spike — ofono vs BlueZ on RPi OS Trixie

**Date:** 2026-02-18
**Hardware:** Raspberry Pi 4, RPi OS Trixie (Debian 13)
**BlueZ:** 5.82-1.1+rpt1
**PipeWire:** 1.4.2 with WirePlumber

## Summary

**Recommendation: Use PipeWire's native BlueZ5 HFP backend. Do NOT install ofono.**

PipeWire + BlueZ already handle HFP natively on Trixie. ofono adds complexity without benefit for our use case (car head unit HFP AG, not cellular modem management).

## Findings

### ofono
- **Not installed** by default on RPi OS Trixie
- Available in the repo: `ofono 2.16-5`
- Designed for cellular modem management — overkill for BT HFP-only use
- Conflicts with ModemManager
- Would require additional D-Bus integration and ofono-specific APIs
- The PipeWire community has been moving away from ofono as the HFP backend

### BlueZ 5.82
- **Already running** and fully functional
- HFP Audio Gateway (AG) profile registered: `0000111f` — this is what we need for a car head unit (Pi acts as the car's hands-free system, phone connects to it)
- Handsfree (HF) profile also registered: `0000111e`
- A2DP sink/source profiles active
- AVRCP profiles active

### PipeWire / WirePlumber BlueZ5 Integration
- PipeWire has native BlueZ5 SPA module: `libspa-bluez5.so`
- BT codecs available: SBC, AAC (aptx), FastStream, G.722, LC3, LDAC, Opus
- WirePlumber monitors BT devices via `scripts/monitors/bluez.lua`
- When a phone pairs and connects with HFP, PipeWire automatically:
  1. Creates audio source/sink nodes for the HFP connection
  2. Handles SCO audio routing
  3. Manages codec negotiation

### What This Means for the Phone Plugin

The Phone plugin needs to:
1. **Monitor BlueZ D-Bus** for HFP connection state (`org.bluez.HandsfreeAudioGateway`)
2. **Use PipeWire** for call audio routing (PipeWire handles SCO automatically)
3. **Send AT commands** via BlueZ D-Bus for call control (dial, answer, hangup)
4. **Listen for call events** via D-Bus signals (incoming call, call ended, etc.)

We do NOT need to:
- Install ofono
- Implement SCO audio handling ourselves
- Negotiate HFP codec selection (PipeWire/BlueZ handle this)
- Manage HFP SDP registration (BlueZ already registered the profiles)

### D-Bus Interface Path

```
org.bluez
  /org/bluez/hci0
    org.bluez.Adapter1
    org.bluez.GattManager1
  /org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX
    org.bluez.Device1
    org.bluez.MediaControl1      (AVRCP)
    org.bluez.MediaTransport1    (A2DP)
    org.freedesktop.DBus.Properties
```

For HFP specifically, after a phone connects with HFP profile:
- Call state changes arrive as property changes on the device
- Audio routing is automatic through PipeWire

### Risks
- BlueZ HFP AG support requires the phone to initiate the HFP connection (normal for car head units)
- Some phones may not support HFP AG mode with a Pi (rare, but worth testing)
- Call audio quality depends on SCO codec negotiation (mSBC preferred, CVSD fallback)

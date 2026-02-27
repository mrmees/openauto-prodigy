# Settings Tree

This document describes every page, section, and control in the on-screen settings UI. Edit this file to describe what you want, and Claude will make the QML match.

## Control Types

| Type | Description |
|------|-------------|
| **Slider** | Drag handle, label + value. Config key auto-saved. |
| **Toggle** | On/off switch. Config key auto-saved. |
| **SegmentedButton** | 2-4 mutually exclusive options shown inline. |
| **Picker** | Taps to open full-screen selection list. |
| **ReadOnly** | Shows current config value, not editable on-screen (use web panel). |
| **Button** | Tappable action (not a config value). |
| **StatusRow** | Live data display (indicator dot + label + value). |

---

## Settings Menu (top level)

Scrollable list. Icon + label per row, chevron on right. Tap opens subpage.

### General
- Display
- Audio
- Connection
- Video
- System

### Companion
- Companion

### Plugins
- *(dynamic — one row per plugin that has a settingsComponent)*

### *(separator)*
- About

---

## Display

### General
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| Slider | Brightness | `display.brightness` | 0–100 |
| ReadOnly | Theme | `display.theme` | |
| SegmentedButton | Orientation | `display.orientation` | Landscape / Portrait |

### Day / Night Mode
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| Picker | Source | `sensors.night_mode.source` | time / gpio / none |
| ReadOnly | Day starts at | `sensors.night_mode.day_start` | visible when source=time |
| ReadOnly | Night starts at | `sensors.night_mode.night_start` | visible when source=time |
| Slider | GPIO Pin | `sensors.night_mode.gpio_pin` | 0–40, visible when source=gpio |
| Toggle | GPIO Active High | `sensors.night_mode.gpio_active_high` | visible when source=gpio |

---

## Audio

### Output
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| Slider | Master Volume | `audio.master_volume` | 0–100, live-applies via AudioService |
| Picker | Output Device | `audio.output_device` | PipeWire device list, restart required |

### Microphone
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| Slider | Mic Gain | `audio.microphone.gain` | 0.5–4.0 |
| Picker | Input Device | `audio.input_device` | PipeWire device list, restart required |

---

## Connection

### Android Auto
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| Toggle | Auto-connect | `connection.auto_connect_aa` | |
| ReadOnly | TCP Port | `connection.tcp_port` | |

### WiFi Access Point
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| Picker | Channel | `connection.wifi_ap.channel` | 1–11, 36/40/44/48. Restart required. |
| SegmentedButton | Band | `connection.wifi_ap.band` | 2.4 GHz / 5 GHz. Restart required. |

### Bluetooth
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| Toggle | Discoverable | `connection.bt_discoverable` | |

---

## Video

### Playback
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| SegmentedButton | FPS | `video.fps` | 30 / 60 |
| Picker | Resolution | `video.resolution` | 480p / 720p / 1080p |
| Slider | DPI | `video.dpi` | 80–400, step 10. Restart required. |

### Video Decoding
Dynamic list from `CodecCapabilityModel`. Per codec:

| Control | Label | Notes |
|---------|-------|-------|
| Toggle | *(codec name: H.264, H.265, VP9, AV1)* | H.264 always on, others toggleable |
| SegmentedButton | Decoder | Software / Hardware (when codec enabled) |
| Picker | Decoder name | Shown when >1 decoder available |

### Sidebar
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| Toggle | Show sidebar during Android Auto | `video.sidebar.enabled` | Restart required |
| SegmentedButton | Position | `video.sidebar.position` | Left / Right. Restart required. |
| Slider | Sidebar Width (px) | `video.sidebar.width` | 80–300, step 10. Restart required. |

---

## System

### Identity
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| ReadOnly | Head Unit Name | `identity.head_unit_name` | |
| ReadOnly | Manufacturer | `identity.manufacturer` | |
| ReadOnly | Model | `identity.model` | |
| ReadOnly | Software Version | `identity.sw_version` | |
| ReadOnly | Car Model | `identity.car_model` | |
| ReadOnly | Car Year | `identity.car_year` | |
| Toggle | Left-Hand Drive | `identity.left_hand_drive` | |

### Hardware
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| ReadOnly | Hardware Profile | `hardware_profile` | |
| ReadOnly | Touch Device | `touch.device` | |

---

## Companion

### Status
| Control | Label | Notes |
|---------|-------|-------|
| StatusRow | *(connection indicator)* | Green dot + "Phone Connected" / "Not Connected" |
| StatusRow | GPS | Lat/lon, visible when connected + not stale |
| StatusRow | Phone Battery | Percentage + charging state, visible when connected |
| StatusRow | Internet Proxy | Proxy address, visible when available |
| StatusRow | Route Active | Green/orange/red dot + state text, visible when internet available |

### Pairing
| Control | Label | Notes |
|---------|-------|-------|
| Button | Generate Pairing Code | Shows QR code + PIN |

### Configuration
| Control | Label | Config Key | Notes |
|---------|-------|------------|-------|
| Toggle | Companion Enabled | `companion.enabled` | Restart required |
| ReadOnly | Listen Port | `companion.port` | |

---

## About

| Control | Label | Notes |
|---------|-------|-------|
| *(text)* | OpenAuto Prodigy | Heading |
| *(text)* | Version X.Y.Z | From `identity.sw_version` |
| Button | Close App | Opens exit confirmation dialog |

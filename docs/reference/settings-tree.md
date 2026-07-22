# Settings Tree

This document describes the settings pages and controls shipped in the
on-screen UI. The top-level menu is a scrollable list; selecting a row pushes
that page onto the settings stack.

## Control Types

| Type | Description |
|---|---|
| Slider | Drag control with a displayed value. |
| Toggle | On/off switch. |
| Segmented button | Small mutually exclusive choice set shown inline. |
| Picker | Opens a full-screen choice list. |
| Read-only field | Displays configuration or runtime information. |
| Action | Performs an operation instead of editing a value. |
| Status row | Displays live service state. |

Controls with a `Config Key` persist through `ConfigService` unless noted.
Rows driven directly by a runtime service apply immediately.

## Top-Level Menu

The fixed pages, in order, are:

1. Android Auto
2. Display
3. Audio
4. Bluetooth
5. Theme
6. Companion
7. System
8. Information
9. Debug

The settings stack can also open a plugin-provided `settingsComponent` by
plugin ID, although the fixed menu does not currently add dynamic plugin rows.

## Android Auto

| Control | Label | Config Key | Notes |
|---|---|---|---|
| Picker | Resolution | `video.resolution` | `480p`, `720p`, or `1080p`. |
| Slider | DPI | `video.dpi` | 80–400, step 10; restart required. |
| Toggle | Auto-connect | `connection.auto_connect_aa` | Enables automatic AA connection. |

Decoder selection and protocol diagnostics live on the Debug page.

## Display

| Section | Control | Label | Config Key / Source | Notes |
|---|---|---|---|---|
| Screen | Read-only field | Screen | `DisplayInfo` | Physical diagonal and computed PPI. |
| Screen | Stepper + reset | UI Scale | `ui.scale` | 0.5–2.0 in 0.1 steps; reset stores `1.0`. |
| Display | Slider | Brightness / Screen Dimming | `display.brightness` | 5–100; label depends on hardware backlight support and applies through `DisplayService`. |
| Navbar | Picker | Navbar Position | `navbar.edge` | Bottom, top, left, or right; applies through `NavbarController`. |
| Navbar | Toggle | Show Navbar during Android Auto | `navbar.show_during_aa` | Restart required. |

## Audio

### Output

| Control | Label | Config Key / Source | Notes |
|---|---|---|---|
| Slider | Master Volume | `AudioService.masterVolume` | 0–100; live and synchronized with external changes. |
| Picker | Output Device | `audio.output_device` | PipeWire output-node list; saved and applied immediately for new streams. |

### Microphone

| Control | Label | Config Key | Notes |
|---|---|---|---|
| Slider | Mic Gain | `audio.microphone.gain` | 0.5–4.0, step 0.1. |
| Picker | Input Device | `audio.microphone.device` | PipeWire input-node list; saved and applied to subsequently created capture streams. |

### Equalizer

The Equalizer row opens a dedicated editor. Its stream selector is Media, Nav,
or System. Each stream has ten gain bands, bundled and user presets, a save
action, and a bypass control. The active preset name appears in the Audio-page
row.

## Bluetooth

| Control | Label | Config Key / Source | Notes |
|---|---|---|---|
| Read-only field | Device Name | `connection.bt_name` | Installer-supplied adapter alias; falls back visually to `OpenAutoProdigy`. |
| Toggle | Accept New Pairings | `BluetoothManager.pairable` | Runtime pairable state, not a persisted setting. |
| Action per device | Forget | `PairedDevicesModel` | Removes that paired device; the row also shows connection state. |

## Theme

| Control | Label | Config Key / Source | Notes |
|---|---|---|---|
| Picker | Theme | `display.theme` | Uses theme IDs and display names discovered by `ThemeService`; applies immediately. |
| Toggle | Custom Wallpaper | `display.wallpaper_override` | Clearing the toggle restores the selected theme's wallpaper. |
| Picker | Wallpaper | `display.wallpaper_override` | Visible while Custom Wallpaper is on. |
| Toggle | Always Use Dark Mode | `display.force_dark_mode` | Applies to the UI immediately. |
| Action | Delete Theme | `ThemeService` | Visible only for a user-installed theme; requires confirmation. |

Bundled theme IDs include `default`, `black`, `colors`, `connected-device`,
`ferns`, `leather`, `retro`, `squares`, `vaporwave`, and `waves`. The default
theme's display name is **Prodigy**; configuration stores the ID, not the
display name.

## Companion

This merged page covers External API pairing, live phone state, and API
listener controls. There is no separate External API or legacy Companion page.

### Remote Client Pairing

| Control | Label / State | Notes |
|---|---|---|
| Status + action | PIN / Start Pairing / Cancel Pairing | Opens or cancels the API pairing window when the API listener is running. |
| Image | Pairing QR | Visible only while pairing is active and QR data is available. |

### Phone Status

| Status | Visibility / Meaning |
|---|---|
| Phone Connected / Not Connected | Always shown when companion state is available. |
| GPS | Shown while connected and the location is not stale. |
| Phone Battery | Shown while connected after battery state has been reported. |
| Internet Proxy | Shown when phone-provided internet is available. |
| Route Active | Shows active, degraded, failed, or inactive system-route state. |

### Advanced

| Control | Label | Config Key | Notes |
|---|---|---|---|
| Toggle | External API Enabled | `api.enabled` | Restart required; powers companion, web widgets, and remote clients. |
| Toggle | Allow LAN Clients | `api.expose_lan` | Restart required. |

## System

### General

| Control | Label | Config Key | Notes |
|---|---|---|---|
| Toggle | Left-Hand Drive | `identity.left_hand_drive` | Controls handed UI placement. |
| Toggle | 24-Hour Clock | `display.clock_24h` | Clock-format preference. |
| Toggle | Always Use Dark Mode | `display.force_dark_mode` | Same setting exposed on the Theme page. |

### Day / Night Mode

This section is disabled visually while Always Use Dark Mode is enabled.

| Control | Label | Config Key | Notes |
|---|---|---|---|
| Picker | Source | `sensors.night_mode.source` | `time`, `gpio`, or `none`. |
| Read-only field | Day starts at | `sensors.night_mode.day_start` | Visible for `time`. |
| Read-only field | Night starts at | `sensors.night_mode.night_start` | Visible for `time`. |
| Slider | GPIO Pin | `sensors.night_mode.gpio_pin` | 0–40; visible for `gpio`. |
| Toggle | GPIO Active High | `sensors.night_mode.gpio_active_high` | Visible for `gpio`. |

### Software

| Control | Label | Source | Notes |
|---|---|---|---|
| Read-only field | Version | `Qt.application.version` | Compiled `OAP_VERSION`. |
| Action | Close App | Application action | Opens the exit confirmation dialog. |

## Information

| Section | Control | Label | Config Key |
|---|---|---|---|
| Identity | Read-only field | Head Unit Name | `identity.head_unit_name` |
| Identity | Read-only field | Manufacturer | `identity.manufacturer` |
| Identity | Read-only field | Model | `identity.model` |
| Identity | Read-only field | Car Model | `identity.car_model` |
| Identity | Read-only field | Car Year | `identity.car_year` |
| Hardware | Read-only field | Hardware Profile | `hardware_profile` |
| Hardware | Read-only field | Touch Device | `touch.device` |

## Debug

### Video Decoding

A dynamic list from `CodecCapabilityModel` shows every detected codec. Each row
can enable or disable the codec where permitted, choose software or hardware
mode when available, and select a named decoder when more than one concrete
decoder exists. Configuration is stored under `video.codecs` and
`video.decoder.<codec>`.

### Diagnostics

| Section | Control | Label | Config Key / Source |
|---|---|---|---|
| Logging | Toggle | Verbose Logging | `logging.verbose` |
| Protocol Capture | Toggle | Enable Capture | `connection.protocol_capture.enabled` |
| Protocol Capture | Segmented button | Format | `connection.protocol_capture.format` (`jsonl` / `tsv`) |
| Protocol Capture | Toggle | Include Media Frames | `connection.protocol_capture.include_media` |
| Protocol Capture | Read-only field | Capture Path | `connection.protocol_capture.path` |
| Connection Info | Read-only field | TCP Port | `connection.tcp_port` |
| WiFi Access Point | Read-only field | Channel | `connection.wifi_ap.channel` |
| WiFi Access Point | Read-only field | Band | `connection.wifi_ap.band` |
| AA Protocol Test | Expandable actions | Media and assistant buttons | Enabled only while projection is connected; dispatches `aa.sendButton` through `ActionRegistry`. |

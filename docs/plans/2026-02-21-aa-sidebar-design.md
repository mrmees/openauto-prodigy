# AA Sidebar — Design Document

**Date:** 2026-02-21
**Status:** Approved

## Overview

Use Android Auto's `VideoConfig.margin_width` / `margin_height` protocol fields to make the phone render its UI into a smaller region within the standard video resolution. Crop the decoded video to show only the content area, and use the remaining screen pixels for a configurable sidebar with quick-action controls.

## Background

AA only supports fixed video resolutions (480p, 720p, 1080p, etc.) — no custom dimensions. However, the `VideoConfig` protobuf message includes `margin_width` and `margin_height` fields that the phone respects: it renders content into a centered sub-region with black bars filling the margins. This technique was documented by Emil Borconi (HeadUnit Reloaded / Crankshaft) for handling non-standard screen sizes and portrait displays.

**Reference:** [Crankshaft issue #403](https://github.com/opencardev/crankshaft/issues/403)

## Protocol Constraints

- Margins are advertised during `ServiceDiscoveryResponse` and locked for the session
- No mid-session renegotiation — `AVChannelSetupRequest` is phone→HU only
- `VideoFocusIndication` can pause/resume video but cannot change resolution or margins
- Changing sidebar config requires AA reconnect or app restart

## Margin Math

Given:
- **Display:** 1024×600
- **Sidebar width:** 150px (configurable)
- **AA viewport:** (1024 - sidebarWidth) × 600

For 720p (1280×720) with a 150px sidebar (AA viewport = 874×600):

```
screenRatio = 874 / 600 = 1.457
remoteRatio = 1280 / 720 = 1.778

Since screenRatio < remoteRatio:
  margin_width = round(1280 - (720 × screenRatio))
               = round(1280 - 1049) = 231
  margin_height = 0
```

AA splits `margin_width` evenly — ~115px black bars on left and right of the 1280×720 frame. Content renders in the ~1049×720 center region. The head unit crops the black bars and scales the content to fill the 874×600 viewport.

### General Formula

```
aaViewportWidth = displayWidth - sidebarWidth
screenRatio = aaViewportWidth / displayHeight

if screenRatio < remoteRatio:
    margin_width = round(remoteWidth - (remoteHeight × screenRatio))
    margin_height = 0
else:
    margin_width = 0
    margin_height = round(remoteHeight - (remoteWidth / screenRatio))
```

Where `remoteWidth` / `remoteHeight` are the chosen AA resolution (e.g., 1280×720).

## Architecture

### Config (YAML)

```yaml
video:
  resolution: "720p"
  dpi: 140
  fps: 30
  sidebar:
    enabled: false        # off by default
    width: 150
    position: "right"     # or "left"
    actions:
      - type: "volume"
      - type: "home"
```

New `YamlConfig` methods: `sidebarEnabled()`, `sidebarWidth()`, `sidebarPosition()` with corresponding setters.

### Protocol Layer (VideoService)

`VideoService::fillFeatures()` reads sidebar config and calculates margins:

- Sidebar enabled → compute `margin_width` / `margin_height` from the formula above
- Sidebar disabled → margins = 0 (current behavior)

No new classes. The margin math lives in `VideoService` or a small utility function.

### QML Display (AndroidAutoMenu.qml)

When sidebar is enabled:

```
┌──────────────────────────────┬──────┐
│                              │ VOL  │
│                              │  +   │
│        AA Video              │      │
│     (cropped/scaled)         │ ───  │
│                              │      │
│                              │  🏠  │
│                              │      │
└──────────────────────────────┴──────┘
         874px                  150px
```

- `RowLayout` with video area + sidebar panel
- Layout order reverses when `position: "left"`
- Video uses `fillMode: PreserveAspectCrop` to fill its region and clip the black margin bars
- When sidebar disabled, standard fullscreen layout (no sidebar component)

### Sidebar Content (v1)

The sidebar is a vertical strip of **action slots** — configurable quick-access controls:

- **Volume control** — vertical slider or +/- tap zones
- **Home button** — return to launcher (triggers AA shutdown flow)

Architecture supports future extensibility: brightness, day/night toggle, mute, skip track, custom user-defined actions — all via YAML `actions` list without code changes to the slot framework.

### Touch Handling

Two distinct touch zones during AA with sidebar:

1. **AA viewport:** Evdev touches mapped to AA coordinate space (0–1280 × 0–720), offset by sidebar width if sidebar is on the left side
2. **Sidebar:** Touch zones interpreted directly from evdev coordinates — rectangular hit regions mapped to action callbacks

Since `EVIOCGRAB` is active during AA (touches don't reach Qt/Wayland), the sidebar's touch zones are handled in `EvdevTouchReader` directly, not through Qt's input system. Each action slot defines a rectangular evdev coordinate region and a corresponding action signal.

`EvdevTouchReader` receives sidebar config (enabled, width, position) to:
- Exclude sidebar touches from AA forwarding
- Map sidebar touches to action slot hit zones
- Adjust AA touch coordinate offset for sidebar position

### Toggle Behavior

Sidebar on/off is a configuration change, not a live toggle:

1. User changes sidebar setting in Settings UI
2. Setting saved to YAML config
3. Next AA connection (or app restart) uses new margin values
4. Settings UI displays note: "Changes take effect on next AA connection"

Future improvement: controlled AA reconnect on toggle (graceful shutdown → reconnect with new margins).

## Touch Coordinate Mapping (with sidebar)

### AA Viewport Mapping

With sidebar on right (default), AA viewport occupies pixels 0–873 horizontally:

```
evdev range:     0 ──────────────────────── 4095
display pixels:  0 ────── 873 | 874 ── 1023
                 AA viewport  | sidebar

Touch in AA zone: map evdev X to [0, 1280] AA coords (accounting for margin crop)
Touch in sidebar: match to action slot hit zone, do not forward to AA
```

With sidebar on left, the zones are reversed.

### Sidebar Hit Zones

Each action slot occupies a vertical segment of the sidebar's evdev coordinate range. For a 150px sidebar on the right of a 1024px display:

```
Sidebar evdev X range: (874/1024) × 4095 ≈ 3495 to 4095
Each slot's Y range: evenly divided across display height
```

## Settings UI

New section in Settings (under Display or as its own page):

- **Sidebar toggle** — enable/disable
- **Position** — left / right dropdown
- **Info text** — "Changes take effect on next Android Auto connection"

Action slot configuration is a v2 concern — hardcoded volume + home for v1.

## Files to Modify

| File | Change |
|------|--------|
| `src/core/YamlConfig.cpp/hpp` | Add sidebar config fields and accessors |
| `src/core/aa/VideoService.cpp` | Margin calculation in `fillFeatures()` |
| `qml/applications/android_auto/AndroidAutoMenu.qml` | RowLayout with video + sidebar |
| `qml/applications/android_auto/Sidebar.qml` | New — sidebar action strip component |
| `src/core/aa/EvdevTouchReader.cpp/hpp` | Sidebar exclusion zone + hit zone detection |
| `qml/applications/settings/DisplaySettings.qml` | Sidebar toggle + position selector |
| `docs/aa-video-resolution.md` | New — document resolution handling and margin trick |

## Future Extensibility

- User-configurable action slots via YAML
- More built-in actions: brightness, mute, skip track, day/night, OBD gauges
- Dynamic sidebar width selection
- Mid-session margin renegotiation (if AA protocol allows in future versions)
- Additional AA resolutions (1440p, 4K, portrait variants) from newer protocol versions

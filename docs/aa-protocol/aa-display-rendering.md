# AA Display Rendering Pipeline

Technical reference for how OpenAuto Prodigy renders Android Auto video on a non-standard display with an optional sidebar. Covers all 4 sidebar positions (left, right, top, bottom).

## The Problem

Android Auto only supports fixed video resolutions: 800x480 (480p), 1280x720 (720p), 1920x1080 (1080p). Our target display is 1024x600 — none of these are a native fit. When a sidebar is enabled, the available video area shrinks further. The rendering pipeline must:

1. Tell the phone to render its UI into an appropriately-sized sub-region (margins)
2. Display the received video frames correctly on the physical display (QML crop/fit)
3. Map touch coordinates from the physical touchscreen back to AA coordinate space (evdev mapping)
4. Handle sidebar touch zones for volume and home (evdev hit detection)

## The Margin Trick

The AA protocol's `VideoConfig` message (sent inside `ServiceDiscoveryResponse`) includes `margin_width` and `margin_height` fields. These tell the phone: "render your UI into a centered rectangle that is `(resolution_width - margin_width)` x `(resolution_height - margin_height)`, and fill the rest with black."

The phone still sends full-resolution video frames (e.g., 1280x720), but the actual UI content occupies a smaller centered rectangle within that frame. The surrounding area is black bars.

**Key constraint:** Margins are locked at session start. They are set during `ServiceDiscoveryResponse` and cannot be changed mid-session. Changing sidebar config requires an app restart.

### Where margins are set

`VideoService::fillFeatures()` in `src/core/aa/VideoService.cpp` computes margins via the `calcMargins` lambda and sets them on each `VideoConfig` entry (primary resolution + 480p fallback).

## Margin Calculation

Given:
- `displayW` x `displayH` = physical display pixels (e.g., 1024 x 600)
- `sidebarW` = sidebar width in pixels (e.g., 150), 0 if disabled
- `position` = sidebar position: "left", "right", "top", or "bottom"
- `rW` x `rH` = AA resolution (e.g., 1280 x 720)

```
horizontal = (position == "top" || position == "bottom")

aaViewportW = horizontal ? displayW : (displayW - sidebarW)
aaViewportH = horizontal ? (displayH - sidebarW) : displayH
screenRatio = aaViewportW / aaViewportH
remoteRatio = rW / rH

if screenRatio < remoteRatio:
    marginW = round(rW - (rH * screenRatio))
    marginH = 0
else:
    marginW = 0
    marginH = round(rH - (rW / screenRatio))
```

For **vertical sidebars** (left/right), the sidebar subtracts from display width → typically produces `marginW` (X-crop mode). For **horizontal sidebars** (top/bottom), the sidebar subtracts from display height → typically produces `marginH` (Y-crop mode).

### Worked Example: Right Sidebar, 150px, 720p

```
aaViewportW = 1024 - 150 = 874
aaViewportH = 600
screenRatio = 874 / 600 = 1.4567
remoteRatio = 1280 / 720 = 1.7778

screenRatio < remoteRatio → X-crop:
  marginW = round(1280 - (720 * 1.4567)) = round(231.2) = 231
  marginH = 0
```

Phone renders UI into centered 1049x720 sub-region. ~116px black bars per side.

### Worked Example: Bottom Sidebar, 75px, 720p

```
aaViewportW = 1024
aaViewportH = 600 - 75 = 525
screenRatio = 1024 / 525 = 1.9505
remoteRatio = 1280 / 720 = 1.7778

screenRatio > remoteRatio → Y-crop:
  marginW = 0
  marginH = round(720 - (1280 / 1.9505)) = round(720 - 656.3) = 64
```

Phone renders UI into centered 1280x656 sub-region. ~32px black bars top and bottom.

### Worked Example: No Sidebar, 720p

```
aaViewportW = 1024
aaViewportH = 600
screenRatio = 1024 / 600 = 1.7067
remoteRatio = 1280 / 720 = 1.7778

screenRatio < remoteRatio → X-crop:
  marginW = round(1280 - (720 * 1.7067)) = round(51.2) = 51
  marginH = 0
```

Small 51px total horizontal margin (~26px per side). AA UI nearly fills the frame.

## Content-Space Touch Mapping

**Critical concept:** We send touch coordinates in **content space** — the sub-region the phone actually uses for its UI — NOT full-frame (1280x720) space. The `touch_screen_config` field in `ServiceDiscoveryResponse` is set to match the content dimensions so the phone maps our coordinates correctly.

### touch_screen_config

Set in `ServiceFactory.cpp`. Computed identically to the margin calculation:

```
touchW = 1280, touchH = 720

if sidebar enabled:
    compute margins same way as VideoService
    if marginW > 0:
        touchW -= marginW    (e.g., 1280 - 231 = 1049 for side sidebar)
    else if marginH > 0:
        touchH -= marginH    (e.g., 720 - 64 = 656 for top/bottom sidebar)
```

This tells the phone: "touch coordinates range from 0 to `touchW` x 0 to `touchH`." The phone applies its own mapping from these coordinates to the content region within its rendered frame.

### Why content-space, not frame-space

If we sent coordinates in full 1280x720 space, we'd need to compute `cropAAOffsetX/Y` and add it to every coordinate. Instead, by setting `touch_screen_config` to the content dimensions, the phone handles the offset mapping internally. Our `mapX`/`mapY` simply map from 0 to `visibleAAWidth`/`visibleAAHeight`.

## QML Display Modes

The `VideoOutput` in `AndroidAutoMenu.qml` uses two fill modes:

### No Sidebar: `PreserveAspectFit`

Video frame (1280x720) scaled to fit entirely within the display (1024x600), preserving aspect ratio. Small letterbox bars at top/bottom since 720p is slightly wider than the display.

### With Sidebar: `PreserveAspectCrop`

Video frame scaled to fill the available area, with overflow cropped. This crops away the phone's black margin bars, leaving only the actual AA UI content visible.

### QML Layout

`AndroidAutoMenu.qml` uses absolute positioning (not RowLayout) to support all 4 sidebar positions:

```
Item {
    anchors.fill: parent

    Item {
        id: videoArea
        x: (position == "left" && showSidebar) ? sidebarWidth : 0
        y: (position == "top" && showSidebar) ? sidebarWidth : 0
        width: parent.width - (isVertical && showSidebar ? sidebarWidth : 0)
        height: parent.height - (isHorizontal && showSidebar ? sidebarWidth : 0)
    }

    Sidebar {
        x: based on position (right edge, left edge, or 0 for top/bottom)
        y: based on position (bottom edge, top edge, or videoArea.y for left/right)
        width: isVertical ? sidebarWidth : parent.width
        height: isVertical ? videoArea.height : sidebarWidth
    }
}
```

**Source:** `qml/applications/android_auto/AndroidAutoMenu.qml`

## Crop Modes: X-Crop vs Y-Crop

### X-Crop (Side Sidebar — left/right)

Video fills display height, X overflow cropped. Used when `marginW > 0`.

```
effectiveDisplayW = displayW - sidebarW
scale = displayH / aaHeight                         = 600 / 720 = 0.8333
totalVideoWidthPx = aaWidth * scale                 = 1280 * 0.8333 = 1066.7
cropPx = (totalVideoWidthPx - effectiveDisplayW) / 2 = (1066.7 - 874) / 2 = 96.3

cropAAOffsetX = cropPx / scale                      = 96.3 / 0.8333 = 115.6
visibleAAWidth = effectiveDisplayW / scale           = 874 / 0.8333 = 1048.8
visibleAAHeight = aaHeight                           = 720

videoEvdev spans effectiveDisplayW in X, full displayH in Y
```

Touch mapping: `mapX(rawX) = clamp((rawX - videoEvdevX0) / videoEvdevW, 0, 1) * visibleAAWidth`

### Y-Crop (Horizontal Sidebar — top/bottom)

Video fills display width, Y overflow cropped. Used when `marginH > 0`.

```
effectiveDisplayH = displayH - sidebarW
scale = displayW / aaWidth                          = 1024 / 1280 = 0.8
totalVideoHeightPx = aaHeight * scale               = 720 * 0.8 = 576
cropPx = (totalVideoHeightPx - effectiveDisplayH) / 2 = (576 - 525) / 2 = 25.5

cropAAOffsetY = cropPx / scale                      = 25.5 / 0.8 = 31.9
visibleAAWidth = aaWidth                             = 1280
visibleAAHeight = effectiveDisplayH / scale          = 525 / 0.8 = 656.3

videoEvdev spans full displayW in X, effectiveDisplayH in Y
```

Touch mapping: `mapY(rawY) = clamp((rawY - videoEvdevY0) / videoEvdevH, 0, 1) * visibleAAHeight`

### Fit Mode (No Sidebar)

Video is aspect-fit into full display. Typically produces small letterbox bars. `visibleAAWidth = aaWidth`, `visibleAAHeight = aaHeight`. Touch maps linearly from video's evdev region to full AA range.

## Sidebar Touch Zones

When sidebar is active, `EvdevTouchReader` defines exclusion zones in evdev coordinate space. Touches in the sidebar region are consumed locally (not forwarded to AA). The sidebar has two functional zones: **volume** (continuous drag) and **home** (tap).

**Important:** During AA, `EVIOCGRAB` routes all touch exclusively through the evdev reader. QML `MouseArea` elements receive no events. All sidebar interaction is handled by evdev hit-zone detection in C++.

### Vertical Sidebar (left/right)

Detection: touch X falls within sidebar X-band (`sidebarEvdevX0` to `sidebarEvdevX1`).

Sub-zones divided by Y position within the X-band:

| Zone | Y Range | Function |
|------|---------|----------|
| Volume | Top 70% of display | Drag maps Y → 0-100% volume (top=100%) |
| Gap | 70%-75% | Dead zone between controls |
| Home | Bottom 25% | Tap triggers `sidebarHome()` |

Left sidebar: X-band is `[0, sidebarW * evdevPerPixelX]`.
Right sidebar: X-band is `[(displayW - sidebarW) * evdevPerPixelX, screenWidth]`.

### Horizontal Sidebar (top/bottom)

Detection: touch Y falls within sidebar Y-band (`sidebarEvdevY0` to `sidebarEvdevY1`).

Sub-zones divided by X position within the Y-band:

| Zone | X Range | Function |
|------|---------|----------|
| Volume | Left edge to ~100px from right edge | Drag maps X → 0-100% volume (right=100%) |
| Home | Rightmost ~80px | Tap triggers `sidebarHome()` |

Top sidebar: Y-band is `[0, sidebarW * evdevPerPixelY]`.
Bottom sidebar: Y-band is `[(displayH - sidebarW) * evdevPerPixelY, screenHeight]`.

### Volume Mapping

Vertical: `volume = (1.0 - relY) * 100` (top = max)
Horizontal: `volume = relX * 100` (right = max)

Supports continuous drag — slot tracked via `sidebarDragSlot_`, updates sent as `sidebarVolumeSet()` signals.

### Touch Routing Logic

On each `SYN_REPORT`, for every active dirty slot:
1. Check if touch falls in sidebar zone (X-band for vertical, Y-band for horizontal)
2. Sidebar touches are processed (volume/home) and their dirty flag cleared
3. If ALL touches are sidebar touches, AA processing is skipped entirely
4. If some touches are on video and some on sidebar, only non-sidebar touches forwarded to AA

## Debug Touch Overlay

`AndroidAutoMenu.qml` includes a debug overlay that draws green crosshairs at each active touch point. Controlled by `TouchHandler.debugOverlay` (defaults to **false**).

The overlay maps content-space coordinates back to screen position using `TouchHandler.contentWidth` and `TouchHandler.contentHeight` (set by EvdevTouchReader after `computeLetterbox()`). This accounts for sidebar margins and crop mode.

**Note:** The phone also draws its own touch indicators (if "Show taps" is enabled in Developer Options). These are drawn in full-frame space and may appear misaligned with actual content — that's a phone-side rendering issue, not ours.

## Gotchas

- **Margins are locked at session start.** The phone negotiates video config once during `ServiceDiscoveryResponse`. Changing sidebar settings requires restarting the app.

- **EVIOCGRAB blocks all QML touch input.** During AA, sidebar interaction is purely C++ evdev hit-zone detection. QML sidebar controls are visual only on Pi.

- **PreserveAspectCrop must match crop touch mapping.** If QML uses crop but touch mapping uses fit-mode math (or vice versa), touches will land at wrong positions. The `sidebarEnabled_` flag in `computeLetterbox()` gates which branch executes.

- **Touch coordinates are content-space, not frame-space.** `mapX()`/`mapY()` return values in `[0, visibleAAWidth]`/`[0, visibleAAHeight]`, NOT `[0, 1280]`/`[0, 720]`. The `cropAAOffsetX/Y` values are computed but NOT added to mapped coordinates — the phone handles offset mapping because `touch_screen_config` is set to content dimensions.

- **Sidebar position affects video origin in evdev space.** A left sidebar shifts the video area rightward; a top sidebar shifts it downward. `computeLetterbox()` accounts for this via `effectiveDisplayX0`/`effectiveDisplayY0`.

- **The 480p fallback gets its own margin calculation.** `VideoService::fillFeatures()` calls `calcMargins()` separately for each resolution config.

- **Phone's `config_index` in `AVChannelSetupRequest` may not match our config list indices.** The phone consistently sends `config_index=3` even though we only advertise 2 configs. We always respond with `add_configs(0)` regardless.

- **Horizontal sidebar touch zones must match QML visual layout.** The home button in horizontal layout is a small icon (~56px) at the right end. If the evdev home zone is too wide (e.g., 25% of screen), volume drags near the right side trigger home instead. Current zones: volume extends to ~100px from right edge, home is rightmost ~80px.

## Source Files

| File | Role |
|------|------|
| `src/core/aa/VideoService.cpp` | Margin calculation, `ServiceDiscoveryResponse` video config |
| `src/core/aa/ServiceFactory.cpp` | `touch_screen_config` dimensions (content-space) |
| `src/core/aa/EvdevTouchReader.cpp` | Touch mapping (fit/X-crop/Y-crop), sidebar hit zones, evdev I/O |
| `src/core/aa/EvdevTouchReader.hpp` | Field definitions, coordinate space constants |
| `src/core/aa/TouchHandler.hpp` | Touch → AA protobuf bridge, content dimensions, debug overlay |
| `qml/applications/android_auto/AndroidAutoMenu.qml` | QML layout, `VideoOutput` fill mode, debug overlay |
| `qml/applications/android_auto/Sidebar.qml` | Sidebar visual layout (vertical + horizontal variants) |

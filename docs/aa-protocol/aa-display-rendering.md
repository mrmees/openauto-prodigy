# Android Auto Display Rendering Pipeline

This guide follows the current path from the shell viewport through AA service
discovery, decoded-frame rendering, and evdev touch ownership.

## Coordinate spaces

The pipeline keeps four related spaces distinct:

| Space | Owner | Purpose |
|---|---|---|
| Display pixels | `DisplayInfo` and the shell | Window layout and pixel-to-evdev conversion |
| Usable AA viewport | Shell/Navbar policy | Display area left after an optional Navbar inset |
| Encoded video mode | AA `VideoConfig` | Full frame sent by the phone |
| AA content space | Service discovery and touch | Encoded mode minus negotiated margins |

The detected display size is preferred; 1024×600 is only the fallback used
when detection is unavailable. The selected AA mode is independently
configured as 800×480, 1280×720, or 1920×1080.

## Shell viewport and Navbar policy

The shell anchors its content host inside the visible Navbar. A top or bottom
Navbar contributes a vertical margin; a left or right Navbar contributes a
horizontal margin. The AA plugin uses that content host like any other plugin.

`navbar.show_during_aa` defaults to `true`. In that mode the Navbar stays
visible, the AA plugin does not request fullscreen, and service discovery
subtracts the DPI-scaled Navbar thickness from the same display edge. When the
setting is false, the plugin requests fullscreen, the Navbar is hidden, and AA
uses the full display viewport.

The Navbar visibility setting is restart-required because the advertised
video margins and input coordinate space are established at session startup.

## Discovery and content dimensions

`ServiceDiscoveryBuilder` calculates content dimensions from the selected
encoded mode and the usable viewport aspect ratio. It writes the difference
between the full mode and the centered content region to
`VideoConfig.margin_width`/`margin_height`.

The builder advertises only the selected landscape mode, repeated once per
enabled recognized codec. The default configs are H.264 and H.265 at 720p,
30 fps, and DPI 140. There is no automatically advertised 480p fallback when a
higher mode is selected.

The input descriptor repeats the same calculation and advertises
`touch_screen_config` as the content dimensions. This shared calculation is
the contract that keeps phone layout, frame cropping, and touch mapping in the
same coordinate space.

For the formulas and illustrative calculations, see
[Android Auto Video Resolution and Margins](aa-video-resolution.md).

## Decoding and QML rendering

`AndroidAutoOrchestrator` queues video-channel data to `VideoDecoder`. The
decoder recognizes H.264 and H.265 Annex B bitstreams, selects hardware decode
when available, falls back to software decode, and publishes the newest decoded
`QVideoFrame` to the QML video sink.

The decoder object persists across AA sessions, but every video
`streamStarted` edge places an ordered reset command in its worker queue. That
barrier discards queued packets from the prior stream and resets codec/parser,
first-frame fallback, latest-frame, and detection state before any later frame
is processed. Video packets enter that queue directly in signal-emission order,
so an old Qt event cannot arrive behind the boundary. A reconnect may therefore
negotiate H.264 or H.265 independently of the preceding session without
restarting the application.

The AA projection view has a black background and one `VideoOutput` anchored to
its parent. Its fill mode is always `PreserveAspectCrop`, whether the Navbar is
shown or hidden. The negotiated content aspect matches the usable viewport;
cropping the full encoded frame therefore removes the phone-rendered margin
bars and fills that viewport. There is no separate fit-mode branch.

The optional debug overlay maps `TouchHandler`'s content-space coordinates
back over the rendered viewport. It is diagnostic only and defaults off.

## Evdev mapping

`AndroidAutoRuntimeBridge` creates `EvdevTouchReader`, gives it the detected
display dimensions, selected AA mode, Navbar edge/thickness, and the shared
content dimensions, then creates `EvdevCoordBridge` for shell-zone
registration. All initial configuration is complete before the reader thread
starts. Later display, negotiated-video, grab, and stop requests are snapshots
or atomics consumed by that reader thread; its descriptor and touch-slot state
are never mutated by the Qt main thread.

`EvdevTouchReader::computeLetterbox()` first removes the Navbar strip from the
display viewport. It compares that viewport with the content aspect ratio,
computes the rendered content rectangle in display pixels, converts it to
evdev coordinates, and maps unclaimed touches linearly into
`0..contentWidth × 0..contentHeight`.

Touch is grabbed with `EVIOCGRAB` when a connected AA session owns the
projection view. It is ungrabbed when projection backgrounds, disconnects, or
the plugin view is deactivated, returning normal shell input to Wayland/Qt.
Poll errors, hangups, and short reads close the lost device and enter a paced
reopen loop. Ungrab, device loss, and shutdown retire any phone-visible touch
stream with valid pointer-up/up messages before clearing local state. A stop
request wakes the reconnect wait immediately.

## Navbar and popup zone ownership

While the evdev device is grabbed, Navbar `MouseArea`s do not receive Pi touch
events. `NavbarController` registers the three Navbar control regions through
`EvdevCoordBridge`; QML reports popup geometry and the controller registers the
corresponding slider/button/dismiss regions. The bridge converts those pixel
rectangles to evdev coordinates and supplies them to `TouchRouter`.

Routing has these properties:

- Higher-priority overlapping zones win.
- Only a down event can claim a zone.
- A claim is sticky to its touch slot through move and up, even if the finger
  leaves the original rectangle.
- Claimed Navbar or popup touches are consumed locally and are not forwarded to
  AA.
- Unclaimed touches fall through to AA.
- Phone-visible pointer membership is independent of raw active slots, so a
  claimed pointer cannot leak into another pointer's AA motion array.
- Multiple downs or ups in one evdev report are serialized into Android
  MotionEvent order with complete pointer arrays and correct action indices.
- The three-finger gesture can suppress AA forwarding without preventing zone
  dispatch.

This keeps Navbar gestures and popup controls operational during projection
without building shell-specific hit testing into the AA touch reader.

## Session and configuration implications

- Resolution, codec configs, video margins, and `touch_screen_config` are fixed
  by the current service-discovery response for the lifetime of a session.
- `video.resolution` and `video.fps` changes force an active-session reconnect.
- Navbar edge/visibility changes are not a live AA viewport feature today; the
  settings surface treats visibility as restart-required.
- Wire ID `0x8012` currently carries the HU's response to phone-supplied theming
  tokens. It is not the mechanism for live Navbar margin changes.

## Current implementation sources

| File | Role |
|---|---|
| `src/core/aa/ServiceDiscoveryBuilder.cpp` | Selected-mode configs, Navbar viewport, margins, content-sized input descriptor |
| `src/core/aa/AndroidAutoOrchestrator.cpp` | Per-session discovery and decode wiring |
| `src/core/aa/VideoDecoder.cpp` | H.264/H.265 detection, decode, latest-frame delivery |
| `src/core/aa/AndroidAutoRuntimeBridge.cpp` | Display/Navbar setup, touch-reader lifecycle, evdev bridge |
| `src/core/aa/EvdevTouchReader.cpp` | Usable-viewport and content-space mapping, grab lifecycle |
| `src/core/aa/TouchRouter.cpp` | Priority and sticky per-slot zone claims |
| `src/core/aa/EvdevCoordBridge.cpp` | Pixel-to-evdev zone conversion |
| `src/ui/NavbarController.cpp` | Navbar/popup zone registration and local actions |
| `qml/components/Shell.qml` | Navbar-aware plugin viewport |
| `qml/components/Navbar.qml` | Navbar visibility, edge layout, and popup geometry reporting |
| `qml/applications/android_auto/` | Projection surface, crop fill mode, and debug overlay |

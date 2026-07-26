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

### Experimental runtime CLUSTER viewport

When the default-off projected CLUSTER experiment is enabled, its independent
display starts with the accepted 800×480 H.264 carrier at 30 fps and 140 DPI.
The baseline CLUSTER video configuration declares total margins of 500
horizontal and 180 vertical pixels, asking the phone to render a centered
300×300 content rectangle at source offset (250, 90). Its paired input
descriptor remains capability-empty; the widget does not accept touch or other
projected input.

Debug Settings provides a runtime CLUSTER lab while the experiment is enabled.
It can stage 480p or 720p, DPI 80–640, any positive centered content rectangle
that fits the carrier with even total margins, a bounded requested GAL choice
(`1.7` or `4.3`), and the 4.3-only `native_turn_card_available` declaration.
One accepted complete profile updates one typed snapshot and, only when it is a
real change while projection is active, gracefully reconnects the Android Auto
session. The replacement discovery descriptor, decoded-frame validation, and
QML crop activate from that same generation immediately before the new session
starts. Invalid and unchanged profiles do not reconnect. No YAML edit, Prodigy
restart, or `ServiceDiscoveryUpdate` is involved; overrides last only for the
current application process and reset returns to its startup profile.

The same path is available to local integrations as
`aa.cluster.applyProfile` with a complete map payload (`resolution`, `dpi`,
`content_width`, `content_height`, `gal_version`, and
`native_turn_card_available`) and `aa.cluster.resetProfile`. External API v1
can dispatch those registered actions with `payload_json`; dispatch confirms
that the handler ran, while the provider diagnostics describe profile
acceptance. `native_turn_card_available` is an honest HU declaration only: it
says that the CLUSTER descriptor can host the native turn-card UI element. It
does not render a turn card, select phone content, or promise that the phone
will use the declaration.

### GAL and per-video UI policy

The requested tuple, not the phone-reported compatible tuple, is the sole
local input to descriptor and UI-feature policy:

| Requested GAL | Version-response admission | Session clock | `VideoConfig` field 11 |
|---|---|---|---|
| 1.7 (default) | Raw status `MATCH` is sufficient; the reported tuple is legacy status-only. | Existing navbar clock bit is set only when the Navbar clock is enabled. | Absent from every MAIN and CLUSTER configuration. Native-turn-card true is rejected. |
| 4.3 (lab) | Raw status `MATCH` plus a numerically equal-or-higher reported tuple. | The session bit is clear. | Each selectable MAIN codec configuration gets one `UI_ELEMENT_CLOCK` only when the Navbar clock is enabled. The CLUSTER configuration gets one `UI_ELEMENT_NAVIGATION_TURN_DATA_AVAILABLE` only when `native_turn_card_available` is true. |

The removed session-configuration value 16 is never emitted. Field 11 is
limited to those two hidden-UI declarations; it does not populate unsupported
`AdditionalVideoConfig` fields 1–4 or 6–8, alter display roles, select a
turn-card service, or change the established video, input, audio, ACK, focus,
or touch contracts.

Version diagnostics retain the complete fixed response prefix: reported major,
minor, raw 16-bit status, and every byte after that six-byte prefix. A short
prefix is reported as malformed and fails before TLS without fabricated values.
For a parseable prefix, logs include requested/reported tuples, raw status,
trailing length, a bounded parsed configuration summary when applicable, and a
bounded opaque-byte prefix; trailing bytes are non-fatal.

Task 0's deployed Pi/Pixel baseline captured request `1.7`, response
`1.7/MATCH`, no trailing bytes, and healthy simultaneous MAIN+CLUSTER media.
The corrected request-only checkpoint then captured requested `4.3` with the
same Pixel reporting `6.0/MATCH`; that compatible response proceeded through
the established projection path, while requested `4.3` remained authoritative
locally. This is request/response and media evidence only. The final
field-11/hidden-UI hardware matrix has not yet passed and is not implied here.

The fixed 3×3 dashboard widget uses one `VideoOutput` inside a centered clipped
viewport sized to the active content aspect (a square for the 300×300
baseline). It uniformly scales and offsets the full decoded carrier so only
the active content rectangle is visible. This is ordinary texture geometry:
there is no second decoder, CPU frame crop, shader, enhancement, stretch, or
nonstandard encoded resolution. The path does not change MAIN projection's
`PreserveAspectCrop` behavior or generalize display type/configuration beyond
the single experimental CLUSTER.

Geometry and DPI are experimental layout inputs, not a claimed map-versus-turn
card selector. A 2026-07-25 Pixel 8/Android Auto 17.3 live matrix exercised the
baseline, turn-data bit, 480p/720p carriers, full-frame/square geometry, and
80/140/280 DPI under three phone states: active Maps route, no route, and no
route with YouTube Music actively playing. All 24 captures remained Google
Maps. Active navigation added the route UI; stopping navigation removed it;
media playback never replaced the map. The turn-data bit did not produce a
visible mode change. Resolution, geometry, and DPI changed framing or scale as
advertised. This hardware result agrees with the static finding that phone
policy selects CLUSTER content and runtime message 26 cannot add or replace
AV/CLUSTER services.

The follow-up AUXILIARY role-swap produced a different, deterministic result.
AA 17.3 accepted MAIN ID 0 plus AUXILIARY ID 1 on the existing channel 12/13
pair. With AV field 8 omitted (`KEYCODE_UNKNOWN`), the phone opened and started
the stream but sent only the codec header and no decodable frame. Advertising
`KEYCODE_TURN_CARD` (65544) kept the stream idle without a route, including
while YouTube Music played, then produced a compact maneuver card as soon as a
Maps route became active. Media never replaced or populated the AUXILIARY
surface. The session-bit-16 A/B made no content change, matching the corrected
17.3 trace.

Maps 26.30.05 publishes separate CLUSTER and AUXILIARY projection services.
Its decompiled routing path identifies AV field 8 as the AUXILIARY initial
content selector: `KEYCODE_NAVIGATION` (65538) selects a limited navigation
map and `KEYCODE_TURN_CARD` selects a turn-card service/fallback. The local
hands-off protocol enum lacks `KEYCODE_NAVIGATION`, and the available Maps
report traces this selector through AA 16.2/16.4 rather than 17.3. The current
17.3 confirmation and enum/provenance update are tracked upstream in
open-android-auto issue #14; Prodigy has not patched the submodule locally.

## Evdev mapping

`AndroidAutoRuntimeBridge` creates `EvdevTouchReader`, gives it the detected
display dimensions, selected AA mode, Navbar edge/thickness, and the shared
content dimensions, then creates `EvdevCoordBridge` for shell-zone
registration. All initial configuration is complete before the reader thread
starts. Later display, negotiated-video, grab, and stop requests are snapshots
or atomics consumed by that reader thread; its descriptor and touch-slot state
are never mutated by the Qt main thread. A display-size change recomputes the
content mapping from the same current viewport contract before the next reader
snapshot, keeping service-discovery and input coordinates aligned.

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
- Multiple transitions in one evdev report are serialized into Android
  MotionEvent order with complete pointer arrays and correct action indices;
  existing contacts retire before new contacts are admitted.
- The three-finger gesture can suppress AA forwarding without preventing zone
  dispatch.

This keeps Navbar gestures and popup controls operational during projection
without building shell-specific hit testing into the AA touch reader.

## Session and configuration implications

- Resolution, codec configs, video margins, and `touch_screen_config` are fixed
  by the current service-discovery response for the lifetime of a session.
- `video.resolution` and `video.fps` changes force an active-session reconnect.
- Runtime CLUSTER profile changes use that same reconnect boundary but do not
  restart Prodigy or persist to YAML.
- Navbar edge/visibility changes are not a live AA viewport feature today; the
  settings surface treats visibility as restart-required.
- Wire ID `0x8012` currently carries the HU's response to phone-supplied theming
  tokens. It is not the mechanism for live Navbar margin changes.

## Current implementation sources

| File | Role |
|---|---|
| `src/core/aa/ServiceDiscoveryBuilder.cpp` | Selected-mode configs, Navbar viewport, margins, content-sized input descriptor |
| `src/core/aa/ProjectedDisplaySession.cpp` | Role-safe CLUSTER geometry properties and decoded-carrier validation |
| `src/core/aa/AndroidAutoOrchestrator.cpp` | Per-session discovery and decode wiring |
| `src/core/aa/VideoDecoder.cpp` | H.264/H.265 detection, decode, latest-frame delivery |
| `src/core/aa/AndroidAutoRuntimeBridge.cpp` | Display/Navbar setup, touch-reader lifecycle, evdev bridge |
| `src/core/aa/EvdevTouchReader.cpp` | Usable-viewport and content-space mapping, grab lifecycle |
| `src/core/aa/TouchRouter.cpp` | Priority and sticky per-slot zone claims |
| `src/core/aa/EvdevCoordBridge.cpp` | Pixel-to-evdev zone conversion |
| `src/ui/NavbarController.cpp` | Navbar/popup zone registration and local actions |
| `src/plugins/android_auto/AAClusterWidgetRegistration.cpp` | Fixed 3×3 experimental CLUSTER widget registration |
| `qml/components/Shell.qml` | Navbar-aware plugin viewport |
| `qml/components/Navbar.qml` | Navbar visibility, edge layout, and popup geometry reporting |
| `qml/applications/android_auto/` | Projection surface, crop fill mode, and debug overlay |
| `qml/applications/settings/DebugSettings.qml` | Runtime CLUSTER profile controls and diagnostics |
| `qml/widgets/AAClusterWidget.qml` | Single-output centered CLUSTER crop and upsize geometry |

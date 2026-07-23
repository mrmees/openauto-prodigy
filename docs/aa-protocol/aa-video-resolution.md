# Android Auto Video Resolution and Margins

## Shipped negotiation model

Android Auto uses enumerated video modes rather than arbitrary encoded-frame
dimensions. OpenAuto Prodigy selects one configured landscape mode and
advertises one `VideoConfig` for each enabled, recognized codec. All advertised
configs therefore have the same resolution, frame rate, density, and margins;
only the codec differs.

| `video.resolution` | Protocol enum | Encoded dimensions |
|---|---|---:|
| `480p` | `VIDEO_800x480` | 800×480 |
| `720p` (default) | `VIDEO_1280x720` | 1280×720 |
| `1080p` | `VIDEO_1920x1080` | 1920×1080 |

The protocol enum also defines 1440p, 4K, and portrait modes. The shipped
service-discovery path does not select or advertise those modes.

The default codec list is H.264/AVC plus H.265/HEVC. Service discovery also
recognizes configured `vp9` and `av1` names and advertises their protocol enum
values. The shipped decoder, however, detects only H.264 and H.265 bitstreams;
an unrecognized stream is treated as H.264. Keep `video.codecs` limited to
H.264/H.265 until the receive path gains end-to-end VP9/AV1 support. If the
configured list contains no service-discovery-recognized entry, discovery
falls back to one H.264 config.

The relevant `VideoConfig` fields are:

```protobuf
message VideoConfig {
    optional VideoResolution.Enum video_resolution = 1;
    optional VideoFPS.Enum video_fps = 2;
    optional uint32 margin_width = 3;
    optional uint32 margin_height = 4;
    optional uint32 dpi = 5;
    optional MediaCodecType.Enum codec = 10;
}
```

`margin_width` and `margin_height` are total margins across both opposing
edges. For example, `margin_width = 100` describes approximately 50 encoded
pixels on each side of a centered content region.

## Navbar-aware viewport

`AndroidAutoRuntimeBridge` obtains the display dimensions from `DisplayInfo`
and supplies them to service discovery and touch mapping. If detection is not
available, the built-in fallback is 1024×600. This fallback is not an AA
protocol invariant.

The bridge also computes the Navbar thickness from display DPI and `ui.scale`,
with 56 pixels as the fallback. `navbar.show_during_aa` defaults to `true`, and
`navbar.edge` defaults to `bottom`.

For a Navbar shown during AA, the usable viewport subtracts that thickness on
the Navbar's axis:

```text
viewportWidth  = displayWidth
viewportHeight = displayHeight

if navbar is shown during AA:
    if edge is top or bottom:
        viewportHeight -= navbarThickness
    else:
        viewportWidth -= navbarThickness
```

When `navbar.show_during_aa` is false, the AA plugin requests fullscreen and
the full detected display is the viewport. Margins may still be nonzero when
the full display aspect ratio differs from the selected AA mode.

## Content and margin calculation

`ServiceDiscoveryBuilder::computeContentDimensions()` is the shared
calculation used by video discovery and touch setup:

```text
viewportRatio = viewportWidth / viewportHeight
videoRatio    = videoWidth / videoHeight

contentWidth  = videoWidth
contentHeight = videoHeight

if viewportRatio < videoRatio:
    contentWidth = videoWidth
                 - round(videoWidth - videoHeight * viewportRatio)
else if viewportRatio > videoRatio:
    contentHeight = videoHeight
                  - round(videoHeight - videoWidth / viewportRatio)

marginWidth  = videoWidth  - contentWidth
marginHeight = videoHeight - contentHeight
```

The phone still sends the full encoded mode. It renders AA into the centered
`contentWidth × contentHeight` region and leaves the negotiated margins around
it. The projection view then crops the encoded frame to fill the shell's usable
viewport.

### Illustrative fallback-display examples

With a 1024×600 display, a 56-pixel bottom Navbar, and the default 1280×720
mode, the usable viewport is 1024×544. The calculation produces content
dimensions 1280×680 and a total vertical margin of 40 encoded pixels.

With the same display and a 56-pixel left Navbar, the usable viewport is
968×600. The calculation produces content dimensions 1162×720 and a total
horizontal margin of 118 encoded pixels.

With the Navbar hidden, the full 1024×600 viewport produces content dimensions
1229×720 and a total horizontal margin of 51 encoded pixels. This illustrates
why "Navbar hidden" does not imply zero video margins.

## Touch coordinate contract

The input descriptor advertises `touch_screen_config` using the same content
dimensions as the selected video mode. `EvdevTouchReader` maps the usable
display viewport into that content coordinate space; it does not map to the
full encoded-frame dimensions and does not add the centered margin offset.

Touches claimed by registered shell zones are consumed locally. Unclaimed
touches fall through to AA, with all active pointers included in the AA motion
message. Phone-visible membership is tracked separately from raw evdev slot
activity, so a slot claimed by a local zone never appears in an AA pointer
array. If evdev batches multiple transitions in one `SYN_REPORT`, the reader
still emits the Android ordering (`DOWN`, then `POINTER_DOWN`; `POINTER_UP`,
then `UP`) with the changed pointer's array index.

## Session lifetime

The shipped path sets the resolution, codec configs, margins, and touch-space
dimensions during service discovery. Those values remain fixed for that AA
session. Changing `video.resolution` or `video.fps` during an active session
causes a disconnect/reconnect so the phone negotiates the new values; the
Navbar visibility setting is marked restart-required in the settings UI.
Frame and content dimensions are published to the reader as one mapping
snapshot, retaining the DPI-scaled Navbar thickness used by discovery.

Wire ID `0x8012` is not a runtime margin-update request. The current gold-traced
definition is the HU's theming-token status response to the phone's `0x8011`
request. Runtime UI configuration, if implemented later, must be designed from
the separately traced update-config messages rather than the superseded
`0x8012` interpretation.

## Current implementation sources

- `src/core/aa/ServiceDiscoveryBuilder.cpp` — video configs, margins, and input
  content dimensions
- `src/core/aa/AndroidAutoOrchestrator.cpp` — detected display/Navbar inputs and
  per-session discovery
- `src/core/aa/AndroidAutoRuntimeBridge.cpp` — display, Navbar, and touch setup
- `src/core/aa/VideoDecoder.cpp` — codec detection and decoder selection
- `src/core/aa/EvdevTouchReader.cpp` — content-space touch mapping
- `qml/components/Shell.qml` and `qml/components/Navbar.qml` — usable shell
  viewport and Navbar visibility

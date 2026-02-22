# Android Auto Video Resolution & Margins

## Resolution Negotiation

AA uses fixed, enumerated resolutions — no custom dimensions. During service discovery, the head unit advertises supported `VideoConfig` entries in the `ServiceDiscoveryResponse`. The phone picks one and begins streaming H.264 video at that resolution.

### Available Resolutions

| Enum Value | Dimensions | Notes |
|-----------|-----------|-------|
| `_480p` | 800×480 | Smallest, good for low-power devices |
| `_720p` | 1280×720 | **Default for OpenAuto Prodigy** |
| `_1080p` | 1920×1080 | High quality, more decode overhead |
| `_1440p` | 2560×1440 | Newer protocol versions |
| `_4K` | 3840×2160 | Newer protocol versions |
| `_720p_p` | 720×1280 | Portrait mode |
| `_1080p_p` | 1080×1920 | Portrait mode |
| `_1440p_p` | 1440×2560 | Portrait mode |
| `_4K_p` | 2160×3840 | Portrait mode |

Portrait resolutions were added around Android 8+ (late 2022). Our aasdk protobuf may need updating to include enum values 4-9 for full support.

### VideoConfig Fields

```protobuf
message VideoConfig {
    required VideoResolution.Enum video_resolution = 1;
    required VideoFPS.Enum video_fps = 2;
    required uint32 margin_width = 3;
    required uint32 margin_height = 4;
    required uint32 dpi = 5;
    optional uint32 additional_depth = 6;
}
```

- `video_fps`: `_30` or `_60`
- `dpi`: Screen density (informational, affects font/icon sizing on phone)
- `margin_width` / `margin_height`: **The key to non-standard screen sizes** (see below)

## The Margin Trick

The phone respects `margin_width` and `margin_height` — it renders its UI into a centered sub-region within the chosen resolution, filling the margins with black. This allows head units with non-standard aspect ratios to receive correctly-proportioned AA content without distortion.

### How It Works

1. Head unit calculates margin values based on its screen aspect ratio vs the chosen AA resolution
2. Margins are included in the `VideoConfig` during service discovery
3. Phone renders AA UI in the center region, black bars in the margins
4. Head unit crops/overscan the decoded video to show only the content area
5. Touch coordinates are remapped to the content region

### Margin Calculation

```
aaViewportWidth = displayWidth - sidebarWidth  (or displayWidth if no sidebar)
screenRatio = aaViewportWidth / displayHeight
remoteRatio = remoteWidth / remoteHeight       (e.g., 1280/720 = 1.778)

if screenRatio < remoteRatio:
    // Screen is "taller" than AA resolution — add horizontal margins
    margin_width = round(remoteWidth - (remoteHeight × screenRatio))
    margin_height = 0
else:
    // Screen is "wider" than AA resolution — add vertical margins
    margin_width = 0
    margin_height = round(remoteHeight - (remoteWidth / screenRatio))
```

AA splits margins evenly: `margin_width = 200` means 100px black bar on left and 100px on right.

### Example: 1024×600 Display with 150px Sidebar

AA viewport = 874×600, using 720p (1280×720):

```
screenRatio = 874/600 = 1.457
remoteRatio = 1280/720 = 1.778

margin_width = round(1280 - (720 × 1.457)) = 231
margin_height = 0

AA renders: 231px total horizontal margin (115px each side)
Content region: ~1049×720 (centered)
Scaled to display: 874×600
```

### Example: 480×800 Portrait Display (from Crankshaft)

Using 720p (1280×720):

```
screenRatio = 480/800 = 0.6
remoteRatio = 1280/720 = 1.778

margin_width = round(1280 - (720 × 0.6)) = 848
margin_height = 0

Content region: ~432×720 (centered)
Scaled to display: 480×800
```

## Protocol Constraints

- **Margins are locked for the session.** They're advertised during `ServiceDiscoveryResponse` and cannot be changed mid-session.
- `AVChannelSetupRequest` is phone→HU only — the head unit cannot initiate renegotiation.
- `VideoFocusIndication` can pause/resume video but cannot change resolution or margins.
- Changing margin config requires an AA reconnect or app restart.

## Implementation in OpenAuto Prodigy

Margins are set in `VideoService::fillFeatures()` (`src/core/aa/VideoService.cpp`). The sidebar config is read from YAML:

```yaml
video:
  resolution: "720p"
  sidebar:
    enabled: true
    width: 150
    position: "right"
```

When `sidebar.enabled` is true, margins are calculated from the formula above. When false, margins are 0 (standard fullscreen behavior).

## Touch Coordinate Impact

With margins, the AA content occupies a sub-region of the video frame. Touch mapping must account for:

1. **Crop offset**: Touches in the content region map to full AA coordinate space (0–remoteWidth × 0–remoteHeight)
2. **Sidebar exclusion**: Touches in the sidebar region are not forwarded to AA
3. **Position awareness**: If sidebar is on the left, the AA viewport is offset by `sidebarWidth` pixels

## References

- [Crankshaft issue #403](https://github.com/opencardev/crankshaft/issues/403) — Emil Borconi's margin/overscan technique
- `libs/aasdk/aasdk_proto/VideoConfigData.proto` — VideoConfig protobuf definition
- `libs/aasdk/aasdk_proto/VideoResolutionEnum.proto` — Resolution enum values
- `src/core/aa/VideoService.cpp` — Where margins are configured

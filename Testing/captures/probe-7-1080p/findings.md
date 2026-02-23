# Probe 7 Findings: 1080p Video Resolution

## Change Made
Config only: `~/.openauto/config.yaml` video resolution changed from `480p` to `1080p`.
No code changes — VideoService already handles 1080p via `VideoResolution::_1080p`.

## Result: 1080p accepted and rendered

### Phone Display Params
- **Baseline (480p):** `displayDimensions Point(800, 410)`, codecWidth=800, codecHeight=480
- **1080p probe:** `displayDimensions Point(1920, 984)`, contentArea: 1920x984

### Key Observations
1. Phone renders UI at full 1920-pixel width with 140 DPI
2. Content height is 984px (not 1080) — margins applied for our 1024x600 sidebar display ratio
3. Phone's config_index in AV_SETUP_REQUEST is still `08 03` (same as baseline) — it's the phone's internal reference, not our config list index
4. `max_unacked=10` in our AV_SETUP_RESPONSE
5. 576 video frames captured in 20s (vs 484 at 480p) — higher resolution, similar frame count

### Display Layout Details
```
displayDimensions Point(1920, 984), dpi 140
contentArea: Rect(0, 0 - 1920, 984)
sw2194dp w2194dp h1124dp 140dpi xlrg long
```
Phone classifies the 1080p display as "xlrg" (extra large) layout, which may unlock different
UI layouts than the 480p "small" layout.

### Performance Note
1080p decode on Pi 4 with software H.264 is CPU-intensive. This probe confirms the protocol
works but doesn't evaluate sustained performance. For production, 720p is likely the sweet spot
for Pi 4 (480p for safety, 1080p for more powerful hardware).

## Verdict
1080p video works out of the box with config-only change. The phone dynamically adjusts its
entire UI layout for the higher resolution. No protocol issues.

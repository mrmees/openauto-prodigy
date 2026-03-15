# Phase 13: Wallpaper Hardening - Context

**Gathered:** 2026-03-15
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix Wallpaper.qml to be memory-safe, clip-correct, and flicker-free. Wallpapers must always fill the screen regardless of image dimensions or screen resolution. The scaling logic (PreserveAspectCrop) is already correct — this phase hardens it with sourceSize, clip, and retainWhileLoading.

</domain>

<decisions>
## Implementation Decisions

### Crop and scaling
- Always cover-fit (PreserveAspectCrop) — fit the shorter axis, let the longer axis overflow, center-crop
- Center anchor for cropping — equal crop from all sides
- Wallpaper fills the entire screen including behind navbar — navbar overlays on top
- Theme wallpaper files stay at full resolution on disk — only the decode is capped

### Transition behavior
- Hold previous wallpaper until new one is fully loaded, then instant swap (no crossfade)
- Use Qt 6.8 `retainWhileLoading: true` to prevent flash-to-background during source change
- Background Rectangle keeps its 300ms color animation for themes with no wallpaper (day/night transitions)

### Memory management
- `sourceSize` capped to actual display dimensions (e.g., 1024x600 on current Pi, auto-adjusts for other screens)
- Original file stays full resolution on disk — sourceSize only affects decode-time texture allocation
- `clip: true` to prevent PreserveAspectCrop painting outside bounds

### Claude's Discretion
- How to bind sourceSize to actual display dimensions (Window.width/height, Screen, or DisplayInfo)
- Whether to add a small margin to sourceSize for subpixel filtering (probably not worth it)

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Wallpaper.qml` (19 lines) — already has PreserveAspectCrop and asynchronous loading
- `ThemeService.wallpaperSource` — Q_PROPERTY providing file:// URL for current wallpaper
- `ThemeService.resolveWallpaper()` — handles theme default vs user override resolution

### Established Patterns
- `asynchronous: true` already set on Image — non-blocking decode
- Background Rectangle with ColorAnimation provides fallback for no-wallpaper themes
- ThemeService emits `wallpaperChanged` signal when source changes

### Integration Points
- `Shell.qml` contains the Wallpaper component — anchors.fill: parent on the root
- `DisplayInfo` singleton exposes window dimensions — can provide sourceSize values
- Companion app imports wallpapers via `ThemeService::importCompanionTheme()` — file lands on disk, sourceSize handles decode

</code_context>

<specifics>
## Specific Ideas

- Theme authors provide oversized wallpapers (e.g., 4K) that work across all target displays (800x480 through 1080p) without needing per-resolution variants
- The same wallpaper JPEG covers landscape and portrait — cover-fit handles the crop automatically based on screen aspect ratio

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 13-wallpaper-hardening*
*Context gathered: 2026-03-15*

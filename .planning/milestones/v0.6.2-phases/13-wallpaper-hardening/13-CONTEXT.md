# Phase 13: Wallpaper Hardening - Context

**Gathered:** 2026-03-15
**Status:** Ready for planning

<domain>
## Phase Boundary

Make wallpaper a stable full-screen visual layer that fills the entire display regardless of navbar position or screen resolution. This requires both Wallpaper.qml hardening (sourceSize, clip, retainWhileLoading) AND a Shell.qml layout change to move the wallpaper from the navbar-inset content area to a root-level shell layer.

</domain>

<decisions>
## Implementation Decisions

### Layout model
- Wallpaper is a full-screen visual layer anchored to the root shell, NOT the navbar-inset content area
- Navbar overlays on top of the wallpaper — wallpaper extends behind it
- This requires moving Wallpaper from inside pluginContentHost in Shell.qml to a root-level shell layer
- An exact-screen-size wallpaper must remain visually centered when navbar position changes (top/bottom/left/right)

### Crop and scaling
- Always cover-fit (PreserveAspectCrop) — fit the shorter axis, let the longer axis overflow, center-crop
- Center anchor for cropping — equal crop from all sides
- Theme wallpaper files stay at full resolution on disk — only the decode is capped

### Transition behavior
- Hold previous wallpaper until new one is fully loaded, then instant swap (no crossfade)
- Use Qt 6.8 `retainWhileLoading: true` to prevent flash-to-background during source change
- Background Rectangle keeps its 300ms color animation for themes with no wallpaper (day/night transitions)

### Memory management
- `sourceSize` capped to full shell dimensions (DisplayInfo.windowWidth/windowHeight), not the reduced content viewport
- Original file stays full resolution on disk — sourceSize only affects decode-time texture allocation
- `clip: true` to prevent PreserveAspectCrop painting outside bounds

### Z-ordering
- Wallpaper sits behind all content: behind pluginContentHost (home/settings/plugin views), behind navbar, behind overlays
- Stack from back to front: wallpaper → content area → navbar → overlays (gesture, picker, dialogs)

### Claude's Discretion
- Whether to add a small margin to sourceSize for subpixel filtering (probably not worth it)

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `Wallpaper.qml` (19 lines) — provides background color fallback, PreserveAspectCrop, and async image loading
- `ThemeService.wallpaperSource` — Q_PROPERTY providing file:// URL for current wallpaper
- `ThemeService.resolveWallpaper()` — handles theme default vs user override resolution
- `DisplayInfo.windowWidth` / `DisplayInfo.windowHeight` — full shell dimensions for sourceSize binding

### Established Patterns
- `asynchronous: true` already set on Image — non-blocking decode
- Background Rectangle with ColorAnimation provides fallback for no-wallpaper themes
- ThemeService emits `wallpaperChanged` signal when source changes

### Integration Points
- `Shell.qml` currently places Wallpaper inside `pluginContentHost`, which is reduced by navbar margins — this must change to a root-level shell layer
- `DisplayInfo` singleton exposes full window dimensions — use for sourceSize, not the reduced content viewport
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

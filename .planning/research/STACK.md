# Stack Research

**Domain:** Automotive head unit UI — M3 Expressive color application + wallpaper cover-fit scaling
**Project:** OpenAuto Prodigy v0.6.2 — Theme Expression & Wallpaper Scaling
**Researched:** 2026-03-15
**Confidence:** HIGH — all findings from direct codebase inspection + Qt 6.8 official documentation

---

## Stack Verdict: No New Dependencies

Everything needed for v0.6.2 is already present in the locked stack. M3 Expressive color application is purely a QML token assignment change — it requires no new libraries, no new C++ types, and no new Qt modules. Wallpaper cover-fit scaling is a one-property change to `Wallpaper.qml` plus alignment properties that landed in Qt 6.8 (already the project's locked version).

---

## Locked Stack (Unchanged)

| Technology | Version | Purpose | Status |
|------------|---------|---------|--------|
| Qt 6.8 | 6.8.2 | UI framework, QML engine, Image type | Locked |
| C++17 | gcc 12+ | ThemeService, no changes needed | Locked |
| CMake | 3.22+ | Build system | Locked |
| PipeWire | System | Audio — untouched | Locked |
| BlueZ D-Bus | System | Bluetooth — untouched | Locked |
| yaml-cpp | System | Config — untouched | Locked |

---

## Feature 1: M3 Expressive Color Application

### What "Expressive" Means for Token Usage

M3 Expressive is not a new color system — all 34 roles already exist in `ThemeService`. It is a usage philosophy: **accent colors should have visible weight on interactive and prominent surfaces, not just be used for tiny icons and text highlights.**

The gap in the current codebase is diagnostic from the token grep:

| Surface | Current Token | M3 Expressive Token | Why |
|---------|--------------|---------------------|-----|
| Navbar background | `surfaceContainer` | `surfaceContainer` (correct) | Neutral chrome — keep |
| Navbar icons (inactive) | `onSurfaceVariant` opacity 0.4 | `onSurfaceVariant` | Correct, already dim |
| NavbarControl active state | `tertiary` on clock text only | `primaryContainer` fill + `onPrimaryContainer` text | Active state should feel filled |
| Tile background | `primary` (correct) | `primary` with `onPrimary` text | Tile already uses primary — fix text token |
| Tile text/icon | `inverseOnSurface` | `onPrimary` | `inverseOnSurface` is wrong role on a `primary` background |
| WidgetHost card | `surfaceContainer` (background) | `surfaceContainer` at 25–35% opacity | Glass card keeps neutral surface — correct |
| WidgetHost active state indicator | None | `primaryContainer` accent dot/border | Missing: active widget has no accent |
| Settings tile (active/selected) | `primaryContainer` fill (correct) | `primaryContainer` | Already correct |
| Settings tile (unselected) | `surfaceContainer` / `surfaceContainerHigh` alternating | Same | Correct neutral background |
| ElevatedButton default | `surfaceContainerLow` | Same | Correct |
| FilledButton default | `primary` (correct) | Same | Correct |
| AA status icon (connected) | `primary` | `primary` | Correct |
| AA status icon (disconnected) | `onSurfaceVariant` | `onSurfaceVariant` | Correct |

**The core problem:** `inverseOnSurface` is used as icon/text color on `primary`-colored tiles. `inverseSurface` is the dark surface used for tooltips/snackbars; `inverseOnSurface` is text that reads on that dark surface. On a `primary` background, the correct text/icon token is `onPrimary`. This is a wrong-role bug, not a missing feature.

**Secondary problem:** Navbar controls have no filled active state. When a popup is open or a gesture is in progress, the NavbarControl looks identical to its resting state. M3 Expressive says interactive elements in active states should use `primaryContainer` fill with `onPrimaryContainer` foreground.

### What Needs to Change (Pure QML Token Reassignment — No New APIs)

All changes are `color:` property reassignments in QML. No new ThemeService roles needed. No new C++ code.

| File | Change | From | To |
|------|--------|------|----|
| `qml/controls/Tile.qml` | Icon + text color on pressed/normal | `inverseOnSurface` | `onPrimary` |
| `qml/components/NavbarControl.qml` | Active state fill | None (transparent) | `primaryContainer` fill when active |
| `qml/components/NavbarControl.qml` | Active state icon | `onSurfaceVariant` opacity 0.4 | `onPrimaryContainer` when active, `onSurfaceVariant` otherwise |
| `qml/components/WidgetHost.qml` | Per-widget active indicator | None | `primaryContainer` accent — small border or dot |
| `qml/controls/ElevatedButton.qml` | `textColor` default | `ThemeService.primary` | Same (already correct) |
| `qml/controls/FilledButton.qml` | Text color on primary bg | `onSurface` (wrong) | `onPrimary` |

**FilledButton text bug:** `FilledButton` uses `primary` background but `onSurface` as its text color default. On a mid-tone primary, `onSurface` may be readable but it's semantically wrong and may fail contrast on vivid themes. Correct token is `onPrimary`.

### What NOT to Add

- No new ThemeService color roles — 34 roles is the complete M3 set; all needed accent tokens exist
- No color derivation math in QML — `Qt.lighter()` / `Qt.darker()` hacks are fragile across theme palettes; use the existing role tokens
- No "expressive mode" toggle — the token corrections are simply right; no configuration needed
- No animation changes — existing `Behavior on color { ColorAnimation }` already handles smooth transitions

---

## Feature 2: Wallpaper Cover-Fit Scaling

### Current State

`Wallpaper.qml` uses `Image.PreserveAspectCrop` with `anchors.fill: parent`. This is already correct cover-fit behavior — the image scales to fill the container, maintaining aspect ratio, cropping excess. It works.

**The actual problem is not the fill mode.** The problem is:

1. **No `sourceSize` constraint** — a 4000x3000 wallpaper image is fully decoded into memory at native resolution before QML scales it. On a Pi 4 with a 1024x600 display this wastes ~46MB of memory for a 4K image when the display only needs ~2.4MB.

2. **No alignment control for crop anchor point** — `PreserveAspectCrop` defaults to center-cropping (both axes). For landscape wallpapers on a widescreen display this is fine. For portrait wallpapers on landscape screens, center-crop clips the subject. Qt 6.8 added `horizontalAlignment` and `verticalAlignment` properties to `Image` that let you choose which part of the image to keep when cropping.

3. **No `clip: true`** on the Image item — `PreserveAspectCrop` can paint outside the item bounds when the image is taller than the container. The parent item has `anchors.fill: parent` which should constrain, but without `clip: true` the painted area is technically unbounded.

### Qt 6.8 Image Properties Relevant Here

| Property | Type | Default | Notes |
|----------|------|---------|-------|
| `fillMode` | enum | `Image.Stretch` | Use `Image.PreserveAspectCrop` — already set |
| `sourceSize` | size | null | Cap decode resolution — add this |
| `horizontalAlignment` | enum | `Image.AlignHCenter` | NEW in Qt 6.8 — controls crop anchor |
| `verticalAlignment` | enum | `Image.AlignVCenter` | NEW in Qt 6.8 — controls crop anchor |
| `clip` | bool | false | Add `clip: true` for safety |
| `retainWhileLoading` | bool | false | NEW in Qt 6.8 — show old image while new loads |
| `asynchronous` | bool | false | Already set to true |
| `smooth` | bool | true | Keep true for display quality |

**Confidence:** HIGH — verified from Qt 6.8 official documentation. `horizontalAlignment` and `verticalAlignment` are Qt 6.8 additions confirmed in the Qt Quick Image type reference.

### Recommended Changes to `Wallpaper.qml`

**Add `sourceSize`** — cap decode at 2x the maximum target resolution to save memory while preserving sharpness for any screen up to 1920x1080:

```qml
sourceSize: Qt.size(2048, 1200)
```

This is a soft cap: Qt scales down images that exceed this size during decode. Images already smaller are untouched. 2048x1200 covers 2x 1024x600 and fits under 1x 1920x1080.

**Add `clip: true`** — contains any overflow from PreserveAspectCrop:

```qml
clip: true
```

**Add `retainWhileLoading: true`** — prevents flash-to-background when switching wallpapers (Qt 6.8 feature, safe to add):

```qml
retainWhileLoading: true
```

**Alignment** — `AlignHCenter` / `AlignVCenter` are already the defaults; no change needed unless a future config option is wanted to let users pin the crop anchor (top, center, or bottom for portrait-on-landscape). That is a v0.7+ enhancement, not v0.6.2 scope.

### What NOT to Add

- No `MultiEffect { blurEnabled: true }` wallpaper blur — GPU-intensive, no GPU on dev VM, not a stated goal
- No dynamic `sourceSize` from `Screen.width / Screen.height` — `Screen` singleton is unreliable in labwc/nested compositor contexts on Pi; static 2048x1200 cap is simpler and correct
- No wallpaper transition animation beyond what QML's `asynchronous: true` already provides — fades between wallpapers on theme switch are not requested
- No `Image` subtype or C++ `QQuickImageProvider` changes — the file:// URL scheme through ThemeService already works

---

## Complete Change Surface

### Files Changed

| File | Change Type | What |
|------|-------------|------|
| `qml/components/Wallpaper.qml` | Property additions | `sourceSize`, `clip`, `retainWhileLoading` |
| `qml/controls/Tile.qml` | Token correction | `inverseOnSurface` → `onPrimary` for icon/text |
| `qml/controls/FilledButton.qml` | Token correction | `onSurface` → `onPrimary` for text color default |
| `qml/components/NavbarControl.qml` | Token addition | Active state: `primaryContainer` fill + `onPrimaryContainer` icon |
| `qml/components/WidgetHost.qml` | Optional token addition | `primaryContainer` accent indicator on active widget |
| Various widget/settings QML | Audit pass | Verify no other `inverseOnSurface`-on-`primary` mismatches |

### Files NOT Changed

- `src/core/services/ThemeService.*` — no new roles, no new properties
- `src/core/services/IThemeService.hpp` — interface unchanged
- `CMakeLists.txt` — no new sources
- Any `.cpp` file — zero C++ changes for this milestone

---

## Version Compatibility

All changes are compatible with both Qt 6.4 (dev VM) and Qt 6.8 (Pi), with one note:

| Addition | Qt 6.4 | Qt 6.8 | Notes |
|----------|--------|--------|-------|
| `Image.PreserveAspectCrop` | Yes | Yes | Already used |
| `Image.sourceSize` | Yes | Yes | Long-standing property |
| `Image.clip` | Yes | Yes | Standard QML Item property |
| `Image.asynchronous` | Yes | Yes | Already used |
| `Image.retainWhileLoading` | No | Yes (Qt 6.8+) | Qt 6.8-only; must guard or accept it silently degrades on 6.4 |
| `Image.horizontalAlignment` | No | Yes (Qt 6.8+) | Qt 6.8-only; not needed for v0.6.2 (defaults are correct) |
| `Image.verticalAlignment` | No | Yes (Qt 6.8+) | Qt 6.8-only; not needed for v0.6.2 (defaults are correct) |
| Token reassignments (`onPrimary` etc.) | Yes | Yes | All these roles exist in ThemeService since v0.5.1 |

**`retainWhileLoading` on Qt 6.4:** The dev VM runs Qt 6.4 which lacks this property. Safest approach: add it without a guard and verify it builds on 6.4 — unknown properties in QML generate a warning at startup but do not fail. Alternatively, guard with a version check. Given the Pi (Qt 6.8) is the deployment target and 6.4 is only the dev build, the warning is acceptable.

---

## Alternatives Considered

| Recommended | Alternative | Why Not |
|-------------|-------------|---------|
| `sourceSize: Qt.size(2048, 1200)` static cap | `sourceSize: Qt.size(Screen.width * 2, Screen.height * 2)` dynamic | `Screen` singleton unreliable in labwc nested compositor; static cap is simpler and sufficient |
| Token correction in QML only | New `onPrimaryForeground` helper property in ThemeService | ThemeService already has `onPrimary`; adding a duplicate role for the same thing creates confusion |
| Active state via `primaryContainer` fill in NavbarControl | Active state via opacity pulse animation | M3 specifies filled container for active states; animation is not a substitute for semantic color |
| No wallpaper blur | `MultiEffect { blurEnabled: true }` for frosted glass | GPU cost on Pi 4; not in scope; not requested |

---

## What NOT to Add

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| New ThemeService color roles | 34 M3 roles covers everything needed; adding custom roles breaks semantic alignment with companion app | Use existing `primaryContainer`, `onPrimary`, etc. |
| `Qt.lighter()` / `Qt.darker()` color derivation in QML | Fragile on vivid themes — e.g., `Qt.lighter(primary, 1.2)` on a near-white primary gives an ugly result | Use the explicitly defined container roles from ThemeService |
| `MultiEffect { colorizeEnabled: true }` tinting | Requires GPU; doesn't match M3 semantics | Use background color tokens |
| Per-surface `ColorAnimation` beyond existing `Behavior on color` | Already in place throughout the codebase | Nothing to add |
| CSS-style `mix()` color blending (manual `r * 0.88 + primary.r * 0.12`) for new surfaces | Already overused in the codebase (NavbarPopup, GestureOverlay, dialogs); produces subtle primary tint that reads as "neutral with a hint" rather than expressive accent | For expressive surfaces, use `primaryContainer` directly instead of hand-blending |

---

## Installation

No new packages. No CMake changes. No new `find_package()`.

```bash
# Dev VM — unchanged
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH=/opt/qt/6.8.2/gcc_64 ..
make -j$(nproc)

# Cross-compile — unchanged
./cross-build.sh
```

---

## Sources

- Qt 6.8 Image QML Type documentation (https://doc.qt.io/qt-6/qml-qtquick-image.html) — `horizontalAlignment`, `verticalAlignment`, `retainWhileLoading` confirmed as Qt 6.8 additions — HIGH confidence
- Direct codebase inspection: `qml/components/Wallpaper.qml`, `qml/controls/Tile.qml`, `qml/controls/FilledButton.qml`, `qml/components/NavbarControl.qml`, full token grep across all QML — HIGH confidence (primary source)
- Material Design 3 color roles documentation (https://m3.material.io/styles/color/roles) — role semantic definitions for `onPrimary`, `primaryContainer`, `onPrimaryContainer` — HIGH confidence
- M3 Expressive overview (https://supercharge.design/blog/material-3-expressive) — Expressive = bold accent usage on interactive surfaces, not new roles — MEDIUM confidence (third-party synthesis; consistent with official M3 docs)
- `src/core/services/ThemeService.hpp` — full 34-role M3 property list — HIGH confidence (direct source)

---

*Stack research for: OpenAuto Prodigy v0.6.2 — Theme Expression & Wallpaper Scaling*
*Researched: 2026-03-15*

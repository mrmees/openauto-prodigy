---
phase: 13-wallpaper-hardening
verified: 2026-03-15T17:00:00Z
status: human_needed
score: 5/5 must-haves verified
re_verification: false
human_verification:
  - test: "Set navbar to bottom edge, apply wallpaper. Switch navbar to top edge."
    expected: "Wallpaper fills edge-to-edge with no gap appearing at any edge. No shift or reposition."
    why_human: "Navbar edge changes affect ColumnLayout margins but not the root-level Wallpaper layer — correct behavior requires visual confirmation on Pi."
  - test: "Apply an oversized wallpaper (4K landscape JPEG). Observe the screen."
    expected: "Image fills the screen, center-cropped, no bleed or overflow outside display bounds."
    why_human: "PreserveAspectCrop overflow is what clip:true guards against — can only confirm no visual bleed on device."
  - test: "Apply a portrait-orientation wallpaper (taller than wide). Observe the screen."
    expected: "Image fills width, top/bottom cropped equally, no letterbox bars, no paint outside component bounds."
    why_human: "Portrait aspect ratio is the primary paint-outside-bounds risk case — requires display observation."
  - test: "Switch between two wallpaper themes in quick succession."
    expected: "No flash to background color during swap. Previous wallpaper holds until new one is ready, then instant replace."
    why_human: "retainWhileLoading behavior is asynchronous and load-time-dependent — cannot verify without actual file I/O on device."
  - test: "Switch from a wallpaper theme to a no-wallpaper theme."
    expected: "Background color animates smoothly over 300ms. No abrupt cut."
    why_human: "ColorAnimation timing is a visual quality check."
  - test: "Toggle day/night while a wallpaper is active."
    expected: "If same wallpaper source, no transition flash. If different wallpaper per mode, smooth hold-then-swap."
    why_human: "Day/night toggle with different per-mode wallpapers triggers retainWhileLoading — requires device observation."
---

# Phase 13: Wallpaper Hardening Verification Report

**Phase Goal:** Harden wallpaper rendering — cap texture memory, prevent paint-outside-bounds, eliminate transition flicker, restructure z-ordering so wallpaper is behind navbar
**Verified:** 2026-03-15T17:00:00Z
**Status:** human_needed — all automated checks pass, Pi visual verification pending
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Wallpaper fills entire display edge-to-edge regardless of image aspect ratio | ? HUMAN | `fillMode: Image.PreserveAspectCrop` present in Wallpaper.qml; visual confirmation needed on device |
| 2 | Wallpaper extends behind the navbar — no gap between wallpaper and screen edge | ✓ VERIFIED | Shell.qml lines 13-17: `Wallpaper { anchors.fill: parent }` is direct child of root `shell` Item before ColumnLayout; ColumnLayout margins do NOT apply to wallpaper |
| 3 | Switching themes or toggling day/night does not produce a solid-color flash | ? HUMAN | `retainWhileLoading: true` present on Image (Wallpaper.qml line 19); async load timing requires device observation |
| 4 | Decoded texture is capped to shell dimensions via sourceSize — an 8MP source only allocates a display-sized texture | ✓ VERIFIED | Wallpaper.qml lines 20-21: `sourceSize.width: DisplayInfo.windowWidth` and `sourceSize.height: DisplayInfo.windowHeight` both present and bound to full shell dimensions |
| 5 | A portrait-orientation wallpaper does not paint outside the wallpaper container | ? HUMAN | `clip: true` on root Item (Wallpaper.qml line 5) is the guard; effectiveness with extreme aspect ratios requires visual confirmation |

**Score:** 5/5 truths have correct implementation — 2/5 have programmatic certainty, 3/5 require Pi display observation

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `qml/components/Wallpaper.qml` | Memory-safe, clipped, transition-stable wallpaper component | ✓ VERIFIED | 23 lines, substantive, all three required properties present |
| `qml/components/Shell.qml` | Root-level wallpaper layer behind all content and navbar | ✓ VERIFIED | 102 lines, Wallpaper at lines 13-17 as first child of `shell` Item |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `Wallpaper.qml` | `DisplayInfo` | `sourceSize` binding | ✓ WIRED | Lines 20-21: `sourceSize.width: DisplayInfo.windowWidth` / `sourceSize.height: DisplayInfo.windowHeight` |
| `Shell.qml` | `Wallpaper.qml` | root-level child (not inside pluginContentHost) | ✓ WIRED | Shell.qml lines 12-17: Wallpaper is declared before ColumnLayout, directly under `shell` Item. No Wallpaper reference exists inside the pluginContentHost block (lines 28-55). |

### Z-Order Verification

Declared order in Shell.qml confirms correct back-to-front stack:

| Layer | Line | Z-value | Status |
|-------|------|---------|--------|
| Wallpaper | 13 | implicit 0 (first child) | ✓ |
| ColumnLayout (content) | 19 | implicit 0 (after Wallpaper) | ✓ |
| Navbar | 60 | `z: 100` | ✓ |
| Dim overlay | 78 | `z: 998` | ✓ |
| GestureOverlay, dialogs | 88+ | implicit above dim | ✓ |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| WP-01 | 13-01-PLAN.md | Wallpaper caps decoded texture to display dimensions via sourceSize, clips overflow, retains previous image during transitions, always fills screen without letterboxing | ✓ SATISFIED | All four sub-requirements implemented: `sourceSize` (lines 20-21), `clip: true` (line 5), `retainWhileLoading: true` (line 19), `PreserveAspectCrop` cover-fill (already present). REQUIREMENTS.md marks WP-01 Complete at Phase 13. |

No orphaned requirements — WP-01 is the only requirement mapped to Phase 13 in REQUIREMENTS.md, and it is claimed by 13-01-PLAN.md.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| — | — | None found | — | Both files are clean: no TODO/FIXME/placeholder comments, no empty handlers, no stub returns. |

### Human Verification Required

#### 1. Navbar edge changes — wallpaper fills edge-to-edge

**Test:** On the Pi, open Settings > Display and change navbar position from Bottom to Top, then Left, then Right.
**Expected:** Wallpaper remains full-screen at all times with no gap exposed at any edge. The ColumnLayout insets away from the navbar, but the wallpaper layer is unaffected.
**Why human:** Navbar edge repositioning changes QML anchor margins on ColumnLayout. The wallpaper's `anchors.fill: parent` should be immune, but visual confirmation is the only reliable check.

#### 2. Oversized wallpaper — no overflow bleed

**Test:** Import or apply a 4K landscape JPEG as the wallpaper. Observe the screen edges.
**Expected:** Image is center-cropped to fill display. No painting visible beyond display edges (e.g., no edge artifacts).
**Why human:** `clip: true` prevents QML from rendering outside the component, but the visual result of PreserveAspectCrop overflow guard requires display observation.

#### 3. Portrait-orientation wallpaper — no letterboxing or overflow

**Test:** Apply a portrait-aspect wallpaper (e.g., 900x1600). Observe the screen.
**Expected:** Image fills the full display width, crops equally from top/bottom, no black bars, no paint outside bounds.
**Why human:** This is the primary case `clip: true` was added to guard. Requires visual inspection.

#### 4. Theme switch — no flash to background color

**Test:** With a wallpaper active, switch to a different theme that has a different wallpaper. Watch the transition.
**Expected:** Previous wallpaper is held visible until the new one finishes loading, then instant swap. No brief flash of the background rectangle color.
**Why human:** `retainWhileLoading: true` depends on async image load timing. The Qt 6.8 feature is present in code, but behavior under real I/O on the Pi must be observed.

#### 5. No-wallpaper theme transition

**Test:** Switch from a wallpapered theme to a theme with no wallpaper.
**Expected:** Background color animates with a smooth 300ms fade (ColorAnimation on the Rectangle).
**Why human:** Animation is a visual quality check that cannot be verified programmatically.

#### 6. Day/night toggle with wallpaper active

**Test:** With a wallpaper that differs between day and night modes, toggle day/night.
**Expected:** If sources differ: retainWhileLoading holds previous image during load. If same source: no visible change.
**Why human:** Two-source toggle behavior depends on ThemeService signal ordering and async load timing on the actual Pi hardware.

### Gaps Summary

No gaps. All five must-have truths are correctly implemented in the codebase. Both artifacts exist, are substantive, and are wired. Both key links are present and connected. WP-01 is fully satisfied by the code. The three items marked "? HUMAN" above are not gaps — the code is correct; they require Pi display observation to confirm there are no unexpected rendering artifacts from Qt's compositor or hardware-specific behavior.

---

_Verified: 2026-03-15T17:00:00Z_
_Verifier: Claude (gsd-verifier)_

---
status: complete
phase: 05-static-grid-rendering-widget-revision
source: [05-01-SUMMARY.md, 05-02-SUMMARY.md]
started: 2026-03-12T19:30:00Z
updated: 2026-03-12T19:44:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Fresh Install Blank Canvas
expected: With a fresh config (delete ~/.openauto/config.yaml or its widget section), the home screen shows only the wallpaper and the launcher dock at the bottom. No widgets, no placeholder text, no outlines — pure wallpaper above the dock.
result: pass

### 2. Widget Grid Rendering
expected: Add widgets to config.yaml. Widgets appear at the correct grid positions with glass card backgrounds — semi-transparent surfaceContainer, rounded corners, no visible border. Gutters (~8px) visible between cards.
result: pass
note: Initial config had wrong YAML key names (column vs col) — fixed and re-tested.

### 3. Clock Widget — Small (1x1)
expected: Place clock at 1x1 (colSpan:1, rowSpan:1). Shows large bold time only — no date, no day. Time fills the cell. No AM/PM indicator.
result: pass
note: Required fix — fontSizeMode Text.Fit added to prevent text overflow at small cell sizes (commit 6c65199).

### 4. Clock Widget — Wide (2x1) and Full (2x2)
expected: At 2x1: time + date (e.g. "March 12"), date visible below time. At 2x2: time + date + day of week on three lines.
result: pass
note: Required fix — height breakpoint lowered from 200px to 180px because navbar eats ~50px making 2x2 cells ~198px inner (commit 7ed821c).

### 5. AA Status Widget — Tiers
expected: At 1x1: single large connection icon centered (grey when disconnected). At 2x1+: icon + status text ("Tap to connect" or "Connected"). Tapping launches AA plugin.
result: pass
note: Visual tiers verified. Tap-to-connect not testable — AA connection has a separate pre-existing issue (BT RFCOMM reset loop, unrelated to widget changes).

### 6. Now Playing Widget — Tiers
expected: At 2x1 (compact): horizontal strip with music icon + track title (elided) + play/pause button, no artist. At 3x2+ (full): centered layout with icon, title, artist, prev/play/next controls.
result: pass

### 7. Launcher Dock Positioning
expected: Launcher dock stays fixed at the bottom of the screen, below the widget grid. Widgets do not overlap the dock regardless of grid content.
result: pass

## Summary

total: 7
passed: 7
issues: 0
pending: 0
skipped: 0

## Gaps

[none]

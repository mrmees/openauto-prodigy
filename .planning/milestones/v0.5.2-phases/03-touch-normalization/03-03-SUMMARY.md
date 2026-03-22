---
phase: 03-touch-normalization
plan: 03
type: gap-closure
status: complete
started: 2026-03-11
completed: 2026-03-11
---

# Summary: Gap Closure — Font Size Increase

## What Was Done

Increased all UiMetrics base font sizes ~1.4x for automotive readability on the 1024x600 display:

| Token | Old Base | New Base | Rendered (1.06x) | Cap Height |
|-------|----------|----------|-------------------|------------|
| fontTiny | 14 | 20 | ~21px | ~3.1mm |
| fontSmall | 16 | 22 | ~23px | ~3.4mm |
| fontBody | 20 | 28 | ~30px | ~4.4mm |
| fontTitle | 22 | 30 | ~32px | ~4.7mm |
| fontHeading | 28 | 36 | ~38px | ~5.6mm |

Font floors updated proportionally. One hardcoded `font.pixelSize: 10` in AndroidAutoMenu.qml replaced with `UiMetrics.fontTiny`.

## Files Modified

- `qml/controls/UiMetrics.qml` — base font sizes and floor defaults
- `qml/applications/android_auto/AndroidAutoMenu.qml` — removed hardcoded font size

## Verification

- User approved text readability on Pi at arm's length
- No clipping or overflow reported across all 9 settings pages
- All settings pages inherit sizes via UiMetrics tokens (no hardcoded values)

## Gap Closed

- UAT Test 4 (Text Readability): **resolved**

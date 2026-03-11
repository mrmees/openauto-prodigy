# Settings Scroll Hints — Design

**Date:** 2026-03-11
**Branch:** `dev/0.5.2`
**Status:** Approved

## Overview

Settings screens should show a small up/down hint when more content exists above or below the current viewport.

The user wants this on:

- the top-level Settings category list
- the stacked subsettings pages

The hint should only appear when there is more content in that direction.

## Approaches Considered

### 1. Recommended: reusable overlay component attached to each settings scroll surface

- Pros: explicit, predictable, easy to tune once, works equally well for `ListView` and `Flickable`
- Cons: each settings surface still needs one small attachment

### 2. Discover the active scrollable surface centrally from `SettingsMenu`

- Pros: one theoretical attachment point
- Cons: brittle because the current page structure mixes wrapped `ListView` and independent `Flickable` pages

### 3. Use Qt scroll bars/scroll indicators

- Pros: built-in
- Cons: reads like a scrollbar, not a directional “more content” hint

## Recommendation

Use approach 1.

Create a tiny shared overlay component that takes a target `Flickable` and renders subtle top/bottom chevrons only when that target has offscreen content in the relevant direction.

## Architecture

- Add a reusable control, tentatively `SettingsScrollHints.qml`, under `qml/controls`.
- It will accept a `Flickable` target so it can work with both `ListView` and page `Flickable`s.
- It will render:
  - one top chevron centered near the top edge
  - one bottom chevron centered near the bottom edge
- Visibility rules:
  - top chevron visible only when `contentY > threshold`
  - bottom chevron visible only when `contentY < maxContentY - threshold`
  - both hidden when the content fits fully in the viewport
- Styling:
  - subtle icon size and opacity
  - short fade animation
  - non-interactive overlay so it never steals touches
- Integration points:
  - attach to the top-level Settings category `ListView` in `SettingsMenu.qml`
  - attach to each stacked subsettings page root `Flickable`

## Testing

- Extend the existing structure regression so it requires:
  - a shared `SettingsScrollHints.qml` control
  - the top-level Settings list to use that control
  - every stacked settings subpage to use that control
- Verify red/green with the targeted `test_settings_menu_structure` test before full verification.

## Out of Scope

- Replacing the hints with permanent scrollbars
- Adding tap-to-scroll behavior on the hints
- Using the hint pattern outside Settings in this task

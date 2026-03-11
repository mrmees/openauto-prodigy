# Settings Back-Hold Indicator Follow-Up — Design

**Date:** 2026-03-11
**Branch:** `dev/0.5.2`
**Status:** Approved

## Overview

Long-hold back now works on interactive settings controls, but the visual ripple indicator only appears when the overlay-owned path handles the gesture. Controls that own long-hold directly call back navigation without telling `SettingsMenu` to show the ripple.

The intended behavior is:

- The same expanding back-arrow ripple should appear for blank-space holds and interactive-control holds.
- There should be one visual style, not separate indicator implementations per control.
- Short taps should not flash the ripple longer than the press lifecycle.

## Approaches Considered

### 1. Duplicate the ripple in each control

- Pros: direct, localized
- Cons: duplicated visuals and animation logic, higher drift risk

### 2. Reuse the existing `SettingsMenu` ripple from controls

- Pros: one source of truth for the effect, minimal behavior change, consistent visuals
- Cons: controls need a small way to find and call the menu ripple API

### 3. Replace the ripple with generic pressed-state feedback

- Pros: least code
- Cons: weaker and inconsistent feedback compared with the rest of the hold gesture

## Recommendation

Use approach 2.

Interactive controls should keep owning the gesture, but they should delegate visual indicator display to `SettingsMenu`, which already owns the ripple effect.

## Architecture

- Add a small shared indicator API on `SettingsMenu.qml` for:
  - show ripple at a given position
  - hide ripple
  - keep existing trigger/back behavior
- Update `SettingsHoldArea.qml` to:
  - locate the enclosing `SettingsMenu`
  - map press coordinates into the menu coordinate space
  - show the ripple on press
  - hide it on cancel/release
  - keep requesting back on long hold
- Update `SettingsSlider.qml` to use the same menu ripple API during its custom hold path.

## Testing

- Extend the existing structure regression test so it fails unless:
  - `SettingsMenu.qml` exposes dedicated ripple show/hide helpers
  - `SettingsHoldArea.qml` uses those helpers
  - `SettingsSlider.qml` uses those helpers on its custom hold path

## Out of Scope

- Changing the ripple appearance
- Reworking the gesture thresholds
- Refactoring the entire settings gesture system

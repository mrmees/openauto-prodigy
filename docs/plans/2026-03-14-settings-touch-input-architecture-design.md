# Settings Touch Input Architecture — Design

**Date:** 2026-03-14
**Branch:** `dev/0.5.2`
**Status:** Approved

## Overview

Settings touch input currently mixes three competing ownership models:

- menu-level `TapHandler`s in `qml/applications/settings/SettingsMenu.qml`
- row-level overlay handlers in `qml/controls/SettingsRow.qml`
- control-level `MouseArea` hold helpers in `qml/controls/SettingsHoldArea.qml`

That split is why the behavior keeps regressing. Overlay handlers can cover blank space, but they steal interaction from controls. Parent-level `TapHandler`s look cleaner, but they do not act as a true subtree-wide passive observer for deep child controls and popup content in Qt 6.8.

The required behavior is:

- single taps still activate settings categories and controls
- long-press anywhere in the settings content triggers back navigation
- sliders, toggles, pickers, and dropdowns keep their normal touch behavior on Pi touchscreen and desktop mouse

## Approaches Considered

### 1. Keep iterating on QML `TapHandler` placement

- Pros: no C++
- Cons: already failed in multiple configurations, still depends on Qt Quick delivery rules that do not match the requirement

### 2. Window-global C++ event filter

- Pros: sees everything, including popups
- Cons: too broad, harder to scope to settings, awkward to coordinate release swallowing and settings-only activation

### 3. Settings-scoped C++ subtree input boundary

- Pros: one blanket mechanism for the whole settings subtree, can observe child pointer traffic without overlay stealing, can suppress the release after long-press so controls do not also click
- Cons: requires a small custom C++ QML type

## Recommendation

Use approach 3.

Implement a small C++ `QQuickItem` boundary at the root of settings that filters child pointer traffic for the settings subtree. It should remain passive during normal interaction and only take control after the long-press gesture has clearly won.

This is the first approach that fits all three requirements without per-control exceptions.

## Why Parent-Level `TapHandler`s Fail Here

The current assumption in `SettingsMenu.qml` is that root-level `TapHandler`s with `target: null` can passively observe all pointer activity inside settings. That assumption does not hold for this use case.

Problems:

- `TapHandler` is attached to one item. It is not a subtree event filter for arbitrary descendants.
- Descendant controls such as `Slider`, `Switch`, `MouseArea`, and popup-backed controls manage their own grabs and gesture recognition. Ancestor handlers are not guaranteed to see or keep the event stream once those controls become active.
- Popup content such as `FullScreenPicker.qml` lives under `Overlay.overlay`, outside the normal visual parent chain of the settings page, so ancestor handlers on `SettingsMenu` cannot be the blanket solution anyway.
- Even when the ancestor handler receives the initial press, it still cannot reliably solve the “long-press won, now do not also click/toggle on release” problem across all controls without becoming invasive.

Qt's documented mechanism for subtree observation is `QQuickItem::setFiltersChildMouseEvents()` with `childMouseEventFilter()`, not stacking more `TapHandler`s on ancestors.

## Architecture

### New C++ boundary item

Add a QML-exposed `SettingsInputBoundary` type implemented in C++ as a `QQuickItem`.

Responsibilities:

- set `filtersChildMouseEvents` so descendant mouse/touch traffic reaches the boundary
- accept touchscreen and left-mouse interaction
- track one active press at a time
- start a 500 ms hold timer on press
- cancel the hold on meaningful movement, cancel/ungrab, or multi-touch
- emit `pressStarted(scenePos)` and `pressEnded()` for the shared ripple UI
- emit `longPressed(scenePos)` when the long-press wins
- swallow the matching release after a successful long-press so the underlying control does not also activate

### QML ownership cleanup

Make `SettingsMenu.qml` the single owner of settings back navigation and ripple presentation.

Keep:

- `goBack()`
- ripple UI
- title updates

Remove:

- root `TapHandler` long-press logic
- row-owned back-hold overlays
- per-control back-hold coordination plumbing

### Control behavior

Controls should go back to owning only their actual UI behavior:

- `SettingsToggle.qml` toggles normally
- `SettingsSlider.qml` drags normally
- `FullScreenPicker.qml` opens normally
- `SettingsHoldArea.qml` becomes a short-click helper only, not a back-hold participant

The boundary handles long-press recognition for the whole subtree. Controls no longer need to know about back-hold state.

## Data Flow

1. User presses anywhere inside settings content.
2. `SettingsInputBoundary` sees the descendant event, stores pointer identity and scene position, starts the hold timer, and emits `pressStarted`.
3. `SettingsMenu.qml` shows the shared ripple at that scene position.
4. If the pointer moves beyond threshold or is canceled, the boundary emits `pressEnded` and abandons the hold.
5. If the pointer is released before timeout, the boundary emits `pressEnded` and allows the child control to complete normally.
6. If the hold timer fires first, the boundary emits `longPressed`, enters swallow-on-release mode, and `SettingsMenu.qml` performs `goBack()` or `ApplicationController.navigateBack()`.
7. The eventual release is consumed by the boundary so the underlying control does not also click or toggle.

## Testing

Update the structure regression coverage to match the new architecture.

The existing `tests/test_settings_menu_structure.cpp` currently encodes the wrong design by requiring root `TapHandler`s and row-owned back-hold logic. That test must be rewritten to require:

- `SettingsMenu.qml` using the boundary type instead of root `TapHandler`s
- `SettingsRow.qml` no longer containing back-hold overlay logic
- `SettingsHoldArea.qml` no longer containing back-hold navigation logic
- `SettingsSlider.qml` no longer coordinating row-level back-hold state

Add focused C++ tests for the new boundary logic:

- short press does not trigger long-press
- movement beyond threshold cancels long-press
- long-press emits once and swallows the matching release
- mouse and touch paths both work

## Risks

- `childMouseEventFilter()` must be implemented carefully so normal control interaction is not disturbed before long-press wins.
- Popup content rendered under `Overlay.overlay` may need an explicit decision:
  - either long-press-back applies only to the settings page content
  - or popup content also opts into the boundary with a second instance

The initial implementation should scope the guarantee to the settings content pages themselves, which matches the described problem area and avoids broad window-global interception.

## Out of Scope

- changing visual styling of the ripple
- introducing new gestures beyond long-press-back
- general-purpose app-wide gesture filtering outside settings

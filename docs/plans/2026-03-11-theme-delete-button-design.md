# Theme Delete Button — Design

**Date:** 2026-03-11
**Branch:** `dev/0.5.2`
**Status:** Approved

## Overview

The current "Delete Theme" row uses a leading trash icon and whole-row click behavior. The requested change is to make it behave more like the Bluetooth device "Forget" action:

- keep normal row text on the left
- move the destructive action into a separate outlined button on the right
- preserve the existing two-tap confirmation flow

## Approaches Considered

### 1. Recommended: keep left-side row text and add a right-side outlined delete button

- Pros: matches the existing Bluetooth "Forget" affordance, keeps destructive action explicit, minimal scope
- Cons: one more one-off row layout

### 2. Make the entire row a destructive outlined-button row

- Pros: fewer internal elements
- Cons: does not match the requested Bluetooth-like pattern and makes the whole row feel too action-heavy

### 3. Keep the row interactive and only restyle the icon area

- Pros: smallest diff
- Cons: still leaves the destructive action ambiguous because the whole row remains clickable

## Recommendation

Use approach 1.

The row should become a normal non-interactive content row with a right-aligned outlined destructive button similar to "Forget".

## Architecture

- Keep the existing `deleteThemeState.confirmPending` timer/reset logic.
- Remove the leading trash icon.
- Keep left-side text as plain row text: `Delete Theme`.
- Add a right-side outlined pill button:
  - default label: `Delete`
  - confirm state label: `Confirm`
  - border/text use destructive color to match the Bluetooth button pattern
- Move the delete action to the button's `SettingsHoldArea` instead of the whole row.

## Testing

- Extend the structure regression to require the `ThemeSettings.qml` delete row to:
  - define a dedicated button text node
  - use an outlined destructive button surface
  - use `SettingsHoldArea` on that button instead of whole-row interaction

## Out of Scope

- Reworking Bluetooth button styling
- Creating a shared destructive button component
- Changing theme deletion rules or confirmation timing

# Settings Subpage Gutters — Design

**Date:** 2026-03-11
**Branch:** `dev/0.5.2`
**Status:** Approved

## Overview

The top-level Settings landing page looks correct, but the stacked subsettings pages are visually too tight against the left and right edges. That makes section headers such as "Display" and "Navbar" feel clipped and leaves row content with too little breathing room.

The requested scope is:

- keep the top-level Settings page unchanged
- add a small horizontal gutter only to subsettings pages
- move section headers and rows inward together so alignment stays consistent

## Approaches Considered

### 1. Add a shared page gutter to subsettings page columns

- Pros: one consistent layout rule, fixes headers and rows together, minimal behavior change
- Cons: requires touching each subsettings page root layout

### 2. Pad only `SectionHeader.qml`

- Pros: very small diff
- Cons: fixes the clipped headers but leaves rows crowded against the page edge

### 3. Increase `marginRow` globally

- Pros: one shared token
- Cons: affects top-level layouts and control internals that already look correct

## Recommendation

Use approach 1.

Add a shared subpage gutter token in `UiMetrics.qml`, and apply it to the root `ColumnLayout` of each stacked subsettings page. That keeps the top-level page untouched while moving subpage headers and rows inward together.

## Architecture

- Add `UiMetrics.settingsPageInset`, sized as a small fraction of the existing page margin.
- Apply `anchors.leftMargin` and `anchors.rightMargin` using that token on the root content column for stacked settings pages such as AA, Audio, Display, System, Connection, Theme, Companion, Information, and Debug.
- Leave `SettingsMenu.qml` and the top-level category list unchanged.
- Leave row-internal control spacing (`marginRow`) unchanged.

## Testing

- Extend the existing structure test so it fails unless:
  - `UiMetrics.qml` exposes a shared subpage inset token
  - representative stacked settings pages use that token on both left and right margins
- Rebuild and redeploy to the Pi for visual confirmation.

## Out of Scope

- Changing the top-level Settings landing page
- Retuning row-internal margins or typography
- Reworking Equalizer page layout unless it is later requested

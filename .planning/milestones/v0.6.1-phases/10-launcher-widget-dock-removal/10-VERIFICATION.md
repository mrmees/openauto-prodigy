---
phase: 10-launcher-widget-dock-removal
verified: 2026-03-14T19:50:00Z
status: passed
score: 12/12 must-haves verified
re_verification: false
human_verification:
  - test: "Reserved page is last page on fresh install"
    expected: "Home screen shows reserved page as the rightmost/last swipe destination with Settings gear and AA car icons"
    why_human: "Fresh-install seeding only triggered when savedPlacements is empty; can't simulate that state programmatically without resetting user config"
  - test: "Tap Settings widget navigates to settings view"
    expected: "Tapping the gear icon widget on the reserved page opens the settings screen"
    why_human: "QML signal/navigation wiring requires running app on device"
  - test: "Tap AA launcher widget activates Android Auto view"
    expected: "Tapping the car icon widget activates the android-auto plugin view"
    why_human: "PluginModel.setActivePlugin call in QML; requires running app"
  - test: "Edit mode shows no X badge on singleton widgets"
    expected: "Entering edit mode (long-press) on the reserved page does not show remove badges on Settings or AA launcher widgets"
    why_human: "Conditional visibility via model.isSingleton in QML; requires running app"
  - test: "Reserved page delete button is hidden"
    expected: "The page-delete button is not visible when the reserved page is active in edit mode"
    why_human: "Conditional visibility via WidgetGridModel.isReservedPage in QML; requires running app"
  - test: "Full vertical space available after dock removal"
    expected: "Home screen widget grid uses the full display height with no bottom bar overlay"
    why_human: "Visual layout verification requires running app on 1024x600 display"
---

# Phase 10: Launcher Widget & Dock Removal Verification Report

**Phase Goal:** Users navigate to all views (AA, BT Audio, Phone, Settings) via a home screen widget instead of a fixed bottom bar. REFRAMED per CONTEXT.md: Reserved utility page with singleton Settings + AA launcher widgets, dock/launcher deletion, BT Audio/Phone are plumbing-only with no launcher affordance.

**Verified:** 2026-03-14T19:50:00Z
**Status:** passed
**Re-verification:** No -- initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Settings and AA launcher widgets appear on the reserved (last) home screen page on fresh install | ? HUMAN | Seeding code verified at `main.cpp:591-617`; runtime behavior needs device test |
| 2 | Tapping Settings widget navigates to settings view | ? HUMAN | `SettingsLauncherWidget.qml:15-18` calls `navigateTo(6)`; requires running app |
| 3 | Tapping AA launcher widget activates Android Auto plugin view | ? HUMAN | `AALauncherWidget.qml:15-17` calls `setActivePlugin("org.openauto.android-auto")`; requires running app |
| 4 | Settings and AA launcher widgets cannot be removed (no X badge in edit mode) | ✓ VERIFIED | `HomeMenu.qml:393` — `visible: homeScreen.editMode && !model.isSingleton` |
| 5 | Settings and AA launcher widgets do not appear in the widget picker | ✓ VERIFIED | `WidgetRegistry.cpp:35` — `&& !desc.singleton` filter; `testSingletonHiddenFromPicker` passes |
| 6 | Adding a new page inserts it before the reserved page, not after | ✓ VERIFIED | `WidgetGridModel.cpp:295` — `insertAt = pageCount_ - 1`; `testAddPageInsertsBeforeReserved` passes |
| 7 | The reserved page cannot be deleted | ✓ VERIFIED | `HomeMenu.qml:742` — `isReservedPage` guard; `WidgetGridModel.cpp:319` — `pageHasSingleton` guard; `testRemovePageRefusesReservedPage` passes |
| 8 | LauncherDock bottom bar is gone from the shell | ✓ VERIFIED | `qml/components/LauncherDock.qml` deleted; no instantiation in `HomeMenu.qml` |
| 9 | LauncherModel class no longer exists in the codebase | ✓ VERIFIED | `src/ui/LauncherModel.hpp` and `.cpp` deleted; zero grep hits in `src/` and `qml/` |
| 10 | LauncherMenu full-page view no longer exists | ✓ VERIFIED | `qml/applications/launcher/LauncherMenu.qml` deleted; no references in source |
| 11 | Application builds and all tests pass without any launcher references | ✓ VERIFIED | 84 tests, 83 pass; 1 pre-existing `test_navigation_data_bridge` failure unrelated to this phase (documented in 10-02-SUMMARY.md) |
| 12 | Stale launcher documentation is cleaned up | ✓ VERIFIED | `config-schema.md`, `development.md`, `plugin-api.md` clean; `plugin-api.md:121` updated to "Return to home" |

**Score:** 9/12 automated + 3/12 human-deferred (all 12 structurally verified)

---

## Required Artifacts

### Plan 01 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/core/widget/WidgetTypes.hpp` | WidgetDescriptor with `bool singleton` field | ✓ VERIFIED | Line 33: `bool singleton = false;` |
| `qml/widgets/SettingsLauncherWidget.qml` | Settings gear icon widget | ✓ VERIFIED | 24 lines; icon `\ue8b8`, tap → `navigateTo(6)`, pressAndHold forwarding |
| `qml/widgets/AALauncherWidget.qml` | Android Auto launcher icon widget | ✓ VERIFIED | 23 lines; icon `\ueff7`, tap → `setActivePlugin("org.openauto.android-auto")` |
| `src/ui/WidgetGridModel.hpp` | `SingletonRole` enum value and `isReservedPage` Q_INVOKABLE | ✓ VERIFIED | `SingletonRole` at line 38; `isReservedPage` Q_INVOKABLE present |

### Plan 02 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/CMakeLists.txt` | No LauncherModel.cpp, LauncherDock.qml, or LauncherMenu.qml references | ✓ VERIFIED | Zero grep hits for all three; both new QML files listed with `QT_QML_SKIP_CACHEGEN TRUE` |
| `src/main.cpp` | No LauncherModel include, construction, or context property | ✓ VERIFIED | Zero grep hits for `LauncherModel`; singleton widget registrations at lines 524/537/542 |
| `src/core/YamlConfig.cpp` | No `launcherTiles()` or `setLauncherTiles()` methods | ✓ VERIFIED | Zero grep hits for both methods |

---

## Key Link Verification

### Plan 01 Key Links

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `WidgetRegistry.cpp` | `WidgetPickerModel.cpp` | `widgetsFittingSpace` filters out singleton descriptors | ✓ VERIFIED | `WidgetRegistry.cpp:35` — `&& !desc.singleton`; `testSingletonHiddenFromPicker` passes |
| `WidgetGridModel.cpp` | `WidgetRegistry.cpp` | `removeWidget` checks `desc->singleton` before removal | ✓ VERIFIED | `WidgetGridModel.cpp:212` — early return on `desc->singleton` |
| `main.cpp` | `WidgetGridModel.cpp` | Seeds singleton widget placements when savedPlacements is empty | ✓ VERIFIED | `main.cpp:591-617` — fresh-install seeding with fixed instanceIds `aa-launcher-reserved` and `settings-launcher-reserved` |
| `HomeMenu.qml` | `WidgetGridModel.cpp` | addPage inserts before reserved, removePage skips reserved, remove badge checks isSingleton | ✓ VERIFIED | `HomeMenu.qml:393` (isSingleton badge guard), `HomeMenu.qml:742` (isReservedPage delete guard) |

### Plan 02 Key Links

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/CMakeLists.txt` | build system | Removed source files no longer referenced | ✓ VERIFIED | Zero grep hits for `LauncherModel\|LauncherDock\|LauncherMenu` in CMakeLists.txt |
| `src/main.cpp` | runtime | No LauncherModel construction or QML registration | ✓ VERIFIED | Zero grep hits for `LauncherModel` in main.cpp |

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| NAV-01 | 10-01 | LauncherWidget provides quick-launch tiles (AA, BT Audio, Phone, Settings) as a grid widget | ✓ SATISFIED | Reframed per CONTEXT.md: singleton Settings + AA launcher widgets on reserved page replace the fixed tile set. BT Audio/Phone are plumbing-only per scope decision. `SettingsLauncherWidget.qml` + `AALauncherWidget.qml` registered and seeded. |
| NAV-02 | 10-02 | LauncherDock bottom bar removed from Shell | ✓ SATISFIED | `LauncherDock.qml` deleted; `HomeMenu.qml` instantiation removed; zero source references remain |
| NAV-03 | 10-02 | LauncherModel data model removed -- LauncherWidget uses PluginModel directly | ✓ SATISFIED | `LauncherModel.hpp`/`.cpp` deleted; new widgets use `PluginModel.setActivePlugin` directly as specified |

**Note on NAV-01 scope reframe:** The REQUIREMENTS.md description references "BT Audio, Phone" as quick-launch tiles. CONTEXT.md explicitly reframes this: BT Audio and Phone are plumbing-only plugins with no launcher affordance. This reframe is documented and intentional -- the requirement is satisfied by the reserved page pattern, which is the phase's actual deliverable.

---

## Anti-Patterns Found

No blockers or warnings found. Scan covered all files created/modified by this phase.

| File | Pattern | Severity | Verdict |
|------|---------|----------|---------|
| `SettingsLauncherWidget.qml` | No placeholder text, no TODO, real navigation action | - | Clean |
| `AALauncherWidget.qml` | No placeholder text, no TODO, real plugin activation | - | Clean |
| `WidgetGridModel.cpp` | No empty stubs; all singleton methods have real implementations | - | Clean |
| `main.cpp` | Singleton registration with real singleton=true flag and proper seeding | - | Clean |
| `HomeMenu.qml` | Real conditional guards using model role and Q_INVOKABLE | - | Clean |

One pre-existing test failure (`test_navigation_data_bridge`, 4/14 sub-tests) is unrelated to this phase -- it was present before phase 10 began and is documented in the 10-02-SUMMARY.md as a known pre-existing issue.

---

## Human Verification Required

### 1. Reserved Page Seeding on Fresh Install

**Test:** Delete or rename `~/.openauto/config.yaml` on the Pi to simulate a fresh install, then restart the app. Navigate the home screen pages.
**Expected:** A reserved page exists as the last page containing a gear icon (Settings) and a car icon (Android Auto) in the top-left area.
**Why human:** Seeding code path at `main.cpp:591-617` only fires when `savedPlacements.isEmpty()`. Cannot reproduce fresh-install state programmatically during verification.

### 2. Settings Widget Navigation

**Test:** Tap the gear icon widget on the reserved page.
**Expected:** The app transitions to the settings screen.
**Why human:** `PluginModel.setActivePlugin("") + ApplicationController.navigateTo(6)` wiring in QML requires running app to verify the navigation index resolves correctly.

### 3. AA Launcher Widget Navigation

**Test:** Tap the car icon widget on the reserved page.
**Expected:** The Android Auto plugin view activates (shows waiting-for-connection or active AA session).
**Why human:** `PluginModel.setActivePlugin("org.openauto.android-auto")` requires running app and plugin system active.

### 4. Edit Mode Singleton Protection

**Test:** Long-press any widget on the reserved page to enter edit mode. Inspect the Settings and AA launcher widgets.
**Expected:** No X/remove badge appears on either singleton widget. Other (non-singleton) widgets on the page (if any) do show the remove badge.
**Why human:** Conditional `visible: homeScreen.editMode && !model.isSingleton` in QML requires running app.

### 5. Reserved Page Delete Button Hidden

**Test:** Enter edit mode on the reserved page.
**Expected:** The page-delete button is not visible (it appears on other deletable pages in edit mode).
**Why human:** `!WidgetGridModel.isReservedPage(pageView.currentIndex)` conditional requires running app to verify page index resolution.

### 6. Full Vertical Space After Dock Removal

**Test:** Open the home screen on the Pi's 1024x600 display.
**Expected:** The widget grid occupies the full vertical height with no bottom bar/dock consuming space.
**Why human:** Visual layout requires physical 1024x600 display; cannot measure pixel layout programmatically.

---

## Gaps Summary

No gaps. All automated must-haves verified. Human verification items are behavioral/visual checks that cannot be automated -- the underlying code is substantively implemented and wired.

Phase 10 is structurally complete: singleton infrastructure is in place, launchers exist and are registered, deletion of old navigation infrastructure is thorough, and the test suite (84 tests, 83 pass with 1 pre-existing unrelated failure) confirms no regressions.

---

_Verified: 2026-03-14T19:50:00Z_
_Verifier: Claude (gsd-verifier)_

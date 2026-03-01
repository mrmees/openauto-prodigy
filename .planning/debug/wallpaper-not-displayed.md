---
status: diagnosed
trigger: "Investigate why wallpaper images don't display on the home screen despite the wallpaper picker working."
created: 2026-03-01T00:00:00Z
updated: 2026-03-01T00:00:00Z
---

## Current Focus

hypothesis: Wallpaper.qml component exists but is never instantiated in the QML scene graph
test: Search all QML files for any `Wallpaper {` instantiation
expecting: Zero results confirms the component is orphaned
next_action: Return diagnosis

## Symptoms

expected: Selected wallpaper image visible behind launcher tiles on home screen
actual: No wallpaper visible anywhere - only solid background color
errors: None
reproduction: Select any wallpaper in Settings > Display > Wallpaper picker, return to home screen
started: Since wallpaper feature was added (02-04)

## Eliminated

(none needed - root cause found on first hypothesis)

## Evidence

- timestamp: 2026-03-01T00:00:00Z
  checked: qml/components/Shell.qml - the top-level app shell
  found: No reference to Wallpaper component anywhere in Shell.qml
  implication: The wallpaper layer is simply missing from the visual tree

- timestamp: 2026-03-01T00:00:00Z
  checked: All QML files for `Wallpaper {` instantiation
  found: Zero matches across entire qml/ directory
  implication: Wallpaper.qml is defined and built but never used

- timestamp: 2026-03-01T00:00:00Z
  checked: src/CMakeLists.txt
  found: Wallpaper.qml is registered as a QML component (lines 147-149, 268)
  implication: Build is fine, purely a wiring issue

- timestamp: 2026-03-01T00:00:00Z
  checked: ThemeService wallpaperSource property
  found: Property exists, has signal, setWallpaper() works, registered as QML context property
  implication: Backend is wired correctly - the picker saves values that nothing displays

## Resolution

root_cause: Wallpaper.qml component is defined but never instantiated in Shell.qml (or anywhere else in the QML tree)
fix: Add Wallpaper {} to Shell.qml as the bottom-most layer inside pluginContentHost, behind LauncherMenu
verification: (pending)
files_changed: []

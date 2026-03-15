---
status: resolved
trigger: "Wallpaper picker doesn't update dynamically when new images are added to ~/.openauto/wallpapers/"
created: 2026-03-01T00:00:00Z
updated: 2026-03-01T22:00:00Z
---

## Current Focus

hypothesis: buildWallpaperList() is only called once at startup via scanThemeDirectories()
test: trace all call sites of buildWallpaperList()
expecting: no runtime re-scan trigger exists
next_action: report root cause

## Symptoms

expected: Dropping a .jpg into ~/.openauto/wallpapers/ should make it appear in the wallpaper picker without restart
actual: New images only appear after app restart
errors: none
reproduction: Add any .jpg to ~/.openauto/wallpapers/ while app is running, open wallpaper picker
started: Always been this way (never implemented)

## Eliminated

(none needed - root cause confirmed on first pass)

## Evidence

- timestamp: 2026-03-01
  checked: ThemeService.cpp call sites for buildWallpaperList()
  found: buildWallpaperList() is called exactly once, from scanThemeDirectories() (line 120). scanThemeDirectories() is a startup-only method. No QFileSystemWatcher, no re-scan trigger, no refresh method exists.
  implication: The wallpaper list is a static snapshot taken at boot. Confirmed root cause.

## Resolution

root_cause: buildWallpaperList() (ThemeService.cpp:149) is only invoked from scanThemeDirectories() (line 120), which runs once at startup. There is no filesystem watcher or manual refresh mechanism to detect new files added to ~/.openauto/wallpapers/ at runtime.
fix: (not applied - diagnosis only)
verification: (n/a)
files_changed: []

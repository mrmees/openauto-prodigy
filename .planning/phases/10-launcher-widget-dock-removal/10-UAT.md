---
status: complete
phase: 10-launcher-widget-dock-removal
source: [10-01-SUMMARY.md, 10-02-SUMMARY.md]
started: 2026-03-14T20:00:00Z
updated: 2026-03-14T20:15:00Z
---

## Current Test

[testing complete]

## Tests

### 1. Settings Widget on Reserved Page
expected: On the home screen, swipe to the last page. You should see a gear icon widget (Settings) and a car icon widget (Android Auto). These are the only system-seeded widgets on this page.
result: pass

### 2. Settings Widget Tap
expected: Tap the gear icon widget. The app should navigate to the Settings view (full screen replacement, same as the old dock behavior). Press back/home to return.
result: pass

### 3. AA Launcher Widget Tap
expected: Tap the car icon widget. The app should activate the Android Auto plugin view (same as tapping the old AA dock icon).
result: pass

### 4. Dock Removed
expected: There should be NO bottom dock bar anywhere on the home screen. The full vertical space is available for widgets — no overlay at the bottom.
result: pass

### 5. Singleton Widgets Non-Removable
expected: Enter edit mode (long-press on the grid). The Settings and AA widgets should NOT show an X removal badge. Other widgets should still show the X badge. You cannot remove these two widgets.
result: pass

### 6. Reserved Page Undeletable
expected: In edit mode, navigate to the reserved (last) page. The delete-page button should NOT appear for this page. Navigate to a different page — the delete button should appear there (if more than 1 user page exists).
result: pass

### 7. Add Page Inserts Before Reserved
expected: In edit mode, tap the add-page button. A new empty page should be created and you should land on it. The reserved page (with Settings/AA widgets) should still be the LAST page — the new page was inserted before it.
result: pass

### 8. Singleton Widgets Movable
expected: In edit mode on the reserved page, long-press the Settings or AA widget and drag it to a different cell. It should move normally — singletons are pinned to existence, not to position.
result: pass

### 9. Singleton Widgets Hidden from Picker
expected: In edit mode, tap the + (add widget) button to open the widget picker. Settings and Android Auto launcher widgets should NOT appear in the picker list. Other widgets (Clock, AA Status, Now Playing, etc.) should still be available.
result: pass

### 10. All Views Reachable Without Dock
expected: Without using SSH or any dock bar, navigate to: (1) Settings — via gear widget on reserved page, (2) Android Auto — via car widget on reserved page or AA Status widget tap, (3) Home — via navbar clock tap. Every view that was reachable from the old dock is reachable without it.
result: pass

## Summary

total: 10
passed: 10
issues: 0
pending: 0
skipped: 0

## Gaps

[none]

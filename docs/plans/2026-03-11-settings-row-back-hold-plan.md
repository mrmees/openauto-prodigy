# Settings Row Back-Hold Ownership Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make every actual settings row own long-hold back so slider label/padding areas and custom row surfaces behave consistently with the rest of the settings UI.

**Architecture:** `SettingsMenu.qml` remains the whitespace/background fallback. `SettingsRow.qml` becomes the single owner of row-level hold state, ripple hookup, and back requests. Child controls keep their primary interaction behavior but coordinate with the row to suppress click/commit after long hold, and `SettingsSlider.qml` cancels or reverts against row-owned hold instead of using its own timer.

**Tech Stack:** Qt 6 QML, Qt Quick Controls, QTest/CTest

---

### Task 1: Lock the row-owned contract in the regression test

**Files:**
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Write the failing test**

Update the structure test so it now requires:

- `SettingsRow.qml` to define row-owned back-hold helpers/state
- `SettingsRow.qml` to block the menu overlay for all real rows
- `SettingsSlider.qml` to coordinate with `SettingsRow.qml` instead of defining its own dedicated hold timer
- `SettingsMenu.qml` to keep the whitespace overlay path

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: FAIL because the current QML still reflects control-owned hold, especially in `SettingsSlider.qml`.

### Task 2: Move long-hold ownership into `SettingsRow.qml`

**Files:**
- Modify: `qml/controls/SettingsRow.qml`
- Modify: `qml/controls/SettingsHoldArea.qml`

**Step 1: Write minimal implementation**

Update `SettingsRow.qml` to:

- own row-level hold state and ripple hookup
- expose helper methods/properties for child suppression and cancellation
- request back on long hold
- keep short-click behavior for `interactive: true` rows

Update `SettingsHoldArea.qml` so it can operate as a short-click-only surface that suppresses its normal action when the enclosing row long hold already won.

**Step 2: Run targeted test**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: still FAIL until slider coordination is updated.

### Task 3: Convert `SettingsSlider.qml` to row-owned hold coordination

**Files:**
- Modify: `qml/controls/SettingsSlider.qml`
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Implement minimal slider coordination**

Update `SettingsSlider.qml` to:

- stop owning a dedicated hold timer
- find the enclosing `SettingsRow.qml`
- remember its press-time value
- cancel row hold on real drag
- restore/suppress commit behavior if row hold wins

**Step 2: Run the targeted test to green**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: PASS

### Task 4: Verify the repo and document the handoff

**Files:**
- Modify: `docs/session-handoffs.md`

**Step 1: Run verification**

Run:

```bash
cd build && cmake --build . -j$(nproc)
cd build && ctest --output-on-failure
bash ./cross-build.sh
```

Expected: all pass.

**Step 2: Update handoff**

Append a new entry with:

- what changed
- why
- status
- next 1-3 steps
- verification commands/results

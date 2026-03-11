# Settings Back-Hold Indicator Follow-Up Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make the existing settings long-hold ripple indicator appear for interactive controls that own the long-hold back gesture.

**Architecture:** Reuse the single ripple owned by `SettingsMenu.qml`. `SettingsHoldArea.qml` and `SettingsSlider.qml` will locate `SettingsMenu`, map press coordinates into menu space, and call shared show/hide helpers while keeping their current gesture ownership.

**Tech Stack:** Qt 6 QML, Qt Quick Controls, QTest/CTest

---

### Task 1: Add a failing regression test for shared ripple hookup

**Files:**
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Write the failing test**

Add assertions that require:

- `SettingsMenu.qml` to expose dedicated ripple helper functions
- `SettingsHoldArea.qml` to call those helpers
- `SettingsSlider.qml` to call those helpers

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: FAIL because the control-owned path does not yet hook into the shared ripple indicator.

**Step 3: Commit the red test**

```bash
git add tests/test_settings_menu_structure.cpp
git commit -m "test(settings): require ripple hookup for control holds"
```

### Task 2: Reuse the shared ripple from control-owned hold paths

**Files:**
- Modify: `qml/applications/settings/SettingsMenu.qml`
- Modify: `qml/controls/SettingsHoldArea.qml`
- Modify: `qml/controls/SettingsSlider.qml`

**Step 1: Write minimal implementation**

Update `SettingsMenu.qml` with explicit ripple helper functions, such as:

- `showHoldIndicator(pos)`
- `hideHoldIndicator()`

Update `SettingsHoldArea.qml` to:

- find the containing `SettingsMenu`
- show the ripple on press
- hide the ripple on cancel/release
- preserve `ApplicationController.requestBack()` on long hold

Update `SettingsSlider.qml` to:

- show the ripple when the slider press arms the hold timer
- hide it on drag, cancel, or release
- hide it before requesting back on long hold

**Step 2: Run targeted test to verify it passes**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: PASS

**Step 3: Commit**

```bash
git add qml/applications/settings/SettingsMenu.qml qml/controls/SettingsHoldArea.qml qml/controls/SettingsSlider.qml tests/test_settings_menu_structure.cpp
git commit -m "fix(settings): show hold ripple on interactive controls"
```

### Task 3: Verify and redeploy

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

**Step 2: Redeploy to Pi**

Run:

```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
ssh matt@192.168.1.152 'sudo systemctl restart openauto-prodigy.service && systemctl is-active openauto-prodigy.service'
```

Expected: service reports `active`.

**Step 3: Update handoff**

Append the follow-up indicator fix details and verification results to `docs/session-handoffs.md`.

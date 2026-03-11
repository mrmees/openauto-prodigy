# Settings Scroll Hints Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add shared top/bottom scroll hints to both the top-level Settings list and stacked subsettings pages, only when more content exists in that direction.

**Architecture:** A reusable QML overlay control will target a `Flickable` and derive top/bottom hint visibility from `contentY`, `contentHeight`, and viewport height. `SettingsMenu.qml` will attach it to the category `ListView`, and each stacked subsettings page `Flickable` will attach the same control so the behavior stays consistent across Settings.

**Tech Stack:** Qt 6 QML, Qt Quick Controls, QTest/CTest

---

### Task 1: Lock the shared scroll-hint contract in the regression test

**Files:**
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Write the failing test**

Add assertions that require:

- a shared `qml/controls/SettingsScrollHints.qml` control file
- that control to expose a `Flickable` target and top/bottom icon logic
- `qml/applications/settings/SettingsMenu.qml` to attach the control to the top-level settings list
- each stacked subsettings page to attach the control to its page `Flickable`

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: FAIL because the shared control and its attachments do not exist yet.

### Task 2: Implement the shared overlay control

**Files:**
- Create: `qml/controls/SettingsScrollHints.qml`
- Modify: `qml/controls/UiMetrics.qml`

**Step 1: Write minimal implementation**

- add the reusable overlay control
- make it non-interactive
- derive top/bottom visibility from target `Flickable` scroll state
- add any small shared metrics needed for sizing/offsets

**Step 2: Run targeted test if it is sufficient to cover the control file**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: still FAIL until the control is wired into the settings screens.

### Task 3: Wire the overlay into the settings surfaces

**Files:**
- Modify: `qml/applications/settings/SettingsMenu.qml`
- Modify: `qml/applications/settings/AASettings.qml`
- Modify: `qml/applications/settings/AudioSettings.qml`
- Modify: `qml/applications/settings/CompanionSettings.qml`
- Modify: `qml/applications/settings/ConnectionSettings.qml`
- Modify: `qml/applications/settings/DebugSettings.qml`
- Modify: `qml/applications/settings/DisplaySettings.qml`
- Modify: `qml/applications/settings/InformationSettings.qml`
- Modify: `qml/applications/settings/SystemSettings.qml`
- Modify: `qml/applications/settings/ThemeSettings.qml`

**Step 1: Attach the overlay**

- give the top-level category list an `id` in `SettingsMenu.qml`
- attach `SettingsScrollHints` to that `ListView`
- attach `SettingsScrollHints` to each subpage root `Flickable`

**Step 2: Run targeted test to green**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: PASS

### Task 4: Verify, document, and deploy

**Files:**
- Modify: `docs/session-handoffs.md`

**Step 1: Run required verification**

Run:

```bash
cd build && cmake --build . -j$(nproc)
cd build && ctest --output-on-failure
bash ./cross-build.sh
```

Expected: all pass.

**Step 2: Update handoff**

Append the change, why it was made, status, next steps, and command results.

**Step 3: Commit**

```bash
git add docs/session-handoffs.md docs/plans/2026-03-11-settings-scroll-hints-design.md docs/plans/2026-03-11-settings-scroll-hints-plan.md qml/controls/SettingsScrollHints.qml qml/controls/UiMetrics.qml qml/applications/settings/SettingsMenu.qml qml/applications/settings/AASettings.qml qml/applications/settings/AudioSettings.qml qml/applications/settings/CompanionSettings.qml qml/applications/settings/ConnectionSettings.qml qml/applications/settings/DebugSettings.qml qml/applications/settings/DisplaySettings.qml qml/applications/settings/InformationSettings.qml qml/applications/settings/SystemSettings.qml qml/applications/settings/ThemeSettings.qml tests/test_settings_menu_structure.cpp
git commit -m "feat(settings): add scroll hints"
```

**Step 4: Redeploy**

Run:

```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
ssh matt@192.168.1.152 'sudo systemctl restart openauto-prodigy.service && systemctl is-active openauto-prodigy.service'
```

Expected: service returns `active`.

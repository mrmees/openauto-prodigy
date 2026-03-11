# Settings Subpage Gutters Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a small shared horizontal gutter to stacked settings subpages without changing the top-level Settings landing page.

**Architecture:** Introduce a shared `UiMetrics` inset token and apply it to the root content column of stacked subsettings pages so section headers and rows shift inward together. Keep row-internal spacing and the top-level page unchanged.

**Tech Stack:** Qt 6 QML, Qt Quick Layouts, QTest/CTest

---

### Task 1: Lock the shared subpage gutter contract in tests

**Files:**
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Write the failing test**

Add assertions that require:

- `UiMetrics.qml` to define a shared settings subpage inset token
- stacked settings pages such as `DisplaySettings.qml`, `AudioSettings.qml`, `SystemSettings.qml`, and `DebugSettings.qml` to apply that token as both left and right margins on their root content columns

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: FAIL because the current settings pages still anchor full-width.

### Task 2: Apply the shared gutter token

**Files:**
- Modify: `qml/controls/UiMetrics.qml`
- Modify: `qml/applications/settings/AASettings.qml`
- Modify: `qml/applications/settings/AudioSettings.qml`
- Modify: `qml/applications/settings/CompanionSettings.qml`
- Modify: `qml/applications/settings/ConnectionSettings.qml`
- Modify: `qml/applications/settings/DebugSettings.qml`
- Modify: `qml/applications/settings/DisplaySettings.qml`
- Modify: `qml/applications/settings/InformationSettings.qml`
- Modify: `qml/applications/settings/SystemSettings.qml`
- Modify: `qml/applications/settings/ThemeSettings.qml`

**Step 1: Write minimal implementation**

- Add `UiMetrics.settingsPageInset`
- Apply it as `anchors.leftMargin` and `anchors.rightMargin` on each stacked settings page root content column

**Step 2: Run the targeted test to green**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: PASS

### Task 3: Verify and document

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

**Step 2: Redeploy**

Run:

```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
ssh matt@192.168.1.152 'sudo systemctl restart openauto-prodigy.service && systemctl is-active openauto-prodigy.service'
```

Expected: service returns `active`.

**Step 3: Update handoff**

Append the visual-padding change and verification results.

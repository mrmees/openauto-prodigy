# Theme Delete Button Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the "Delete Theme" trash/icon row affordance with a right-side outlined delete button that matches the Bluetooth "Forget" pattern while keeping two-tap confirmation.

**Architecture:** `ThemeSettings.qml` will keep the delete confirmation state/timer but stop using whole-row interaction. The row will show plain text on the left and a button-style outlined action on the right, driven by `SettingsHoldArea` so row-owned long-hold suppression still works correctly.

**Tech Stack:** Qt 6 QML, Qt Quick Layouts, QTest/CTest

---

### Task 1: Lock the delete-button pattern in the regression test

**Files:**
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Write the failing test**

Add assertions that require `ThemeSettings.qml` to:

- define a delete button text node
- use an outlined destructive button surface
- use `SettingsHoldArea` for the delete action
- stop relying on whole-row interaction for delete

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: FAIL because the current delete row still uses a leading trash icon and interactive row click.

### Task 2: Implement the button affordance

**Files:**
- Modify: `qml/applications/settings/ThemeSettings.qml`

**Step 1: Write minimal implementation**

- remove whole-row delete interaction
- keep left-side delete label text
- add right-side outlined destructive button
- preserve the existing two-tap confirmation/timer behavior through the button click

**Step 2: Run targeted test to green**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: PASS

### Task 3: Verify, redeploy, and document

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

Append the delete-button change and verification results.

# Settings Back-Hold Interactive Controls Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make long-hold back work on settings controls such as toggles, pickers, sliders, and interactive rows while preserving normal short-tap and drag behavior.

**Architecture:** Interactive controls will own the short-tap vs long-hold decision instead of relying on the `SettingsMenu.qml` overlay. A shared hold-aware helper will handle tap-driven controls, `SettingsSlider.qml` will keep its drag path with dedicated long-hold suppression, and the overlay will remain only for blank/non-interactive space.

**Tech Stack:** Qt 6 QML, Qt Quick Controls, C++/QTest build harness, CTest

---

### Task 1: Lock the corrected gesture ownership model in tests

**Files:**
- Modify: `tests/test_settings_menu_structure.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test**

Replace the current expectation that toggle/slider/picker controls must not block back-hold at the root. Add assertions that reflect the approved design instead:

- `SettingsToggle.qml` owns back-hold
- `SettingsSlider.qml` owns back-hold
- `FullScreenPicker.qml` owns back-hold
- `SettingsListItem.qml` owns back-hold
- `SettingsRow.qml` owns back-hold when `interactive: true`
- `SettingsMenu.qml` still contains the overlay path for non-interactive space

Add new source-string expectations for the hold-aware helper usage and slider long-hold timer/suppression path.

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: FAIL because the control files do not yet implement the new ownership model.

**Step 3: Commit the red test**

```bash
git add tests/test_settings_menu_structure.cpp
git commit -m "test(settings): define interactive back-hold ownership"
```

### Task 2: Add a reusable hold-aware surface for tap-driven settings controls

**Files:**
- Create: `qml/controls/SettingsHoldArea.qml`
- Modify: `src/CMakeLists.txt`
- Modify: `qml/controls/SettingsToggle.qml`
- Modify: `qml/controls/FullScreenPicker.qml`
- Modify: `qml/controls/SettingsListItem.qml`
- Modify: `qml/controls/SettingsRow.qml`

**Step 1: Write the failing implementation expectation**

Use the Task 1 red test as the failing guard. Do not add production code until it is red.

**Step 2: Write minimal implementation**

Create `SettingsHoldArea.qml` as a reusable full-surface `MouseArea` that:

- tracks whether long hold already fired
- triggers long hold at 500 ms
- emits a short-click signal only if long hold did not fire
- calls `ApplicationController.requestBack()` on long hold

Adopt it in these controls:

- `SettingsToggle.qml`: route short tap through the helper and manually flip/save the switch state
- `FullScreenPicker.qml`: route short tap through the helper and open the dialog only on short tap
- `SettingsListItem.qml`: route short tap through the helper and keep existing click behavior
- `SettingsRow.qml`: when `interactive: true`, use the helper instead of the plain `MouseArea`

Restore or add `readonly property bool blocksBackHold: true` where these controls now own the gesture.

**Step 3: Run the targeted test to verify it passes**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: PASS

**Step 4: Commit**

```bash
git add qml/controls/SettingsHoldArea.qml src/CMakeLists.txt qml/controls/SettingsToggle.qml qml/controls/FullScreenPicker.qml qml/controls/SettingsListItem.qml qml/controls/SettingsRow.qml
git commit -m "fix(settings): add hold-aware surfaces for interactive rows"
```

### Task 3: Add slider-specific long-hold back without breaking drag behavior

**Files:**
- Modify: `qml/controls/SettingsSlider.qml`
- Modify: `qml/applications/settings/SettingsMenu.qml`
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Extend the failing test**

Add source assertions that `SettingsSlider.qml` has:

- `blocksBackHold: true`
- a dedicated 500 ms timer or equivalent long-hold state
- movement/drag cancellation for the hold path
- suppression of normal commit behavior when long hold wins

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: FAIL because the slider still only behaves as a plain slider.

**Step 3: Write minimal implementation**

Update `SettingsSlider.qml` to:

- own back-hold via `blocksBackHold: true`
- arm a 500 ms timer when the slider becomes pressed
- record the press-time value
- cancel long-hold when the user actually starts dragging
- on long hold, request back and suppress save/commit behavior

Update `SettingsMenu.qml` only as needed so the overlay clearly remains the non-interactive fallback path and does not try to own slider surfaces anymore.

**Step 4: Run targeted tests**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`

Expected: PASS

**Step 5: Commit**

```bash
git add qml/controls/SettingsSlider.qml qml/applications/settings/SettingsMenu.qml tests/test_settings_menu_structure.cpp
git commit -m "fix(settings): support long-hold back on slider rows"
```

### Task 4: Verify repository-wide behavior and document the handoff

**Files:**
- Modify: `docs/session-handoffs.md`

**Step 1: Run focused validation**

Run:

```bash
cd build && cmake --build . -j$(nproc)
cd build && ctest --output-on-failure
```

Expected: build passes, full test suite passes.

**Step 2: Run Pi-facing verification build**

Run: `bash ./cross-build.sh`

Expected: cross-build passes and produces `build-pi/src/openauto-prodigy`.

**Step 3: Update handoff**

Append a new entry to `docs/session-handoffs.md` with:

- what changed
- why
- status
- next 1-3 steps
- verification commands/results

**Step 4: Commit**

```bash
git add docs/session-handoffs.md
git commit -m "docs: record settings interactive back-hold handoff"
```

# Settings Touch Input Architecture Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the fragile QML-only settings long-press system with one settings-scoped C++ input boundary that supports blanket long-press-back without breaking normal control interaction.

**Architecture:** Add a `SettingsInputBoundary` `QQuickItem` that filters descendant pointer traffic, owns long-press detection, and swallows the matching release after a successful long-press. Simplify the settings QML so `SettingsMenu.qml` owns ripple and navigation while rows and controls stop coordinating back-hold state.

**Tech Stack:** C++17, Qt 6.8, Qt Quick/QML, QTest, CMake

---

### Task 1: Lock the new architecture in regression tests

**Files:**
- Modify: `tests/test_settings_menu_structure.cpp`

**Step 1: Write the failing test**

Update the structure test to require:

- `qml/applications/settings/SettingsMenu.qml` uses `SettingsInputBoundary`
- `qml/applications/settings/SettingsMenu.qml` no longer defines root `TapHandler` back-hold logic
- `qml/controls/SettingsRow.qml` no longer contains `backHoldOverlay`, `cancelBackHold()`, or `consumeBackHoldTrigger()`
- `qml/controls/SettingsHoldArea.qml` no longer contains `enableBackHold`, `holdTriggered`, or `ApplicationController.requestBack()`
- `qml/controls/SettingsSlider.qml` no longer looks up the enclosing row to coordinate back-hold state

**Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest --output-on-failure -R test_settings_menu_structure`

Expected: FAIL because the repo still reflects the old root/row/control hold architecture.

**Step 3: Commit the red test**

```bash
git add tests/test_settings_menu_structure.cpp
git commit -m "test(settings): require boundary-based touch architecture"
```

### Task 2: Add the C++ settings input boundary

**Files:**
- Create: `src/ui/SettingsInputBoundary.hpp`
- Create: `src/ui/SettingsInputBoundary.cpp`
- Modify: `src/CMakeLists.txt`

**Step 1: Write the failing unit test scaffold**

Add a new QTest target or extend the existing tests to cover the boundary class. The first red assertions should verify:

- a press arms the timer
- movement beyond threshold cancels it
- a completed long-press emits once
- the release after long-press is swallowed

If direct event simulation against the item is awkward, start with unit-level tests around state transitions and emitted signals, then add integration coverage later.

**Step 2: Run the new test to verify it fails**

Run: `cd build && cmake --build . --target <new-boundary-test-target> -j$(nproc) && ctest --output-on-failure -R <new-boundary-test-name>`

Expected: FAIL because `SettingsInputBoundary` does not exist yet.

**Step 3: Write minimal implementation**

Implement `SettingsInputBoundary` as a `QQuickItem` with:

- `QML_ELEMENT`
- `setFiltersChildMouseEvents(true)`
- `setAcceptTouchEvents(true)`
- `setAcceptedMouseButtons(Qt::LeftButton)`
- a `QTimer` for the hold interval
- movement threshold based on `QStyleHints::startDragDistance()` or a small configurable property
- state for the active pointer, press position, and swallow-on-release mode
- signals:
  - `pressStarted(QPointF scenePos)`
  - `pressEnded()`
  - `longPressed(QPointF scenePos)`

Implement filtering so:

- normal press, move, drag, and release continue to children until long-press wins
- once long-press fires, the matching release is consumed
- cancel/ungrab always clears state and emits `pressEnded()`

**Step 4: Run the new test to verify it passes**

Run: `cd build && cmake --build . --target <new-boundary-test-target> -j$(nproc) && ctest --output-on-failure -R <new-boundary-test-name>`

Expected: PASS

**Step 5: Commit**

```bash
git add src/ui/SettingsInputBoundary.hpp src/ui/SettingsInputBoundary.cpp src/CMakeLists.txt
git commit -m "feat(settings): add subtree input boundary for long press"
```

### Task 3: Rewire SettingsMenu to the boundary

**Files:**
- Modify: `qml/applications/settings/SettingsMenu.qml`

**Step 1: Write minimal implementation**

Replace the current root `TapHandler` back-hold logic with `SettingsInputBoundary` as the settings root or a direct wrapper around the content stack.

Wire:

- `onPressStarted` to `showHoldIndicator()`
- `onPressEnded` to `hideHoldIndicator()`
- `onLongPressed` to:
  - hide the indicator
  - call `goBack()`
  - fall back to `ApplicationController.navigateBack()` when the stack is already at root

Preserve:

- title behavior
- stack transitions
- top-level category navigation
- ripple visuals

**Step 2: Run the structure test**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest --output-on-failure -R test_settings_menu_structure`

Expected: still FAIL because rows and controls still contain old back-hold plumbing.

**Step 3: Commit**

```bash
git add qml/applications/settings/SettingsMenu.qml
git commit -m "refactor(settings): move menu hold handling to boundary"
```

### Task 4: Remove row-owned back-hold plumbing

**Files:**
- Modify: `qml/controls/SettingsRow.qml`

**Step 1: Write minimal implementation**

Delete:

- `_backHoldArmed`
- `_backHoldTriggered`
- `_settingsMenu`
- `_findSettingsMenu()`
- `_armBackHold()`
- `cancelBackHold()`
- `consumeBackHoldTrigger()`
- `_triggerBackHold()`
- `backHoldOverlay`

Keep:

- alternating row backgrounds
- interactive row pressed-state feedback
- normal `clicked()` behavior for rows that are intentionally whole-row interactive

**Step 2: Run the structure test**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest --output-on-failure -R test_settings_menu_structure`

Expected: still FAIL because `SettingsHoldArea.qml` and `SettingsSlider.qml` still encode old back-hold coordination.

**Step 3: Commit**

```bash
git add qml/controls/SettingsRow.qml
git commit -m "refactor(settings): remove row-level back hold ownership"
```

### Task 5: Simplify control helpers to normal interaction only

**Files:**
- Modify: `qml/controls/SettingsHoldArea.qml`
- Modify: `qml/controls/SettingsSlider.qml`

**Step 1: Write minimal implementation**

Update `SettingsHoldArea.qml` to:

- remove back-hold properties and logic
- keep only short-click behavior with `MouseArea`

Update `SettingsSlider.qml` to:

- remove `_findSettingsRow()`
- remove `_settingsRow`
- remove long-hold suppression and revert logic tied to row ownership
- keep normal drag behavior and config debounce saving

The slider should save values based only on user movement and release, not on back-hold coordination.

**Step 2: Run the structure test**

Run: `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest --output-on-failure -R test_settings_menu_structure`

Expected: PASS

**Step 3: Commit**

```bash
git add qml/controls/SettingsHoldArea.qml qml/controls/SettingsSlider.qml tests/test_settings_menu_structure.cpp
git commit -m "refactor(settings): remove per-control back hold plumbing"
```

### Task 6: Verify full settings behavior

**Files:**
- Modify: `docs/session-handoffs.md`

**Step 1: Run required verification**

Run:

```bash
cd build && cmake --build . -j$(nproc)
cd build && ctest --output-on-failure -R test_settings_menu_structure
cd build && ctest --output-on-failure
```

Expected: build passes, targeted structure test passes, full test suite passes.

**Step 2: Manual QA checklist**

Verify on desktop mouse and Pi touchscreen:

- single tap on each top-level settings category opens the subpage
- long-press on blank page space navigates back
- long-press on a title/header area navigates back
- long-press on a toggle navigates back without toggling
- long-press on a slider navigates back without committing an unintended value
- slider drag still works normally
- picker row tap still opens its dialog
- dropdown selection still works normally

**Step 3: Update handoff**

Append to `docs/session-handoffs.md`:

- what changed
- why
- status
- next steps
- verification commands and results

**Step 4: Commit**

```bash
git add docs/session-handoffs.md
git commit -m "docs: record settings touch boundary handoff"
```

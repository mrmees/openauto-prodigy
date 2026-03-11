# Session Handoffs

Newest entries first.

---

## 2026-03-11 — Strengthen settings scroll hint visibility

**What changed:**
- Updated [qml/controls/SettingsScrollHints.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsScrollHints.qml) so the overflow chevrons use `UiMetrics.iconSize` instead of `UiMetrics.iconSmall`.
- Increased the shared hint opacity in [qml/controls/SettingsScrollHints.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsScrollHints.qml) from `0.55` to `0.8` so the indicators remain readable on the Pi screen at driving distance.
- Extended [tests/test_settings_menu_structure.cpp](/home/matt/claude/personal/openautopro/openauto-prodigy/tests/test_settings_menu_structure.cpp) so the regression now requires the stronger scroll-hint size and opacity values.

**Why:**
- The first pass made the hints behave correctly, but on the actual Pi display they were too small and faint to read comfortably from farther away. This follow-up keeps the same overflow-only behavior and just makes the indicators legible.

**Status:** Targeted regression, full local build, full `ctest`, and Pi cross-build are complete. Pi redeploy/restart is the remaining step.

**Next steps:**
1. Deploy `build-pi/src/openauto-prodigy` to the Pi and restart `openauto-prodigy.service`.
2. Recheck top-level Settings and longer subpages on hardware to confirm the larger/stronger chevrons are readable without feeling obnoxious.
3. If they are still too timid or too loud, continue tuning only in `SettingsScrollHints.qml` so the whole settings stack stays consistent.

**Verification commands/results:**
- `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`
  - First run (before implementation): failed because `SettingsScrollHints.qml` still used the smaller/fainter values.
  - Second run (after implementation): passed.
- `cd build && cmake --build . -j$(nproc)`
  - Passed.
- `cd build && ctest --output-on-failure`
  - Passed: `100% tests passed, 0 tests failed out of 72`.
- `bash ./cross-build.sh`
  - Passed: produced `build-pi/src/openauto-prodigy`.
  - Notes: emitted the existing Qt QML plugin-link warnings and locale warnings during configure/build, but the aarch64 build completed successfully.

---

## 2026-03-11 — Settings scroll hints for overflowed pages

**What changed:**
- Added approved design/plan docs in [docs/plans/2026-03-11-settings-scroll-hints-design.md](/home/matt/claude/personal/openautopro/openauto-prodigy/docs/plans/2026-03-11-settings-scroll-hints-design.md) and [docs/plans/2026-03-11-settings-scroll-hints-plan.md](/home/matt/claude/personal/openautopro/openauto-prodigy/docs/plans/2026-03-11-settings-scroll-hints-plan.md).
- Added shared [qml/controls/SettingsScrollHints.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsScrollHints.qml), a non-interactive overlay that targets any `Flickable`/`ListView` and fades small up/down chevrons in only when there is offscreen content in that direction.
- Registered the new control in [src/CMakeLists.txt](/home/matt/claude/personal/openautopro/openauto-prodigy/src/CMakeLists.txt) so it is compiled into the QML module and validated during native/Pi builds.
- Attached the shared hint overlay to the top-level Settings category list in [qml/applications/settings/SettingsMenu.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/SettingsMenu.qml) and to every stacked settings subpage `Flickable` in [qml/applications/settings/AASettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/AASettings.qml), [qml/applications/settings/AudioSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/AudioSettings.qml), [qml/applications/settings/CompanionSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/CompanionSettings.qml), [qml/applications/settings/ConnectionSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/ConnectionSettings.qml), [qml/applications/settings/DebugSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/DebugSettings.qml), [qml/applications/settings/DisplaySettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/DisplaySettings.qml), [qml/applications/settings/InformationSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/InformationSettings.qml), [qml/applications/settings/SystemSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/SystemSettings.qml), and [qml/applications/settings/ThemeSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/ThemeSettings.qml).
- Extended [tests/test_settings_menu_structure.cpp](/home/matt/claude/personal/openautopro/openauto-prodigy/tests/test_settings_menu_structure.cpp) so the regression now requires the shared hint control, its `Flickable`-driven logic, the top-level settings list attachment, and the subpage attachments.

**Why:**
- Settings pages could scroll beyond the visible viewport with no directional cue. Small overflow-only hints make it obvious there is more content above or below without adding permanent scrollbar chrome.

**Status:** Targeted red/green regression, full local build, full `ctest`, and Pi cross-build are complete. Pi deploy/hardware validation is the remaining step.

**Next steps:**
1. Deploy `build-pi/src/openauto-prodigy` to the Pi and restart `openauto-prodigy.service`.
2. On hardware, check that the top-level Settings list and stacked subpages show subtle hints only when they actually overflow.
3. If the hints feel too loud or too faint on the Pi, tune `SettingsScrollHints.qml` icon size/opacity/inset in one place rather than per page.

**Verification commands/results:**
- `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`
  - First run (before implementation): failed because `qml/controls/SettingsScrollHints.qml` did not exist.
  - Second run (after implementation): passed.
- `cd build && cmake --build . -j$(nproc)`
  - Passed.
- `cd build && ctest --output-on-failure`
  - Passed: `100% tests passed, 0 tests failed out of 72`.
- `bash ./cross-build.sh`
  - Passed: produced `build-pi/src/openauto-prodigy`.
  - Notes: emitted the existing Qt QML plugin-link warnings and locale warnings during configure/build, but the aarch64 build completed successfully.

---

## 2026-03-11 — Theme delete row uses Bluetooth-style action button

**What changed:**
- Added approved design/plan docs in [docs/plans/2026-03-11-theme-delete-button-design.md](/home/matt/claude/personal/openautopro/openauto-prodigy/docs/plans/2026-03-11-theme-delete-button-design.md) and [docs/plans/2026-03-11-theme-delete-button-plan.md](/home/matt/claude/personal/openautopro/openauto-prodigy/docs/plans/2026-03-11-theme-delete-button-plan.md).
- Updated [qml/applications/settings/ThemeSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/ThemeSettings.qml) so the "Delete Theme" row is no longer whole-row interactive, keeps plain left-side label text, and uses a separate outlined destructive button on the right that mirrors the Bluetooth "Forget" affordance.
- Preserved the existing delete confirmation flow in [qml/applications/settings/ThemeSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/ThemeSettings.qml) by keeping the timer/reset logic and routing the action through a dedicated `triggerDeleteThemeAction()` helper.
- Extended [tests/test_settings_menu_structure.cpp](/home/matt/claude/personal/openautopro/openauto-prodigy/tests/test_settings_menu_structure.cpp) so the regression now requires the dedicated button label, destructive outline styling, `SettingsHoldArea` button click handling, and the removal of the old whole-row trash-icon pattern.

**Why:**
- The old delete row mixed destructive intent into the entire row and used a leading trash icon, which made it feel different from the established Bluetooth device action pattern. Moving delete into its own button makes the destructive action explicit while keeping the row itself readable and calm.

**Status:** Local targeted regression coverage, full local build, full `ctest`, and Pi cross-build are complete. Pi deploy/hardware verification is pending a fresh go-ahead for remote access.

**Next steps:**
1. If approved, `rsync` the new `build-pi/src/openauto-prodigy` binary to the Pi and restart `openauto-prodigy.service`.
2. On hardware, verify the Theme page delete row reads cleanly and that the `Delete` -> `Confirm` button flow feels right at touch size.
3. If more destructive rows appear later, consider extracting this outlined action affordance into a shared reusable QML control instead of cloning the pattern.

**Verification commands/results:**
- `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`
  - First run (before implementation): failed because `ThemeSettings.qml` did not define the required button-based delete affordance.
  - Second run (after implementation): passed.
- `cd build && cmake --build . -j$(nproc)`
  - Passed.
- `cd build && ctest --output-on-failure`
  - Passed: `100% tests passed, 0 tests failed out of 72`.
- `bash ./cross-build.sh`
  - Passed: produced `build-pi/src/openauto-prodigy`.
  - Notes: emitted the existing Qt QML plugin-link warnings and locale warnings during configure/build, but the aarch64 build completed successfully.

---

## 2026-03-11 — Settings subpage gutter padding

**What changed:**
- Added approved design/plan docs in [docs/plans/2026-03-11-settings-subpage-gutters-design.md](/home/matt/claude/personal/openautopro/openauto-prodigy/docs/plans/2026-03-11-settings-subpage-gutters-design.md) and [docs/plans/2026-03-11-settings-subpage-gutters-plan.md](/home/matt/claude/personal/openautopro/openauto-prodigy/docs/plans/2026-03-11-settings-subpage-gutters-plan.md).
- Added shared `UiMetrics.settingsPageInset` in [qml/controls/UiMetrics.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/UiMetrics.qml) so stacked settings pages can use one consistent horizontal gutter.
- Applied that inset to the root content column of stacked settings pages in [qml/applications/settings/AASettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/AASettings.qml), [qml/applications/settings/AudioSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/AudioSettings.qml), [qml/applications/settings/CompanionSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/CompanionSettings.qml), [qml/applications/settings/ConnectionSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/ConnectionSettings.qml), [qml/applications/settings/DebugSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/DebugSettings.qml), [qml/applications/settings/DisplaySettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/DisplaySettings.qml), [qml/applications/settings/InformationSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/InformationSettings.qml), [qml/applications/settings/SystemSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/SystemSettings.qml), and [qml/applications/settings/ThemeSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/ThemeSettings.qml).
- Extended [tests/test_settings_menu_structure.cpp](/home/matt/claude/personal/openautopro/openauto-prodigy/tests/test_settings_menu_structure.cpp) so the regression test now requires the shared subpage inset token and its use across the stacked settings pages.
- Redeployed the updated `build-pi/src/openauto-prodigy` binary to `matt@192.168.1.152` and restarted `openauto-prodigy.service`.

**Why:**
- The top-level Settings landing page already looked correct, but stacked subsettings pages were too tight against the screen edges. That made section headers like "Display" and "Navbar" feel clipped and left the page content without enough breathing room.

**Status:** Local targeted regression test, full local build, full `ctest`, cross-build, Pi deploy, and Pi service restart are complete. Visual confirmation of the new gutter on hardware is pending user verification.

**Next steps:**
1. Confirm on the Pi that section headers and row content on subsettings pages now have enough breathing room without feeling detached.
2. If it still feels cramped, increase `UiMetrics.settingsPageInset` slightly rather than changing row-internal `marginRow`.
3. Leave the top-level Settings landing page unchanged unless a separate request comes in for that screen.

**Verification commands/results:**
- `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`
  - First run (before implementation): failed because `UiMetrics.qml` did not define `settingsPageInset`.
  - Second run (after implementation): passed.
- `cd build && cmake --build . -j$(nproc)`
  - Passed.
- `cd build && ctest --output-on-failure`
  - Passed: `100% tests passed, 0 tests failed out of 72`.
- `bash ./cross-build.sh`
  - Passed: produced `build-pi/src/openauto-prodigy`.
  - Notes: emitted the existing Qt QML plugin-link warnings and locale warnings during configure/build, but the aarch64 build completed successfully.
- `rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/`
  - Passed.
- `ssh matt@192.168.1.152 'sudo systemctl restart openauto-prodigy.service && systemctl is-active openauto-prodigy.service'`
  - Passed: `active`.

---

## 2026-03-11 — Settings row-owned back-hold refactor

**What changed:**
- Added approved design/plan docs in [docs/plans/2026-03-11-settings-row-back-hold-design.md](/home/matt/claude/personal/openautopro/openauto-prodigy/docs/plans/2026-03-11-settings-row-back-hold-design.md) and [docs/plans/2026-03-11-settings-row-back-hold-plan.md](/home/matt/claude/personal/openautopro/openauto-prodigy/docs/plans/2026-03-11-settings-row-back-hold-plan.md).
- Refactored [qml/controls/SettingsRow.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsRow.qml) so every actual settings row now blocks the menu overlay, owns row-level long-hold state, drives the shared ripple through `SettingsMenu`, and exposes cancel/consume helpers for child controls.
- Updated [qml/controls/SettingsHoldArea.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsHoldArea.qml) with `enableBackHold` so it can run as a short-click-only surface that suppresses normal actions after the enclosing row long-hold wins.
- Updated [qml/controls/SettingsSlider.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsSlider.qml) to stop owning its own hold timer and instead coordinate with `SettingsRow` by canceling row hold on drag and restoring the press-time value if long-hold wins.
- Switched reusable tap-driven controls and one-off settings-row button surfaces to row-owned long-hold with short-click-only `SettingsHoldArea` use in [qml/controls/SettingsToggle.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsToggle.qml), [qml/controls/SettingsListItem.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsListItem.qml), [qml/controls/FullScreenPicker.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/FullScreenPicker.qml), [qml/controls/SegmentedButton.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SegmentedButton.qml), [qml/applications/settings/DisplaySettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/DisplaySettings.qml), [qml/applications/settings/ConnectionSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/ConnectionSettings.qml), [qml/applications/settings/DebugSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/DebugSettings.qml), and [qml/applications/settings/CompanionSettings.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/CompanionSettings.qml).
- Replaced the old slider-owned regression expectation in [tests/test_settings_menu_structure.cpp](/home/matt/claude/personal/openautopro/openauto-prodigy/tests/test_settings_menu_structure.cpp) so the structure test now guards the row-owned back-hold contract.

**Why:**
- The previous architecture split long-hold ownership across `SettingsMenu`, `SettingsHoldArea`, and `SettingsSlider`. That left dead zones, most visibly the slider label/title strip: the row blocked the menu overlay, but only the inner `Slider` armed long-hold back. Making `SettingsRow` the owner fixes that boundary instead of continuing per-control patches.

**Status:** Local targeted regression test, full local build, full `ctest`, and Pi cross-build are complete. Pi hardware validation is still pending.

**Next steps:**
1. Verify on the Pi that long-hold back now works from slider labels, row padding, and icon areas, not just the actual slider track.
2. Check custom settings rows with nested controls on hardware, especially Bluetooth pairing and Debug codec rows, to confirm row hold suppresses the subcontrol action cleanly during long hold.
3. If any nested control still leaks a normal click after long hold on Pi, either migrate that surface to `SettingsHoldArea` or explicitly opt that row out instead of reintroducing per-control timers.

**Verification commands/results:**
- `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`
  - Passed.
- `cd build && cmake --build . -j$(nproc)`
  - Passed.
- `cd build && ctest --output-on-failure`
  - Passed: `100% tests passed, 0 tests failed out of 72`.
- `bash ./cross-build.sh`
  - Passed: produced `build-pi/src/openauto-prodigy`.
  - Notes: emitted the existing Qt QML plugin-link warnings and locale warnings during configure/build, but the aarch64 build completed successfully.

---

## 2026-03-11 — Settings interactive hold ripple follow-up

**What changed:**
- Added shared ripple helpers in [qml/applications/settings/SettingsMenu.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/SettingsMenu.qml) so the existing back-hold indicator can be shown and hidden by child controls, not just the overlay path.
- Updated [qml/controls/SettingsHoldArea.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsHoldArea.qml) to find the enclosing settings menu, show the ripple on press, hide it on release/cancel, and hide it before firing long-hold back.
- Updated [qml/controls/SettingsSlider.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsSlider.qml) to drive the same shared ripple during its custom hold timer path.
- Extended [tests/test_settings_menu_structure.cpp](/home/matt/claude/personal/openautopro/openauto-prodigy/tests/test_settings_menu_structure.cpp) so the regression test now requires the control-owned path to hook into the shared ripple helpers.
- Cross-built, redeployed the new `build-pi/src/openauto-prodigy` binary to `matt@192.168.1.152`, and restarted `openauto-prodigy.service`.

**Why:**
- The long-hold gesture was working on interactive controls, but the feedback was inconsistent because only the overlay-owned path showed the ripple indicator. Controls were navigating back silently.

**Status:** Local build, full test suite, cross-build, Pi deploy, and Pi service restart are complete. Visual confirmation of the ripple on hardware is pending user verification.

**Next steps:**
1. Verify on the Pi that toggles, sliders, pickers, and segmented controls now show the same back-hold ripple during the hold.
2. If the slider ripple position feels off, consider tracking the actual press point instead of the slider center for that control.
3. Remove or reduce the temporary `BackHold-*` debug logging in [qml/applications/settings/SettingsMenu.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/SettingsMenu.qml) once Pi validation is finished.

**Verification commands/results:**
- `cd build && cmake --build . --target test_settings_menu_structure -j$(nproc) && ctest -R test_settings_menu_structure --output-on-failure`
  - Passed.
- `cd build && cmake --build . -j$(nproc)`
  - Passed.
- `cd build && ctest --output-on-failure`
  - Passed: `100% tests passed, 0 tests failed out of 72`.
- `bash ./cross-build.sh`
  - Passed: produced `build-pi/src/openauto-prodigy`.
  - Notes: cross-build emitted the existing Qt QML plugin warnings during configure, but the aarch64 build completed successfully.
- `rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/`
  - Passed.
- `ssh matt@192.168.1.152 'sudo systemctl restart openauto-prodigy.service && systemctl is-active openauto-prodigy.service'`
  - Passed: `active`.

---

## 2026-03-11 — Settings interactive control back-hold ownership

**What changed:**
- Added shared [qml/controls/SettingsHoldArea.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsHoldArea.qml) to turn `MouseArea`-driven settings controls into short-tap vs long-hold surfaces.
- Updated [qml/controls/SettingsToggle.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsToggle.qml), [qml/controls/FullScreenPicker.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/FullScreenPicker.qml), [qml/controls/SettingsListItem.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsListItem.qml), [qml/controls/SettingsRow.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsRow.qml), and [qml/controls/SegmentedButton.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SegmentedButton.qml) so long hold requests back and suppresses the normal click action.
- Updated [qml/controls/SettingsSlider.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/controls/SettingsSlider.qml) with a dedicated 500 ms hold timer that cancels on drag, requests back on long hold, and suppresses slider value commit when hold wins.
- Kept the overlay/TapHandler path in [qml/applications/settings/SettingsMenu.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/SettingsMenu.qml) as the fallback for blank and non-interactive settings space.
- Replaced the old regression assumption in [tests/test_settings_menu_structure.cpp](/home/matt/claude/personal/openautopro/openauto-prodigy/tests/test_settings_menu_structure.cpp) so the test now guards the correct ownership model instead of insisting that form controls stay invisible to back-hold logic.

**Why:**
- The overlay-only approach was fine for empty space and simple rows, but it was the wrong model for reusable interactive controls. Those controls already own the working touch path on Pi, so long-hold back needs to live there too.

**Status:** Local build, full test suite, and cross-build are complete. Pi hardware validation is still pending.

**Next steps:**
1. Deploy `build-pi/src/openauto-prodigy` to the Pi and verify long-hold back on toggles, sliders, pickers, and segmented controls with real touch input.
2. Check custom one-off settings controls in `DebugSettings.qml` and similar pages for any remaining interactive surfaces that still need the shared hold-aware path.
3. Remove or reduce the temporary `BackHold-*` debug logging in [qml/applications/settings/SettingsMenu.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/SettingsMenu.qml) once Pi validation is confirmed.

**Verification commands/results:**
- `cd build && cmake --build . -j$(nproc)`
  - Passed.
- `cd build && ctest --output-on-failure`
  - Passed: `100% tests passed, 0 tests failed out of 72`.
- `bash ./cross-build.sh`
  - Passed: produced `build-pi/src/openauto-prodigy`.
  - Notes: cross-build emitted the existing Qt QML plugin warnings during configure, but the aarch64 build completed successfully.

---

## 2026-03-11 — Settings back-hold touch delivery fix

**What changed:**
- Moved the settings long-press back `TapHandler`s into a transparent full-screen overlay in [qml/applications/settings/SettingsMenu.qml](/home/matt/claude/personal/openautopro/openauto-prodigy/qml/applications/settings/SettingsMenu.qml) with `objectName: "backHoldOverlay"` and `z: 1000`.
- Kept the existing long-press logic, `blocksBackHoldAt()` hit-testing, and ripple feedback intact; this change only alters where the handlers sit in the scene graph.
- Added `tests/test_settings_menu_structure.cpp` to guard the required overlay structure in `SettingsMenu.qml`.
- Registered the new regression test in `tests/CMakeLists.txt`.

**Why:**
- Pi touch input was never reaching the root-level `TapHandler`s. The Settings screen is covered by a full-screen `StackView` with `ListView`/`Flickable` children and nested `MouseArea`s, so the handlers needed to sit on a high-`z` glass pane above that content to observe fresh presses.

**Status:** Code change, local build, full test suite, and cross-build are complete. Pi hardware validation is still pending.

**Next steps:**
1. Deploy `build-pi/src/openauto-prodigy` to the Pi and verify that long-press back now logs `BackHold-TOUCH` events and navigates correctly on the DFRobot touchscreen.
2. If Pi behavior is correct, remove or reduce the temporary `BackHold-*` debug logging noise in `SettingsMenu.qml`.
3. If Pi still drops touch delivery, capture fresh logs with the overlay in place before changing gesture logic again.

**Verification commands/results:**
- `cd build && cmake --build . -j$(nproc)`
  - Passed.
- `cd build && ctest --output-on-failure`
  - Passed: `100% tests passed, 0 tests failed out of 72`.
- `bash ./cross-build.sh`
  - Passed: produced `build-pi/src/openauto-prodigy`.
  - Notes: cross-build emitted existing Qt QML plugin warnings during CMake configure, but the aarch64 build completed successfully.

---

## 2026-02-27 — Bluetooth cleanup, install script overhaul, aasdk removal

**What changed:**

Bluetooth:
- Created `BluetoothManager` — D-Bus Adapter1 setup, Agent1 pairing, PairedDevicesModel, auto-connect retry loop
- HSP HS profile registered by C++; HFP AG owned by PipeWire's bluez5 plugin (no conflict on fresh Trixie)
- `PairingDialog.qml` overlay for on-screen PIN confirmation
- BlueZ polkit rule for non-root pairing agent registration
- `updateConnectedDevice()` tracks Device1.Connected property changes

Install script (`install.sh`):
- Reordered: deps → build → hardware config (interactive prompts grouped after build)
- Hardware detection: INPUT_PROP_DIRECT touch filtering with device names, WiFi interface scan, audio sink selection via `pactl`
- WiFi: random password per install, country code auto-detection (iw reg → locale → ipinfo.io → US fallback), rfkill unblock
- BlueZ: `--compat` systemd override for SDP registration
- labwc: `mouseEmulation="no"` for multi-touch
- Services: openauto-prodigy, web config, system service — all created and enabled
- Launch option at end: starts app via systemd (works from SSH)
- Robustness: `{ ... exit; }` wrapper for self-update safety, ERR trap, `set -e`-safe conditionals (no `[[ ]] &&` pattern), hostapd failure non-fatal
- Build: cmake warnings suppressed by default (`--verbose`/`-v` to restore)

Documentation:
- Removed obsolete aasdk references from source code, CLAUDE.md, README, development.md, INDEX.md, aa-video-resolution.md
- Historical docs (design-decisions, debugging-notes, troubleshooting, phone-side-debug, cross-reference, archive) left as-is
- Updated roadmap: BT cleanup, install overhaul, aasdk removal → Done

**Status:** Install script validated on fresh RPi OS Trixie. Pairing works. AA connection blocked by SDP "Permission denied" (group membership needs reboot). Not yet validated end-to-end on fresh drive.

**Next steps:**
1. Reboot Pi and validate full AA session on fresh install
2. Investigate/suppress system pairing notification (draws over Prodigy UI)
3. Stale BT device pruning (two-pass approach — deferred)
4. Remove Python profile registration from codebase (Task 16 — pending)

---

## 2026-02-27 — Settings UI restructure & visual cleanup

**What changed:**
- Replaced settings tile grid with scrollable ListView + section headers
- Added `SettingsListItem.qml` control (icon + label + chevron)
- Added `SettingsQmlRole` to PluginModel for dynamic plugin settings pages
- Restructured AudioSettings into Output/Microphone sections
- Converted AboutSettings to Flickable
- Removed all small subtext, subtitles, hint text, and info banners from all settings pages — design rule: if it's not important enough to show prominently, it belongs in the web config panel
- Increased `rowH` (64→80) and `iconSize` (28→36) in UiMetrics for car-screen glanceability
- Removed dead `pinHint` reference from CompanionSettings
- Created `docs/settings-tree.md` — editable spec of every settings page, section, and control

**Why:**
- Original tile grid didn't scale with growing settings pages. Scrollable list with section headers is standard for settings UIs and handles any number of entries.
- Small screen at arm's length in a car needs big touch targets and no squinting at subtext.

**Status:** Complete. All 48 tests pass. Cross-compiled and validated on Pi.

**Key design rule established:**
- No `fontTiny` or `fontSmall` italic hint text on the Pi UI. Either show it at `fontBody` or move it to the web config panel.
- Edit `docs/settings-tree.md` to describe desired settings layout changes.

---

## 2026-02-27 — system-service shutdown timeout hardening

**What changed:**
- Updated `system-service/bt_profiles.py`:
  - `BtProfileManager.close()` now wraps `disconnect()` in try/except (called on event loop thread — `to_thread` is unsafe since dbus-next touches loop internals).
  - On exception, logs a warning and still clears `self._bus = None`.
- Updated `system-service/openauto_system.py` shutdown block:
  - Added `shutdown_sequence()` wrapped by `asyncio.wait_for(..., timeout=10.0)` for an overall teardown deadline.
  - Wrapped `proxy.disable()` with `asyncio.wait_for(..., timeout=5.0)` and warning on timeout.
  - Wrapped `ipc.stop()` with `asyncio.wait_for(..., timeout=3.0)` and warning on timeout.
  - If overall teardown timeout hits, logs forced shutdown error.
- Added/updated tests:
  - `system-service/tests/test_bt_profiles.py` for `close()` timeout warning + bus cleanup.
  - `system-service/tests/test_openauto_system.py` for proxy-disable timeout continuation, IPC-stop timeout warning, and overall forced-shutdown logging.

**Why:**
- Prevent shutdown deadlocks when D-Bus or bluetoothd is unresponsive and ensure teardown continues far enough to avoid lingering IPC socket/proxy state.

**Status:** Complete for requested `system-service` fixes and targeted tests. Repository-wide `ctest` has pre-existing environment/integration failures unrelated to these edits.

**Next steps:**
1. Re-run `ctest --output-on-failure` in an environment with display/network test prerequisites (or apply existing CI/headless test profile).
2. Validate daemon shutdown on target Pi with induced bluetoothd/D-Bus fault conditions.
3. If needed, add a focused `system-service` test target to CI so these Python reliability checks run independently of Qt integration constraints.

**Verification commands/results:**
- `pytest -q system-service/tests/test_bt_profiles.py`
  - Passed (`15 passed`).
- `pytest -q system-service/tests/test_openauto_system.py -k shutdown`
  - Passed (`3 passed, 9 deselected`).
- `pytest -q system-service/tests/test_openauto_system.py`
  - Passed (`12 passed`).
- `cd build && cmake --build . -j$(nproc)`
  - Passed (`Built target openauto-prodigy`).
- `cd build && ctest --output-on-failure`
  - Failed with 4 tests not related to `system-service` changes:
    - `test_tcp_transport` (listen/bind failure)
    - `test_companion_listener` (Qt platform/display plugin init failure)
    - `test_aa_orchestrator` (listen/bind failure on port 15277)
    - `test_video_frame_pool` (Qt platform/display plugin init failure)

---

## 2026-02-26 — Proto Repo Migration & Community Release

**What changed:**
- Created standalone repo [open-android-auto](https://github.com/mrmees/open-android-auto) with:
  - 164 proto files organized into 13 categories (moved from `libs/open-androidauto/proto/`)
  - Protocol docs: reference, cross-reference, wireless BT setup, video resolution, display rendering, phone-side debug, troubleshooting
  - Decompiled headunit firmware analysis (Alpine, Kenwood, Pioneer, Sony)
  - APK indexer tools + 156MB SQLite database (git-lfs)
- Integrated open-android-auto as git submodule in openauto-prodigy
- Updated CMakeLists.txt with custom protoc invocation (preserves `oaa/<category>/` directory structure)
- Updated 25 C++ source files (129 includes) for new proto paths
- Removed old flat proto files from openauto-prodigy
- Cleaned duplicated docs from openauto-prodigy (firmware, protocol reference, APK indexer)
- Fixed broken `docs/INDEX.md` links (aa-protocol/ paths never existed)
- Merged PR #5 (video ACK delta fix) and removed dead `ackCounter_` from both handlers

**Why:**
- Proto definitions are the most broadly useful artifact from this project. Standalone repo lets the AA community use them without pulling in the full head unit implementation.
- Deduplication keeps openauto-prodigy focused on implementation.

**Status:** Complete. Both repos pushed, Pi deployed with latest build, 48/48 tests pass.

**Key gotcha for future reference:**
- `protobuf_generate_cpp` puts all generated files flat — doesn't preserve directory structure. Must use custom `foreach` + `add_custom_command` with proper `--proto_path` when protos have subdirectory imports.

---

## 2026-02-26 — Video ACK Delta Fix (Gearhead RxVid Crash Candidate)

**What changed:**
- Updated video ACK behavior in `libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp`:
  - `AVMediaAckIndication.value` now sends delta permits (`1` per frame) instead of cumulative `ackCounter_`.
- Added regression coverage in `tests/test_video_channel_handler.cpp`:
  - `testMediaDataEmitsFrameAndAck` now sends two frames and validates both ACK payload values are `1`.

**Why:**
- Phone logs showed repeated Gearhead crash: `FATAL EXCEPTION: RxVid` with `java.lang.Error: Maximum permit count exceeded`.
- Cumulative video ACK values can over-replenish phone-side permits (triangular growth) and plausibly trigger semaphore overflow.
- Audio channel already uses delta ACK semantics; video now matches that flow-control model.

**Status:** Complete and verified locally (build + full tests pass).

**Next steps:**
1. Run extended real-device AA session (>40 minutes at 30fps) to confirm no recurrence of `RxVid` / `Maximum permit count exceeded`.
2. Capture and compare phone logcat + Pi hostapd timeline during validation session.
3. Commit and push this change set to the active Prodigy branch/PR.

**Verification commands/results:**
- `cd build && cmake --build . -j$(nproc) --target test_video_channel_handler && ctest -R test_video_channel_handler --output-on-failure`
  - First run (before fix): failed on ACK payload value (`actual 2`, `expected 1`).
  - Second run (after fix): passed.
- `cd build && cmake --build . -j$(nproc)`
  - Passed (`Built target openauto-prodigy`).
- `cd build && ctest --output-on-failure`
  - Passed (`100% tests passed, 0 tests failed out of 48`).

---

## 2026-02-26 — Documentation Cleanup & Structured Workflow

**What changed:**
- Created structured workflow files: AGENTS.md, project-vision.md, roadmap-current.md, session-handoffs.md, wishlist.md
- Moved 10 AA protocol docs into `docs/aa-protocol/` subdirectory
- Imported openauto-pro-community repo into `docs/OpenAutoPro_archive_information/` (18 files)
- Consolidated 57 completed plan docs into 5 milestone summaries (`milestone-01` through `milestone-05`)
- Moved 3 active plans to `docs/plans/active/`
- Moved 4 workspace loose files to `docs/OpenAutoPro_archive_information/needs-review/`
- Rewrote INDEX.md to match new structure
- Rewrote CLAUDE.md — trimmed from ~20KB to ~15KB, removed stale status/philosophy sections, added workflow references
- Updated workspace CLAUDE.md to reflect consolidation
- Trimmed MEMORY.md to remove content now captured in milestone summaries

**Why:** 10 days of intense development left docs scattered across 3 repos with no workflow structure. Companion-app had a proven AGENTS.md workflow loop — replicated it here.

**Status:** Complete. All 14 tasks executed. Commits are on `feat/proxy-routing-exceptions` branch.

**Next steps:**
1. Review `docs/OpenAutoPro_archive_information/needs-review/` files — decide final disposition for miata-hardware-reference.md (likely move to main docs as plugin input)
2. Start using the workflow loop — next session should reference roadmap-current.md priorities
3. Consider archiving the openauto-pro-community GitHub repo now that content is imported

**Verification:**
- 14 reference docs at `docs/` root
- 5 milestone summaries + 3 active plans in `docs/plans/`
- 10 AA protocol docs in `docs/aa-protocol/`
- 22 archive files in `docs/OpenAutoPro_archive_information/`
- No orphaned plan files

---

## 2026-02-28 — Fix Python Proto Parser Test Paths

**What changed:**
- Updated `tools/test_proto_parser.py` to use current proto locations under `libs/open-androidauto/proto/oaa/...`.
- Updated one stale expectation in `test_parse_message` from `cardinality: required` to `cardinality: optional` to match current proto3 optional fields.

**Why:**
- `tools/test_*.py` had 5 failing parser tests due to stale file paths from older proto layout and one outdated cardinality expectation.

**Status:** Complete. Parser tests now pass.

**Next steps:**
1. If desired, we can do the same path-audit for any other standalone scripts that hardcode old proto paths.
2. Keep `tools/test_*.py` in regular CI/local verification since they caught real drift.

**Verification commands/results:**
- `python3 -m pytest tools/test_*.py -v` -> `19 passed`.
- `cd build && cmake --build . -j$(nproc)` -> build passed.
- `cd build && ctest --output-on-failure` -> `100% tests passed, 0 tests failed out of 50`.

---

## 2026-02-28 — Protocol Capture Dumps (JSONL/TSV) for Proto Validation

**What changed:**
- Extended `oaa::ProtocolLogger` (`libs/open-androidauto`) with:
  - output mode switch: `TSV` (existing) or `JSONL` (validator-ready)
  - media payload inclusion toggle (`include_media`)
  - JSONL rows with fields: `ts_ms`, `direction`, `channel_id`, `message_id`, `message_name`, `payload_hex`
- Wired capture lifecycle in `AndroidAutoOrchestrator`:
  - starts capture on new AA session when enabled
  - attaches at messenger layer (`session_->messenger()`)
  - closes/detaches on teardown
- Added new YAML defaults under `connection.protocol_capture.*`:
  - `enabled: false`
  - `format: "jsonl"`
  - `include_media: false`
  - `path: "/tmp/oaa-protocol-capture.jsonl"`
- Removed duplicate app-local logger implementation and test:
  - deleted `src/core/aa/ProtocolLogger.hpp/.cpp`
  - deleted `tests/test_oap_protocol_logger.cpp`
  - removed related CMake entries
- Updated docs:
  - `docs/config-schema.md` (new capture keys + examples)
  - `docs/roadmap-current.md` (done item)
  - `docs/aa-troubleshooting-runbook.md` (test reference updated)

**Why:**
- Enable repeatable capture dumps that can feed protobuf regression validation tooling directly.
- Avoid high-noise AV payloads by default while preserving optional inclusion when needed.
- Remove duplicate logger code paths to prevent drift.

**Status:** Complete. Build + full test suite pass.

**Next steps:**
1. Add UI/web-config controls for `connection.protocol_capture.*` so capture can be toggled without manual YAML edits.
2. Capture one real AA non-media session and run it through `open-android-auto` validator workflow.
3. If needed, add capture rotation/size limits for long-running sessions.

**Verification commands/results:**
- RED (expected before implementation):
  - `cd build && cmake --build . -j$(nproc)` -> failed in `test_oaa_protocol_logger` (missing `setFormat` / `setIncludeMedia`).
  - `cd build && ctest --output-on-failure -R "test_yaml_config|test_config_key_coverage"` -> failed on missing `connection.protocol_capture.*` defaults.
- GREEN (after implementation):
  - `cd build && cmake --build . -j$(nproc)` -> passed (`Built target openauto-prodigy`).
  - `cd build && ctest --output-on-failure` -> `100% tests passed, 0 tests failed out of 51`.
  - `./cross-build.sh` -> passed (`Build complete: build-pi/src/openauto-prodigy`).

---

## 2026-02-28 — Fix `install.sh --list-prebuilt` in No-TERM Environments

**What changed:**
- Updated `install.sh` `print_header()` to avoid calling `clear` when running in non-interactive/no-`TERM` contexts.
- Updated `tests/test_install_list_prebuilt.py` to explicitly unset `TERM` in the test environment so CI/ctest consistently validates this path.

**Why:**
- `test_install_list_prebuilt` could fail when `TERM` was unset because `clear` exited non-zero under `set -e`, causing `install.sh --list-prebuilt` to exit before listing releases.

**Status:** Complete and verified locally.

**Next steps:**
1. Commit and push this fix branch.
2. Open PR noting that installer list mode now works in minimal/CI environments.

**Verification commands/results:**
- `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
  - Passed (`100% tests passed, 0 tests failed out of 51`).

---

## 2026-02-28 — Resolve `0x800*` Logger Names for Nav/Media/Phone Status Channels

**What changed:**
- Updated `libs/open-androidauto/src/Messenger/ProtocolLogger.cpp`:
  - Added channel name mappings for:
    - `ChannelId::Navigation` -> `NAVIGATION`
    - `ChannelId::MediaStatus` -> `MEDIA_STATUS`
    - `ChannelId::PhoneStatus` -> `PHONE_STATUS`
  - Added message name mappings for:
    - Navigation channel: `0x8003` -> `NAVIGATION_STATE`, `0x8006` -> `NAVIGATION_NOTIFICATION`, `0x8007` -> `NAVIGATION_DISTANCE`, `0x8004` -> `NAVIGATION_TURN_EVENT`
    - Media status channel: `0x8001` -> `MEDIA_PLAYBACK_STATUS`, `0x8003` -> `MEDIA_PLAYBACK_METADATA`
    - Phone status channel: `0x8001` -> `PHONE_STATUS_UPDATE`
- Updated `libs/open-androidauto/tests/test_protocol_logger.cpp` to assert the new channel/message-name mappings.

**Why:**
- Protocol capture JSONL/TSV previously emitted `message_name` as raw hex (`0x8001`, `0x8003`, etc.) for navigation/media/phone-status traffic, reducing capture readability and making validator map construction harder.

**Status:** Complete and verified locally.

**Next steps:**
1. Re-run a fresh AA capture on Pi and confirm these tuples now log as named messages (no `0x800*` for mapped tuples).
2. Mirror these mapping updates to standalone `open-android-auto` repo if not already present there.
3. Use the named tuples to tighten validator baseline map and treat remaining unknowns as explicit coverage gaps.

**Verification commands/results:**
- Tuple evidence confirmation (capture decode against proto candidates):
  - Generated `/home/matt/claude/personal/openautopro/_captures/chunks/oaa-protocol-capture-20260228-130258/tuple_decode_validation.tsv`
  - Result: 100% decode success for mapped tuples:
    - `(Phone->HU,10,32769)` -> `MediaPlaybackStatus` (`1993/1993`)
    - `(Phone->HU,10,32771)` -> `MediaPlaybackMetadata` (`25/25`)
    - `(Phone->HU,9,32774)` -> `NavigationNotification` (`8/8`)
    - `(Phone->HU,9,32775)` -> `NavigationDistance` (`8/8`)
    - `(Phone->HU,9,32771)` -> `NavigationState` (`2/2`)
    - `(Phone->HU,11,32769)` -> `PhoneStatusUpdate` (`1/1`)
- Required repo verification:
  - `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
  - Passed (`100% tests passed, 0 tests failed out of 51`).

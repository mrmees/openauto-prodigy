# UI Input and Widget-Grid Lifecycle Remediation — Implementation Plan

Status: ACTIVE
Date: 2026-07-24
Design: `docs/plans/2026-07-24-ui-input-widget-grid-lifecycle-remediation-design.md`
Base: `origin/main` at `7ac241596073cc9d6909832644e653272bbc4797`
Branch: `agent/ui-input-widget-grid-lifecycle-remediation`

## Global constraints

- Execute one bounded task and commit at a time. Nobody pushes mid-execution.
- Read `src/AGENTS.md` and `qml/AGENTS.md` before implementation.
- Preserve wireless-only AA, the HFP HF role, no-ofono, frozen numerics/API,
  frozen YAML fields, and the protocol-submodule boundary.
- QML reports rendered navbar geometry; C++ owns input/session state. Do not add
  a second compensating coordinate conversion in a consumer.
- Retained placements remain model-owned even when temporarily hidden.
- Behavior documentation changes ship in the same commit as the behavior.
- Use private audit artifacts only for ledger state; never expose identifiers or
  evidence in tracked files.
- A task is complete only after its focused tests are green.

## Task 1 — Navbar geometry, touch ownership, and popup sessions

Tier: main

Files:

- Modify `src/ui/NavbarController.hpp`
- Modify `src/ui/NavbarController.cpp`
- Modify `qml/components/Navbar.qml`
- Modify `tests/test_navbar_controller.cpp`
- Modify `docs/design-decisions.md`

Steps:

1. Add failing tests for all four rendered edge geometries, invalid/stale
   replacement, duplicate same-control presses, per-slot popup buttons, stale
   popup clears, and bidirectional popup switching.
2. Replace fixed navbar zone construction with a generation-safe geometry
   handshake carrying the three rendered control rectangles.
3. Have QML publish geometry after layout, scale, edge, size, or visibility
   changes and clear it when it is no longer claimable.
4. Preserve the accepted slot in each control until its matching release or
   cancellation; ignored presses must not mutate it.
5. Scope popup-button press coordinates by popup generation, zone, and slot.
6. Make stale popup cleanup remove only stale zones without changing the newer
   popup's visible/session state.
7. Document the QML/C++ geometry and gesture-ownership boundary.

Acceptance criteria:

- Registered evdev rectangles exactly match QML's rendered 20/60/20 split and
  scaled thickness on top, bottom, left, and right edges.
- Invalid, empty, non-finite, hidden, or stale geometry leaves no prior navbar
  claim zone active.
- A second press in one control cannot replace its accepted slot; a non-owner
  release cannot end the gesture; the owner release emits exactly one action.
- Popup button presses do not share coordinates across zones, touch slots, or
  generations.
- Switching between volume, brightness, and power popups works in both
  directions without a second tap or reentrant hide.
- Existing roles, actions, gesture timing, sliders, dismiss behavior,
  widget-edit controls, and unclaimed AA forwarding remain unchanged.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_navbar_controller -j$(nproc)
ctest --output-on-failure -R 'test_navbar_controller$'
```

Out of scope: AA touch serialization/mapping, visual dimensions, new controls,
gesture-duration changes, focus policy, and widget placement behavior.

Commit: `fix(ui): align navbar geometry and gesture ownership`

## Task 2 — Widget remap and page lifecycle

Tier: main

Files:

- Modify `src/ui/WidgetGridModel.hpp`
- Modify `src/ui/WidgetGridModel.cpp`
- Modify `qml/applications/home/HomeMenu.qml`
- Modify `tests/test_widget_grid_model.cpp`
- Modify `tests/test_widget_grid_remap.cpp`
- Modify `docs/design-decisions.md`
- Modify `docs/reference/widget-developer-guide.md`

Steps:

1. Add failing tests for spill beyond the last page, reserved singleton-page
   ordering, hidden placement occupancy, deferred-remap edits, and recovery
   after dimensions expand again.
2. Grow `pageCount` when spill assigns a higher page and publish the existing
   model notifications needed for persistence and page navigation.
3. Make destructive page-occupancy queries include all retained placements,
   including those hidden at the current dimensions.
4. Keep Home cleanup model-driven so a visually empty page containing a hidden
   placement is retained.
5. Before move, resize, edge-resize, opacity, configuration, or deletion, apply
   a pending remap and then promote the resulting user edit as the new base.
6. Preserve singleton protection and page ordering during spill and removal.
7. Document reachability, hidden-placement retention, and edit-after-remap
   semantics.

Acceptance criteria:

- Every visible placement satisfies `0 <= page < pageCount` after load, remap,
  spill, user edit, and page removal.
- Spill expansion persists, leaves every assigned page navigable, and keeps
  reserved singleton pages reachable and correctly ordered.
- Hidden placements prevent destructive empty-page removal and become visible
  again when dimensions permit.
- A dimension change followed by move, either resize path, opacity,
  configuration, or deletion applies the pending remap before promoting the
  edit.
- Existing proportional positioning, collision nudging, singleton protection,
  YAML fields, dashboard switching, picker behavior, and visible-row roles
  remain unchanged.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_widget_grid_model test_widget_grid_remap -j$(nproc)
ctest --output-on-failure -R 'test_widget_grid_(model|remap)$'
```

Out of scope: new placement algorithms, new widget types, schema changes,
visual redesign, dashboard migration, and navbar gesture behavior.

Commit: `fix(ui): keep widget remaps and pages reachable`

## Task 3 — Plugin-view and screen-signal lifetimes

Tier: opus

Files:

- Modify `src/ui/PluginViewHost.hpp`
- Modify `src/ui/PluginViewHost.cpp`
- Add `src/ui/ScreenDpiBinding.hpp`
- Add `src/ui/ScreenDpiBinding.cpp`
- Modify `src/ui/DisplayInfo.hpp`
- Modify `src/ui/DisplayInfo.cpp`
- Modify `src/main.cpp`
- Modify `src/CMakeLists.txt`
- Add `tests/test_plugin_view_host.cpp`
- Modify `tests/test_display_info.cpp`
- Modify `tests/CMakeLists.txt`
- Modify `docs/architecture.md`

Steps:

1. Add a plugin-host test with a real event loop proving immediate logical
   detach, survival through the current dispatch, eventual deletion, reload,
   and destruction with a still-live view.
2. Clear the active pointer and emit the existing logical transition before
   scheduling the outgoing QML item for deferred deletion; retain explicit
   ownership until deletion is safe.
3. Add a `ScreenDpiBinding` owner that holds replaceable window/current-screen
   connections and publishes only the active screen's DPI to `DisplayInfo`.
4. Replace the inline `main.cpp` connection chain with the binding owner and
   cover repeated same-screen, screen replacement, old-screen signal, null
   screen, and teardown cases.
5. Register the new source and test targets, and document the lifetime owners.

Acceptance criteria:

- `clearView()` reports no active view and emits `viewCleared` immediately but
  does not destroy the outgoing QML subtree inside the current event dispatch.
- Deferred deletion completes on the event loop; loading a replacement view
  cannot revive or confuse the outgoing view.
- Host destruction releases a still-live or pending-deletion view without a
  leak, double delete, or callback through a destroyed host.
- Rebinding disconnects the previous screen; repeated binding cannot accumulate
  callbacks; an old screen cannot update `DisplayInfo`.
- Null/current-screen changes and owner teardown are safe.
- DPI remains structural only: existing cell-size calculations and signals are
  unchanged.
- Plugin activation, focus, contexts, load-failure reporting, and view
  parenting remain unchanged.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . --target test_plugin_view_host test_display_info -j$(nproc)
ctest --output-on-failure -R 'test_(plugin_view_host|display_info)$'
```

Out of scope: plugin ABI/loading policy, focus redesign, multi-monitor widget
sizing, QML visual changes, and dashboard placement behavior.

Commit: `fix(ui): defer plugin views and own screen DPI bindings`

## Task 4 — Integration, review, deployment, and closure

Tier: sonnet

Files:

- Modify `docs/roadmap-current.md`
- Append exactly one entry to `docs/session-handoffs.md`
- Update the private remediation ledger overlay (ignored, never tracked)
- Move this design and plan to `docs/archive/plans/` with
  `Status: COMPLETED 2026-07-24`

Steps:

1. Run the focused tests from Tasks 1–3.
2. Run the full local build, explicit application target, full suite, public
   documentation checks, private-reference scan, and whitespace check.
3. Run `bash scripts/codex-review.sh origin/main`; adjudicate every P1/P2/P3.
   Fix confirmed findings and rerun the gate once if a substantial fix lands.
4. Cross-build with `./cross-build.sh`.
5. Preserve Pi rollback material, deploy only the application binary, and
   restart only `openauto-prodigy.service`.
6. Execute every required live row in the design matrix; execute optional rows
   when physical touch is available and record unavailable rows honestly.
7. Close and recompute the private ledger, update the roadmap, append one
   handoff, mark/archive the design and plan, and commit closure.
8. Push the completed branch and open a draft PR under Matthew's standing
   approval. Nobody pushes earlier.

Acceptance criteria:

- `cmake --build . -j$(nproc)` passes.
- `cmake --build . --target openauto-prodigy -j$(nproc)` passes.
- `ctest --output-on-failure` passes.
- `git diff --check origin/main..HEAD` passes.
- Public documentation contains no private identifier or evidence reference,
  and all current-document links resolve.
- Every review finding is recorded as fixed or dismissed with a bounded reason.
- `./cross-build.sh` succeeds.
- The Pi has one responsive application process, clean shell/QML startup,
  reachable dashboard pages, registered evdev navbar geometry, and available
  wireless H.265 projection; hostapd/Bluetooth PIDs and restart counts are
  unchanged; exact config and unrelated checkout state are preserved.
- The private ledger validates and recomputes; the draft PR contains the whole
  bounded tranche.

Test command:

```bash
cd ~/builds/openauto-prodigy
cmake --build . -j$(nproc)
cmake --build . --target openauto-prodigy -j$(nproc)
ctest --output-on-failure
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git diff --check origin/main..HEAD
bash scripts/codex-review.sh origin/main
./cross-build.sh
```

Out of scope: milestone tag/release, merging the PR, daemon restarts beyond the
application, re-pairing, HFP/AA protocol capture, companion work, unrelated Pi
cleanup, and optional physical-touch rows when no operator is present.

Commit: `docs: close UI input and widget-grid lifecycle remediation`

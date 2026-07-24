# UI Input and Widget-Grid Lifecycle Remediation — Design

Status: ACTIVE
Date: 2026-07-24
Grounded against: `origin/main` at `7ac241596073cc9d6909832644e653272bbc4797`

## 1. Objective

Repair a bounded set of shell input, widget-grid, and Qt object-lifetime defects
without redesigning the interface or changing Android Auto input semantics. The
work has three ownership boundaries:

1. QML owns the navbar's rendered rectangles, while `NavbarController` owns
   gesture and popup-session state;
2. `WidgetGridModel` owns reachability, remapping, and the complete placement
   set, including temporarily hidden widgets; and
3. dedicated Qt owners control plugin-view destruction and screen-DPI signal
   lifetimes.

This is one consolidated implementation tranche and one pull request.

## 2. Revalidated current state

Revalidation against the grounded commit confirmed these defects:

- QML renders navbar controls as 20/60/20 at scaled thickness, while the evdev
  path registers a 25/50/25 split at a fixed 56 pixels.
- Widget spill can assign a page beyond `pageCount`, leaving the placement
  unreachable and vulnerable to later cleanup.
- Clearing one popup's QML regions can reentrantly invalidate a newly opened
  popup session.
- `PluginViewHost::clearView()` synchronously destroys a live QML subtree and
  can do so inside its own input-event dispatch.
- Home-page cleanup counts only currently visible rows, so it can delete pages
  containing placements hidden by a grid remap.
- Popup button zones share press coordinates between zones, sessions, and touch
  slots. The single-finger drag premise was refuted, but the multi-touch state
  collision remains.
- A user edit during a selection-deferred dimension change promotes the stale
  base and cancels the pending remap.
- Each window screen change adds another DPI connection without disconnecting
  the former screen. The defect is latent on the single-screen target but the
  lifetime is still incorrect.
- An ignored duplicate navbar press can replace the accepted touch slot, so the
  first finger's release no longer completes its gesture.

The focused navbar, widget-grid, display, and QML baseline tests are green but
do not pin all of these cross-boundary contracts.

## 3. Decisions

### 3.1 Rendered navbar geometry is authoritative

`Navbar.qml` reports the actual control rectangles after layout, scaling, edge,
or visibility changes. `NavbarController` converts those rectangles to evdev
zones and replaces the complete old set atomically from the controller's point
of view. A missing, stale, non-finite, or empty geometry report unregisters the
old zones; the controller never falls back to a conflicting hard-coded split.

The bridge continues to own coordinate normalization and sticky per-slot
routing. This work changes only the shell-owned navbar claim rectangles, not
the mapping or forwarding of unclaimed Android Auto touches.

### 3.2 An accepted touch slot owns its control gesture

Each navbar control accepts one active slot. Duplicate or competing presses are
ignored without mutating ownership. Only that accepted slot may update or end
the gesture. Cancellation and teardown still reset the owner explicitly.

Popup button tracking is stored per registered zone and per touch slot, scoped
to the popup generation. A release can only complete the matching press from
the same session. Existing slider drag and dismiss behavior remain unchanged.

### 3.3 Popup generations are monotonic

Opening a popup allocates and publishes the incoming generation before the old
QML item clears its regions. A stale `clearPopupRegions()` call may remove only
its own registered zones; it cannot hide or invalidate a newer popup. Switching
volume, brightness, or power popup in either direction therefore completes in
one action without reentrant state loss.

### 3.4 Every live placement remains reachable

Spill is allowed to grow the dashboard by increasing `pageCount` to include the
highest assigned page. Expansion and reserved singleton-page ordering are
performed by the model and persisted through the existing placement/page
signals. Every visible placement satisfies `0 <= page < pageCount`.

Page occupancy and destructive cleanup count all retained placements, not only
rows currently visible at the active dimensions. A placement hidden because it
cannot fit remains recoverable when dimensions permit; an apparently empty page
containing such a placement is not removed.

### 3.5 Pending remap completes before a user mutation

Dimension changes may remain deferred while a widget is selected, preserving
the current editing gesture. Before move, resize, opacity, configuration, or
deletion mutates the placement set, the model first applies the pending remap.
The user mutation then promotes that remapped live state as the new base. This
preserves existing proportional placement, collision nudging, and serialization
semantics while preventing the saved dimensions from being overwritten early.

### 3.6 Logical plugin detach precedes deferred destruction

`PluginViewHost::clearView()` immediately clears the active-view identity and
emits the existing logical transition, then schedules the QML object for Qt
deferred deletion. The outgoing subtree therefore survives the current event
dispatch but cannot be treated as the active view. Host destruction still owns
and releases any live or deferred view without leaking it or relying on another
event-loop turn.

### 3.7 One binding owns the active screen connection

A small `ScreenDpiBinding` owner encapsulates `QWindow::screenChanged` and the
current `QScreen` DPI connection. Rebinding disconnects the old screen before
connecting the new one; repeated binding is idempotent. `DisplayInfo` continues
to consume DPI as structural display information. Cell-size formulas and
single-screen behavior do not change.

## 4. Acceptance contracts

- Navbar evdev rectangles equal the rendered 20/60/20 control rectangles and
  scaled thickness for top, bottom, left, and right edges.
- Invalid or stale geometry cannot leave an old claim zone active.
- A second finger cannot replace an accepted control slot, and only the owner
  can finish its gesture.
- Popup buttons retain independent per-zone/per-slot press state, and switching
  among popup types works in both directions with one action.
- Unclaimed touch still follows the existing sticky route into Android Auto.
- Every visible widget remains on a reachable page; spill expansion persists
  and respects reserved-page ordering.
- Temporarily hidden placements prevent destructive empty-page cleanup and
  reappear when the grid can contain them.
- Pending remap is applied before every user placement mutation.
- Plugin-view logical detach is immediate and QObject destruction is deferred
  past the current dispatch.
- Screen rebinding has exactly one current DPI publisher and no callbacks from
  a former screen.
- Existing navbar roles/actions/timing, widget YAML fields, dashboard switching,
  plugin activation/focus, and display cell sizing remain unchanged.

## 5. Dynamic verification

Focused tests drive the real navbar controller, widget-grid model/remap paths,
plugin-view host, and screen-DPI binding. The final repository gate is:

1. focused target builds and tests;
2. full local build;
3. explicit `openauto-prodigy` target build;
4. full `ctest --output-on-failure`;
5. `git diff --check`;
6. repository review gate with every finding adjudicated;
7. aarch64 cross-build; and
8. binary-only Pi deployment and live smoke checks.

## 6. Pi/live acceptance matrix

### Required after deployment

- Preserve a rollback copy of the application binary and exact configuration.
- Deploy only the cross-built application binary and restart only
  `openauto-prodigy.service`.
- Exactly one application process owns responsive IPC and reports the deployed
  version.
- The shell starts without QML binding/load errors; dashboard pages and retained
  widgets are reachable.
- The evdev device is grabbed with navbar zones registered from rendered
  geometry.
- Wireless Android Auto reconnects and H.265 projection resumes when the phone
  is available.
- Hostapd and Bluetooth retain their PIDs and restart counts.
- The Pi checkout and unrelated configuration remain untouched; restore the
  exact original configuration after any bounded test mutation.

### Optional when physical touch is available

- Tap both sides of each navbar boundary and its content-facing inner edge
  during projection.
- Exercise two fingers on one control and independent popup buttons.
- Switch volume, brightness, and power popups in every direction.
- Change display density while a widget is selected, then edit and deselect it.
- Navigate home while another touch owns a plugin view.

Unavailable physical-touch rows are reported honestly rather than inferred.

### Not required

- Bluetooth daemon restart, re-pairing, HFP call testing, AA protocol capture,
  unrelated QML inspection, multi-monitor hardware testing, or companion-app
  work.

Matthew's standing authorization covers application deployment/restart and Pi
operations for this wave. Avoid unrelated service disruption.

## 7. Out of scope

- Android Auto protocol, touch-event encoding, projection viewport mapping,
  protobuf, wireless transport, video, audio, HFP, or Bluetooth behavior.
- `proto/api/`, External API semantics, JS-shim capabilities, or YAML field
  names.
- Visual redesign, new navbar controls, new widget features, or new placement
  heuristics beyond making existing placements reachable.
- Focus policy, plugin ABI, plugin loading architecture, or dashboard schema.
- New multi-monitor cell-sizing behavior.
- Logging configuration or companion-repository changes.

All project rails in `AGENTS.md`, `src/AGENTS.md`, and `qml/AGENTS.md` remain
binding during implementation.

# Multi-Dashboards + Overlay Framework — Design (Phase E)

**Date:** 2026-07-05
Status: COMPLETED 2026-07-06
**Grounded against:** `fable-design-sprint` at `5ff16e6`. Re-verify substrate if files cited here have moved.
**Rails consumed:** arch design §6 (dashboards = multiple `WidgetGridModel`s; `dashboards[]` v3→v4 YAML migration; `WebWidgetHost` as a normal `WidgetDescriptor`; `OverlayService`/`OverlayHost` with fixed z-bands; visibility as ActionRegistry actions), §2 R4 (all mutation through actions/invokables), JS runtime design D8 (web *overlays* deferred; structure must accommodate `prodigy.context.kind: "overlay"`).

## 1. Purpose & scope

Two subsystems that share only the Shell and the ActionRegistry — designed together because arch §6 locked their contracts together, **implemented as two independent plans**:

- **Multi-dashboards:** multiple *named* dashboards, each its own widget grid (with its own pages), switchable at runtime, persisted as `dashboards[]` (YAML v4).
- **Overlay framework:** generalize the hardcoded Shell overlay stack into a registry + host with fixed z-bands and action-driven visibility.

Out of scope: overlay drag-to-move UX (the `move` *action* ships; a drag affordance is later UX), web overlays (structure ready, deferred per JS runtime D8), wiring NotificationService's non-toast kinds into overlays (seam documented, §5.6).

## 2. Substrate findings (2026-07-05, at `5ff16e6`)

1. **The current Shell z-stack already violates the locked band contract.** `IncomingCallOverlay` sits at `z:1000` ("above everything" — above the gesture overlay at 999), and `PairingDialog` (998), `NotificationArea` (998), and the dim rectangle (998) collide. Arch §6 requires content < notifications < overlays(user) < system-modal < gesture. The overlay plan re-pins all legacy z-values to band constants in one task, before any component migrates into the host.
2. **Widget instance ids are model-scoped.** `WidgetGridModel.cpp:154`: `instanceId = widgetId + "-" + nextInstanceId_++`. Per-dashboard model instances therefore scope ids naturally; `next_instance_id` becomes per-dashboard state in v4 with zero collision risk (contexts are keyed per `WidgetContextFactory`, which is also per-model).
3. **QML reaches the grid through two root context properties** (`WidgetGridModel`, `WidgetContextFactory` — `main.cpp:925,927`; ~30 references in `HomeMenu.qml`). Qt context properties are dynamic: re-calling `setContextProperty` re-resolves every binding. Dashboard switching re-points both properties at the active dashboard's instances — **zero mechanical QML churn**.
4. **Grid persistence is a main.cpp lambda** (`saveGridState`, `main.cpp:682-693`) connected to `placementsChanged`/`pageCountChanged`, loaded before connect to avoid startup clobber (comment at `main.cpp:670-672` — this load-before-connect ordering is load-bearing and must be preserved per dashboard).
5. **Reserved/singleton pages are derived, not stored**: `isReservedPage()` = `pageHasSingleton()` (`WidgetGridModel.cpp:922-924`). Singletons enter only via fresh-install seeding (`main.cpp:696-720`). Consequence: seeding only the "home" dashboard automatically keeps reserved-page semantics home-only — no new flag needed.
6. **The picker path filters by kind**: `WidgetRegistry::widgetsFittingSpace()` accepts only `contributionKind == Widget` (`WidgetRegistry.hpp:22-24`). `DashboardContributionKind` gains `WebWidget` (arch §6); the filter must accept it — one guarded change + test, carried in the dashboards plan.
7. `home.gridDensityBias`, `widget_grid.saved_cols/saved_rows` are display-derived and stay **global** (not per-dashboard) in v4.

## 3. Multi-dashboards

### 3.1 Model: `DashboardManager` (new: `src/ui/DashboardManager.{hpp,cpp}`)

Owns an ordered list of dashboards; each entry = `{ QString id, QString name, WidgetGridModel* model, WidgetContextFactory* factory }`.

- `Q_PROPERTY`: `activeDashboardId` (NOTIFY `activeDashboardChanged`), `activeIndex`, `count`, `QStringList dashboardNames` (NOTIFY `dashboardsChanged`) — enough for a pill/switcher UI without a full QAIM.
- `Q_INVOKABLE`: `switchTo(id)`, `switchToIndex(int)`, `nextDashboard()`, `previousDashboard()`, `addDashboard(name)` (id = slugified name + numeric suffix on collision; new model gets `pageCount=1`, empty placements), `removeDashboard(id)` (refused for the last remaining dashboard and for `"home"`), `renameDashboard(id, name)`, `idAt(index)`, `nameOf(id)`.
- Accessors for main.cpp wiring: `activeModel()`, `activeFactory()`, `modelForId(id)`.
- **Grid dimensions fan-out:** QML (HomeMenu) is the sole authority for grid dims (main.cpp:723-727 comment). `DashboardManager::setGridDimensions(cols, rows)` forwards to **all** models so inactive dashboards remap while hidden (their `setSavedDimensions` load-path unchanged). HomeMenu's existing `WidgetGridModel.setGridDimensions(...)` call is replaced by `DashboardManager.setGridDimensions(...)` — the one QML call-site change.
- **Persistence:** DashboardManager owns the save path (moves the `saveGridState` lambda logic out of main.cpp): connects each model's `placementsChanged`/`pageCountChanged` to a save of the **whole v4 block** (all dashboards + active id + global dims). Load order per dashboard mirrors today's: set page count → placements → next id → saved dims → *then* connect save signals (finding §2.4).
- **Soft cap:** `addDashboard` refuses beyond 8 (config-free constant; a car UI with 9+ dashboards is a UX failure, not a feature).

### 3.2 Persistence: YAML v4 + migration

```yaml
widget_grid:
  version: 4
  saved_cols: 8          # global, display-derived (unchanged)
  saved_rows: 4
  active_dashboard: home # restored at boot; falls back to first entry
  dashboards:
    - id: home           # stable slug, never renamed (name is the display label)
      name: Home
      next_instance_id: 7
      page_count: 2
      placements: [ ... ] # exact v3 per-placement schema, unchanged
```

- **Migration (in `YamlConfig`, runs once inside the load path when `widget_grid.version == 3`):** wrap existing `placements`, `page_count`, `next_instance_id` into `dashboards[0]` with `id: home, name: Home`; set `active_dashboard: home`, `version: 4`; delete the old flat keys. v3 readers never see v4 files (one-way, standard for this config — same posture as v2→v3).
- New accessors: `dashboards()` → `QList<DashboardConfig>{id, name, nextInstanceId, pageCount, placements}`, `setDashboards(...)`, `activeDashboardId()/setActiveDashboardId()`. The existing flat accessors (`gridPlacements()` etc.) remain for the migration internals and get `// v3 — internal to migration` comments; `initDefaults()` seeds a v4 block with one empty "home" dashboard.
- Fresh-install seeding of the reserved launcher page (`main.cpp:696-720`) moves into DashboardManager's load path: if the home dashboard has zero placements, seed the two singleton launchers exactly as today.

### 3.3 QML strategy: re-pointed context properties

On `activeDashboardChanged`, main.cpp re-sets the root context properties:

```cpp
engine.rootContext()->setContextProperty("WidgetGridModel", dashboardManager->activeModel());
engine.rootContext()->setContextProperty("WidgetContextFactory", dashboardManager->activeFactory());
```

All ~30 existing `HomeMenu.qml` references keep working; GridView delegates rebuild on switch (acceptable — switching is a deliberate user action, and page swipes within a dashboard stay model-internal). `DashboardManager` itself is exposed as a context property for the switcher UI and dimension fan-out. Rejected alternative: a `DashboardManager.activeModel` property consumed via a `property var gridModel` alias in HomeMenu — same effect, ~30 mechanical QML edits for no behavioral gain.

### 3.4 Switching UX + actions (rail R4)

- **Actions** (registered in main.cpp beside the existing `app.*` block, `main.cpp:763+`): `app.dashboard.next`, `app.dashboard.previous`, `app.dashboard.select` (payload: dashboard id string). QML, key bindings, and the External API all switch through these (`DispatchActionRequest` gets it free).
- **Switcher UI:** a dashboard pill row (name chips + add "+" chip) shown in HomeMenu **edit mode only** (the mode that already exists for widget placement), top-center. Tap = switch (dispatches `app.dashboard.select`); long-press a chip = rename/remove sheet; "+" = add. Normal (non-edit) switching: `app.dashboard.next/previous` — bindable to steering-wheel keys later; no always-visible chrome on the driving screen.

### 3.5 Widget size options (paid-alternative-parity picker UX)

Placement today uses `defaultCols×defaultRows` with post-placement edge-drag resize. The picker sheet gains **size preset chips** on the selected widget: the set {1×1, 2×1, 2×2, 3×2} intersected with the descriptor's `[minCols..maxCols]×[minRows..maxRows]`, default chip = descriptor default. Chips only (no free-form input); `WidgetPickerModel` already exposes the needed roles (`DefaultCols/Rows`; add `MinCols/MinRows/MaxCols/MaxRows` roles). Placement calls the existing `placeWidget(widgetId, col, row, colSpan, rowSpan)` with the chosen span — no model changes.

### 3.6 Web widgets enter the grid

Per arch §6 + JS runtime design §4: `DashboardContributionKind` gains `WebWidget`; web-widget descriptors carry `qmlComponent = WebWidgetHost.qml` and are registered by the JS-runtime package scanner (that plan's scope). The dashboards plan carries only the grid-side acceptance: `widgetsFittingSpace()` accepts `Widget` **or** `WebWidget`, `descriptorsByKind` untouched, one regression test. Nothing else in the grid path cares about kind (verified: kind is only read in the registry filter).

## 4. Overlay framework

### 4.1 Z-band contract (normative — arch §6, made numeric)

| Band | Constant (`OverlayService::ZBand`) | Base z | Occupants |
|---|---|---|---|
| Content | — | 0 | plugin views, home grid, navbar (100) |
| Notifications | `Notifications` | 1000 | NotificationArea (toasts) |
| User overlays | `User` | 2000 | third-party/plugin overlays, future web overlays |
| System-modal | `SystemModal` | 3000 | PairingDialog, IncomingCallOverlay |
| — dim fixture | — | 3500 | DisplayService dim rectangle (non-interactive Shell fixture; dims modals too, stays outside the framework) |
| Gesture | `Gesture` | 4000 | GestureOverlay (must beat everything interactive) |

Within a band: registration order (first registered = lowest). **No per-overlay arbitrary z** — an overlay picks a band, nothing else. These constants land in Shell immediately (legacy components re-pinned) even before components migrate into the host, killing the §2.1 inversions in one commit.

### 4.2 `OverlayService` (new: `src/core/services/OverlayService.{hpp,cpp}`)

C++ registry + `QAbstractListModel` in one class (the list is small; a separate model object buys nothing):

```cpp
struct OverlayDescriptor {
    QString id;              // "pairing", "incoming-call", reverse-dns for plugins
    QString sourcePluginId;  // "" for system overlays
    QUrl qmlComponent;       // loaded by OverlayHost
    ZBand band = ZBand::User;
    bool visible = false;    // initial state
    QVariantMap geometry;    // optional: {x,y,width,height}; empty = component anchors itself
};
```

- API: `registerOverlay(descriptor)` (rejects duplicate ids), `unregisterOverlay(id)`, `setVisible(id, bool)`, `toggle(id)`, `move(id, QVariantMap geometry)`, `isVisible(id)`. Roles for QML: `overlayId, qmlComponent, z (band base + intra-band index), visible, geometry`.
- **Action auto-registration (rail R4):** `OverlayService` takes the `ActionRegistry*` in its constructor. `registerOverlay("pairing", ...)` auto-registers `overlay.pairing.show`, `.hide`, `.toggle`, `.move` (move payload: QVariantMap/JSON `{x,y,width,height}`) dispatching into the service; `unregisterOverlay` removes them. QML, key bindings, and the External API (via action dispatch) all drive overlays through one path. The `overlay.` action prefix is already reserved against API clients (API design §9.1).
- Main-thread only, like ActionRegistry. Visibility changes emit `dataChanged` (model) + `overlayVisibilityChanged(id, visible)` (for C++ listeners).

### 4.3 `OverlayHost.qml` (new: `qml/components/OverlayHost.qml`)

A `Repeater` over `OverlayService` inside Shell (one host instance covering bands 1000–4000; it sits above content, and each delegate sets its own `z` from the model role):

```qml
Repeater {
    model: OverlayService
    delegate: Loader {
        active: model.visible          // load lazily, unload when hidden
        z: model.z
        source: model.qmlComponent
        // anchors.fill parent unless model.geometry provides x/y/width/height
    }
}
```

Loader-per-overlay with `active: visible` keeps hidden overlays out of memory (matters once web overlays exist). Overlay components keep binding their content to whatever context properties they already use (PairingDialog → `BluetoothManager`, IncomingCallOverlay → `CallStateProvider`) — the framework owns *stacking and visibility*, not data.

### 4.4 Registration paths & visibility drivers

- **System overlays:** registered in main.cpp after service construction. Visibility is wired by connecting the owning service's signal to an action dispatch, e.g. `BluetoothManager::pairingActiveChanged` → `actionRegistry->dispatch(pairingActive ? "overlay.pairing.show" : "overlay.pairing.hide")`. The state source stays authoritative; the action path stays the single mutation route.
- **Plugin overlays:** `IHostContext` gains `OverlayService* overlayService()` (nullable for tests, pattern of every other accessor). Dynamic plugins register in `initialize()`, must unregister in `shutdown()`.
- **Web overlays (deferred):** a future package descriptor maps to `OverlayDescriptor{qmlComponent: WebOverlayHost.qml, band: User}` with `prodigy.context.kind: "overlay"` — no framework change needed then (JS runtime D8).

### 4.5 Migration story (incremental — framework first, per arch §6)

| Component | Action in overlay plan | Later |
|---|---|---|
| z-value re-pin (all five legacy items) | **Task now** — band constants applied in place | — |
| `PairingDialog` | **Migrates now** (proof of framework: registered as `pairing`, SystemModal, visibility via `overlay.pairing.*` from `pairingActiveChanged`) | — |
| `IncomingCallOverlay` | stays in Shell (D2 just touched it; avoid churn) | migrate as `incoming-call`, SystemModal |
| `NotificationArea` | stays (toast rendering works) | migrate as `notifications`, Notifications band |
| `GestureOverlay` | stays (C++ finds it by `objectName`; EvdevTouchReader coupling) | migrate last, keep objectName |
| dim rectangle | re-pinned to 3500, permanently a Shell fixture | never migrates (display emulation, not UI) |

### 4.6 NotificationService seam (documented, not wired)

Arch §6 nominates the overlay framework as the eventual home for the unrendered notification kinds (`incoming_call`, `status_icon`). Deliberately **not wired in v1**: the incoming-call UI is already provider-driven (D2), and `status_icon` has no producer worth rendering yet. The seam when wanted: a small adapter observing `NotificationService` posts and dispatching `overlay.<kind>.show`. Recorded so nobody invents a second notification pipeline instead.

## 5. What stays open (deliberately)

| Question | Why deferred |
|---|---|
| Overlay drag-to-move UX | `move` action exists; drag affordance is pure UX (arch §7) |
| Web overlay packaging/descriptor | JS runtime plan owns packages; framework slot ready |
| Per-dashboard wallpaper/theme accents | No user pull yet; additive to v4 schema (`dashboards[].wallpaper`) |
| Dashboard switch gesture (swipe-from-edge) | Conflicts with page swipe; needs on-device feel testing — actions ship first |

## 6. Executor Guidance (mandatory)

**Invariants — violating any is stop-and-ask:**
1. Enum/int values already persisted or QML-compared are frozen: `DashboardContributionKind::Widget/LiveSurfaceWidget` order (append `WebWidget` after), v3 placement field names (v4 reuses them verbatim inside `dashboards[]`).
2. All overlay/dashboard mutation flows through ActionRegistry actions or the owning service invokables (rail R4). No QML writes service state directly.
3. Z values come from band constants only — if a component needs "just one higher", the design is wrong; stop.
4. The load-before-connect ordering for grid persistence (§2.4) applies per dashboard — connecting save signals before placements load wipes user config on boot.
5. `removeDashboard("home")` must stay refused — the reserved launcher page (singletons) lives there and seeding assumes it exists.
6. Migration must be idempotent: a v4 file passes through untouched; only `version: 3` triggers the wrap.

**Pitfalls:**
- Re-calling `setContextProperty` with the same name is the designed switch mechanism (§3.3) — do not "optimize" it into a one-time set.
- `WidgetGridModel::setPlacements` emits `placementsChanged` — DashboardManager must guard against its own save handler during load (same clobber class as §2.4).
- QML `Loader.source` + `active` on the OverlayHost delegate: setting `active: false` destroys the item — overlays must not hold state they can't rebuild from their bound services.
- Q_OBJECT in new headers → .cpp listed in `src/CMakeLists.txt` (MOC).
- ActionRegistry handlers run synchronously on the main thread — overlay show/hide handlers must stay trivial (a `setVisible` call).

**Test strategy:** YamlConfig v4 round-trip + v3→v4 migration (feed a literal v3 YAML string, assert dashboards[0] contents + version 4 + idempotence); DashboardManager add/remove/switch/rename + persistence + seeding (real YamlConfig on temp file); OverlayService registration/duplicate-reject/band-z assignment/visibility/action auto-(un)registration (real ActionRegistry); registry `widgetsFittingSpace` accepts WebWidget. Full suite + cross-build per project workflow.

**Definition of done:** both plans' tasks committed; full ctest + `./cross-build.sh` green; on-device sanity: create a second dashboard, place widgets, switch via action dispatch from web-config or QML, reboot → both dashboards restored; pairing dialog still appears via the framework path.

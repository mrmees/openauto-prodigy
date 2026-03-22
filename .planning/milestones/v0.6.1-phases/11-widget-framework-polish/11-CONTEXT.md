# Phase 11: Widget Framework Polish - Context

**Gathered:** 2026-03-14
**Status:** Ready for planning

<domain>
## Phase Boundary

Formalize the widget framework contract so that first-party widgets follow consistent rules and third-party widget authors have a clear, documented spec to build against. Rewrite all 6 existing widgets to comply with the formalized contract (4 content widgets as primary reference implementations, 2 launcher widgets as low-complexity compliance work). Implement resize clamping enforcement in the grid model. This is "make the widget framework a real developer platform" — not just pixel breakpoint fixes.

Four contract areas: size/layout, data+actions, lifecycle/performance, authoring/docs.

</domain>

<decisions>
## Implementation Decisions

### 1. Size/Layout Contract

#### Sizing envelope
- `minCols/minRows/maxCols/maxRows/defaultCols/defaultRows` in WidgetDescriptor IS the full sizing contract
- **No aspect ratio enforcement** — widgets must gracefully adapt to any span inside their declared min/max range. No host-level ratio policy.
- **Resize clamping (WF-02):** WidgetGridModel enforces descriptor min/max as single source of truth during drag-resize. The QML resize handle must be clamped — user cannot drag beyond the declared limits.

#### Adaptation model
- **Single QML component per widget with span-driven internal adaptation**
- Host exposes `colSpan` and `rowSpan` as live properties (with NOTIFY) in the widget context — these are the stable layout contract
- **Spans only** — no derived convenience booleans (isCompact, isWide, etc.). Widgets derive their own internal states from spans.
- Widgets must render meaningfully at every span inside their declared envelope
- If a widget needs radically different UIs at different sizes, split into internal subcomponents — not separate registered widget variants
- If a widget can't adapt cleanly, its min/max contract is wrong and should be tightened

#### Minimum size rules
A widget's declared minimum span is valid only if, at that span:
1. Critical text is legible
2. Critical content is not clipped
3. The widget is not reduced to decorative placeholder chrome
4. Any distinct tappable target exposed at that span meets `UiMetrics.touchMin`

Nuance: don't require every widget to have multiple separate buttons at min size. If the widget is too small for separate controls, it can collapse to one larger tap target. If it still can't do that cleanly, its minimum span is wrong.

#### Grid-doesn't-fit policy
If a grid configuration cannot satisfy a widget's declared minimum span, Prodigy does not render that widget at a smaller size. The placement is retained and the widget is suppressed until a compatible grid is available. No auto-shrink below min, no auto-bump to another page.

### 2. Data + Actions Contract

#### Data ingress
- **Ambient platform singletons** (ThemeService, UiMetrics) may stay ambient — they are app-wide styling/layout concerns, not widget-specific data
- **Widget data providers** (ProjectionStatus, NavigationProvider, MediaStatus, AudioService, etc.) should be exposed through WidgetInstanceContext / IHostContext injection, NOT fished from root QML context properties. This aligns with the platform refactor direction (2026-03-13-platform-plugin-architecture-design.md) and WidgetInstanceContext.hpp
- **Published plugin capabilities** for domain-specific cross-plugin data (OBD, cameras, radio, etc.) — when the first real case lands. For now: document the pattern, reserve the architectural direction, don't build a capability registry yet.
- **No arbitrary global spelunking** — widgets may not reach into undocumented context properties or plugin internals
- **IHostContext remains for platform-owned services.** Plugin-published capabilities will later use a separate typed mechanism — not "add more random stuff to root context."
- `createWidgetContext()` style injection is fine for a plugin's own widget helpers. Cross-plugin reuse goes through published contracts.
- **Missing capability handling (deferred implementation):** the intent is that widgets depending on an absent capability get filtered from the picker and show placeholder if already placed. However, WidgetDescriptor currently has no field to declare required capabilities (WidgetTypes.hpp), and WidgetPickerModel only filters by available space (WidgetPickerModel.cpp:75). **Phase 11 documents this as a future contract rule but does NOT add capability metadata or picker filtering — that ships when the capability registry lands with the first real cross-plugin case.**

#### Command egress
- **ActionRegistry for cross-cutting app/platform actions** (navigation, theme toggle, app launch, shell-level commands)
- **Typed provider/capability methods for domain-specific operations** (media transport, phone answer/hangup, future OBD reset)
- **No generic "widget controller" by default.** Only add a widget-specific controller when genuinely instance-specific behavior doesn't belong on a shared provider/capability interface.

#### Per-instance config
- Host-owned placement state: top-level placement fields (col, row, colSpan, rowSpan, page, visible, opacity)
- **Widget-owned per-instance prefs: nested under `widget_grid.placements[].config`** — same YAML placement record, but separate namespace from host fields
- `defaultConfig` from descriptor provides defaults for missing keys
- Global plugin defaults belong under `plugin_config`, not copied into every instance
- **Compatibility rule:** widgets must tolerate missing/unknown config keys and merge against `defaultConfig` on read. This avoids undefined upgrade behavior when a widget changes its config schema — no migration code needed, just graceful fallback to defaults.

### 3. Lifecycle/Performance Contract

#### Lifecycle
- **No custom created/destroying signals** — QML `Component.onCompleted` and `Component.onDestruction` are sufficient
- **Expose colSpan/rowSpan as live properties with NOTIFY** — widgets adapt declaratively via property bindings, not lifecycle callbacks
- Explicit lifecycle signals reserved for exceptional cases only
- Contract: widgets rely on normal QML creation/destruction hooks. The host exposes live layout properties; widgets adapt declaratively.

#### Performance rules (strict, not guidelines)
- Normal dashboard widgets are expected to be cheap
- **No polling timers** unless there is no practical event-driven source and the interval is justified
- **Off-page widgets must not keep expensive work alive.** The host currently keeps current, previous, and next pages instantiated (HomeMenu.qml:149) — "off-page" means pages beyond that window, not "not loaded." The host is responsible for managing widget loading/unloading. Widgets that need to gate expensive work should bind to a host-provided `isCurrentPage` property (or equivalent) rather than assuming unloaded === off-page.
- Subscriptions, signal connections, and transient resources must clean up when the widget unloads
- Continuous rendering, video, heavy Canvas, or other expensive surfaces are not normal widgets — they belong in a different contribution type
- Escape hatch: "allowed with justification" rather than "forbidden" — but the default answer is no

#### Interaction model
- Normal widgets may use **tap on explicit controls** (multiple tap targets allowed — e.g., Now Playing prev/play/next)
- Normal widgets **must reserve press-and-hold for host edit/context handling** (forward via `requestContextMenu()`)
- Normal widgets **may not own** drag, flick/scroll, text input, selection gestures, or popup/dialog workflows
- If a widget needs richer interaction, it is not a normal dashboard widget — it belongs in a different contribution type or full app surface

#### Error/empty/disconnected states
- Default contract: widget stays visible and shows a **muted placeholder state**
- Standard treatment: dimmed content (~40% opacity), widget icon, short status text
- Widget-specific parts: exact icon and message
- No host-level auto-hide for normal runtime unavailability
- Already the converged pattern in NowPlayingWidget.qml and NavigationWidget.qml

### 4. Authoring/Docs Contract

#### Theme and metrics rules
- **Required for structural UI:** widgets must use `UiMetrics` for layout, sizing, spacing, fonts, and touch targets; must use `ThemeService` for structural UI colors (backgrounds, text, borders)
- **Flexible for visualization/content colors:** widgets may define custom content palettes for domain visuals (gauges, charts, route lines, album art accents, warning bands). Custom colors should sit on top of theme-derived surfaces and maintain decent contrast.

#### Canonical example
- **Rewrite all 4 content widgets** (Clock, AA Status, Now Playing, Navigation) as **primary reference implementations** — spans instead of pixels, proper pressAndHold forwarding, placeholder states, theme/metrics compliance, WidgetInstanceContext provider access
- **Update 2 singleton launcher widgets** (SettingsLauncher, AALauncher) as **secondary compliance targets** — low-complexity work verifying:
  - Span-based sizing compliance (descriptor min/max honored)
  - ThemeService / UiMetrics compliance (structural colors, touch targets)
  - Interaction contract compliance (tap + pressAndHold forwarding)
  - Edit-mode context-menu forwarding (already present, verify correctness)
- The 4 content widgets serve as the primary "how to write a Prodigy widget" reference; the 2 launchers demonstrate the minimal-compliance case

#### Type split (document now, build later)
- **Normal dashboard widget:** cheap tile, declarative layout, tap controls only, no focus ownership, no heavy continuous rendering
- **Live surface (future):** reserved for video, maps, camera, heavy canvas/render loops, text input, scroll/drag-heavy interaction, or anything needing suspend/resume/resource admission rules
- Phase 11 formalizes this boundary in documentation. LiveSurface infrastructure is a future milestone.

### Verification Targets
Phase 11 must prove:
- Resize clamping: drag-resize stops at declared min/max (not just model rejection)
- Live span updates: changing widget size updates colSpan/rowSpan properties and widget reflows
- Widget-context provider access: data providers are injected via WidgetInstanceContext, not fished from root context
- Picker filtering: no regressions in widget picker behavior after contract changes
- Placeholder states: all 4 content widgets show correct muted state when their data source is unavailable
- Span-based breakpoints: all 4 content widgets adapt layout based on colSpan/rowSpan, not pixel dimensions

### Claude's Discretion
- Exact resize clamping UX (visual feedback during drag-resize when hitting min/max limits)
- How colSpan/rowSpan are injected into widget context (model role bindings vs explicit QObject properties)
- Whether to create a WidgetContract.md or fold the contract into the existing plugin-api.md
- Instance config API surface (read/write methods, change notification mechanism)
- Which existing widget serves as the primary documented walkthrough vs supporting examples
- Stale comment cleanup in HomeMenu.qml (line 118 "above page dots" reference)

</decisions>

<specifics>
## Specific Ideas

- "I want to formalize it, and get it where we can start developing new widgets in accordance with a formalized framework"
- Codex structured the discussion into four sections: size/layout, data+actions, lifecycle/performance, authoring/docs — that structure should carry through to the contract document
- OBD plugin example: plugin publishes IObdTelemetryProvider → separate gauge-widget plugin declares it wants that capability → host injects if available → widget renders against interface, not plugin internals
- "Don't let widget authors randomly reach into globals"
- "Strict rules without hesitation" on performance — "this is one of the places where 'guidelines only' turns into a pile of slow, battery-draining bullshit later"
- "If you leave it loose, every rewritten widget will invent its own dead-state UX for no good reason"

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `WidgetDescriptor` (WidgetTypes.hpp): Already has min/max/default cols/rows + singleton flag — needs colSpan/rowSpan exposure to QML
- `WidgetGridModel`: Exposes MinColsRole/MaxColsRole etc. — resizeWidget() already has min/max checks but they may not be enforced during QML drag
- `NowPlayingWidget.qml:192`, `NavigationWidget.qml:142`: Already use muted placeholder pattern (~40% opacity + icon + status text) — formalize as the standard
- `UiMetrics.qml:80`: `touchMin` property (56 * scale default) — use as the touch target floor, not an invented number
- `ActionRegistry`: Already handles navbar gestures and app-level actions — blessed path for widget command dispatch
- `requestContextMenu()` pattern: Already in AAStatusWidget.qml:43 and NavigationWidget.qml:166 — formalize as required

### Current Pixel Breakpoints (to be replaced with spans)
- `ClockWidget.qml:9-10`: `width >= 250`, `height >= 180`
- `AAStatusWidget.qml:8`: `width >= 250`
- `NowPlayingWidget.qml:9-10`: `width >= 400`, `height >= 180`
- `NavigationWidget.qml:9-11`: `width >= 400`, `height >= 180`, `width >= 500`

### Integration Points
- `HomeMenu.qml` resize handle: must enforce WidgetDescriptor min/max during drag
- `WidgetGridModel::resizeWidget()`: Already has descriptor checks — verify QML drag clamps before calling
- Widget context injection: colSpan/rowSpan need to be live-updating properties accessible to widget QML
- `plugin-api.md`: Needs the formalized widget contract (or a new WidgetContract.md)

</code_context>

<deferred>
## Deferred Ideas

- Capability registry for cross-plugin data sharing — build when first real case (OBD) lands, not speculatively
- LiveSurface contribution type — future milestone, just document the boundary now
- Widget settings UI pattern — how widgets expose per-instance configuration to the user (future)
- Widget-specific controller QObject — only when a real case demands instance-specific behavior

</deferred>

---

*Phase: 11-widget-framework-polish*
*Context gathered: 2026-03-14*

# Phase 06: Content Widgets - Research

**Researched:** 2026-03-12
**Domain:** Qt 6 / C++ data bridges + QML widget UI, AA protocol data surfacing
**Confidence:** HIGH

## Summary

Phase 06 wires existing AA protocol data (navigation turn events, media status) to QML through two new C++ bridge objects, then builds two content widgets that consume that data. The protocol handlers already emit all needed signals -- the work is (1) creating Q_PROPERTY bridge classes that accumulate state, (2) exposing them as root QML context properties, (3) building responsive QML widgets with pixel-based breakpoints, and (4) implementing a QQuickImageProvider for phone-provided maneuver icon PNGs.

The unified Now Playing widget replaces the BT-only NowPlayingWidget by introducing a MediaDataBridge that merges AA media and BT A2DP metadata with deterministic source priority (AA > BT > none). The existing BtAudioPlugin's Q_PROPERTY pattern is the model to follow, but the bridge lives at root context level rather than inside a plugin activation scope.

**Primary recommendation:** Build two simple Q_OBJECT bridge classes (NavigationDataBridge, MediaDataBridge), expose them as root context properties in main.cpp, wire them to handler signals in the orchestrator, and let widget QML bind to their properties. No EventBus needed -- direct signal connections are simpler and type-safe.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Navigation widget layouts: 2x1 (icon+distance), 3x1 (icon+distance+road), 3x2+ (primary+next-turn preview), 4x2 (spacious 3x2)
- Navigation inactive: muted placeholder at ~40% opacity, stays visible
- Distance units: use phone's distance_unit field directly (meters=1, km=2, miles=3, feet=4, yards=5)
- Source priority: instant switch to AA on connect, BT returns on disconnect, no manual toggle
- Now Playing inactive: muted placeholder at ~40% opacity, controls visible but dimmed
- Now Playing layouts: 2x1 (title+controls, no artist), 3x1 (horizontal strip), 3x2 (full metadata+controls)
- Source indicator: small icon (BT/AA icon), NOT text label
- Album art: deferred to POLISH-02. Bridge should accept/store bytes but not expose to QML yet
- Tap-to-action: nav widget always opens AA fullscreen; NP widget has no background tap, only control buttons
- Playback controls: AVRCP for BT, InputEvent (sendButtonPress) for AA

### Claude's Discretion
- NavigationDataBridge and MediaDataBridge internal architecture (Q_PROPERTYs, signal wiring)
- QQuickImageProvider implementation for maneuver icon PNGs
- Distance formatting logic (when to show meters vs km, decimal places)
- Control button sizing and spacing at different breakpoints
- How to expose bridges to widget QML context (root context vs WidgetInstanceContext)
- EventBus vs direct signal connections for data flow

### Deferred Ideas (OUT OF SCOPE)
- Album art display in Now Playing widget -- POLISH-02 (future milestone)
- Lane guidance arrows in nav widget -- LANE-01 (uncharacterized bitmap format)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| NAV-01 | Navigation widget shows maneuver icon, road name, distance | NavigationChannelHandler.navigationTurnEvent provides all fields; NavigationDataBridge accumulates latest values as Q_PROPERTYs |
| NAV-02 | Maneuver icon renders phone PNG via QQuickImageProvider, Material fallback | QQuickImageProvider subclass stores latest PNG bytes; QML Image uses `image://navicon/current`; fallback MaterialIcon when no image |
| NAV-03 | Muted "No active navigation" state | NavigationChannelHandler.navigationStateChanged(false) + bridge property `navActive`; QML opacity 0.4 on inactive |
| NAV-04 | Tapping nav widget opens AA fullscreen | MouseArea calls `PluginModel.setActivePlugin("org.openauto.android-auto")` -- same pattern as AAStatusWidget |
| NAV-05 | NavigationDataBridge with Q_PROPERTYs | New C++ class wired to navHandler_ signals in orchestrator, exposed as root context property |
| MEDIA-01 | Unified Now Playing: title, artist, controls | MediaDataBridge merges AA + BT sources; QML widget binds to unified properties |
| MEDIA-02 | AA metadata when connected, BT as fallback | MediaDataBridge.source property switches on AA connect/disconnect signal |
| MEDIA-03 | Source indicator label | Bridge exposes `source` enum (AA/BT/None); QML shows corresponding icon |
| MEDIA-04 | Controls map to correct source | Bridge exposes Q_INVOKABLE play/pause/next/prev that delegates to correct backend |
| MEDIA-05 | AA media metadata exposed via MediaDataBridge | New C++ class connected to mediaStatusHandler_ signals |
| MEDIA-06 | Replaces BT-only NowPlayingWidget | Unified widget registered as `org.openauto.now-playing`; old BT widget descriptor removed from BtAudioPlugin |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6.4+ (dev) / Qt 6.8 (Pi) | 6.4-6.8 | QObject property system, QQuickImageProvider | Already in use; all data bridges are QObjects |
| QML | (bundled) | Widget UI with responsive breakpoints | Already in use for all widgets |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| QQuickImageProvider | Qt built-in | Serve phone PNG icons to QML Image elements | Nav widget maneuver icons |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Root context properties | EventBus | EventBus is untyped (QVariant), loses property binding reactivity, adds indirection. Direct Q_PROPERTY bindings are simpler and the established pattern (AAOrchestrator, BtAudioPlugin). |
| QQuickImageProvider | Base64 in Q_PROPERTY | Base64 works but is less efficient for binary data and requires `Image { source: "data:image/png;base64,..." }` which has caching issues. ImageProvider is Qt's intended mechanism. |
| Single MediaDataBridge | Separate AA + BT models | Single bridge with source switching is cleaner for QML -- one set of bindings, automatic switching. |

## Architecture Patterns

### Recommended Project Structure
```
src/
  core/
    aa/
      NavigationDataBridge.hpp/cpp   # Q_PROPERTY bridge for nav data
      MediaDataBridge.hpp/cpp        # Q_PROPERTY bridge merging AA+BT media
      ManeuverIconProvider.hpp/cpp   # QQuickImageProvider for nav PNGs
qml/
  widgets/
    NavigationWidget.qml             # New nav turn-by-turn widget
    NowPlayingWidget.qml             # Replaced unified version
```

### Pattern 1: Q_PROPERTY Data Bridge
**What:** A QObject subclass that accumulates protocol handler signals into named Q_PROPERTYs with NOTIFY signals, exposed as a root context property.
**When to use:** When protocol data needs to be reactive in QML.
**Example:**
```cpp
// NavigationDataBridge -- accumulates nav handler signals into QML-bindable properties
class NavigationDataBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool navActive READ navActive NOTIFY navActiveChanged)
    Q_PROPERTY(QString roadName READ roadName NOTIFY turnDataChanged)
    Q_PROPERTY(int maneuverType READ maneuverType NOTIFY turnDataChanged)
    Q_PROPERTY(int turnDirection READ turnDirection NOTIFY turnDataChanged)
    Q_PROPERTY(QString formattedDistance READ formattedDistance NOTIFY turnDataChanged)
    Q_PROPERTY(int distanceMeters READ distanceMeters NOTIFY turnDataChanged)
    Q_PROPERTY(bool hasManeuverIcon READ hasManeuverIcon NOTIFY turnDataChanged)

public:
    // Called from orchestrator to wire signals
    void connectToHandler(oaa::hu::NavigationChannelHandler* handler);

    // Distance formatting: "0.3 mi", "500 m", "0.5 km"
    QString formattedDistance() const;

private slots:
    void onNavigationStateChanged(bool active);
    void onNavigationTurnEvent(const QString& roadName, int maneuverType,
                                int turnDirection, const QByteArray& turnIcon,
                                int distanceMeters, int distanceUnit);
};
```

### Pattern 2: QQuickImageProvider for Protocol PNGs
**What:** Subclass `QQuickImageProvider` to serve phone-provided maneuver icon PNGs to QML `Image` elements.
**When to use:** When binary image data from protocol needs to render in QML.
**Example:**
```cpp
class ManeuverIconProvider : public QQuickImageProvider {
public:
    ManeuverIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString& id, QSize* size,
                         const QSize& requestedSize) override {
        QMutexLocker lock(&mutex_);
        QImage img;
        img.loadFromData(currentIcon_);
        if (size) *size = img.size();
        if (requestedSize.isValid())
            img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return img;
    }

    void updateIcon(const QByteArray& pngData) {
        QMutexLocker lock(&mutex_);
        currentIcon_ = pngData;
        // Emit signal to trigger QML Image reload via cache-busting
    }

private:
    QMutex mutex_;
    QByteArray currentIcon_;
};
```

**CRITICAL: QML Image cache-busting.** `Image { source: "image://navicon/current" }` caches by URL. To force reload when the icon changes, append a counter: `source: "image://navicon/current?" + navBridge.iconVersion`. The bridge increments `iconVersion` (Q_PROPERTY int) each time the icon updates.

### Pattern 3: Source-Switching Media Bridge
**What:** Single QObject that presents unified media metadata from the highest-priority available source.
**When to use:** When multiple data sources provide the same semantic data (track info, playback state).
**Example:**
```cpp
class MediaDataBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(int playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(int source READ source NOTIFY sourceChanged)  // 0=None, 1=BT, 2=AA
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY sourceChanged)

public:
    enum Source { None = 0, Bluetooth = 1, AndroidAuto = 2 };
    Q_ENUM(Source)

    void connectToAAOrchestrator(oap::aa::AndroidAutoOrchestrator* orch);
    void connectToBtAudio(oap::plugins::BtAudioPlugin* bt);

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();

private:
    // On AA connect: source_ = AA, copy AA metadata
    // On AA disconnect: source_ = BT (if connected), else None
    // Controls delegate: AA -> orchestrator->sendButtonPress(keycode), BT -> btPlugin->play/pause/next/prev
};
```

### Pattern 4: Pixel-Based Responsive Widget Layout
**What:** Use `width`/`height` breakpoints to switch between layout tiers, matching existing widget convention.
**When to use:** All widgets in this project.
**Example (NavigationWidget.qml):**
```qml
Item {
    id: navWidget
    // Breakpoints (consistent with existing widget patterns)
    readonly property bool showRoadName: width >= 400  // 3+ cells wide
    readonly property bool showNextTurn: height >= 180  // 2+ cells tall
    readonly property bool isInactive: !navBridge.navActive
    // ...
}
```

### Anti-Patterns to Avoid
- **Exposing bridges in WidgetInstanceContext:** Don't couple bridges to the grid system. Bridges are global singletons -- expose on `engine.rootContext()` like AAOrchestrator is.
- **Using EventBus for continuous data:** EventBus is fire-and-forget. QML needs persistent property bindings. EventBus misses the "current state" for widgets loaded after data was published.
- **Reusing BtAudioPlugin's context-scoped exposure:** The current NowPlayingWidget accesses `BtAudioPlugin` which is only set during plugin activation (`onActivated`). Widgets on the home screen don't have this context. MediaDataBridge solves this by living at root context level.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Binary image in QML | Base64 encoding/decoding in QML | QQuickImageProvider | Qt's built-in mechanism, handles threading, caching, scaling |
| Distance formatting | Inline QML formatting logic | C++ `formattedDistance()` in bridge | Consistent formatting, unit conversion logic, testable |
| Source switching state machine | Ad-hoc QML conditional bindings | C++ MediaDataBridge with source enum | Single source of truth, avoids binding loops, testable |

## Common Pitfalls

### Pitfall 1: QML Image Caching Prevents Icon Updates
**What goes wrong:** `Image { source: "image://navicon/current" }` loads once and caches. Subsequent maneuver icon updates from the phone don't trigger a re-request.
**Why it happens:** QML Image caches by source URL string. Same URL = no reload.
**How to avoid:** Use cache-busting counter: `source: navBridge.hasManeuverIcon ? "image://navicon/current?" + navBridge.iconVersion : ""`. Bridge increments `iconVersion` Q_PROPERTY each time icon data changes.
**Warning signs:** Nav icon stays as the first maneuver through the entire route.

### Pitfall 2: Widget QML Missing QT_QML_SKIP_CACHEGEN
**What goes wrong:** Widget QML files loaded via `Loader { source: url }` fail silently on Qt 6.8 Pi.
**Why it happens:** Qt 6.8 cachegen compiles QML to C++ -- Loader URL lookup can't find the compiled version.
**How to avoid:** Every new widget .qml file needs `set_source_files_properties(... PROPERTIES QT_QML_SKIP_CACHEGEN TRUE)` in CMakeLists.txt. This is a known gotcha (documented in CLAUDE.md).
**Warning signs:** Widget shows blank on Pi but works on dev VM.

### Pitfall 3: BtAudioPlugin Not Available in Widget Root Context
**What goes wrong:** The current NowPlayingWidget references `BtAudioPlugin` which is only available during plugin activation, not on the home screen root context.
**Why it happens:** `BtAudioPlugin` is set via `context->setContextProperty("BtAudioPlugin", this)` in `onActivated()`, which only applies to the plugin's scoped QML context, not the root context where widgets load.
**How to avoid:** MediaDataBridge is exposed at root context. It internally connects to BtAudioPlugin for BT metadata. Widget QML never references BtAudioPlugin directly.
**Warning signs:** `typeof BtAudioPlugin !== "undefined"` returns false in widget context.

### Pitfall 4: Threading -- Protocol Signals from ASIO Thread
**What goes wrong:** NavigationChannelHandler and MediaStatusChannelHandler emit signals from the ASIO protocol thread. Direct slot connections to QObject bridges that emit QML-bound signals would cause cross-thread property updates.
**Why it happens:** AA protocol runs on Boost.ASIO threads; QML properties must be updated on the Qt main thread.
**How to avoid:** The orchestrator already bridges this -- it connects to handler signals with `this` as context (running on main thread since orchestrator is a QObject). The bridge classes connect to the orchestrator's forwarded signals (or the EventBus publishes which are on the subscriber's thread). Alternatively, the bridge connects directly to handlers but uses `Qt::QueuedConnection`.
**Warning signs:** Sporadic crashes or "QML property changed from non-GUI thread" warnings.

### Pitfall 5: Stale Media State After Source Switch
**What goes wrong:** When AA disconnects, the bridge switches to BT source but shows stale AA metadata for a moment.
**Why it happens:** Source switch happens on disconnect signal, but BT metadata properties haven't been refreshed.
**How to avoid:** On source switch, immediately read current BT state from BtAudioPlugin properties (they're always live). Clear AA-specific cached metadata on disconnect.
**Warning signs:** Brief flash of AA track info after disconnect before BT info appears.

### Pitfall 6: AA Media Keycodes Might Be Ignored
**What goes wrong:** `sendButtonPress` for media keycodes (85/87/88) may be ignored by the phone.
**Why it happens:** CLAUDE.md notes: "InputEventIndication button_event (field 4): phone ignores keycodes 85/87/88." However, these are SDP-registered and may work via the proper sendButtonEvent path.
**How to avoid:** Test with real phone. If keycodes don't work, the AA media controls should be visually present but may need a "not responding" fallback. This is a known limitation documented in the codebase.
**Warning signs:** AA media play/pause/skip buttons do nothing.

## Code Examples

### Registering QQuickImageProvider
```cpp
// In main.cpp, before engine.load():
auto* maneuverIconProvider = new ManeuverIconProvider();
engine.addImageProvider("navicon", maneuverIconProvider);
// Note: engine takes ownership of the provider
```

### Widget Registration (Filling Stubs)
```cpp
// In main.cpp, update existing nav-turn stub:
navDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/NavigationWidget.qml");
// Update existing now-playing stub:
npDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/NowPlayingWidget.qml");
```

### Wiring Bridges in main.cpp
```cpp
// After aaPlugin and btAudioPlugin creation:
auto* navBridge = new NavigationDataBridge(&app);
auto* mediaBridge = new MediaDataBridge(&app);

// Wire after plugin initialization (orchestrator exists)
navBridge->connectToHandler(aaPlugin->orchestrator());  // or direct handler access
mediaBridge->connectToAAOrchestrator(aaPlugin->orchestrator());
mediaBridge->connectToBtAudio(btAudioPlugin);

// Expose as root context properties
engine.rootContext()->setContextProperty("NavigationBridge", navBridge);
engine.rootContext()->setContextProperty("MediaBridge", mediaBridge);
```

### Distance Formatting Logic
```cpp
QString NavigationDataBridge::formattedDistance() const {
    // distance_unit from phone: 1=meters, 2=km, 3=miles, 4=feet, 5=yards
    switch (distanceUnit_) {
    case 1: // meters
        if (distanceMeters_ >= 1000)
            return QString::number(distanceMeters_ / 1000.0, 'f', 1) + " km";
        return QString::number(distanceMeters_) + " m";
    case 2: // km
        return QString::number(distanceMeters_ / 1000.0, 'f', 1) + " km";
    case 3: // miles
        return QString::number(distanceMeters_ / 1609.34, 'f', 1) + " mi";
    case 4: // feet
        return QString::number(qRound(distanceMeters_ * 3.28084)) + " ft";
    case 5: // yards
        return QString::number(qRound(distanceMeters_ / 0.9144)) + " yd";
    default:
        return QString::number(distanceMeters_) + " m";
    }
}
```
**Note:** The `distanceMeters` field from the proto is always in meters regardless of `distance_unit`. The `distance_unit` tells us what the PHONE is displaying, so we format to match.

### Removing Old BT Widget Registration
```cpp
// In BtAudioPlugin::widgetDescriptors():
// Return empty list -- unified widget replaces BT-only widget
QList<oap::WidgetDescriptor> BtAudioPlugin::widgetDescriptors() const {
    return {};  // Unified NowPlayingWidget handles BT+AA -- registered in main.cpp
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| BT-only NowPlayingWidget via plugin scope | Unified MediaDataBridge at root context | Phase 06 | Single widget, multiple sources, always accessible |
| Nav events logged only (debug) | NavigationDataBridge with Q_PROPERTYs | Phase 06 | Nav data available to widgets |
| EventBus for nav (aa.nav.state etc) | Direct signal + bridge Q_PROPERTYs | Phase 06 | EventBus publishes kept for other consumers; bridge is primary QML source |

## Open Questions

1. **AA media keycodes: do they actually work?**
   - What we know: CLAUDE.md says "phone ignores keycodes 85/87/88" but they're SDP-registered
   - What's unclear: Whether the `sendButtonPress` path (proper InputEventIndication) works vs the earlier button_event attempt
   - Recommendation: Implement the controls, test on real phone. If they fail, controls are cosmetic only during AA (user controls media from phone). Not a blocker.

2. **ManeuverIconProvider threading**
   - What we know: `requestImage()` is called from Qt's image loading thread, `updateIcon()` is called from main thread
   - What's unclear: Whether QMutex is sufficient or if QReadWriteLock is better
   - Recommendation: QMutex is fine -- icon updates are infrequent (~1/sec during nav), contention is negligible

3. **Orchestrator handler access for bridge wiring**
   - What we know: `navHandler_` and `mediaStatusHandler_` are private members of AndroidAutoOrchestrator
   - What's unclear: Whether to add accessors or use EventBus topics for bridge wiring
   - Recommendation: Add public accessor methods to orchestrator (`navigationHandler()`, `mediaStatusHandler()`) OR wire bridges to the EventBus topics that orchestrator already publishes (aa.nav.*, aa.media.*). The EventBus route is simpler since it doesn't require modifying the orchestrator API, but loses the signal parameter types. Best approach: add thin accessor methods to orchestrator since the bridges are wired once at startup.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | QTest (Qt Test) |
| Config file | tests/CMakeLists.txt with `oap_add_test()` helper |
| Quick run command | `cd build && ctest -R "test_navigation\|test_media\|test_widget" --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| NAV-05 | NavigationDataBridge Q_PROPERTYs update from handler signals | unit | `ctest -R test_navigation_data_bridge -x` | Wave 0 |
| NAV-01 | formattedDistance returns correct strings for all units | unit | `ctest -R test_navigation_data_bridge -x` | Wave 0 |
| NAV-02 | ManeuverIconProvider serves PNG and signals version bump | unit | `ctest -R test_maneuver_icon_provider -x` | Wave 0 |
| NAV-03 | navActive false when handler emits navigationStateChanged(false) | unit | `ctest -R test_navigation_data_bridge -x` | Wave 0 |
| NAV-04 | Widget tap activates AA plugin | manual-only | Pi touch test | N/A |
| MEDIA-05 | MediaDataBridge updates from AA metadata signals | unit | `ctest -R test_media_data_bridge -x` | Wave 0 |
| MEDIA-02 | Source switches to AA on connect, BT on disconnect | unit | `ctest -R test_media_data_bridge -x` | Wave 0 |
| MEDIA-04 | play/pause/next/prev delegates to correct backend | unit | `ctest -R test_media_data_bridge -x` | Wave 0 |
| MEDIA-03 | source property reflects current source | unit | `ctest -R test_media_data_bridge -x` | Wave 0 |
| MEDIA-01 | Unified widget shows title/artist/controls | manual-only | Pi visual test | N/A |
| MEDIA-06 | BtAudioPlugin no longer registers NowPlaying widget | unit | `ctest -R test_widget_plugin_integration -x` | Existing (update) |

### Sampling Rate
- **Per task commit:** `cd build && ctest -R "test_navigation_data_bridge\|test_media_data_bridge\|test_maneuver_icon" --output-on-failure`
- **Per wave merge:** `cd build && ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_navigation_data_bridge.cpp` -- covers NAV-01, NAV-03, NAV-05 (distance formatting, state changes, property updates)
- [ ] `tests/test_media_data_bridge.cpp` -- covers MEDIA-02, MEDIA-03, MEDIA-04, MEDIA-05 (source switching, control delegation, metadata)
- [ ] `tests/test_maneuver_icon_provider.cpp` -- covers NAV-02 (PNG serving, cache busting)

## Sources

### Primary (HIGH confidence)
- Codebase analysis: `src/core/aa/AndroidAutoOrchestrator.hpp/cpp` -- handler signals, EventBus wiring, sendButtonPress
- Codebase analysis: `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/NavigationChannelHandler.hpp` -- exact signal signatures
- Codebase analysis: `libs/prodigy-oaa-protocol/include/oaa/HU/Handlers/MediaStatusChannelHandler.hpp` -- exact signal signatures
- Codebase analysis: `src/plugins/bt_audio/BtAudioPlugin.hpp` -- Q_PROPERTY pattern, AVRCP control methods
- Codebase analysis: `src/main.cpp` -- root context property exposure pattern, widget descriptor registration
- Codebase analysis: `qml/widgets/NowPlayingWidget.qml` -- existing widget pattern, breakpoint convention
- Codebase analysis: `qml/widgets/AAStatusWidget.qml` -- widget tap action pattern
- Codebase analysis: `qml/applications/home/HomeMenu.qml` -- widget grid rendering, Loader pattern

### Secondary (MEDIUM confidence)
- Qt documentation: QQuickImageProvider pattern (well-established Qt API, stable since Qt 5)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all components are existing Qt patterns already in use in the codebase
- Architecture: HIGH -- follows established patterns (Q_PROPERTY bridges, root context, pixel breakpoints)
- Pitfalls: HIGH -- identified from actual codebase analysis and documented gotchas in CLAUDE.md
- Distance formatting: MEDIUM -- proto field semantics inferred from handler signal names and test data; need to verify `distanceMeters` is always meters regardless of `distance_unit`

**Research date:** 2026-03-12
**Valid until:** 2026-04-12 (stable -- internal codebase, no external dependency changes expected)

# Overlay Framework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generalize the hardcoded Shell overlay stack into `OverlayService` (registry + list model, fixed z-bands, action-driven visibility) + `OverlayHost.qml`, fix the current z-order inversions, and migrate `PairingDialog` as the proof.

**Architecture:** `OverlayService` is a `QAbstractListModel` registry of `OverlayDescriptor`s; registering an overlay auto-registers `overlay.<id>.show/hide/toggle/move` actions (rail R4 — QML, keys, and the External API all mutate through one path). `OverlayHost.qml` is a Repeater of lazy Loaders inside Shell. Spec: `docs/superpowers/specs/2026-07-05-dashboards-overlays-design.md` §4 + §6 — read both before coding.

**Tech Stack:** Qt 6.8, CMake/ctest.

## Global Constraints

- **Z values come from band constants only** (spec §4.1): Notifications=1000, User=2000, SystemModal=3000, dim fixture=3500, Gesture=4000. Within a band: registration order. If a component needs "one higher", stop — the design is wrong.
- All overlay mutation via ActionRegistry actions or `OverlayService` invokables (rail R4).
- ActionRegistry handlers are synchronous main-thread — overlay action handlers must be a bare `setVisible`/`move` call.
- The `overlay.` action prefix is reserved against External-API client registration (API design §9.1) — nothing to do, just don't rename the prefix.
- Q_OBJECT headers need their .cpp in `src/CMakeLists.txt` (MOC).
- Build: `cd build && cmake .. && make -j$(nproc)`; test: `ctest --output-on-failure`; final gate adds `./cross-build.sh`.

---

### Task 1: `OverlayService`

**Files:**
- Create: `src/core/services/OverlayService.hpp`, `src/core/services/OverlayService.cpp`
- Modify: `src/CMakeLists.txt` (SOURCES)
- Test: `tests/test_overlay_service.cpp`; Modify: `tests/CMakeLists.txt` (`oap_add_test(test_overlay_service SOURCES test_overlay_service.cpp)`)

**Interfaces:**
- Consumes: `oap::ActionRegistry` (`registerAction(id, Handler)`, `unregisterAction(id)`, `dispatch(id, payload)` — `ActionRegistry.hpp:22-25`).
- Produces (Tasks 2, 3 rely on exact names):
```cpp
namespace oap {

class OverlayService : public QAbstractListModel {
    Q_OBJECT
public:
    enum class ZBand { Notifications = 1000, User = 2000, SystemModal = 3000, Gesture = 4000 };
    Q_ENUM(ZBand)

    struct OverlayDescriptor {
        QString id;              // "pairing"; reverse-dns for plugin overlays
        QString sourcePluginId;  // "" for system overlays
        QUrl qmlComponent;
        ZBand band = ZBand::User;
        bool visible = false;
        QVariantMap geometry;    // optional {x,y,width,height}; empty = component self-anchors
    };

    enum Roles { OverlayIdRole = Qt::UserRole + 1, QmlComponentRole, ZRole,
                 VisibleRole, GeometryRole };

    explicit OverlayService(ActionRegistry* actions, QObject* parent = nullptr);

    // QAbstractListModel
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool registerOverlay(const OverlayDescriptor& descriptor);  // false on duplicate id
    void unregisterOverlay(const QString& id);
    Q_INVOKABLE void setVisible(const QString& id, bool visible);
    Q_INVOKABLE void toggle(const QString& id);
    Q_INVOKABLE void move(const QString& id, const QVariantMap& geometry);
    Q_INVOKABLE bool isVisible(const QString& id) const;

signals:
    void overlayVisibilityChanged(const QString& id, bool visible);
};

} // namespace oap
```

- [ ] **Step 1: Failing test** — `tests/test_overlay_service.cpp`:
```cpp
#include <QtTest/QtTest>
#include "core/services/OverlayService.hpp"
#include "core/services/ActionRegistry.hpp"

using OS = oap::OverlayService;

static OS::OverlayDescriptor desc(const QString& id, OS::ZBand band) {
    OS::OverlayDescriptor d;
    d.id = id;
    d.qmlComponent = QUrl("qrc:/x/" + id + ".qml");
    d.band = band;
    return d;
}

class TestOverlayService : public QObject {
    Q_OBJECT
private slots:
    void testRegisterAndRoles();
    void testDuplicateRejected();
    void testBandZAssignment();
    void testVisibilityAndSignal();
    void testActionsAutoRegistered();
    void testUnregisterRemovesActions();
    void testMoveGeometry();
};

void TestOverlayService::testRegisterAndRoles() {
    oap::ActionRegistry ar; OS svc(&ar);
    QVERIFY(svc.registerOverlay(desc("pairing", OS::ZBand::SystemModal)));
    QCOMPARE(svc.rowCount(), 1);
    auto idx = svc.index(0, 0);
    QCOMPARE(svc.data(idx, OS::OverlayIdRole).toString(), QString("pairing"));
    QCOMPARE(svc.data(idx, OS::VisibleRole).toBool(), false);
    QCOMPARE(svc.data(idx, OS::ZRole).toInt(), 3000);
}

void TestOverlayService::testDuplicateRejected() {
    oap::ActionRegistry ar; OS svc(&ar);
    QVERIFY(svc.registerOverlay(desc("a", OS::ZBand::User)));
    QVERIFY(!svc.registerOverlay(desc("a", OS::ZBand::User)));
    QCOMPARE(svc.rowCount(), 1);
}

void TestOverlayService::testBandZAssignment() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("u1", OS::ZBand::User));       // 2000
    svc.registerOverlay(desc("u2", OS::ZBand::User));       // 2001 (registration order)
    svc.registerOverlay(desc("n1", OS::ZBand::Notifications)); // 1000
    QCOMPARE(svc.data(svc.index(0,0), OS::ZRole).toInt(), 2000);
    QCOMPARE(svc.data(svc.index(1,0), OS::ZRole).toInt(), 2001);
    QCOMPARE(svc.data(svc.index(2,0), OS::ZRole).toInt(), 1000);
}

void TestOverlayService::testVisibilityAndSignal() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("a", OS::ZBand::User));
    QSignalSpy spy(&svc, &OS::overlayVisibilityChanged);
    QSignalSpy dataSpy(&svc, &OS::dataChanged);
    svc.setVisible("a", true);
    QVERIFY(svc.isVisible("a"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(dataSpy.count() >= 1);
    svc.setVisible("a", true);          // no-op — no re-emit
    QCOMPARE(spy.count(), 1);
    svc.toggle("a");
    QVERIFY(!svc.isVisible("a"));
    svc.setVisible("nope", true);       // unknown id: logged, no crash
}

void TestOverlayService::testActionsAutoRegistered() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("pairing", OS::ZBand::SystemModal));
    QVERIFY(ar.registeredActions().contains("overlay.pairing.show"));
    QVERIFY(ar.registeredActions().contains("overlay.pairing.hide"));
    QVERIFY(ar.registeredActions().contains("overlay.pairing.toggle"));
    QVERIFY(ar.registeredActions().contains("overlay.pairing.move"));
    QVERIFY(ar.dispatch("overlay.pairing.show"));
    QVERIFY(svc.isVisible("pairing"));
    QVERIFY(ar.dispatch("overlay.pairing.hide"));
    QVERIFY(!svc.isVisible("pairing"));
    QVERIFY(ar.dispatch("overlay.pairing.toggle"));
    QVERIFY(svc.isVisible("pairing"));
}

void TestOverlayService::testUnregisterRemovesActions() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("a", OS::ZBand::User));
    svc.unregisterOverlay("a");
    QCOMPARE(svc.rowCount(), 0);
    QVERIFY(!ar.registeredActions().contains("overlay.a.show"));
    QVERIFY(!ar.dispatch("overlay.a.show"));
}

void TestOverlayService::testMoveGeometry() {
    oap::ActionRegistry ar; OS svc(&ar);
    svc.registerOverlay(desc("a", OS::ZBand::User));
    QVariantMap g{{"x", 10}, {"y", 20}, {"width", 300}, {"height", 200}};
    QVERIFY(ar.dispatch("overlay.a.move", g));
    QCOMPARE(svc.data(svc.index(0,0), OS::GeometryRole).toMap().value("x").toInt(), 10);
}

QTEST_MAIN(TestOverlayService)
#include "test_overlay_service.moc"
```

- [ ] **Step 2: Verify failure. Step 3: Implement `OverlayService.cpp`:**
```cpp
#include "OverlayService.hpp"
#include "ActionRegistry.hpp"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcOverlay, "oap.overlay")

namespace oap {

OverlayService::OverlayService(ActionRegistry* actions, QObject* parent)
    : QAbstractListModel(parent), actions_(actions)
{
}

int OverlayService::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : overlays_.size();
}

QVariant OverlayService::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= overlays_.size()) return {};
    const auto& e = overlays_[index.row()];
    switch (role) {
    case OverlayIdRole: return e.desc.id;
    case QmlComponentRole: return e.desc.qmlComponent;
    case ZRole: return e.z;
    case VisibleRole: return e.desc.visible;
    case GeometryRole: return e.desc.geometry;
    }
    return {};
}

QHash<int, QByteArray> OverlayService::roleNames() const
{
    return { {OverlayIdRole, "overlayId"}, {QmlComponentRole, "qmlComponent"},
             {ZRole, "z"}, {VisibleRole, "overlayVisible"}, {GeometryRole, "geometry"} };
}

bool OverlayService::registerOverlay(const OverlayDescriptor& descriptor)
{
    if (descriptor.id.isEmpty() || findOverlay(descriptor.id) >= 0) {
        qCWarning(lcOverlay) << "registerOverlay rejected:" << descriptor.id;
        return false;
    }
    Entry e;
    e.desc = descriptor;
    // Fixed band base + intra-band registration order (design §4.1)
    int intra = 0;
    for (const auto& other : overlays_)
        if (other.desc.band == descriptor.band) ++intra;
    e.z = static_cast<int>(descriptor.band) + intra;

    beginInsertRows({}, overlays_.size(), overlays_.size());
    overlays_.append(e);
    endInsertRows();

    if (actions_) {
        const QString base = QStringLiteral("overlay.%1.").arg(descriptor.id);
        const QString id = descriptor.id;
        actions_->registerAction(base + "show",   [this, id](const QVariant&) { setVisible(id, true); });
        actions_->registerAction(base + "hide",   [this, id](const QVariant&) { setVisible(id, false); });
        actions_->registerAction(base + "toggle", [this, id](const QVariant&) { toggle(id); });
        actions_->registerAction(base + "move",   [this, id](const QVariant& v) { move(id, v.toMap()); });
    }
    qCInfo(lcOverlay) << "Overlay registered:" << descriptor.id
                      << "band" << static_cast<int>(descriptor.band) << "z" << e.z;
    return true;
}

void OverlayService::unregisterOverlay(const QString& id)
{
    const int i = findOverlay(id);
    if (i < 0) return;
    if (actions_) {
        const QString base = QStringLiteral("overlay.%1.").arg(id);
        for (const char* suffix : {"show", "hide", "toggle", "move"})
            actions_->unregisterAction(base + suffix);
    }
    beginRemoveRows({}, i, i);
    overlays_.removeAt(i);
    endRemoveRows();
}

void OverlayService::setVisible(const QString& id, bool visible)
{
    const int i = findOverlay(id);
    if (i < 0) { qCWarning(lcOverlay) << "setVisible: unknown overlay" << id; return; }
    if (overlays_[i].desc.visible == visible) return;
    overlays_[i].desc.visible = visible;
    const auto idx = index(i, 0);
    emit dataChanged(idx, idx, {VisibleRole});
    emit overlayVisibilityChanged(id, visible);
}

void OverlayService::toggle(const QString& id)
{
    const int i = findOverlay(id);
    if (i >= 0) setVisible(id, !overlays_[i].desc.visible);
}

void OverlayService::move(const QString& id, const QVariantMap& geometry)
{
    const int i = findOverlay(id);
    if (i < 0) return;
    overlays_[i].desc.geometry = geometry;
    const auto idx = index(i, 0);
    emit dataChanged(idx, idx, {GeometryRole});
}

bool OverlayService::isVisible(const QString& id) const
{
    const int i = findOverlay(id);
    return i >= 0 && overlays_[i].desc.visible;
}

int OverlayService::findOverlay(const QString& id) const
{
    for (int i = 0; i < overlays_.size(); ++i)
        if (overlays_[i].desc.id == id) return i;
    return -1;
}

} // namespace oap
```
Header additions beyond the Interfaces block: `private: struct Entry { OverlayDescriptor desc; int z = 0; }; int findOverlay(const QString& id) const; ActionRegistry* actions_; QList<Entry> overlays_;`.

- [ ] **Step 4: Run** `ctest -R test_overlay_service --output-on-failure` → PASS (7 tests). Full suite → PASS.
- [ ] **Step 5: Commit** — `git add src/core/services/OverlayService.* src/CMakeLists.txt tests/test_overlay_service.cpp tests/CMakeLists.txt && git commit -m "feat(ui): OverlayService — z-banded overlay registry with action-driven visibility"`

---

### Task 2: `OverlayHost.qml` + Shell z re-pin

**Files:**
- Create: `qml/components/OverlayHost.qml` (register alongside the other components — same qrc/CMake list as `NotificationArea.qml`)
- Modify: `qml/components/Shell.qml`, `qml/components/NotificationArea.qml:14`, `qml/components/GestureOverlay.qml:14`, `qml/components/PairingDialog.qml:11`, `qml/applications/phone/IncomingCallOverlay.qml:12`
- Modify: `src/main.cpp` (construct service + context property)

- [ ] **Step 1: Re-pin every legacy z to its band constant** (design §4.1 — this kills the inversion where IncomingCall(1000) sat above Gesture(999)):
| File | Old | New |
|---|---|---|
| `NotificationArea.qml:14` | `z: 998` | `z: 1000  // Notifications band` |
| `PairingDialog.qml:11` | `z: 998` | `z: 3000  // SystemModal band` |
| `IncomingCallOverlay.qml:12` | `z: 1000  // above everything` | `z: 3000  // SystemModal band` |
| Shell.qml dim Rectangle (`Shell.qml:83`) | `z: 998` | `z: 3500  // dim fixture: above modals, below gesture (design §4.1)` |
| `GestureOverlay.qml:14` | `z: 999` | `z: 4000  // Gesture band — beats everything interactive` |

- [ ] **Step 2: Create `qml/components/OverlayHost.qml`:**
```qml
import QtQuick

/// Hosts framework-registered overlays (design 2026-07-05 §4.3).
/// Stacking/visibility live in OverlayService; content data-binding stays in
/// each overlay component. Loaders are lazy: hidden overlays hold no memory.
///
/// The component ROOT is the Repeater on purpose: a Repeater inserts its
/// delegates as children of ITS OWN parent (the Shell root), so delegate z
/// competes in the same stacking context as the legacy overlays. Wrapping
/// this in an Item would trap all framework overlays at the wrapper's z=0
/// and every legacy overlay (1000-4000) would beat them.
Repeater {
    model: OverlayService
    delegate: Loader {
        active: model.overlayVisible
        source: model.qmlComponent
        z: model.z
        // Self-anchoring (fills Shell) unless the descriptor carries geometry
        anchors.fill: (model.geometry && model.geometry.width !== undefined) ? undefined : parent
        x: model.geometry && model.geometry.x !== undefined ? model.geometry.x : 0
        y: model.geometry && model.geometry.y !== undefined ? model.geometry.y : 0
        width: model.geometry && model.geometry.width !== undefined ? model.geometry.width : undefined
        height: model.geometry && model.geometry.height !== undefined ? model.geometry.height : undefined
    }
}
```

- [ ] **Step 3: Shell + main.cpp.** In `Shell.qml`, after the dim Rectangle block (`Shell.qml:85`):
```qml
    // Framework-registered overlays (OverlayService)
    OverlayHost {
    }
```
In `src/main.cpp`: construct after `actionRegistry` exists (find `new oap::ActionRegistry` / where `ActionRegistry` context property is set, `main.cpp:890` region):
```cpp
    auto overlayService = new oap::OverlayService(actionRegistry, &app);
```
and beside the other context properties: `engine.rootContext()->setContextProperty("OverlayService", overlayService);` (include `"core/services/OverlayService.hpp"`).

- [ ] **Step 4: Build + full suite** (no behavior change yet — the model is empty). If a desktop session is available, boot and confirm toasts/gesture/pairing still render at sane stacking.
- [ ] **Step 5: Commit** — `git commit -am "feat(ui): OverlayHost + z-band re-pin — fixes incoming-call-above-gesture inversion"`

---

### Task 3: Migrate `PairingDialog` (proof of framework) + plugin access

**Files:**
- Modify: `qml/components/PairingDialog.qml` (drop self-visibility + z)
- Modify: `qml/components/Shell.qml` (remove hardcoded instance)
- Modify: `src/main.cpp` (register overlay + wire visibility)
- Modify: `src/core/plugin/IHostContext.hpp`, `src/core/plugin/HostContext.hpp` (accessor)

- [ ] **Step 1: PairingDialog gives up self-stacking.** In `PairingDialog.qml`, delete lines `10-11`:
```qml
    visible: BluetoothManager ? BluetoothManager.pairingActive : false
    z: 3000  // SystemModal band
```
(both — the framework Loader now controls visibility via `active`, and z comes from the model). The component's content bindings to `BluetoothManager` remain untouched.
- [ ] **Step 2: Remove from Shell.** Delete the `PairingDialog { id: pairingDialog }` block (`Shell.qml:93-96`). Verify nothing references `pairingDialog` by id: `grep -rn "pairingDialog" qml/` → must be zero after removal.
- [ ] **Step 3: Register + wire in main.cpp** (after `overlayService` construction; `bluetoothManager` exists earlier):
```cpp
    {
        oap::OverlayService::OverlayDescriptor d;
        d.id = QStringLiteral("pairing");
        d.qmlComponent = QUrl(QStringLiteral("qrc:/OpenAutoProdigy/PairingDialog.qml"));
        d.band = oap::OverlayService::ZBand::SystemModal;
        overlayService->registerOverlay(d);
        // Visibility rides the action path (rail R4): state source stays authoritative.
        auto syncPairing = [actionRegistry, bluetoothManager]() {
            actionRegistry->dispatch(bluetoothManager->pairingActive()
                ? QStringLiteral("overlay.pairing.show")
                : QStringLiteral("overlay.pairing.hide"));
        };
        QObject::connect(bluetoothManager, &oap::BluetoothManager::pairingActiveChanged,
                         overlayService, syncPairing);
        syncPairing();
    }
```
Verify the qrc URL: `grep -rn "PairingDialog" src/CMakeLists.txt qml/CMakeLists.txt *.qrc 2>/dev/null` and match how `Shell.qml`'s sibling components resolve (same URI prefix as `PhoneView.qml`'s `qrc:/OpenAutoProdigy/`). Verify the property spelling: `grep -n "pairingActive" src/core/services/BluetoothManager.hpp`.
- [ ] **Step 4: `IHostContext` accessor** (plugin overlays, design §4.4). In `IHostContext.hpp` beside `callStateProvider()`: `virtual OverlayService* overlayService() = 0;` (forward-declare `class OverlayService;`). In `HostContext.hpp`: member + `void setOverlayService(OverlayService* s)` + override returning it (pattern: the existing `setCallStateProvider`/`callStateProvider` pair at `HostContext.hpp:22,37`). In main.cpp: `hostContext->setOverlayService(overlayService);` — **before** `pluginManager` initializes plugins. Check for other IHostContext implementors that must gain the override: `grep -rln ": public IHostContext" src/ tests/` — test fakes return `nullptr`.
- [ ] **Step 5: Build + full suite.** On-device or desktop: trigger BT pairing (or temporarily `ActionRegistry.dispatch("overlay.pairing.toggle")` from a QML console) → dialog appears above content and below the gesture overlay, absorbs touch, disappears on cancel.
- [ ] **Step 6: Commit** — `git commit -am "refactor(ui): PairingDialog rides the overlay framework; IHostContext exposes OverlayService"`

---

### Task 4: Verification sweep + handoff

- [ ] Full gate: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`, then `./cross-build.sh`; deploy to Pi per CLAUDE.md workflow.
- [ ] On-device: pairing flow end-to-end (first-run pairable mode → dialog via framework); toast + gesture overlay + incoming-call stacking sanity (call overlay now BELOW gesture — the D2-fixed `=== 1` visibility applies if D2 landed).
- [ ] Migration table check (design §4.5): confirm IncomingCallOverlay/NotificationArea/GestureOverlay remain Shell-hosted at band constants (later migrations), dim at 3500.
- [ ] Append results + deviations to `docs/session-handoffs.md`; commit.

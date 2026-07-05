# HFP Call Audio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Real Bluetooth hands-free calling — dial/answer/hangup/DTMF against PipeWire's Telephony D-Bus API, a truthful call state machine behind `IPhoneStateService`, and AA-coexistence policy — replacing the UI mocks.

**Architecture:** `TelephonyClient` (session-bus D-Bus adapter to `org.pipewire.Telephony`, mechanics only) + `ScoNodeMonitor` (PipeWire registry watch for SCO node state) feed a call state machine inside the existing `PhoneStateService`. `CallAudioPolicy` toggles `RejectSCO` from projection state. `PhonePlugin` becomes a pure view. SCO audio itself is routed by WirePlumber automatically — no prodigy audio-path code. Spec: `docs/superpowers/specs/2026-07-05-hfp-call-audio-design.md` (READ IT FIRST — esp. §5 state machine table and §12 Executor Guidance).

**Tech Stack:** Qt 6.8 (QtDBus, QtTest), PipeWire 1.4.2 client API, CMake/ctest, cross-build.sh for Pi.

## Global Constraints

- **Never touch `libs/prodigy-oaa-protocol/`** (community submodule) or edit `proto/api/*.proto` semantics (frozen, additive-only).
- **Never register BT profile `0x111f` or `0x1108`** — the Pi is the HFP HF role; the phone is the AG. This plan *deletes* profile registration; if you find yourself adding one back, stop (design §12.1).
- **`ICallStateProvider` enum values `Idle=0, Ringing=1, Active=2` are frozen** — QML compares raw ints. New states append only.
- **`can_hold_swap`/`can_multiparty` stay false everywhere** — no code path sets them (frozen proto contract).
- **All Qt D-Bus handlers run on the main thread — keep them light**; every D-Bus method call is async (`QDBusPendingCallWatcher`) except startup `GetManagedObjects` (short timeout, existing style).
- **`QDBusArgument >>` cannot extract `QVariantMap` directly** — manual `beginMap()/endMap()` with `QDBusVariant` (pattern: `src/core/services/PhoneStateService.cpp:173-199`).
- **PipeWire callbacks run on the PW thread loop** — marshal to Qt with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`; registry/proxy setup and teardown under `pw_thread_loop_lock`.
- **Do not install ofono; do not enable `provide-ofono`; no WirePlumber config drop-ins** (telephony is on by default in Trixie).
- Constructors never touch a bus; every `start()` guards `isConnected()` (CI may have no session bus).
- Build: `cd build && cmake .. && make -j$(nproc)`. Test: `ctest --output-on-failure` (from `build/`). Final gate additionally: `./cross-build.sh` from repo root.
- Commit after every task; messages in the existing `feat:`/`refactor:`/`fix:` style.

---

### Task 1: Delete the dead HFP AG/HSP HS profile registration

Fails every boot ("UUID already registered" — PipeWire owns those UUIDs), feeds nothing functional, and is a latent boot-order race (design §8.1). Auto-connect cancellation is already handled by `updateConnectedDevice()` at `BluetoothManager.cpp:736-740` — verified redundant.

**Files:**
- Modify: `src/core/services/BluetoothManager.cpp`
- Modify: `src/core/services/BluetoothManager.hpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: nothing — pure deletion. No other file references the deleted symbols (verify in Step 1).

- [ ] **Step 1: Verify the deletion surface is closed**

Run: `grep -rn "profileNewConnection\|registerProfiles\|unregisterProfiles\|BluezProfile1Handler\|profileFds_\|profileObjects_\|registeredProfilePaths_" src/ tests/ qml/`
Expected: hits ONLY inside `BluetoothManager.cpp` and `BluetoothManager.hpp`. If anything else references these symbols, STOP and re-check the design doc §8.1 — the substrate moved.

- [ ] **Step 2: Delete from `BluetoothManager.cpp`**

Delete all of:
1. The `BluezProfile1Handler` class (comment + class, around `:52-79` — starts at `// D-Bus adaptor implementing org.bluez.Profile1 — holds NewConnection fds`).
2. `registerProfiles()` and `unregisterProfiles()` method bodies (around `:540-603`).
3. The call `registerProfiles();` in `initialize()` (around `:352`).
4. The call `registerProfiles();` inside the BlueZ-restart lambda (around `:375`).
5. The connect block (around `:378-379`):
```cpp
    // Cancel auto-connect when RFCOMM connection arrives
    connect(this, &BluetoothManager::profileNewConnection,
            this, &BluetoothManager::cancelAutoConnect);
```
6. `unregisterProfiles();` call in the shutdown path (around `:798`).
7. The stale comment at `:282`: replace
```cpp
            // Don't cancel yet — wait for profileNewConnection (RFCOMM) as the true success signal
```
with
```cpp
            // Cancellation happens in updateConnectedDevice() when Device1.Connected flips
```
8. If `BluezProfile1Handler`'s includes (`<unistd.h>` for `::close`, `QDBusUnixFileDescriptor`) are now unused, remove them — check with grep before removing each.

- [ ] **Step 3: Delete from `BluetoothManager.hpp`**

Delete:
- `void profileNewConnection();  // RFCOMM NewConnection — auto-connect stop signal` (signal, `:66`)
- `void registerProfiles();` and `void unregisterProfiles();` (`:81-82`)
- `QStringList registeredProfilePaths_;`, `std::vector<std::unique_ptr<QObject>> profileObjects_;`, `std::vector<int> profileFds_;` (`:126-128`)
- The `friend class BluezProfile1Handler;` declaration if one exists (grep for it).

- [ ] **Step 4: Build and run the full suite**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: clean build, all 88 tests PASS. Any compile error means a missed reference — grep the symbol and delete the straggler.

- [ ] **Step 5: Commit**

```bash
git add src/core/services/BluetoothManager.cpp src/core/services/BluetoothManager.hpp
git commit -m "refactor(bt): delete dead HFP AG/HSP HS profile registration

Fails every boot (PipeWire owns 0x111f/0x1108), profileFds_ never read,
and the profileNewConnection auto-connect stop was redundant with
updateConnectedDevice(). Removes the latent boot-order registration race.
Design: docs/superpowers/specs/2026-07-05-hfp-call-audio-design.md §8.1"
```

---

### Task 2: `TelephonyClient` — D-Bus adapter for org.pipewire.Telephony

**Files:**
- Create: `src/core/services/TelephonyClient.hpp`
- Create: `src/core/services/TelephonyClient.cpp`
- Modify: `src/CMakeLists.txt` (add `core/services/TelephonyClient.cpp` to the SOURCES list next to `core/services/PhoneStateService.cpp`, line ~57)
- Test: `tests/test_telephony_client.cpp`
- Modify: `tests/CMakeLists.txt` (add `oap_add_test(test_telephony_client SOURCES test_telephony_client.cpp)` next to the `test_phone_state_service` line)

**Interfaces:**
- Consumes: session bus only.
- Produces (used by Tasks 4, 5, 6):
```cpp
namespace oap {
class TelephonyClient : public QObject {
    void start(); void stop();
    bool available() const;            // service up AND AudioGateway1 present
    QString agAddress() const; QString transportState() const; QString codec() const;
    void dial(const QString& number); void answer(); void hangupAll();
    void sendTones(const QString& tones);
    void setRejectSco(bool reject);    // cached; re-applied on transport appearance
signals:
    void availableChanged(bool);
    void transportStateChanged(const QString&); void codecChanged(const QString&);
    void callSetupStarted(const QString& state, const QString& line, const QString& name);
    void callSetupChanged(const QString& state); void callSetupEnded();
    void commandFailed(const QString& op, const QString& message);
};
}
```

- [ ] **Step 1: Write the failing test**

`tests/test_telephony_client.cpp`:
```cpp
// Bus-independent safety tests. Protocol conformance is live-check territory
// (design doc §11) — do NOT fake a session bus here.
#include <QtTest/QtTest>
#include "core/services/TelephonyClient.hpp"

class TestTelephonyClient : public QObject {
    Q_OBJECT
private slots:
    void testConstructIsInert();
    void testStartStopSafeWithoutService();
    void testCommandsSafeWhenUnavailable();
    void testRejectScoCachedWhenUnavailable();
};

void TestTelephonyClient::testConstructIsInert() {
    oap::TelephonyClient c;               // must not touch any bus
    QVERIFY(!c.available());
    QVERIFY(c.agAddress().isEmpty());
    QVERIFY(c.transportState().isEmpty());
    QVERIFY(c.codec().isEmpty());
}

void TestTelephonyClient::testStartStopSafeWithoutService() {
    oap::TelephonyClient c;
    c.start();                            // session bus may or may not exist in CI
    c.start();                            // idempotent
    QVERIFY(!c.available());              // org.pipewire.Telephony not running here
    c.stop();
    c.stop();
    QVERIFY(!c.available());
}

void TestTelephonyClient::testCommandsSafeWhenUnavailable() {
    oap::TelephonyClient c;
    QSignalSpy failSpy(&c, &oap::TelephonyClient::commandFailed);
    c.dial("5551234");
    c.answer();
    c.hangupAll();
    c.sendTones("1");
    QCOMPARE(failSpy.count(), 4);         // each op fails fast, no crash
}

void TestTelephonyClient::testRejectScoCachedWhenUnavailable() {
    oap::TelephonyClient c;
    c.setRejectSco(true);                 // no transport — must only cache
    QVERIFY(!c.available());
}

QTEST_MAIN(TestTelephonyClient)
#include "test_telephony_client.moc"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake .. && make -j$(nproc) 2>&1 | tail -5`
Expected: FAIL to compile — `TelephonyClient.hpp` not found.

- [ ] **Step 3: Write `TelephonyClient.hpp`**

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QDBusObjectPath>
#include <QDBusMessage>

class QDBusServiceWatcher;

namespace oap {

/// D-Bus client for PipeWire's telephony service (org.pipewire.Telephony,
/// SESSION bus, owned by WirePlumber). Mechanics only — call-state policy
/// lives in PhoneStateService.
///
/// Role reminder: the Pi is the HFP Hands-Free (HF) unit; the phone is the
/// Audio Gateway. The AudioGateway1 object represents the REMOTE PHONE.
///
/// Call1 objects are EPHEMERAL (setup phase only) and are never returned by
/// GetManagedObjects — they are tracked exclusively via InterfacesAdded/
/// InterfacesRemoved (live-verified, HFP decision record §6.4).
class TelephonyClient : public QObject {
    Q_OBJECT
public:
    explicit TelephonyClient(QObject* parent = nullptr);
    ~TelephonyClient() override;

    void start();
    void stop();

    bool available() const { return serviceUp_ && !agPath_.isEmpty(); }
    QString agAddress() const { return agAddress_; }
    QString transportState() const { return transportState_; }
    QString codec() const { return codec_; }

    void dial(const QString& number);
    void answer();
    void hangupAll();
    void sendTones(const QString& tones);
    void setRejectSco(bool reject);

signals:
    void availableChanged(bool available);
    void transportStateChanged(const QString& state);
    void codecChanged(const QString& codec);
    void callSetupStarted(const QString& state, const QString& line, const QString& name);
    void callSetupChanged(const QString& state);
    void callSetupEnded();
    void commandFailed(const QString& op, const QString& message);

private slots:
    void onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                             const QStringList& invalidated, const QDBusMessage& msg);

private:
    void onServiceUp();
    void onServiceDown();
    void scanExistingObjects();
    void adoptAg(const QString& path, const QVariantMap& props);
    void adoptTransport(const QString& path, const QVariantMap& props);
    void adoptCall(const QString& path, const QVariantMap& props);
    void applyRejectSco();
    void asyncCall(const QString& op, QDBusMessage msg);
    /// Extract a{sv} that arrived as a QDBusArgument inside a QVariant.
    static QVariantMap demarshalProps(const QVariant& v);

    QDBusServiceWatcher* watcher_ = nullptr;
    bool started_ = false;
    bool serviceUp_ = false;
    bool rejectSco_ = false;

    QString agPath_;
    QString agAddress_;
    QString transportPath_;
    QString transportState_;
    QString codec_;
    QString callPath_;
};

} // namespace oap
```

- [ ] **Step 4: Write `TelephonyClient.cpp`**

```cpp
#include "TelephonyClient.hpp"
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcTel, "oap.telephony")

namespace {
const QString kService = QStringLiteral("org.pipewire.Telephony");
const QString kRoot = QStringLiteral("/org/pipewire/Telephony");
const QString kAgIface = QStringLiteral("org.pipewire.Telephony.AudioGateway1");
const QString kTransportIface = QStringLiteral("org.pipewire.Telephony.AudioGatewayTransport1");
const QString kCallIface = QStringLiteral("org.pipewire.Telephony.Call1");
const QString kPropsIface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kObjMgrIface = QStringLiteral("org.freedesktop.DBus.ObjectManager");
} // namespace

namespace oap {

TelephonyClient::TelephonyClient(QObject* parent) : QObject(parent) {}

TelephonyClient::~TelephonyClient() { stop(); }

void TelephonyClient::start()
{
    if (started_) return;
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCWarning(lcTel) << "No session bus — telephony disabled";
        return;
    }

    watcher_ = new QDBusServiceWatcher(kService, bus,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
        this);
    connect(watcher_, &QDBusServiceWatcher::serviceRegistered, this, [this]() { onServiceUp(); });
    connect(watcher_, &QDBusServiceWatcher::serviceUnregistered, this, [this]() { onServiceDown(); });

    bus.connect(kService, kRoot, kObjMgrIface, QStringLiteral("InterfacesAdded"),
        this, SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap)));
    bus.connect(kService, kRoot, kObjMgrIface, QStringLiteral("InterfacesRemoved"),
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    // Empty path = all objects of the service; QDBusMessage tail arg gives the sender path.
    bus.connect(kService, QString(), kPropsIface, QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));

    started_ = true;

    if (bus.interface() && bus.interface()->isServiceRegistered(kService))
        onServiceUp();
}

void TelephonyClient::stop()
{
    if (!started_) return;
    auto bus = QDBusConnection::sessionBus();
    bus.disconnect(kService, kRoot, kObjMgrIface, QStringLiteral("InterfacesAdded"),
        this, SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap)));
    bus.disconnect(kService, kRoot, kObjMgrIface, QStringLiteral("InterfacesRemoved"),
        this, SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    bus.disconnect(kService, QString(), kPropsIface, QStringLiteral("PropertiesChanged"),
        this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));
    delete watcher_;
    watcher_ = nullptr;
    started_ = false;
    onServiceDown();
}

void TelephonyClient::onServiceUp()
{
    if (serviceUp_) return;
    serviceUp_ = true;
    qCInfo(lcTel) << "org.pipewire.Telephony is up";
    scanExistingObjects();
}

void TelephonyClient::onServiceDown()
{
    const bool wasAvailable = available();
    serviceUp_ = false;
    if (!callPath_.isEmpty()) {
        callPath_.clear();
        emit callSetupEnded();
    }
    agPath_.clear();
    agAddress_.clear();
    transportPath_.clear();
    transportState_.clear();
    codec_.clear();
    if (wasAvailable)
        emit availableChanged(false);
}

void TelephonyClient::scanExistingObjects()
{
    // Enumerates AudioGateway/transport objects ONLY — Call1 children are
    // never listed by GetManagedObjects (decision record §6.4).
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, kRoot, kObjMgrIface, QStringLiteral("GetManagedObjects"));
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        qCWarning(lcTel) << "GetManagedObjects failed:" << reply.errorMessage();
        return;
    }

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QDBusObjectPath objPath;
        arg >> objPath;

        const QDBusArgument ifacesArg = qvariant_cast<QDBusArgument>(arg.asVariant());
        ifacesArg.beginMap();
        while (!ifacesArg.atEnd()) {
            ifacesArg.beginMapEntry();
            QString iface;
            ifacesArg >> iface;

            const QDBusArgument propsArg = qvariant_cast<QDBusArgument>(ifacesArg.asVariant());
            QVariantMap props;
            propsArg.beginMap();
            while (!propsArg.atEnd()) {
                propsArg.beginMapEntry();
                QString key;
                QDBusVariant val;
                propsArg >> key >> val;
                props[key] = val.variant();
                propsArg.endMapEntry();
            }
            propsArg.endMap();
            ifacesArg.endMapEntry();

            if (iface == kAgIface) adoptAg(objPath.path(), props);
            else if (iface == kTransportIface) adoptTransport(objPath.path(), props);
        }
        ifacesArg.endMap();
        arg.endMapEntry();
    }
    arg.endMap();
}

QVariantMap TelephonyClient::demarshalProps(const QVariant& v)
{
    QVariantMap out;
    const QDBusArgument arg = qvariant_cast<QDBusArgument>(v);
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QString key;
        QDBusVariant val;
        arg >> key >> val;
        out[key] = val.variant();
        arg.endMapEntry();
    }
    arg.endMap();
    return out;
}

void TelephonyClient::adoptAg(const QString& path, const QVariantMap& props)
{
    if (!agPath_.isEmpty() && agPath_ != path) {
        qCWarning(lcTel) << "Second AudioGateway ignored (single-AG v1):" << path;
        return;
    }
    agPath_ = path;
    agAddress_ = props.value(QStringLiteral("Address")).toString();
    qCInfo(lcTel) << "AudioGateway appeared:" << path << agAddress_;
    emit availableChanged(true);
}

void TelephonyClient::adoptTransport(const QString& path, const QVariantMap& props)
{
    transportPath_ = path;
    const QString state = props.value(QStringLiteral("State")).toString();
    const QString codec = props.value(QStringLiteral("Codec")).toString();
    if (!state.isEmpty() && state != transportState_) {
        transportState_ = state;
        emit transportStateChanged(state);
    }
    if (!codec.isEmpty() && codec != codec_) {
        codec_ = codec;
        qCInfo(lcTel) << "HFP codec:" << codec;
        emit codecChanged(codec);
    }
    applyRejectSco();
}

void TelephonyClient::adoptCall(const QString& path, const QVariantMap& props)
{
    if (!callPath_.isEmpty()) {
        // Second concurrent Call1 (call-waiting): still emit — the state
        // machine decides (it ignores setup while Active, design §5).
        qCInfo(lcTel) << "Additional Call1 during setup:" << path;
    }
    callPath_ = path;
    emit callSetupStarted(
        props.value(QStringLiteral("State")).toString(),
        props.value(QStringLiteral("LineIdentification")).toString(),
        props.value(QStringLiteral("Name")).toString());
}

void TelephonyClient::onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces)
{
    // interfaces: keys are interface names; values are QDBusArgument-wrapped a{sv}.
    if (interfaces.contains(kAgIface))
        adoptAg(path.path(), demarshalProps(interfaces.value(kAgIface)));
    if (interfaces.contains(kTransportIface))
        adoptTransport(path.path(), demarshalProps(interfaces.value(kTransportIface)));
    if (interfaces.contains(kCallIface))
        adoptCall(path.path(), demarshalProps(interfaces.value(kCallIface)));
}

void TelephonyClient::onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces)
{
    const QString p = path.path();
    if (p == callPath_ && interfaces.contains(kCallIface)) {
        callPath_.clear();
        emit callSetupEnded();
    }
    if (p == agPath_ && interfaces.contains(kAgIface)) {
        agPath_.clear();
        agAddress_.clear();
        transportPath_.clear();
        transportState_.clear();
        codec_.clear();
        if (!callPath_.isEmpty()) {
            callPath_.clear();
            emit callSetupEnded();
        }
        qCInfo(lcTel) << "AudioGateway removed:" << p;
        emit availableChanged(false);
    }
}

void TelephonyClient::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                          const QStringList& /*invalidated*/,
                                          const QDBusMessage& msg)
{
    const QString path = msg.path();
    if (interface == kCallIface && path == callPath_) {
        if (changed.contains(QStringLiteral("State")))
            emit callSetupChanged(changed.value(QStringLiteral("State")).toString());
    } else if (interface == kTransportIface && path == transportPath_) {
        if (changed.contains(QStringLiteral("State"))) {
            transportState_ = changed.value(QStringLiteral("State")).toString();
            emit transportStateChanged(transportState_);
        }
        if (changed.contains(QStringLiteral("Codec"))) {
            codec_ = changed.value(QStringLiteral("Codec")).toString();
            qCInfo(lcTel) << "HFP codec:" << codec_;
            emit codecChanged(codec_);
        }
    }
}

void TelephonyClient::asyncCall(const QString& op, QDBusMessage msg)
{
    QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(msg);
    auto* w = new QDBusPendingCallWatcher(pending, this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, op](QDBusPendingCallWatcher* w) {
        w->deleteLater();
        if (w->isError()) {
            qCWarning(lcTel) << op << "failed:" << w->error().message();
            emit commandFailed(op, w->error().message());
        }
    });
}

void TelephonyClient::dial(const QString& number)
{
    if (!available()) { emit commandFailed(QStringLiteral("Dial"), QStringLiteral("no audio gateway")); return; }
    auto msg = QDBusMessage::createMethodCall(kService, agPath_, kAgIface, QStringLiteral("Dial"));
    msg << number;
    asyncCall(QStringLiteral("Dial"), msg);
}

void TelephonyClient::answer()
{
    if (callPath_.isEmpty()) { emit commandFailed(QStringLiteral("Answer"), QStringLiteral("no call in setup")); return; }
    asyncCall(QStringLiteral("Answer"),
        QDBusMessage::createMethodCall(kService, callPath_, kCallIface, QStringLiteral("Answer")));
}

void TelephonyClient::hangupAll()
{
    if (!available()) { emit commandFailed(QStringLiteral("HangupAll"), QStringLiteral("no audio gateway")); return; }
    asyncCall(QStringLiteral("HangupAll"),
        QDBusMessage::createMethodCall(kService, agPath_, kAgIface, QStringLiteral("HangupAll")));
}

void TelephonyClient::sendTones(const QString& tones)
{
    if (!available()) { emit commandFailed(QStringLiteral("SendTones"), QStringLiteral("no audio gateway")); return; }
    auto msg = QDBusMessage::createMethodCall(kService, agPath_, kAgIface, QStringLiteral("SendTones"));
    msg << tones;
    asyncCall(QStringLiteral("SendTones"), msg);
}

void TelephonyClient::setRejectSco(bool reject)
{
    rejectSco_ = reject;
    applyRejectSco();
}

void TelephonyClient::applyRejectSco()
{
    if (transportPath_.isEmpty() || !serviceUp_) return;
    auto msg = QDBusMessage::createMethodCall(kService, transportPath_, kPropsIface, QStringLiteral("Set"));
    msg << kTransportIface << QStringLiteral("RejectSCO")
        << QVariant::fromValue(QDBusVariant(rejectSco_));
    asyncCall(QStringLiteral("Set RejectSCO"), msg);
    qCInfo(lcTel) << "RejectSCO →" << rejectSco_;
}

} // namespace oap
```

- [ ] **Step 5: Register in build files**

In `src/CMakeLists.txt`, add `core/services/TelephonyClient.cpp` to SOURCES (next to `core/services/PhoneStateService.cpp`).
In `tests/CMakeLists.txt`, add `oap_add_test(test_telephony_client SOURCES test_telephony_client.cpp)` (next to `test_phone_state_service`, line ~57).

- [ ] **Step 6: Run test to verify it passes**

Run: `cd build && cmake .. && make -j$(nproc) && ctest -R test_telephony_client --output-on-failure`
Expected: PASS (4 tests).

- [ ] **Step 7: Commit**

```bash
git add src/core/services/TelephonyClient.hpp src/core/services/TelephonyClient.cpp \
        src/CMakeLists.txt tests/CMakeLists.txt tests/test_telephony_client.cpp
git commit -m "feat(phone): TelephonyClient D-Bus adapter for org.pipewire.Telephony"
```

---

### Task 3: `ScoNodeMonitor` + AudioService accessors

Watches the PipeWire registry for SCO nodes (`api.bluez5.profile == "headset-audio-gateway"`) and reports whether any is RUNNING — the reliable in-call signal (design §4.2).

**Files:**
- Create: `src/core/audio/ScoNodeMonitor.hpp`
- Create: `src/core/audio/ScoNodeMonitor.cpp`
- Modify: `src/core/services/AudioService.hpp` (two accessors)
- Modify: `src/CMakeLists.txt` (add `core/audio/ScoNodeMonitor.cpp` next to `core/audio/PipeWireDeviceRegistry.cpp`, line ~40)
- Test: `tests/test_sco_node_monitor.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `pw_thread_loop*`/`pw_core*` from `AudioService` (new accessors below).
- Produces (used by Tasks 4, 6):
```cpp
namespace oap {
class ScoNodeMonitor : public QObject {
    void start(struct pw_thread_loop* loop, struct pw_core* core); // caller must NOT hold the loop lock
    void stop();
    bool scoRunning() const;              // thread-safe (atomic)
signals:
    void scoRunningChanged(bool running); // emitted on the Qt main thread
};
}
// AudioService additions:
struct pw_thread_loop* pwThreadLoop() const;
struct pw_core* pwCore() const;
```

- [ ] **Step 1: Write the failing test**

`tests/test_sco_node_monitor.cpp`:
```cpp
// Inert-without-PipeWire safety only. Node-state tracking is live-check
// territory (design doc §11 L4) — there is no PipeWire daemon in CI.
#include <QtTest/QtTest>
#include "core/audio/ScoNodeMonitor.hpp"

class TestScoNodeMonitor : public QObject {
    Q_OBJECT
private slots:
    void testInertWithoutPipeWire();
};

void TestScoNodeMonitor::testInertWithoutPipeWire() {
    oap::ScoNodeMonitor m;
    QVERIFY(!m.scoRunning());
    m.start(nullptr, nullptr);   // must be a guarded no-op
    QVERIFY(!m.scoRunning());
    m.stop();
    m.stop();                    // idempotent
    QVERIFY(!m.scoRunning());
}

QTEST_MAIN(TestScoNodeMonitor)
#include "test_sco_node_monitor.moc"
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd build && cmake .. && make -j$(nproc) 2>&1 | tail -5`
Expected: FAIL — header not found.

- [ ] **Step 3: Write `ScoNodeMonitor.hpp`**

```cpp
#pragma once

#include <QObject>
#include <atomic>
#include <map>
#include <pipewire/pipewire.h>

namespace oap {

/// Watches the PipeWire registry for HFP SCO nodes
/// (api.bluez5.profile == "headset-audio-gateway") and reports whether any
/// is in the RUNNING state. Node state RUNNING is the reliable "call audio
/// is flowing" signal (HFP decision record §6.3); the D-Bus transport State
/// is advisory only.
///
/// All PipeWire callbacks run on the PW thread loop; the signal is
/// marshaled to the Qt main thread. If PipeWire is unavailable, the monitor
/// stays inert and scoRunning() is permanently false.
class ScoNodeMonitor : public QObject {
    Q_OBJECT
public:
    explicit ScoNodeMonitor(QObject* parent = nullptr);
    ~ScoNodeMonitor() override;

    /// Caller must NOT hold the thread-loop lock (start takes it itself).
    void start(struct pw_thread_loop* loop, struct pw_core* core);
    void stop();

    bool scoRunning() const { return anyRunning_.load(); }

signals:
    void scoRunningChanged(bool running);

private:
    struct Tracked {
        ScoNodeMonitor* owner = nullptr;
        uint32_t id = 0;
        struct pw_node* node = nullptr;
        struct spa_hook listener{};
        bool running = false;
    };

    static void onGlobal(void* data, uint32_t id, uint32_t permissions,
                         const char* type, uint32_t version,
                         const struct spa_dict* props);
    static void onGlobalRemove(void* data, uint32_t id);
    static void onNodeInfo(void* data, const struct pw_node_info* info);
    void recomputeRunning();   // PW thread only
    void destroyTracked(Tracked* t);  // PW thread or under loop lock

    struct pw_thread_loop* threadLoop_ = nullptr;
    struct pw_registry* registry_ = nullptr;
    struct spa_hook registryListener_{};
    std::map<uint32_t, Tracked*> tracked_;   // PW thread only (+ stop() under lock)
    std::atomic<bool> anyRunning_{false};
};

} // namespace oap
```

- [ ] **Step 4: Write `ScoNodeMonitor.cpp`**

```cpp
#include "ScoNodeMonitor.hpp"
#include <spa/utils/dict.h>
#include <pipewire/keys.h>
#include <QLoggingCategory>
#include <cstring>

Q_LOGGING_CATEGORY(lcSco, "oap.sco")

namespace oap {

ScoNodeMonitor::ScoNodeMonitor(QObject* parent) : QObject(parent) {}

ScoNodeMonitor::~ScoNodeMonitor() { stop(); }

void ScoNodeMonitor::start(struct pw_thread_loop* loop, struct pw_core* core)
{
    if (!loop || !core) {
        qCInfo(lcSco) << "PipeWire unavailable — SCO monitor inert";
        return;
    }
    if (registry_) return;  // already started
    threadLoop_ = loop;

    pw_thread_loop_lock(threadLoop_);
    registry_ = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    if (registry_) {
        static const struct pw_registry_events registryEvents = {
            .version = PW_VERSION_REGISTRY_EVENTS,
            .global = onGlobal,
            .global_remove = onGlobalRemove,
        };
        spa_zero(registryListener_);
        pw_registry_add_listener(registry_, &registryListener_, &registryEvents, this);
    }
    pw_thread_loop_unlock(threadLoop_);
}

void ScoNodeMonitor::stop()
{
    if (!registry_ || !threadLoop_) return;
    pw_thread_loop_lock(threadLoop_);
    for (auto& [id, t] : tracked_)
        destroyTracked(t);
    tracked_.clear();
    spa_hook_remove(&registryListener_);
    pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(registry_));
    registry_ = nullptr;
    pw_thread_loop_unlock(threadLoop_);
    threadLoop_ = nullptr;
    anyRunning_.store(false);
}

void ScoNodeMonitor::destroyTracked(Tracked* t)
{
    spa_hook_remove(&t->listener);
    if (t->node)
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(t->node));
    delete t;
}

void ScoNodeMonitor::onGlobal(void* data, uint32_t id, uint32_t /*permissions*/,
                              const char* type, uint32_t /*version*/,
                              const struct spa_dict* props)
{
    auto* self = static_cast<ScoNodeMonitor*>(data);
    if (!type || std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0 || !props)
        return;

    // Match on the bluez profile property alone — do NOT filter on
    // media.class (SCO node classes vary; the profile string is the
    // discriminator, per the live inspection).
    const char* profile = spa_dict_lookup(props, "api.bluez5.profile");
    if (!profile || std::strcmp(profile, "headset-audio-gateway") != 0)
        return;

    auto* t = new Tracked{};
    t->owner = self;
    t->id = id;
    t->node = static_cast<struct pw_node*>(
        pw_registry_bind(self->registry_, id, type, PW_VERSION_NODE, 0));
    if (!t->node) { delete t; return; }

    static const struct pw_node_events nodeEvents = {
        .version = PW_VERSION_NODE_EVENTS,
        .info = onNodeInfo,
    };
    pw_node_add_listener(t->node, &t->listener, &nodeEvents, t);
    self->tracked_[id] = t;
    qCInfo(lcSco) << "Tracking SCO node" << id;
}

void ScoNodeMonitor::onGlobalRemove(void* data, uint32_t id)
{
    auto* self = static_cast<ScoNodeMonitor*>(data);
    auto it = self->tracked_.find(id);
    if (it == self->tracked_.end()) return;
    self->destroyTracked(it->second);
    self->tracked_.erase(it);
    qCInfo(lcSco) << "SCO node removed" << id;
    self->recomputeRunning();
}

void ScoNodeMonitor::onNodeInfo(void* data, const struct pw_node_info* info)
{
    auto* t = static_cast<Tracked*>(data);
    if (!(info->change_mask & PW_NODE_CHANGE_MASK_STATE)) return;
    const bool running = (info->state == PW_NODE_STATE_RUNNING);
    if (running == t->running) return;
    t->running = running;
    t->owner->recomputeRunning();
}

void ScoNodeMonitor::recomputeRunning()
{
    bool any = false;
    for (const auto& [id, t] : tracked_)
        if (t->running) { any = true; break; }

    const bool prev = anyRunning_.exchange(any);
    if (prev == any) return;

    QMetaObject::invokeMethod(this, [this, any]() {
        qCInfo(lcSco) << "SCO running:" << any;
        emit scoRunningChanged(any);
    }, Qt::QueuedConnection);
}

} // namespace oap
```

- [ ] **Step 5: Add AudioService accessors**

In `src/core/services/AudioService.hpp`, in the public section right after `bool isAvailable() const { return threadLoop_ != nullptr; }`:
```cpp
    /// PipeWire handles for auxiliary watchers (e.g. ScoNodeMonitor).
    /// Null when PipeWire is unavailable.
    struct pw_thread_loop* pwThreadLoop() const { return threadLoop_; }
    struct pw_core* pwCore() const { return core_; }
```

- [ ] **Step 6: Register in build files**

`src/CMakeLists.txt`: add `core/audio/ScoNodeMonitor.cpp` next to `core/audio/PipeWireDeviceRegistry.cpp` (line ~40).
`tests/CMakeLists.txt`: add `oap_add_test(test_sco_node_monitor SOURCES test_sco_node_monitor.cpp)`.

- [ ] **Step 7: Run tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest -R test_sco_node_monitor --output-on-failure`
Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add src/core/audio/ScoNodeMonitor.hpp src/core/audio/ScoNodeMonitor.cpp \
        src/core/services/AudioService.hpp src/CMakeLists.txt \
        tests/test_sco_node_monitor.cpp tests/CMakeLists.txt
git commit -m "feat(audio): ScoNodeMonitor — PipeWire watch for HFP SCO node state"
```

---

### Task 4: Widen the provider interfaces + real call state machine in PhoneStateService

The heart of D2. Implements the design §5 state machine exactly — read that table before coding. Also adds the `phone.*` config keys.

**Files:**
- Modify: `src/core/services/ICallStateProvider.hpp`
- Modify: `src/core/services/IPhoneStateService.hpp`
- Modify: `src/core/services/PhoneStateService.hpp`
- Modify: `src/core/services/PhoneStateService.cpp`
- Modify: `src/core/YamlConfig.cpp` (initDefaults, ~line 46 area)
- Modify: `tests/test_phone_state_service.cpp` (extend; keep existing cases)
- Modify: `tests/test_config_key_coverage.cpp`

**Interfaces:**
- Consumes: `TelephonyClient` (Task 2 signals/slots), `ScoNodeMonitor` (Task 3 signal).
- Produces (used by Tasks 6, 7, 8):
```cpp
// ICallStateProvider: enum + bool-returning commands
enum CallState { Idle = 0, Ringing, Active, Dialing, Alerting, Held, Waiting };
Q_INVOKABLE virtual bool answer() = 0;   // was void; QML ignores returns
Q_INVOKABLE virtual bool hangup() = 0;
// IPhoneStateService additions:
Q_INVOKABLE virtual bool dial(const QString& number) = 0;
Q_INVOKABLE virtual bool sendDtmf(const QString& tones) = 0;
virtual bool telephonyAvailable() const = 0;
signals: void telephonyAvailableChanged();
// PhoneStateService additions (event slots are PUBLIC — they are the test API):
void attachTelephony(TelephonyClient* client);
void attachScoMonitor(ScoNodeMonitor* monitor);
void setSettleGraceMs(int ms); void setScoDebounceMs(int ms);
public slots:
    void onCallSetupStarted(const QString& state, const QString& line, const QString& name);
    void onCallSetupChanged(const QString& state);
    void onCallSetupEnded();
    void onScoRunningChanged(bool running);
    void onTransportStateChanged(const QString& state);
    void onTelephonyAvailable(bool available);
```

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_phone_state_service.cpp` (add slots to the class declaration AND the bodies; keep every existing test unchanged):
```cpp
    // --- D2 state machine (design doc §5 table) ---
    void testTelephonyAvailability();
    void testIncomingAnsweredCleanPath();
    void testIncomingAnsweredViaScoSettle();
    void testIncomingRejectedSettleTimeout();
    void testOutgoingFullPath();
    void testActiveEndsOnScoDropDebounced();
    void testActiveSurvivesScoBlip();
    void testActiveEndsOnTransportIdle();
    void testRecoveryScoRunningFromIdle();
    void testCallWaitingIgnoredWhileActive();
    void testAgVanishResetsToIdle();
    void testDialGuards();
```
```cpp
using CS = oap::ICallStateProvider;

static oap::PhoneStateService* makeFastService(QObject* parent = nullptr) {
    auto* s = new oap::PhoneStateService(parent);
    s->setSettleGraceMs(50);
    s->setScoDebounceMs(50);
    s->onTelephonyAvailable(true);
    return s;
}

void TestPhoneStateService::testTelephonyAvailability() {
    oap::PhoneStateService s;
    QVERIFY(!s.telephonyAvailable());
    QSignalSpy spy(&s, &oap::IPhoneStateService::telephonyAvailableChanged);
    s.onTelephonyAvailable(true);
    QVERIFY(s.telephonyAvailable());
    QCOMPARE(spy.count(), 1);
}

void TestPhoneStateService::testIncomingAnsweredCleanPath() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "+15125551212", "Test Caller");
    QCOMPARE(s->callState(), (int)CS::Ringing);
    QCOMPARE(s->callerNumber(), QString("+15125551212"));
    QCOMPARE(s->callerName(), QString("Test Caller"));
    s->onCallSetupChanged("active");          // Call1 State → active before removal
    QCOMPARE(s->callState(), (int)CS::Active);
    s->onCallSetupEnded();                     // ephemeral object destroyed — stays Active
    QCOMPARE(s->callState(), (int)CS::Active);
}

void TestPhoneStateService::testIncomingAnsweredViaScoSettle() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "+15125551212", "");
    s->onCallSetupEnded();                     // no State→active seen: settling
    QCOMPARE(s->callState(), (int)CS::Ringing); // keeps reporting prior state
    s->onScoRunningChanged(true);
    QCOMPARE(s->callState(), (int)CS::Active);
}

void TestPhoneStateService::testIncomingRejectedSettleTimeout() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "+15125551212", "");
    s->onCallSetupEnded();
    QTRY_COMPARE_WITH_TIMEOUT(s->callState(), (int)CS::Idle, 500); // grace 50ms expires
    QVERIFY(s->callerNumber().isEmpty());
}

void TestPhoneStateService::testOutgoingFullPath() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("dialing", "+15125551212", "");
    QCOMPARE(s->callState(), (int)CS::Dialing);
    s->onCallSetupChanged("alerting");
    QCOMPARE(s->callState(), (int)CS::Alerting);
    s->onCallSetupChanged("active");
    QCOMPARE(s->callState(), (int)CS::Active);
    s->onCallSetupEnded();
    QCOMPARE(s->callState(), (int)CS::Active);
    s->onScoRunningChanged(true);
    s->onScoRunningChanged(false);            // hangup: SCO drops
    QTRY_COMPARE_WITH_TIMEOUT(s->callState(), (int)CS::Idle, 500);
}

void TestPhoneStateService::testActiveEndsOnScoDropDebounced() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "1", "");
    s->onCallSetupEnded();
    s->onScoRunningChanged(true);
    QCOMPARE(s->callState(), (int)CS::Active);
    s->onScoRunningChanged(false);
    QCOMPARE(s->callState(), (int)CS::Active); // debounce window — not yet
    QTRY_COMPARE_WITH_TIMEOUT(s->callState(), (int)CS::Idle, 500);
}

void TestPhoneStateService::testActiveSurvivesScoBlip() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "1", "");
    s->onCallSetupEnded();
    s->onScoRunningChanged(true);
    s->onScoRunningChanged(false);
    s->onScoRunningChanged(true);              // back within debounce
    QTest::qWait(150);
    QCOMPARE(s->callState(), (int)CS::Active); // blip did not end the call
}

void TestPhoneStateService::testActiveEndsOnTransportIdle() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "1", "");
    s->onCallSetupChanged("active");
    // SCO never observed running (monitor inert): transport idle ends it
    s->onTransportStateChanged("idle");
    QCOMPARE(s->callState(), (int)CS::Idle);
}

void TestPhoneStateService::testRecoveryScoRunningFromIdle() {
    QObject root; auto* s = makeFastService(&root);
    QCOMPARE(s->callState(), (int)CS::Idle);
    s->onScoRunningChanged(true);              // restarted mid-call / audio routed back
    QCOMPARE(s->callState(), (int)CS::Active);
}

void TestPhoneStateService::testCallWaitingIgnoredWhileActive() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "+1111", "First");
    s->onCallSetupChanged("active");
    s->onCallSetupStarted("incoming", "+2222", "Second"); // call-waiting: ignored in v1
    QCOMPARE(s->callState(), (int)CS::Active);
    QCOMPARE(s->callerNumber(), QString("+1111"));
    s->onCallSetupEnded();                     // waiting call resolved — no transition
    QCOMPARE(s->callState(), (int)CS::Active);
}

void TestPhoneStateService::testAgVanishResetsToIdle() {
    QObject root; auto* s = makeFastService(&root);
    s->onCallSetupStarted("incoming", "1", "");
    s->onCallSetupChanged("active");
    s->onTelephonyAvailable(false);
    QCOMPARE(s->callState(), (int)CS::Idle);
    QVERIFY(!s->telephonyAvailable());
}

void TestPhoneStateService::testDialGuards() {
    QObject root; auto* s = makeFastService(&root);
    // telephony marked available but no TelephonyClient attached →
    // mock-mode dial: transitions locally so the dev VM UI still works
    QVERIFY(s->dial("5551234"));
    QCOMPARE(s->callState(), (int)CS::Dialing);
    QVERIFY(!s->dial("5551234"));              // not Idle → rejected
    QVERIFY(!s->sendDtmf("1"));                // not Active → rejected
}
```

Note: two existing tests encode mock transitions (`testAnswerOnlyFromRinging`, `testHangupFromActive` — they call `answer()`/`hangup()` expecting local state changes). Mock mode (no `TelephonyClient` attached) preserves exactly that behavior; those tests must keep passing UNMODIFIED apart from the `answer()`/`hangup()` return type now being `bool` (no test change needed — returns were ignored).

- [ ] **Step 2: Run to verify failures**

Run: `cd build && cmake .. && make -j$(nproc) 2>&1 | tail -5`
Expected: compile FAIL (missing enum values / methods).

- [ ] **Step 3: Widen `ICallStateProvider.hpp`**

Replace the enum and the two invokables (rest of file unchanged):
```cpp
    // Frozen numeric values — QML compares raw ints (IncomingCallOverlay).
    // New states APPEND ONLY. Held/Waiting are declared for parity with the
    // API v1 CallState enum but are unproducible in v1 (backend call objects
    // are ephemeral; see HFP call audio design §4.4).
    enum CallState { Idle = 0, Ringing, Active, Dialing, Alerting, Held, Waiting };
    Q_ENUM(CallState)
```
```cpp
    /// Returns true if the command was dispatched (call-state guard passed).
    /// The API bridge maps false → FAILED; QML callers ignore the return.
    Q_INVOKABLE virtual bool answer() = 0;
    Q_INVOKABLE virtual bool hangup() = 0;
```

- [ ] **Step 4: Widen `IPhoneStateService.hpp`**

Add inside the class:
```cpp
    /// True while the telephony backend is reachable AND a phone (audio
    /// gateway) is connected — the single source of truth for the API's
    /// can_dial/can_answer/can_hangup/can_send_dtmf capability flags.
    virtual bool telephonyAvailable() const = 0;

    /// Place an outgoing call. Only dispatched from Idle. Returns true if
    /// dispatched.
    Q_INVOKABLE virtual bool dial(const QString& number) = 0;

    /// Send DTMF tones into the active call. Only dispatched while Active.
    Q_INVOKABLE virtual bool sendDtmf(const QString& tones) = 0;
```
And to its `signals:` block:
```cpp
    void telephonyAvailableChanged();
```

- [ ] **Step 5: Rewrite `PhoneStateService.hpp`**

Full new content:
```cpp
#pragma once

#include "IPhoneStateService.hpp"
#include <QDBusObjectPath>
#include <QTimer>

class QDBusServiceWatcher;

namespace oap {

class INotificationService;
class TelephonyClient;
class ScoNodeMonitor;

/// Core service owning the HFP call state machine.
///
/// Inputs: TelephonyClient (PipeWire org.pipewire.Telephony — call setup
/// objects, transport state), ScoNodeMonitor (SCO node running = call audio
/// flowing), BlueZ Device1 monitoring (phone connected / device name).
///
/// State machine: docs/superpowers/specs/2026-07-05-hfp-call-audio-design.md §5.
/// Key semantics (live-verified): Call1 objects exist during call SETUP only;
/// "active call" truth comes from SCO node state, with transport state as a
/// fallback inside the settle window. Transitions come only from telephony/
/// SCO events — never optimistically from command dispatch.
///
/// Mock mode: with no TelephonyClient attached (dev VM, tests), answer()/
/// hangup()/dial() perform local transitions so the UI remains drivable.
class PhoneStateService : public IPhoneStateService {
    Q_OBJECT
public:
    explicit PhoneStateService(QObject* parent = nullptr);
    ~PhoneStateService() override;

    // ICallStateProvider
    int callState() const override;
    QString callerName() const override;
    QString callerNumber() const override;
    bool answer() override;
    bool hangup() override;

    // IPhoneStateService
    bool phoneConnected() const override { return phoneConnected_; }
    QString deviceName() const override { return deviceName_; }
    int callDuration() const override { return callDuration_; }
    bool telephonyAvailable() const override { return telephonyAvailable_; }
    bool dial(const QString& number) override;
    bool sendDtmf(const QString& tones) override;

    /// Set notification service for incoming call alerts (optional)
    void setNotificationService(INotificationService* svc) { notificationService_ = svc; }

    /// Wire the real telephony backend. Connects the client's signals to the
    /// event slots below. Without this, the service runs in mock mode.
    void attachTelephony(TelephonyClient* client);
    void attachScoMonitor(ScoNodeMonitor* monitor);

    /// Tunables (config: phone.settle_grace_ms; tests set low values).
    void setSettleGraceMs(int ms) { settleGraceMs_ = ms; }
    void setScoDebounceMs(int ms) { scoDebounceMs_ = ms; }

    /// Called by test code to simulate an incoming call (mock mode).
    void setIncomingCall(const QString& number, const QString& name);

    /// Start D-Bus monitoring for HFP devices (BlueZ, system bus)
    void startDBusMonitoring();
    void stopDBusMonitoring();

public slots:
    // State-machine event API — public because it IS the unit-test surface.
    void onCallSetupStarted(const QString& state, const QString& line, const QString& name);
    void onCallSetupChanged(const QString& state);
    void onCallSetupEnded();
    void onScoRunningChanged(bool running);
    void onTransportStateChanged(const QString& state);
    void onTelephonyAvailable(bool available);

private slots:
    void onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                             const QStringList& invalidated);
    void onSettleTimeout();
    void onScoDebounceTimeout();

private:
    void scanExistingDevices();
    void setCallStateInternal(ICallStateProvider::CallState state);
    void updateCallDuration();
    bool inSetupState() const {
        return callState_ == Ringing || callState_ == Dialing || callState_ == Alerting;
    }

    INotificationService* notificationService_ = nullptr;
    QDBusServiceWatcher* bluezWatcher_ = nullptr;
    TelephonyClient* telephony_ = nullptr;   // null = mock mode
    QTimer callTimer_;
    QTimer settleTimer_;
    QTimer scoDebounceTimer_;
    bool monitoring_ = false;

    ICallStateProvider::CallState callState_ = ICallStateProvider::Idle;
    QString callerNumber_;
    QString callerName_;
    int callDuration_ = 0;
    bool phoneConnected_ = false;
    QString deviceName_;
    QString devicePath_;
    QString activeCallNotificationId_;

    bool telephonyAvailable_ = false;
    bool scoRunning_ = false;
    bool inSettle_ = false;
    QString transportState_;
    int settleGraceMs_ = 2000;
    int scoDebounceMs_ = 1000;
};

} // namespace oap
```

- [ ] **Step 6: Extend `PhoneStateService.cpp`**

Keep everything BlueZ-related (`startDBusMonitoring` through `onPropertiesChanged`) unchanged. Apply these changes:

Constructor — add timer setup after the existing `callTimer_` lines:
```cpp
    settleTimer_.setSingleShot(true);
    connect(&settleTimer_, &QTimer::timeout, this, &PhoneStateService::onSettleTimeout);
    scoDebounceTimer_.setSingleShot(true);
    connect(&scoDebounceTimer_, &QTimer::timeout, this, &PhoneStateService::onScoDebounceTimeout);
```

Replace `answer()` and `hangup()`:
```cpp
bool PhoneStateService::answer()
{
    if (callState_ != ICallStateProvider::Ringing) return false;
    if (telephony_ && telephonyAvailable_) {
        // Real mode: request only. The transition arrives via telephony/SCO
        // events — never optimistically (design §12.5).
        telephony_->answer();
        return true;
    }
    setCallStateInternal(ICallStateProvider::Active);  // mock mode
    return true;
}

bool PhoneStateService::hangup()
{
    if (callState_ == ICallStateProvider::Idle) return false;
    if (telephony_ && telephonyAvailable_) {
        telephony_->hangupAll();
        return true;
    }
    callerNumber_.clear();  // mock mode
    callerName_.clear();
    setCallStateInternal(ICallStateProvider::Idle);
    return true;
}
```
(`TelephonyClient` is forward-declared in the header; the .cpp includes `TelephonyClient.hpp`, so direct calls are fine.)

Add the new methods:
```cpp
bool PhoneStateService::dial(const QString& number)
{
    if (callState_ != ICallStateProvider::Idle || number.isEmpty()) return false;
    if (telephony_ && telephonyAvailable_) {
        telephony_->dial(number);
        return true;
    }
    if (!telephony_) {                          // mock mode: keep dev UI alive
        callerNumber_ = number;
        callerName_.clear();
        setCallStateInternal(ICallStateProvider::Dialing);
        return true;
    }
    return false;                               // attached but no phone
}

bool PhoneStateService::sendDtmf(const QString& tones)
{
    if (callState_ != ICallStateProvider::Active || tones.isEmpty()) return false;
    if (telephony_ && telephonyAvailable_) {
        telephony_->sendTones(tones);
        return true;
    }
    return telephony_ == nullptr;               // mock mode pretends success
}

void PhoneStateService::attachTelephony(TelephonyClient* client)
{
    telephony_ = client;
    if (!client) return;
    connect(client, &TelephonyClient::callSetupStarted,
            this, &PhoneStateService::onCallSetupStarted);
    connect(client, &TelephonyClient::callSetupChanged,
            this, &PhoneStateService::onCallSetupChanged);
    connect(client, &TelephonyClient::callSetupEnded,
            this, &PhoneStateService::onCallSetupEnded);
    connect(client, &TelephonyClient::transportStateChanged,
            this, &PhoneStateService::onTransportStateChanged);
    connect(client, &TelephonyClient::availableChanged,
            this, &PhoneStateService::onTelephonyAvailable);
    onTelephonyAvailable(client->available());
}

void PhoneStateService::attachScoMonitor(ScoNodeMonitor* monitor)
{
    if (!monitor) return;
    connect(monitor, &ScoNodeMonitor::scoRunningChanged,
            this, &PhoneStateService::onScoRunningChanged);
    onScoRunningChanged(monitor->scoRunning());
}
```
(Include `"TelephonyClient.hpp"` and `"core/audio/ScoNodeMonitor.hpp"` at the top of the .cpp.)

The state machine slots (design §5 table — implement EXACTLY this):
```cpp
void PhoneStateService::onCallSetupStarted(const QString& state, const QString& line,
                                           const QString& name)
{
    if (callState_ == ICallStateProvider::Active) {
        // Call-waiting: single-call model in v1 — log and ignore (design §5).
        qWarning() << "PhoneStateService: second call ignored (call-waiting unsupported):" << line;
        return;
    }
    callerNumber_ = line;
    callerName_ = name;
    if (state == QLatin1String("incoming"))
        setCallStateInternal(ICallStateProvider::Ringing);
    else if (state == QLatin1String("dialing"))
        setCallStateInternal(ICallStateProvider::Dialing);
    else if (state == QLatin1String("alerting"))
        setCallStateInternal(ICallStateProvider::Alerting);
    else
        qWarning() << "PhoneStateService: unknown setup state" << state;
}

void PhoneStateService::onCallSetupChanged(const QString& state)
{
    if (!inSetupState()) return;
    if (state == QLatin1String("alerting")) {
        setCallStateInternal(ICallStateProvider::Alerting);
    } else if (state == QLatin1String("active")) {
        inSettle_ = false;
        settleTimer_.stop();
        setCallStateInternal(ICallStateProvider::Active);
    }
}

void PhoneStateService::onCallSetupEnded()
{
    if (!inSetupState()) return;   // Active: waiting-call resolution etc. — no transition
    if (scoRunning_) {
        setCallStateInternal(ICallStateProvider::Active);
        return;
    }
    // Ambiguous: answered (SCO imminent) or rejected/cancelled. Keep
    // reporting the setup state during the grace window (no UI flap).
    inSettle_ = true;
    settleTimer_.start(settleGraceMs_);
}

void PhoneStateService::onSettleTimeout()
{
    if (!inSettle_) return;
    inSettle_ = false;
    callerNumber_.clear();
    callerName_.clear();
    setCallStateInternal(ICallStateProvider::Idle);
}

void PhoneStateService::onScoRunningChanged(bool running)
{
    scoRunning_ = running;
    if (running) {
        scoDebounceTimer_.stop();
        if (inSettle_) {
            inSettle_ = false;
            settleTimer_.stop();
            setCallStateInternal(ICallStateProvider::Active);
        } else if (callState_ == ICallStateProvider::Idle && telephonyAvailable_) {
            // Recovery: restarted mid-call, or user routed audio back to car.
            setCallStateInternal(ICallStateProvider::Active);
        }
    } else {
        if (callState_ == ICallStateProvider::Active)
            scoDebounceTimer_.start(scoDebounceMs_);
    }
}

void PhoneStateService::onScoDebounceTimeout()
{
    if (callState_ == ICallStateProvider::Active && !scoRunning_) {
        callerNumber_.clear();
        callerName_.clear();
        setCallStateInternal(ICallStateProvider::Idle);
    }
}

void PhoneStateService::onTransportStateChanged(const QString& state)
{
    transportState_ = state;
    // Transport state is ADVISORY (pre-call "active" quirk, decision §6.4):
    // trusted only (a) as the settle-window accept signal when SCO is
    // unobservable, (b) as an end signal while Active with SCO down.
    if (state == QLatin1String("active") && inSettle_) {
        inSettle_ = false;
        settleTimer_.stop();
        setCallStateInternal(ICallStateProvider::Active);
    } else if (state == QLatin1String("idle")
               && callState_ == ICallStateProvider::Active && !scoRunning_) {
        scoDebounceTimer_.stop();
        callerNumber_.clear();
        callerName_.clear();
        setCallStateInternal(ICallStateProvider::Idle);
    }
}

void PhoneStateService::onTelephonyAvailable(bool available)
{
    if (available == telephonyAvailable_) return;
    telephonyAvailable_ = available;
    if (!available) {
        inSettle_ = false;
        settleTimer_.stop();
        scoDebounceTimer_.stop();
        callerNumber_.clear();
        callerName_.clear();
        setCallStateInternal(ICallStateProvider::Idle);
    }
    emit telephonyAvailableChanged();
}
```

One more change — `testActiveEndsOnTransportIdle` covers a Call1 `State→active` where SCO is never observed; `onCallSetupChanged("active")` handles it. No change to `setCallStateInternal` besides what exists (the incoming-call notification block already keys on `Ringing`).

- [ ] **Step 7: Config keys**

In `src/core/YamlConfig.cpp` `initDefaults()`, after the `root_["touch"]["device"] = "";` line:
```cpp
    root_["phone"]["reject_sco_during_aa"] = false;  // design §6: flip only after live check L4
    root_["phone"]["settle_grace_ms"] = 2000;
```
In `tests/test_config_key_coverage.cpp`, add to `testAllRuntimeKeys()`:
```cpp
    QVERIFY(svc.value("phone.reject_sco_during_aa").isValid());
    QVERIFY(svc.value("phone.settle_grace_ms").isValid());
```

- [ ] **Step 8: Run tests**

Run: `cd build && cmake .. && make -j$(nproc) && ctest -R "test_phone_state_service|test_config" --output-on-failure`
Expected: PASS — all existing + 12 new cases.

- [ ] **Step 9: Full suite**

Run: `ctest --output-on-failure`
Expected: PASS. (Watch for compile breaks in anything using `answer()`/`hangup()` — the return type changed. `grep -rn "->answer()\|->hangup()\|\.answer()\|\.hangup()" src/ tests/ qml/` and confirm only PhoneStateService, its tests, and QML files appear; QML ignores return values.)

- [ ] **Step 10: Commit**

```bash
git add src/core/services/ICallStateProvider.hpp src/core/services/IPhoneStateService.hpp \
        src/core/services/PhoneStateService.hpp src/core/services/PhoneStateService.cpp \
        src/core/YamlConfig.cpp tests/test_phone_state_service.cpp tests/test_config_key_coverage.cpp
git commit -m "feat(phone): real HFP call state machine in PhoneStateService

Widens ICallStateProvider (Dialing/Alerting/Held/Waiting, bool commands),
adds dial/sendDtmf/telephonyAvailable to IPhoneStateService, implements the
design §5 state machine over TelephonyClient + ScoNodeMonitor events.
Mock mode (no client attached) preserved for tests and the dev VM."
```

---

### Task 5: `CallAudioPolicy` — RejectSCO from projection state

**Files:**
- Create: `src/core/services/CallAudioPolicy.hpp`
- Create: `src/core/services/CallAudioPolicy.cpp`
- Modify: `src/CMakeLists.txt` (SOURCES)
- Test: `tests/test_call_audio_policy.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `IProjectionStatusProvider` (`projectionState()`, values `Disconnected=0..Backgrounded=4`, signal `projectionStateChanged`).
- Produces (used by Task 6):
```cpp
namespace oap {
class CallAudioPolicy : public QObject {
    CallAudioPolicy(IProjectionStatusProvider* projection, bool rejectScoDuringAa,
                    QObject* parent = nullptr);
    bool wantReject() const;
signals:
    void rejectScoWanted(bool reject);  // emitted on every change
};
}
```

- [ ] **Step 1: Write the failing test**

`tests/test_call_audio_policy.cpp`:
```cpp
#include <QtTest/QtTest>
#include "core/services/CallAudioPolicy.hpp"
#include "core/services/IProjectionStatusProvider.hpp"

class FakeProjection : public oap::IProjectionStatusProvider {
    Q_OBJECT
public:
    using oap::IProjectionStatusProvider::IProjectionStatusProvider;
    int projectionState() const override { return state_; }
    QString statusMessage() const override { return {}; }
    void setState(int s) { state_ = s; emit projectionStateChanged(); }
private:
    int state_ = 0;
};

class TestCallAudioPolicy : public QObject {
    Q_OBJECT
private slots:
    void testDisabledNeverRejects();
    void testEnabledFollowsProjection();
    void testNullProviderNeverRejects();
};

void TestCallAudioPolicy::testDisabledNeverRejects() {
    FakeProjection proj;
    oap::CallAudioPolicy p(&proj, /*rejectScoDuringAa=*/false);
    QSignalSpy spy(&p, &oap::CallAudioPolicy::rejectScoWanted);
    QVERIFY(!p.wantReject());
    proj.setState(oap::IProjectionStatusProvider::Connected);
    QVERIFY(!p.wantReject());
    QCOMPARE(spy.count(), 0);
}

void TestCallAudioPolicy::testEnabledFollowsProjection() {
    FakeProjection proj;
    oap::CallAudioPolicy p(&proj, true);
    QSignalSpy spy(&p, &oap::CallAudioPolicy::rejectScoWanted);
    QVERIFY(!p.wantReject());                                     // Disconnected
    proj.setState(oap::IProjectionStatusProvider::Connecting);
    QVERIFY(!p.wantReject());                                     // not yet a session
    proj.setState(oap::IProjectionStatusProvider::Connected);
    QVERIFY(p.wantReject());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toBool(), true);
    proj.setState(oap::IProjectionStatusProvider::Backgrounded);  // still a session
    QVERIFY(p.wantReject());
    QCOMPARE(spy.count(), 1);                                     // no re-emit, unchanged
    proj.setState(oap::IProjectionStatusProvider::Disconnected);
    QVERIFY(!p.wantReject());
    QCOMPARE(spy.count(), 2);
}

void TestCallAudioPolicy::testNullProviderNeverRejects() {
    oap::CallAudioPolicy p(nullptr, true);
    QVERIFY(!p.wantReject());
}

QTEST_MAIN(TestCallAudioPolicy)
#include "test_call_audio_policy.moc"
```

- [ ] **Step 2: Verify compile failure**, then **Step 3: implement**

`CallAudioPolicy.hpp`:
```cpp
#pragma once

#include <QObject>

namespace oap {

class IProjectionStatusProvider;

/// Decides when HFP SCO audio should be rejected so the phone keeps call
/// audio during an Android Auto session (design §6). Mechanism (the actual
/// RejectSCO D-Bus write) lives in TelephonyClient — main.cpp connects
/// rejectScoWanted → TelephonyClient::setRejectSco.
///
/// Default policy is OFF (HFP owns call audio always, like commercial head
/// units) until live check L4 justifies flipping phone.reject_sco_during_aa.
class CallAudioPolicy : public QObject {
    Q_OBJECT
public:
    explicit CallAudioPolicy(IProjectionStatusProvider* projection,
                             bool rejectScoDuringAa, QObject* parent = nullptr);

    bool wantReject() const { return wantReject_; }

signals:
    void rejectScoWanted(bool reject);

private:
    void recompute();

    IProjectionStatusProvider* projection_;
    bool enabled_;
    bool wantReject_ = false;
};

} // namespace oap
```

`CallAudioPolicy.cpp`:
```cpp
#include "CallAudioPolicy.hpp"
#include "IProjectionStatusProvider.hpp"

namespace oap {

CallAudioPolicy::CallAudioPolicy(IProjectionStatusProvider* projection,
                                 bool rejectScoDuringAa, QObject* parent)
    : QObject(parent), projection_(projection), enabled_(rejectScoDuringAa)
{
    if (projection_) {
        connect(projection_, &IProjectionStatusProvider::projectionStateChanged,
                this, &CallAudioPolicy::recompute);
        recompute();
    }
}

void CallAudioPolicy::recompute()
{
    const int s = projection_ ? projection_->projectionState() : 0;
    const bool sessionUp = (s == IProjectionStatusProvider::Connected
                            || s == IProjectionStatusProvider::Backgrounded);
    const bool want = enabled_ && sessionUp;
    if (want == wantReject_) return;
    wantReject_ = want;
    emit rejectScoWanted(want);
}

} // namespace oap
```

- [ ] **Step 4: Register in CMake, run tests**

`src/CMakeLists.txt`: add `core/services/CallAudioPolicy.cpp`. `tests/CMakeLists.txt`: add `oap_add_test(test_call_audio_policy SOURCES test_call_audio_policy.cpp)`.
Run: `cd build && cmake .. && make -j$(nproc) && ctest -R test_call_audio_policy --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/services/CallAudioPolicy.hpp src/core/services/CallAudioPolicy.cpp \
        src/CMakeLists.txt tests/test_call_audio_policy.cpp tests/CMakeLists.txt
git commit -m "feat(phone): CallAudioPolicy — RejectSCO follows AA session state"
```

---

### Task 6: main.cpp wiring

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: everything from Tasks 2–5.
- Produces: running integration; nothing new for later tasks.

- [ ] **Step 1: Add includes** (near the existing `#include "core/services/PhoneStateService.hpp"`, line ~36):
```cpp
#include "core/services/TelephonyClient.hpp"
#include "core/services/CallAudioPolicy.hpp"
#include "core/audio/ScoNodeMonitor.hpp"
```

- [ ] **Step 2: Wire the telephony stack**

At the phone-service block (`main.cpp:363`, `// --- Core phone state service ...`), replace:
```cpp
    auto phoneStateService = new oap::PhoneStateService(&app);
    phoneStateService->setNotificationService(notificationService);
    phoneStateService->startDBusMonitoring();
    hostContext->setCallStateProvider(phoneStateService);
```
with:
```cpp
    auto phoneStateService = new oap::PhoneStateService(&app);
    phoneStateService->setNotificationService(notificationService);
    phoneStateService->setSettleGraceMs(
        yamlConfig->valueByPath("phone.settle_grace_ms").toInt());
    phoneStateService->startDBusMonitoring();
    hostContext->setCallStateProvider(phoneStateService);

    // PipeWire telephony (org.pipewire.Telephony, session bus): real HFP
    // call control. See docs/superpowers/specs/2026-07-05-hfp-call-audio-design.md
    auto telephonyClient = new oap::TelephonyClient(&app);
    auto scoMonitor = new oap::ScoNodeMonitor(&app);
    if (audioService->isAvailable())
        scoMonitor->start(audioService->pwThreadLoop(), audioService->pwCore());
    phoneStateService->attachTelephony(telephonyClient);
    phoneStateService->attachScoMonitor(scoMonitor);
    telephonyClient->start();
```

- [ ] **Step 3: Wire the policy**

After the projection-provider block (`main.cpp:474-477`, `projectionStatusProvider = new oap::ProjectionStatusProvider(orch, &app);` inside its `if`), add immediately AFTER that if-block (so it runs whether or not the provider exists):
```cpp
    // AA-coexistence policy for call audio (design §6). With no projection
    // provider the policy is inert and SCO is always accepted.
    auto callAudioPolicy = new oap::CallAudioPolicy(
        projectionStatusProvider,
        yamlConfig->valueByPath("phone.reject_sco_during_aa").toBool(), &app);
    QObject::connect(callAudioPolicy, &oap::CallAudioPolicy::rejectScoWanted,
                     telephonyClient, &oap::TelephonyClient::setRejectSco);
    telephonyClient->setRejectSco(callAudioPolicy->wantReject());
```
Note: `telephonyClient` is declared at the Step 2 site — both sites are in `main()`'s scope; if the projection block sits in a nested scope, hoist nothing — the Step 2 declarations are already at function scope above line 474.

- [ ] **Step 4: Build + full suite**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: clean build, all tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "feat(phone): wire TelephonyClient, ScoNodeMonitor, CallAudioPolicy in main"
```

---

### Task 7: PhonePlugin becomes a pure view + QML fixes

Deletes the plugin's duplicate BlueZ monitoring; binds everything to `IPhoneStateService`. Fixes the role-inverted header comment and the `IncomingCallOverlay` enum bug (design §2.2–2.3, §4.6).

**Files:**
- Modify: `src/plugins/phone/PhonePlugin.hpp` (rewrite)
- Modify: `src/plugins/phone/PhonePlugin.cpp` (rewrite)
- Modify: `qml/applications/phone/IncomingCallOverlay.qml:6,11`

**Interfaces:**
- Consumes: `IPhoneStateService` via `IHostContext::callStateProvider()` (qobject_cast).
- Produces: unchanged QML API — context property `PhonePlugin` with the same property names and enum values (`Idle=0, Dialing=1, Ringing=2, Active=3, HeldActive=4, Ended=5`); `qml/applications/phone/PhoneView.qml` keeps working untouched.

- [ ] **Step 1: Rewrite `PhonePlugin.hpp`**

```cpp
#pragma once

#include "core/plugin/IPlugin.hpp"
#include <QObject>
#include <QString>
#include <QTimer>

class QQmlContext;

namespace oap {

class IHostContext;
class IPhoneStateService;

namespace plugins {

/// Bluetooth HFP (Hands-Free Profile) phone plugin — UI layer only.
///
/// Role: the Pi is the HFP Hands-Free (HF) unit; the PHONE is the Audio
/// Gateway. Call control and state come from the core PhoneStateService
/// (PipeWire org.pipewire.Telephony backend); SCO audio is routed by
/// PipeWire/WirePlumber natively. This plugin holds no D-Bus code.
///
/// Provides:
///   - Dialer UI (number pad, call/hangup, DTMF passthrough)
///   - A UI-facing CallState (adds a transient "Ended" flash state)
class PhonePlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)

    Q_PROPERTY(int callState READ callState NOTIFY callStateChanged)
    Q_PROPERTY(QString callerNumber READ callerNumber NOTIFY callInfoChanged)
    Q_PROPERTY(QString callerName READ callerName NOTIFY callInfoChanged)
    Q_PROPERTY(QString dialedNumber READ dialedNumber NOTIFY dialedNumberChanged)
    Q_PROPERTY(int callDuration READ callDuration NOTIFY callDurationChanged)
    Q_PROPERTY(bool phoneConnected READ phoneConnected NOTIFY connectionChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY connectionChanged)

public:
    /// UI-facing states. Values are frozen — PhoneView.qml compares ints.
    enum CallState {
        Idle = 0,
        Dialing,     // outgoing (provider Dialing or Alerting)
        Ringing,     // incoming
        Active,
        HeldActive,  // provider Held/Waiting (unproducible in v1)
        Ended        // 1.5 s flash after a call ends
    };
    Q_ENUM(CallState)

    explicit PhonePlugin(QObject* parent = nullptr);
    ~PhonePlugin() override;

    // IPlugin — Identity
    QString id() const override { return QStringLiteral("org.openauto.phone"); }
    QString name() const override { return QStringLiteral("Phone"); }
    QString version() const override { return QStringLiteral("1.0.0"); }
    int apiVersion() const override { return 1; }

    // IPlugin — Lifecycle
    bool initialize(IHostContext* context) override;
    void shutdown() override;

    // IPlugin — Activation
    void onActivated(QQmlContext* context) override;
    void onDeactivated() override;

    // IPlugin — UI
    QUrl qmlComponent() const override;
    QUrl iconSource() const override;
    QString iconText() const override { return QString(QChar(0xe61d)); }  // phone_in_talk
    QUrl settingsComponent() const override { return {}; }

    // IPlugin — Capabilities
    QStringList requiredServices() const override { return {}; }
    bool wantsFullscreen() const override { return false; }

    // Properties
    int callState() const { return callState_; }
    QString callerNumber() const;
    QString callerName() const;
    QString dialedNumber() const { return dialedNumber_; }
    int callDuration() const;
    bool phoneConnected() const;
    QString deviceName() const;

    // Call controls (invokable from QML) — all forwarded to the service
    Q_INVOKABLE void dial(const QString& number);
    Q_INVOKABLE void answer();
    Q_INVOKABLE void hangup();
    Q_INVOKABLE void appendDigit(const QString& digit);
    Q_INVOKABLE void clearDialed();
    Q_INVOKABLE void sendDTMF(const QString& tone);

signals:
    void callStateChanged();
    void callInfoChanged();
    void dialedNumberChanged();
    void callDurationChanged();
    void connectionChanged();
    void incomingCall(const QString& number, const QString& name);

private:
    void onProviderStateChanged();
    void setUiState(CallState state);

    IHostContext* hostContext_ = nullptr;
    IPhoneStateService* service_ = nullptr;
    QTimer* endedFlashTimer_ = nullptr;
    CallState callState_ = Idle;
    QString dialedNumber_;
};

} // namespace plugins
} // namespace oap
```

- [ ] **Step 2: Rewrite `PhonePlugin.cpp`**

```cpp
#include "PhonePlugin.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/services/IPhoneStateService.hpp"
#include <QQmlContext>
#include <QTimer>

namespace oap {
namespace plugins {

PhonePlugin::PhonePlugin(QObject* parent)
    : QObject(parent)
{
}

PhonePlugin::~PhonePlugin()
{
    shutdown();
}

bool PhonePlugin::initialize(IHostContext* context)
{
    hostContext_ = context;

    endedFlashTimer_ = new QTimer(this);
    endedFlashTimer_->setSingleShot(true);
    endedFlashTimer_->setInterval(1500);
    connect(endedFlashTimer_, &QTimer::timeout, this, [this]() {
        if (callState_ == Ended)
            setUiState(Idle);
    });

    service_ = qobject_cast<IPhoneStateService*>(
        context ? context->callStateProvider() : nullptr);
    if (service_) {
        connect(service_, &ICallStateProvider::callStateChanged,
                this, &PhonePlugin::onProviderStateChanged);
        connect(service_, &IPhoneStateService::callDurationChanged,
                this, &PhonePlugin::callDurationChanged);
        connect(service_, &IPhoneStateService::connectionChanged,
                this, &PhonePlugin::connectionChanged);
        onProviderStateChanged();
    } else if (hostContext_) {
        hostContext_->log(LogLevel::Warning,
            QStringLiteral("Phone: no IPhoneStateService — plugin inert"));
    }

    if (hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Phone plugin initialized"));
    return true;
}

void PhonePlugin::shutdown()
{
    if (endedFlashTimer_) {
        endedFlashTimer_->stop();
        endedFlashTimer_ = nullptr;
    }
    service_ = nullptr;
}

void PhonePlugin::onProviderStateChanged()
{
    const int ps = service_->callState();
    CallState mapped = Idle;
    switch (ps) {
    case ICallStateProvider::Ringing:  mapped = Ringing; break;
    case ICallStateProvider::Active:   mapped = Active; break;
    case ICallStateProvider::Dialing:
    case ICallStateProvider::Alerting: mapped = Dialing; break;
    case ICallStateProvider::Held:
    case ICallStateProvider::Waiting:  mapped = HeldActive; break;
    default:                           mapped = Idle; break;
    }

    if (mapped == Ringing && callState_ != Ringing)
        emit incomingCall(service_->callerNumber(), service_->callerName());

    if (mapped == Idle
        && (callState_ == Active || callState_ == Dialing || callState_ == Ringing)) {
        setUiState(Ended);                 // brief "Call ended" flash
        endedFlashTimer_->start();
    } else {
        setUiState(mapped);
    }
    emit callInfoChanged();
}

void PhonePlugin::setUiState(CallState state)
{
    if (state == callState_) return;
    callState_ = state;
    emit callStateChanged();
}

QString PhonePlugin::callerNumber() const { return service_ ? service_->callerNumber() : QString(); }
QString PhonePlugin::callerName() const { return service_ ? service_->callerName() : QString(); }
int PhonePlugin::callDuration() const { return service_ ? service_->callDuration() : 0; }
bool PhonePlugin::phoneConnected() const { return service_ && service_->phoneConnected(); }
QString PhonePlugin::deviceName() const { return service_ ? service_->deviceName() : QString(); }

void PhonePlugin::dial(const QString& number)
{
    if (service_ && service_->dial(number) && hostContext_)
        hostContext_->log(LogLevel::Info, QStringLiteral("Phone: Dialing %1").arg(number));
}

void PhonePlugin::answer()
{
    if (service_) service_->answer();
}

void PhonePlugin::hangup()
{
    if (service_) service_->hangup();
}

void PhonePlugin::appendDigit(const QString& digit)
{
    if (callState_ == Active) {
        sendDTMF(digit);
    } else {
        dialedNumber_ += digit;
        emit dialedNumberChanged();
    }
}

void PhonePlugin::clearDialed()
{
    if (dialedNumber_.isEmpty()) return;
    dialedNumber_.chop(1);
    emit dialedNumberChanged();
}

void PhonePlugin::sendDTMF(const QString& tone)
{
    if (service_) service_->sendDtmf(tone);
}

void PhonePlugin::onActivated(QQmlContext* context)
{
    if (!context) return;
    context->setContextProperty("PhonePlugin", this);
}

void PhonePlugin::onDeactivated()
{
    // Child context destroyed by PluginRuntimeContext
}

QUrl PhonePlugin::qmlComponent() const
{
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/PhoneView.qml"));
}

QUrl PhonePlugin::iconSource() const
{
    return {};  // Font-based icons — see MaterialIcon.qml ( phone)
}

} // namespace plugins
} // namespace oap
```
Note: the incoming-call NOTIFICATION is now posted solely by `PhoneStateService` (it already does; the plugin's duplicate posting is gone with the rewrite).

- [ ] **Step 3: Fix `IncomingCallOverlay.qml`**

Line 6, replace the comment:
```
/// Shell.qml instantiates this; visible while CallStateProvider.callState === 1 (Ringing).
```
Line 11:
```qml
    visible: CallStateProvider && CallStateProvider.callState === 1
```
(`ICallStateProvider`: `Idle=0, Ringing=1, Active=2` — the old `=== 2` triggered on Active; design §2.3.)

- [ ] **Step 4: Sanity-grep the QML for stale comparisons**

Run: `grep -rn "callState ===\|callState ==" qml/`
Expected: `PhoneView.qml` comparisons against `PhonePlugin` values 1/2/3/5 (unchanged enum — correct); `IncomingCallOverlay.qml` now `=== 1`. Anything else comparing `CallStateProvider.callState` must use provider numbering — fix on sight.

- [ ] **Step 5: Build + full suite**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/plugins/phone/PhonePlugin.hpp src/plugins/phone/PhonePlugin.cpp \
        qml/applications/phone/IncomingCallOverlay.qml
git commit -m "refactor(phone): PhonePlugin is a pure view over IPhoneStateService

Deletes the plugin's duplicate BlueZ monitoring and mock call logic, fixes
the role-inverted header comment (Pi is HF, phone is AG), and fixes
IncomingCallOverlay triggering on Active instead of Ringing (=== 2 → === 1)."
```

---

### Task 8: External API integration (conditional — depends on whether API v1 has been implemented)

The frozen contract: capability flags and command results must never contradict (`proto/api/phone.proto` header). This task makes them truthful.

**Files (only if they exist — API plan Tasks 7/11):**
- Modify: `src/core/api/ApiSerializers.cpp`
- Modify: `src/core/api/ApiRequestHandlers.cpp`
- Modify: `tests/test_api_serializers.cpp`, `tests/test_api_request_handlers.cpp`

- [ ] **Step 1: Check whether the API implementation exists**

Run: `ls src/core/api/ApiSerializers.cpp src/core/api/ApiRequestHandlers.cpp 2>/dev/null`
- **If missing:** the API executor runs after you. Append this note to `docs/session-handoffs.md` and SKIP to Task 9:
  > HFP D2 landed before API v1. When implementing API plan Tasks 7/11: PhoneStatus serialization and phoneCommand() must use the widened provider — see D2 plan Task 8 for the exact mapping code and tests; capabilities come from `IPhoneStateService::telephonyAvailable()`; `can_hold_swap`/`can_multiparty` stay false.
- **If present:** continue.

- [ ] **Step 2: Serializer — truthful capabilities and full state mapping**

In `ApiSerializers.cpp`, the phone-domain serializer's state mapping becomes:
```cpp
static prodigy::api::v1::CallState mapCallState(int s)
{
    using PS = oap::ICallStateProvider;
    switch (s) {
    case PS::Ringing:  return prodigy::api::v1::CALL_STATE_INCOMING;
    case PS::Dialing:  return prodigy::api::v1::CALL_STATE_DIALING;
    case PS::Alerting: return prodigy::api::v1::CALL_STATE_ALERTING;
    case PS::Active:   return prodigy::api::v1::CALL_STATE_ACTIVE;
    case PS::Held:     return prodigy::api::v1::CALL_STATE_HELD;
    case PS::Waiting:  return prodigy::api::v1::CALL_STATE_WAITING;
    default:           return prodigy::api::v1::CALL_STATE_UNSPECIFIED;
    }
}
```
And the capabilities block:
```cpp
    const bool avail = phone->telephonyAvailable();
    auto* caps = status.mutable_capabilities();
    caps->set_can_dial(avail);
    caps->set_can_answer(avail);
    caps->set_can_hangup(avail);
    caps->set_can_send_dtmf(avail);
    caps->set_can_hold_swap(false);    // hard-false in v1 — frozen contract
    caps->set_can_multiparty(false);   // hard-false in v1 — frozen contract
```
`calls[]` gets one element while `callState() != Idle` (Idle → empty list), with `started_at_unix_ms` per the existing §8.4 rule (computed on the transition into ACTIVE). The publisher must also connect `telephonyAvailableChanged` to the phone-status publish debounce (capability changes are status changes).

- [ ] **Step 3: `phoneCommand()` — swap the unconditional-UNAVAILABLE body**

```cpp
// Capability flag false → UNAVAILABLE (never contradicts the snapshot).
// Guard-rejected dispatch (wrong call state) → FAILED. Dispatched → OK.
PhoneCommandResult ApiRequestHandlers::phoneCommand(PhoneOp op, const QString& arg,
                                                    QString* detail)
{
    if (!deps_.phone || !deps_.phone->telephonyAvailable()) {
        *detail = QStringLiteral("telephony unavailable");
        return PHONE_COMMAND_RESULT_UNAVAILABLE;
    }
    bool dispatched = false;
    switch (op) {
    case PhoneOp::Dial:     dispatched = deps_.phone->dial(arg); break;
    case PhoneOp::Answer:   dispatched = deps_.phone->answer(); break;
    case PhoneOp::Hangup:   dispatched = deps_.phone->hangup(); break;
    case PhoneOp::SendDtmf: dispatched = deps_.phone->sendDtmf(arg); break;
    }
    if (!dispatched) {
        *detail = QStringLiteral("rejected in current call state");
        return PHONE_COMMAND_RESULT_FAILED;
    }
    return PHONE_COMMAND_RESULT_OK;
}
```
(Adapt names to the actual helper signature the API executor produced — the contract, not the spelling, is normative.)

- [ ] **Step 4: Update the API tests**

`testAllPhoneCommandsUnavailable` still holds with a default `PhoneStateService` (telephony never available in tests). Add:
```cpp
void TestApiRequestHandlers::testPhoneCommandsFollowCapability() {
    // ... standard handler setup with a PhoneStateService* phone ...
    phone->onTelephonyAvailable(true);          // mock mode + capability on
    // Dial from Idle → OK (mock dispatch succeeds)
    // Answer with no ring → FAILED (guard rejects, capability true)
    // Verify PhoneStatus snapshot now carries can_dial=true, can_hold_swap=false
}
```
Write it fully against the handler test file's existing fixtures (they exist if this task is live).

- [ ] **Step 5: Run API tests + full suite, commit**

Run: `cd build && cmake .. && make -j$(nproc) && ctest -R "test_api" --output-on-failure && ctest --output-on-failure`
```bash
git add src/core/api/ tests/
git commit -m "feat(api): truthful phone capabilities and command dispatch from real provider"
```

---

### Task 9: Final verification sweep + handoff

**Files:**
- Modify: `docs/session-handoffs.md`

- [ ] **Step 1: Full local gate**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: 100% pass. Then from repo root: `./cross-build.sh`
Expected: `build-pi/src/openauto-prodigy` produced without errors.

- [ ] **Step 2: Deploy to Pi (if reachable)**

```bash
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/
git push && ssh matt@192.168.1.152 'cd ~/openauto-prodigy && git pull && sudo systemctl restart openauto-prodigy.service'
```
Then smoke-check the journal: `ssh matt@192.168.1.152 'journalctl -u openauto-prodigy -n 50 --no-pager | grep -i "telephony\|sco\|phone"'`
Expected: "org.pipewire.Telephony is up" (if WirePlumber is running) and NO "UUID already registered" errors (Task 1 removed their source).

- [ ] **Step 3: Live checklist**

Run design doc §11 checks **L1–L6** (self-contained there — phone required for L2–L6). Record results inline in the design doc or `docs/session-handoffs.md`. L4's verdict decides whether `phone.reject_sco_during_aa` default flips (edit `YamlConfig.cpp` + coverage test if so, separate commit).

- [ ] **Step 4: Session handoff entry**

Append to `docs/session-handoffs.md`: tasks completed, deviations from the design doc (should be none — if any, list them explicitly), live-check results or "pending, phone unavailable", and the Task 8 branch taken (applied vs. deferred-to-API-executor).

- [ ] **Step 5: Commit**

```bash
git add docs/session-handoffs.md docs/superpowers/specs/2026-07-05-hfp-call-audio-design.md
git commit -m "docs: HFP call audio execution handoff + live check results"
```

---

## Self-review notes (spec → plan coverage)

- Design §3 (SCO routing = platform-native): no routing task exists — that IS the implementation. §3's limitations table needs no code.
- Design §4.1–4.6 → Tasks 2, 3, 4, 5, 7. §5 table → Task 4 Step 6 (every row has a test in Step 1). §6 → Tasks 5, 6 (+ L4 gate in Task 9). §7 → logging only (Task 2 `codecChanged` handler logs). §8 → Tasks 1, 7. §9 → Task 4 Step 7. §10 → distributed test steps. §11 → Task 9 Step 3. §4.4 API seam → Task 8.
- Type consistency: `TelephonyClient` signal names in Task 2 == connects in Task 4 `attachTelephony`; `ScoNodeMonitor::scoRunningChanged` == Task 4 `attachScoMonitor`; `CallAudioPolicy::rejectScoWanted` == Task 6 connect; enum spellings match `ICallStateProvider` throughout.

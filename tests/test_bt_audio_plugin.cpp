// tests/test_bt_audio_plugin.cpp
#include <QtTest/QtTest>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <limits>
#include "core/plugin/IHostContext.hpp"
#include "core/services/MediaStatusService.hpp"
#include "plugins/bt_audio/BtAudioPlugin.hpp"

using oap::plugins::BtAudioPlugin;
using oap::plugins::BtInterfaceMap;
using oap::plugins::BtManagedObjectMap;

using TestManagedObjectMap = QMap<QDBusObjectPath, BtInterfaceMap>;
Q_DECLARE_METATYPE(TestManagedObjectMap)

class ObjectManagerFixture : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.DBus.ObjectManager")
public:
    enum class ReplyMode { Valid, MalformedSignature, Error };
    TestManagedObjectMap objects;
    int replyDelayMs = 500;
    int callCount = 0;
    ReplyMode replyMode = ReplyMode::Valid;

public slots:
    void GetManagedObjects() {
        ++callCount;
        setDelayedReply(true);
        const QDBusConnection replyBus = connection();
        QDBusMessage reply;
        if (replyMode == ReplyMode::MalformedSignature) {
            reply = message().createReply(QStringLiteral("not-managed-objects"));
        } else if (replyMode == ReplyMode::Error) {
            reply = message().createErrorReply(
                QStringLiteral("org.bluez.Error.Failed"),
                QStringLiteral("fixture failure"));
        } else {
            reply = message().createReply(QVariant::fromValue(objects));
        }
        QTimer::singleShot(replyDelayMs, this, [replyBus, reply]() {
            replyBus.send(reply);
        });
    }
};

class LoggingHostContext : public oap::IHostContext {
public:
    oap::IAudioService* audioService() override { return nullptr; }
    oap::IBluetoothService* bluetoothService() override { return nullptr; }
    oap::IConfigService* configService() override { return nullptr; }
    oap::IThemeService* themeService() override { return nullptr; }
    oap::IDisplayService* displayService() override { return nullptr; }
    oap::IEventBus* eventBus() override { return nullptr; }
    oap::ActionRegistry* actionRegistry() override { return nullptr; }
    oap::INotificationService* notificationService() override { return nullptr; }
    oap::IEqualizerService* equalizerService() override { return nullptr; }
    oap::IProjectionStatusProvider* projectionStatusProvider() override { return nullptr; }
    oap::INavigationProvider* navigationProvider() override { return nullptr; }
    oap::IMediaStatusProvider* mediaStatusProvider() override { return nullptr; }
    oap::ICallStateProvider* callStateProvider() override { return nullptr; }
    oap::OverlayService* overlayService() override { return nullptr; }
    void log(oap::LogLevel, const QString& message) override { messages.append(message); }

    QStringList messages;
};

class TrackMapFixture : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.openauto.TestTrack")
public slots:
    QVariantMap GetTrack() const {
        QVariantMap track;
        track.insert(QStringLiteral("Title"), QStringLiteral("D-Bus Track"));
        track.insert(QStringLiteral("Duration"), QVariant::fromValue(quint32(215000)));
        return track;
    }
};

// Regression coverage for the BlueZ D-Bus signal handlers. Before the fix the
// three handlers were plain private methods (not slots), so every string-based
// bus.connect() failed at startup and the plugin ran blind; InterfacesAdded was
// also typed QVariantMap, which QtDBus refuses to deliver for the a{sa{sv}}
// payload; and PropertiesChanged had no sender-path filter, so any BlueZ
// object's update stomped the tracked transport/player. These tests drive the
// handlers straight through the meta-object (proving they are invokable slots)
// and assert the sender-path guard.
class TestBtAudioPlugin : public QObject {
    Q_OBJECT
private slots:
    void propertiesChanged_wrongSenderPath_ignored();
    void propertiesChanged_matchingSender_applied();
    void interfacesAdded_slotInvokableWithRegisteredType();
    // transportActiveChanged edge (Task 6): interface presence is NOT audio
    // activity — the edge tracks MediaTransport1.State == "active" only.
    void transportActive_edgeFollowsState();
    void transportActive_removalForcesInactive();
    void transportActive_bluezLossForcesInactive();
    void transportActive_uiConnectedStateDecoupled();
    // Per-transport activity tracking: a second idle phone must not force the
    // aggregate edge false while the first is still playing.
    void transportActive_secondIdleDoesNotSilenceFirst();
    void transportActive_removeActiveKeepsOtherActive();
    void transportActive_bothIdleClearsEdge();
    void transportActive_secondPathFlipsActiveEvenIfArrivedFirst();
    void transportActive_bluezLossClearsAllTransports();
    // Finding B: onInterfacesAdded must honour the State carried in its own
    // payload instead of a racy synchronous read-back.
    void transportActive_payloadStateActivatesWithoutPropertiesChanged();
    void playerTimes_initialAdoptionPreservesMilliseconds();
    void playerTimes_propertiesChangedPreservesMilliseconds();
    void playerTimes_durationOnlyUpdateIsIndependent();
    void playerTimes_unchangedValuesDoNotNotify();
    void playerTimes_uint32RangeDoesNotWrap();
    void playerTimes_qdbusArgumentTrackIsDemarshaled();
    void playerTimes_missingAndInvalidValuesUseUnknowns();
    void playerTimes_invalidatedValuesUseUnknowns();
    void playerTimes_newPlayerClearsMissingState();
    void playerTimes_bluezLossClearsState();
    void startupEnumeration_isAsynchronousAndUsesCarriedProperties();
    void startupEnumeration_malformedAndErrorRetainStateButEmptyClears();
    void startupEnumeration_validFallbackSurvivesTrailingError();
    void startupEnumeration_bluezLossRejectsStaleReply();
    void hotplugMissingPropertiesRemainUnknownUntilDelivery();
    void deviceName_nameOnlyDeltaPreservesAlias();
    void deviceName_aliasInvalidationFallsBackToCachedName();
    void deviceName_removalAndBluezResetPurgeCache();
    void playerRemoval_resetsAllStateWithEdgeOnlySignals();
    void playerTimes_startupAndReconnectCatchupPreservesMilliseconds();
    void playerTimes_flowUnchangedToMediaStatusService();

private:
    // Seed the plugin's tracked player path through its adoption flow: an
    // InterfacesAdded carrying org.bluez.MediaPlayer1 sets playerPath_ (the
    // subsequent D-Bus property read-back fails harmlessly with no BlueZ on the
    // build box, but the path is recorded first).
    static bool adoptPlayer(BtAudioPlugin& plugin, const QString& path,
                            const QVariantMap& props = {}) {
        BtInterfaceMap ifaces;
        ifaces.insert(QStringLiteral("org.bluez.MediaPlayer1"), props);
        return QMetaObject::invokeMethod(
            &plugin, "onInterfacesAdded", Qt::DirectConnection,
            Q_ARG(QDBusObjectPath, QDBusObjectPath(path)),
            Q_ARG(BtInterfaceMap, ifaces));
    }

    // Seed the plugin's tracked transport path through its adoption flow: an
    // InterfacesAdded carrying org.bluez.MediaTransport1 sets transportPath_ (the
    // subsequent D-Bus State read-back fails harmlessly with no BlueZ on the
    // build box, so the audio-activity edge stays false until a State arrives).
    static bool adoptTransport(BtAudioPlugin& plugin, const QString& path) {
        BtInterfaceMap ifaces;
        ifaces.insert(QStringLiteral("org.bluez.MediaTransport1"), QVariantMap{});
        return QMetaObject::invokeMethod(
            &plugin, "onInterfacesAdded", Qt::DirectConnection,
            Q_ARG(QDBusObjectPath, QDBusObjectPath(path)),
            Q_ARG(BtInterfaceMap, ifaces));
    }

    // Remove a transport interface through onInterfacesRemoved (the seam BlueZ
    // fires when an A2DP transport disappears).
    static bool removeTransport(BtAudioPlugin& plugin, const QString& path) {
        return QMetaObject::invokeMethod(
            &plugin, "onInterfacesRemoved", Qt::DirectConnection,
            Q_ARG(QDBusObjectPath, QDBusObjectPath(path)),
            Q_ARG(QStringList, QStringList{ QStringLiteral("org.bluez.MediaTransport1") }));
    }

    // Deliver a MediaTransport1 PropertiesChanged carrying a new State value from
    // the tracked transport path — the seam that reaches updateTransportState()
    // without a live bus (same meta-object entry point the handlers use).
    static bool driveTransportState(BtAudioPlugin& plugin, const QString& transportPath,
                                    const QString& state) {
        QVariantMap changed;
        changed.insert(QStringLiteral("State"), state);
        QDBusMessage msg = propsSignal(transportPath);
        return QMetaObject::invokeMethod(
            &plugin, "onPropertiesChanged", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("org.bluez.MediaTransport1")),
            Q_ARG(QVariantMap, changed),
            Q_ARG(QStringList, QStringList{}),
            Q_ARG(QDBusMessage, msg));
    }

    // A PropertiesChanged `changed` map with a fresh Track title so a delivered
    // update produces exactly one metadataChanged emission.
    static QVariantMap trackChange(const QString& title) {
        QVariantMap track;
        track.insert(QStringLiteral("Title"), title);
        QVariantMap changed;
        changed.insert(QStringLiteral("Track"), track);
        return changed;
    }

    static QVariantMap playerProperties(const QString& title, quint32 durationMs,
                                        quint32 positionMs) {
        QVariantMap track;
        track.insert(QStringLiteral("Title"), title);
        track.insert(QStringLiteral("Artist"), QStringLiteral("Artist"));
        track.insert(QStringLiteral("Album"), QStringLiteral("Album"));
        track.insert(QStringLiteral("Duration"), QVariant::fromValue(durationMs));

        QVariantMap props;
        props.insert(QStringLiteral("Track"), track);
        props.insert(QStringLiteral("Position"), QVariant::fromValue(positionMs));
        return props;
    }

    static bool drivePlayerProperties(BtAudioPlugin& plugin, const QString& playerPath,
                                      const QVariantMap& changed,
                                      const QStringList& invalidated = {}) {
        QDBusMessage msg = propsSignal(playerPath);
        return QMetaObject::invokeMethod(
            &plugin, "onPropertiesChanged", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("org.bluez.MediaPlayer1")),
            Q_ARG(QVariantMap, changed),
            Q_ARG(QStringList, invalidated),
            Q_ARG(QDBusMessage, msg));
    }

    static bool driveDeviceProperties(BtAudioPlugin& plugin, const QString& devicePath,
                                      const QVariantMap& changed,
                                      const QStringList& invalidated = {}) {
        QDBusMessage msg = propsSignal(devicePath);
        return QMetaObject::invokeMethod(
            &plugin, "onPropertiesChanged", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("org.bluez.Device1")),
            Q_ARG(QVariantMap, changed),
            Q_ARG(QStringList, invalidated),
            Q_ARG(QDBusMessage, msg));
    }

    static void seedNamedTransport(BtAudioPlugin& plugin, const QString& devicePath,
                                   const QString& transportPath, const QString& alias,
                                   const QString& name) {
        BtManagedObjectMap objects;
        objects[devicePath][QStringLiteral("org.bluez.Device1")] = {
            {QStringLiteral("Alias"), alias},
            {QStringLiteral("Name"), name},
        };
        objects[transportPath][QStringLiteral("org.bluez.MediaTransport1")] = {
            {QStringLiteral("Device"), QVariant::fromValue(QDBusObjectPath(devicePath))},
            {QStringLiteral("State"), QStringLiteral("idle")},
        };
        plugin.applyManagedObjectsSnapshot(objects);
    }

    // A PropertiesChanged signal message whose object path round-trips through
    // path() without a live bus (QDBusMessage::createSignal).
    static QDBusMessage propsSignal(const QString& senderPath) {
        return QDBusMessage::createSignal(
            senderPath, QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));
    }
};

void TestBtAudioPlugin::propertiesChanged_wrongSenderPath_ignored()
{
    BtAudioPlugin plugin;
    const QString playerPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    QVERIFY(adoptPlayer(plugin, playerPath));

    QSignalSpy spy(&plugin, &BtAudioPlugin::metadataChanged);

    // Update arrives for a DIFFERENT object than the tracked player.
    QVariantMap changed = trackChange(QStringLiteral("Ghost Track"));
    QDBusMessage msg = propsSignal(QStringLiteral("/org/bluez/hci0/dev_CC_DD/player9"));
    bool ok = QMetaObject::invokeMethod(
        &plugin, "onPropertiesChanged", Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("org.bluez.MediaPlayer1")),
        Q_ARG(QVariantMap, changed),
        Q_ARG(QStringList, QStringList{}),
        Q_ARG(QDBusMessage, msg));
    QVERIFY(ok);                       // slot ran (it chose to ignore, not fail to invoke)
    QCOMPARE(spy.count(), 0);          // foreign sender filtered out
    QVERIFY(plugin.trackTitle().isEmpty());
}

void TestBtAudioPlugin::propertiesChanged_matchingSender_applied()
{
    BtAudioPlugin plugin;
    const QString playerPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    QVERIFY(adoptPlayer(plugin, playerPath));

    QSignalSpy spy(&plugin, &BtAudioPlugin::metadataChanged);

    QVariantMap changed = trackChange(QStringLiteral("Real Track"));
    QDBusMessage msg = propsSignal(playerPath);   // sender == tracked player
    bool ok = QMetaObject::invokeMethod(
        &plugin, "onPropertiesChanged", Qt::DirectConnection,
        Q_ARG(QString, QStringLiteral("org.bluez.MediaPlayer1")),
        Q_ARG(QVariantMap, changed),
        Q_ARG(QStringList, QStringList{}),
        Q_ARG(QDBusMessage, msg));
    QVERIFY(ok);
    QCOMPARE(spy.count(), 1);          // matching sender applied
    QCOMPARE(plugin.trackTitle(), QStringLiteral("Real Track"));
}

void TestBtAudioPlugin::interfacesAdded_slotInvokableWithRegisteredType()
{
    BtAudioPlugin plugin;
    QSignalSpy spy(&plugin, &BtAudioPlugin::connectionStateChanged);

    BtInterfaceMap ifaces;
    ifaces.insert(QStringLiteral("org.bluez.MediaTransport1"), QVariantMap{});

    // Invoke through the meta-object with the registered map metatype — proves
    // the slot signature is both invokable and matched by BtInterfaceMap.
    bool ok = QMetaObject::invokeMethod(
        &plugin, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(QStringLiteral("/org/bluez/hci0/dev_AA_BB/fd0"))),
        Q_ARG(BtInterfaceMap, ifaces));
    QVERIFY(ok);                       // no crash, slot reached
    QCOMPARE(spy.count(), 1);          // transport adopted -> connection state changed
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
}

// The audio-activity edge fires exactly on active<->non-active transitions of
// the tracked transport's State, and never re-emits on an unchanged value.
void TestBtAudioPlugin::transportActive_edgeFollowsState()
{
    BtAudioPlugin plugin;
    const QString transportPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB/fd0");
    QVERIFY(adoptTransport(plugin, transportPath));

    QSignalSpy spy(&plugin, &BtAudioPlugin::transportActiveChanged);
    QVERIFY(!plugin.transportActive());               // starts inactive

    // idle -> no edge (already false)
    QVERIFY(driveTransportState(plugin, transportPath, QStringLiteral("idle")));
    QCOMPARE(spy.count(), 0);
    QVERIFY(!plugin.transportActive());

    // idle -> active -> exactly one true edge
    QVERIFY(driveTransportState(plugin, transportPath, QStringLiteral("active")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toBool(), true);
    QVERIFY(plugin.transportActive());

    // active -> active -> no re-emit on unchanged value
    QVERIFY(driveTransportState(plugin, transportPath, QStringLiteral("active")));
    QCOMPARE(spy.count(), 1);

    // active -> pending -> one false edge
    QVERIFY(driveTransportState(plugin, transportPath, QStringLiteral("pending")));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.last().at(0).toBool(), false);
    QVERIFY(!plugin.transportActive());
}

// Removal of the tracked transport interface forces the edge inactive.
void TestBtAudioPlugin::transportActive_removalForcesInactive()
{
    BtAudioPlugin plugin;
    const QString transportPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB/fd0");
    QVERIFY(adoptTransport(plugin, transportPath));
    QVERIFY(driveTransportState(plugin, transportPath, QStringLiteral("active")));
    QVERIFY(plugin.transportActive());

    QSignalSpy spy(&plugin, &BtAudioPlugin::transportActiveChanged);

    QStringList removed{ QStringLiteral("org.bluez.MediaTransport1") };
    bool ok = QMetaObject::invokeMethod(
        &plugin, "onInterfacesRemoved", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(transportPath)),
        Q_ARG(QStringList, removed));
    QVERIFY(ok);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toBool(), false);
    QVERIFY(!plugin.transportActive());
}

// BlueZ vanishing from the bus forces the edge inactive.
void TestBtAudioPlugin::transportActive_bluezLossForcesInactive()
{
    BtAudioPlugin plugin;
    const QString transportPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB/fd0");
    QVERIFY(adoptTransport(plugin, transportPath));
    QVERIFY(driveTransportState(plugin, transportPath, QStringLiteral("active")));
    QVERIFY(plugin.transportActive());

    QSignalSpy spy(&plugin, &BtAudioPlugin::transportActiveChanged);

    // Same handler QDBusServiceWatcher::serviceUnregistered fires into.
    bool ok = QMetaObject::invokeMethod(
        &plugin, "onBluezServiceUnregistered", Qt::DirectConnection);
    QVERIFY(ok);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toBool(), false);
    QVERIFY(!plugin.transportActive());
}

// The UI Connected/Disconnected mapping is preserved exactly: idle, pending and
// active all read as Connected, independent of the audio-activity edge.
void TestBtAudioPlugin::transportActive_uiConnectedStateDecoupled()
{
    BtAudioPlugin plugin;
    const QString transportPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB/fd0");
    QVERIFY(adoptTransport(plugin, transportPath));

    QVERIFY(driveTransportState(plugin, transportPath, QStringLiteral("idle")));
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
    QVERIFY(!plugin.transportActive());

    QVERIFY(driveTransportState(plugin, transportPath, QStringLiteral("pending")));
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
    QVERIFY(!plugin.transportActive());

    QVERIFY(driveTransportState(plugin, transportPath, QStringLiteral("active")));
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
    QVERIFY(plugin.transportActive());
}

// Two transports: A active, B idle arrives — the aggregate edge STAYS true (a
// second silent phone must not silence the one that is playing).
void TestBtAudioPlugin::transportActive_secondIdleDoesNotSilenceFirst()
{
    BtAudioPlugin plugin;
    const QString a = QStringLiteral("/org/bluez/hci0/dev_AA/fd0");
    const QString b = QStringLiteral("/org/bluez/hci0/dev_BB/fd0");

    QVERIFY(adoptTransport(plugin, a));
    QVERIFY(driveTransportState(plugin, a, QStringLiteral("active")));
    QVERIFY(plugin.transportActive());

    QSignalSpy spy(&plugin, &BtAudioPlugin::transportActiveChanged);

    // B arrives (idle) then explicitly reports idle — edge must stay true.
    QVERIFY(adoptTransport(plugin, b));
    QVERIFY(driveTransportState(plugin, b, QStringLiteral("idle")));
    QVERIFY(plugin.transportActive());
    QCOMPARE(spy.count(), 0);   // no false edge emitted
}

// A active + B active; removing A leaves B active — edge stays true.
void TestBtAudioPlugin::transportActive_removeActiveKeepsOtherActive()
{
    BtAudioPlugin plugin;
    const QString a = QStringLiteral("/org/bluez/hci0/dev_AA/fd0");
    const QString b = QStringLiteral("/org/bluez/hci0/dev_BB/fd0");

    QVERIFY(adoptTransport(plugin, a));
    QVERIFY(driveTransportState(plugin, a, QStringLiteral("active")));
    QVERIFY(adoptTransport(plugin, b));
    QVERIFY(driveTransportState(plugin, b, QStringLiteral("active")));
    QVERIFY(plugin.transportActive());

    QSignalSpy spy(&plugin, &BtAudioPlugin::transportActiveChanged);

    QVERIFY(removeTransport(plugin, a));
    QVERIFY(plugin.transportActive());          // B still active
    QCOMPARE(spy.count(), 0);                   // no edge change
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
}

// Both transports active, then both go idle — only when the LAST active one
// drops does the aggregate edge fall to false.
void TestBtAudioPlugin::transportActive_bothIdleClearsEdge()
{
    BtAudioPlugin plugin;
    const QString a = QStringLiteral("/org/bluez/hci0/dev_AA/fd0");
    const QString b = QStringLiteral("/org/bluez/hci0/dev_BB/fd0");

    QVERIFY(adoptTransport(plugin, a));
    QVERIFY(driveTransportState(plugin, a, QStringLiteral("active")));
    QVERIFY(adoptTransport(plugin, b));
    QVERIFY(driveTransportState(plugin, b, QStringLiteral("active")));
    QVERIFY(plugin.transportActive());

    QSignalSpy spy(&plugin, &BtAudioPlugin::transportActiveChanged);

    QVERIFY(driveTransportState(plugin, a, QStringLiteral("idle")));
    QVERIFY(plugin.transportActive());          // B still active — no edge
    QCOMPARE(spy.count(), 0);

    QVERIFY(driveTransportState(plugin, b, QStringLiteral("idle")));
    QVERIFY(!plugin.transportActive());         // both idle now — one false edge
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toBool(), false);
}

// B is adopted FIRST, then A (so transportPath_ == A, the most-recent). A
// State-active PropertiesChanged from B's sender path must still be honoured —
// the old single-path filter (sender != transportPath_) dropped it, silencing
// the actually-playing phone.
void TestBtAudioPlugin::transportActive_secondPathFlipsActiveEvenIfArrivedFirst()
{
    BtAudioPlugin plugin;
    const QString a = QStringLiteral("/org/bluez/hci0/dev_AA/fd0");
    const QString b = QStringLiteral("/org/bluez/hci0/dev_BB/fd0");

    QVERIFY(adoptTransport(plugin, b));   // B first
    QVERIFY(adoptTransport(plugin, a));   // A last => transportPath_ == A
    QVERIFY(!plugin.transportActive());

    QSignalSpy spy(&plugin, &BtAudioPlugin::transportActiveChanged);

    // B (NOT the most-recent path) goes active — must be accepted.
    QVERIFY(driveTransportState(plugin, b, QStringLiteral("active")));
    QVERIFY(plugin.transportActive());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toBool(), true);
}

// BlueZ vanishing clears ALL tracked transports and forces the edge false.
void TestBtAudioPlugin::transportActive_bluezLossClearsAllTransports()
{
    BtAudioPlugin plugin;
    const QString a = QStringLiteral("/org/bluez/hci0/dev_AA/fd0");
    const QString b = QStringLiteral("/org/bluez/hci0/dev_BB/fd0");

    QVERIFY(adoptTransport(plugin, a));
    QVERIFY(driveTransportState(plugin, a, QStringLiteral("active")));
    QVERIFY(adoptTransport(plugin, b));
    QVERIFY(driveTransportState(plugin, b, QStringLiteral("active")));
    QVERIFY(plugin.transportActive());

    QSignalSpy spy(&plugin, &BtAudioPlugin::transportActiveChanged);

    QVERIFY(QMetaObject::invokeMethod(&plugin, "onBluezServiceUnregistered",
                                      Qt::DirectConnection));
    QVERIFY(!plugin.transportActive());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toBool(), false);
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Disconnected));

    // With all transports cleared, a stale PropertiesChanged from A is ignored
    // (no longer tracked) and cannot resurrect the edge.
    QVERIFY(driveTransportState(plugin, a, QStringLiteral("active")));
    QVERIFY(!plugin.transportActive());
    QCOMPARE(spy.count(), 1);
}

// An InterfacesAdded whose payload already carries State: "active" must fire the
// audio-activity edge true immediately — no follow-up PropertiesChanged, and NO
// synchronous read-back (the build box has no BlueZ, so a read-back would leave
// State empty and the edge false, exactly the race Finding B closes).
void TestBtAudioPlugin::transportActive_payloadStateActivatesWithoutPropertiesChanged()
{
    BtAudioPlugin plugin;
    const QString transportPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB/fd0");

    QSignalSpy spy(&plugin, &BtAudioPlugin::transportActiveChanged);

    BtInterfaceMap ifaces;
    QVariantMap props;
    props.insert(QStringLiteral("State"), QStringLiteral("active"));
    ifaces.insert(QStringLiteral("org.bluez.MediaTransport1"), props);

    bool ok = QMetaObject::invokeMethod(
        &plugin, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(transportPath)),
        Q_ARG(BtInterfaceMap, ifaces));
    QVERIFY(ok);

    // Edge fired true straight from the payload — no PropertiesChanged delivered.
    QVERIFY(plugin.transportActive());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(0).toBool(), true);
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
}

void TestBtAudioPlugin::playerTimes_initialAdoptionPreservesMilliseconds()
{
    BtAudioPlugin plugin;
    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    QSignalSpy durationSpy(&plugin, &BtAudioPlugin::durationChanged);
    QSignalSpy positionSpy(&plugin, &BtAudioPlugin::positionChanged);
    QSignalSpy progressSpy(&plugin, &BtAudioPlugin::progressChanged);

    QVERIFY(adoptPlayer(plugin, path,
                        playerProperties(QStringLiteral("Track"), 215000u, 61000u)));

    QCOMPARE(plugin.trackDuration(), qint64(215000));
    QCOMPARE(plugin.trackPosition(), qint64(61000));
    QVERIFY(plugin.hasTrackPosition());
    QCOMPARE(durationSpy.count(), 1);
    QCOMPARE(positionSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(progressSpy.first().at(0).toLongLong(), qint64(61000));
    QCOMPARE(progressSpy.first().at(1).toLongLong(), qint64(215000));
}

void TestBtAudioPlugin::playerTimes_propertiesChangedPreservesMilliseconds()
{
    BtAudioPlugin plugin;
    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    QVERIFY(adoptPlayer(plugin, path,
                        playerProperties(QStringLiteral("Track"), 215000u, 61000u)));

    QSignalSpy progressSpy(&plugin, &BtAudioPlugin::progressChanged);
    QVERIFY(drivePlayerProperties(
        plugin, path, playerProperties(QStringLiteral("Next"), 245000u, 62000u)));

    QCOMPARE(plugin.trackDuration(), qint64(245000));
    QCOMPARE(plugin.trackPosition(), qint64(62000));
    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(progressSpy.first().at(0).toLongLong(), qint64(62000));
    QCOMPARE(progressSpy.first().at(1).toLongLong(), qint64(245000));
}

void TestBtAudioPlugin::playerTimes_durationOnlyUpdateIsIndependent()
{
    BtAudioPlugin plugin;
    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    const QVariantMap initial = playerProperties(QStringLiteral("Track"), 215000u, 61000u);
    QVERIFY(adoptPlayer(plugin, path, initial));

    QSignalSpy metadataSpy(&plugin, &BtAudioPlugin::metadataChanged);
    QSignalSpy durationSpy(&plugin, &BtAudioPlugin::durationChanged);
    QSignalSpy positionSpy(&plugin, &BtAudioPlugin::positionChanged);
    QSignalSpy progressSpy(&plugin, &BtAudioPlugin::progressChanged);

    QVariantMap track = initial.value(QStringLiteral("Track")).toMap();
    track.insert(QStringLiteral("Duration"), QVariant::fromValue(quint32(245000)));
    QVariantMap changed;
    changed.insert(QStringLiteral("Track"), track);
    QVERIFY(drivePlayerProperties(plugin, path, changed));

    QCOMPARE(plugin.trackDuration(), qint64(245000));
    QCOMPARE(plugin.trackPosition(), qint64(61000));
    QCOMPARE(metadataSpy.count(), 0);
    QCOMPARE(durationSpy.count(), 1);
    QCOMPARE(positionSpy.count(), 0);
    QCOMPARE(progressSpy.count(), 1);
}

void TestBtAudioPlugin::playerTimes_unchangedValuesDoNotNotify()
{
    BtAudioPlugin plugin;
    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    const QVariantMap props = playerProperties(QStringLiteral("Track"), 215000u, 61000u);
    QVERIFY(adoptPlayer(plugin, path, props));

    QSignalSpy metadataSpy(&plugin, &BtAudioPlugin::metadataChanged);
    QSignalSpy durationSpy(&plugin, &BtAudioPlugin::durationChanged);
    QSignalSpy positionSpy(&plugin, &BtAudioPlugin::positionChanged);
    QSignalSpy progressSpy(&plugin, &BtAudioPlugin::progressChanged);
    QVERIFY(drivePlayerProperties(plugin, path, props));

    QCOMPARE(metadataSpy.count(), 0);
    QCOMPARE(durationSpy.count(), 0);
    QCOMPARE(positionSpy.count(), 0);
    QCOMPARE(progressSpy.count(), 0);
}

void TestBtAudioPlugin::playerTimes_uint32RangeDoesNotWrap()
{
    BtAudioPlugin plugin;
    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    const quint32 aboveSignedMax = quint32(std::numeric_limits<qint32>::max()) + 123u;
    QVERIFY(adoptPlayer(
        plugin, path, playerProperties(QStringLiteral("Long"), aboveSignedMax,
                                       aboveSignedMax)));

    QCOMPARE(plugin.trackDuration(), qint64(aboveSignedMax));
    QCOMPARE(plugin.trackPosition(), qint64(aboveSignedMax));
}

void TestBtAudioPlugin::playerTimes_qdbusArgumentTrackIsDemarshaled()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY2(bus.isConnected(), "CTest must provide an isolated session D-Bus");

    TrackMapFixture fixture;
    const QString fixturePath = QStringLiteral("/org/openauto/TestTrack");
    QVERIFY(bus.registerObject(fixturePath, &fixture, QDBusConnection::ExportAllSlots));

    QDBusMessage call = QDBusMessage::createMethodCall(
        bus.baseService(), fixturePath, QStringLiteral("org.openauto.TestTrack"),
        QStringLiteral("GetTrack"));
    QDBusPendingCallWatcher watcher(bus.asyncCall(call));
    QSignalSpy finishedSpy(&watcher, &QDBusPendingCallWatcher::finished);
    if (!watcher.isFinished())
        QVERIFY(finishedSpy.wait(2000));
    const QDBusMessage reply = watcher.reply();
    bus.unregisterObject(fixturePath);

    QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
    QCOMPARE(reply.arguments().size(), 1);
    const QVariant trackArgument = reply.arguments().first();
    QVERIFY(trackArgument.canConvert<QDBusArgument>());

    BtAudioPlugin plugin;
    QVariantMap props;
    props.insert(QStringLiteral("Track"), trackArgument);
    props.insert(QStringLiteral("Position"), QVariant::fromValue(quint32(61000)));
    QVERIFY(adoptPlayer(plugin,
                        QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0"), props));
    QCOMPARE(plugin.trackTitle(), QStringLiteral("D-Bus Track"));
    QCOMPARE(plugin.trackDuration(), qint64(215000));
    QCOMPARE(plugin.trackPosition(), qint64(61000));
}

void TestBtAudioPlugin::playerTimes_missingAndInvalidValuesUseUnknowns()
{
    BtAudioPlugin plugin;
    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    QVERIFY(adoptPlayer(plugin, path,
                        playerProperties(QStringLiteral("Track"), 215000u, 61000u)));

    QVariantMap track;
    track.insert(QStringLiteral("Title"), QStringLiteral("Track"));
    track.insert(QStringLiteral("Artist"), QStringLiteral("Artist"));
    track.insert(QStringLiteral("Album"), QStringLiteral("Album"));
    QVariantMap changed;
    changed.insert(QStringLiteral("Track"), track);  // Duration missing => unknown/zero
    changed.insert(QStringLiteral("Position"), QStringLiteral("not-a-uint32"));
    QVERIFY(drivePlayerProperties(plugin, path, changed));

    QCOMPARE(plugin.trackDuration(), qint64(0));
    QCOMPARE(plugin.trackPosition(), qint64(0));
    QVERIFY(!plugin.hasTrackPosition());
}

void TestBtAudioPlugin::playerTimes_invalidatedValuesUseUnknowns()
{
    BtAudioPlugin plugin;
    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    QVERIFY(adoptPlayer(plugin, path,
                        playerProperties(QStringLiteral("Track"), 215000u, 61000u)));

    QSignalSpy metadataSpy(&plugin, &BtAudioPlugin::metadataChanged);
    QSignalSpy durationSpy(&plugin, &BtAudioPlugin::durationChanged);
    QSignalSpy positionSpy(&plugin, &BtAudioPlugin::positionChanged);
    QSignalSpy progressSpy(&plugin, &BtAudioPlugin::progressChanged);
    QVERIFY(drivePlayerProperties(
        plugin, path, {},
        {QStringLiteral("Track"), QStringLiteral("Position")}));

    QVERIFY(plugin.trackTitle().isEmpty());
    QCOMPARE(plugin.trackDuration(), qint64(0));
    QCOMPARE(plugin.trackPosition(), qint64(0));
    QVERIFY(!plugin.hasTrackPosition());
    QCOMPARE(metadataSpy.count(), 1);
    QCOMPARE(durationSpy.count(), 1);
    QCOMPARE(positionSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(progressSpy.first().at(0).toLongLong(), qint64(-1));
    QCOMPARE(progressSpy.first().at(1).toLongLong(), qint64(0));
}

void TestBtAudioPlugin::playerTimes_newPlayerClearsMissingState()
{
    BtAudioPlugin plugin;
    const QString firstPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    const QString secondPath = QStringLiteral("/org/bluez/hci0/dev_CC_DD/player0");
    QVERIFY(adoptPlayer(plugin, firstPath,
                        playerProperties(QStringLiteral("First"), 215000u, 61000u)));

    QVariantMap secondProps;
    secondProps.insert(QStringLiteral("Status"), QStringLiteral("paused"));
    QSignalSpy progressSpy(&plugin, &BtAudioPlugin::progressChanged);
    QVERIFY(adoptPlayer(plugin, secondPath, secondProps));

    QVERIFY(plugin.trackTitle().isEmpty());
    QCOMPARE(plugin.trackDuration(), qint64(0));
    QCOMPARE(plugin.trackPosition(), qint64(0));
    QVERIFY(!plugin.hasTrackPosition());
    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(progressSpy.first().at(0).toLongLong(), qint64(-1));
    QCOMPARE(progressSpy.first().at(1).toLongLong(), qint64(0));
}

void TestBtAudioPlugin::playerTimes_bluezLossClearsState()
{
    BtAudioPlugin plugin;
    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    QVERIFY(adoptPlayer(plugin, path,
                        playerProperties(QStringLiteral("Track"), 215000u, 61000u)));

    QSignalSpy progressSpy(&plugin, &BtAudioPlugin::progressChanged);
    QVERIFY(QMetaObject::invokeMethod(&plugin, "onBluezServiceUnregistered",
                                      Qt::DirectConnection));

    QVERIFY(plugin.trackTitle().isEmpty());
    QCOMPARE(plugin.trackDuration(), qint64(0));
    QCOMPARE(plugin.trackPosition(), qint64(0));
    QVERIFY(!plugin.hasTrackPosition());
    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(progressSpy.first().at(0).toLongLong(), qint64(-1));
    QCOMPARE(progressSpy.first().at(1).toLongLong(), qint64(0));
}

void TestBtAudioPlugin::startupEnumeration_isAsynchronousAndUsesCarriedProperties()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY2(bus.isConnected(), "CTest must provide an isolated session D-Bus");
    qDBusRegisterMetaType<TestManagedObjectMap>();

    const QString devicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB");
    const QString transportPath = devicePath + QStringLiteral("/fd0");
    const QString playerPath = devicePath + QStringLiteral("/player0");

    ObjectManagerFixture fixture;
    QVariantMap deviceProps;
    deviceProps.insert(QStringLiteral("Alias"), QStringLiteral("Moto"));
    fixture.objects[QDBusObjectPath(devicePath)]
        .insert(QStringLiteral("org.bluez.Device1"), deviceProps);

    QVariantMap transportProps;
    transportProps.insert(QStringLiteral("State"), QStringLiteral("active"));
    transportProps.insert(QStringLiteral("Device"),
                          QVariant::fromValue(QDBusObjectPath(devicePath)));
    fixture.objects[QDBusObjectPath(transportPath)]
        .insert(QStringLiteral("org.bluez.MediaTransport1"), transportProps);

    QVariantMap playerProps =
        playerProperties(QStringLiteral("Async Track"), 215000u, 61000u);
    playerProps.insert(QStringLiteral("Status"), QStringLiteral("playing"));
    fixture.objects[QDBusObjectPath(playerPath)]
        .insert(QStringLiteral("org.bluez.MediaPlayer1"), playerProps);

    QVERIFY(bus.registerObject(QStringLiteral("/"), &fixture,
                               QDBusConnection::ExportAllSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.bluez")));

    BtAudioPlugin plugin(bus);
    QSignalSpy progressSpy(&plugin, &BtAudioPlugin::progressChanged);
    QElapsedTimer elapsed;
    elapsed.start();
    QVERIFY(plugin.initialize(nullptr));
    QVERIFY2(elapsed.elapsed() < 250,
             "initialize must not wait for the delayed ObjectManager reply");
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Disconnected));

    // Routine property traffic during discovery must not invalidate every
    // topology reply. Queue/replay preserves the newest value while allowing
    // the one startup scan to complete.
    quint32 livePosition = 61000;
    QTimer propertyTraffic;
    propertyTraffic.setInterval(25);
    connect(&propertyTraffic, &QTimer::timeout, this, [&]() {
        ++livePosition;
        QDBusMessage signal = QDBusMessage::createSignal(
            playerPath, QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"));
        signal << QStringLiteral("org.bluez.MediaPlayer1")
               << QVariantMap{{QStringLiteral("Position"), livePosition}}
               << QStringList{};
        QVERIFY(bus.send(signal));
    });
    propertyTraffic.start();

    QTRY_COMPARE(fixture.callCount, 1);
    QTRY_COMPARE_WITH_TIMEOUT(plugin.connectionState(),
                              static_cast<int>(BtAudioPlugin::Connected), 2000);
    QCOMPARE(plugin.deviceName(), QStringLiteral("Moto"));
    QVERIFY(plugin.transportActive());
    QCOMPARE(plugin.playbackState(), static_cast<int>(BtAudioPlugin::Playing));
    QCOMPARE(plugin.trackTitle(), QStringLiteral("Async Track"));
    QCOMPARE(plugin.trackDuration(), qint64(215000));
    QVERIFY(plugin.hasTrackPosition());
    QTest::qWait(150);
    propertyTraffic.stop();
    QCOMPARE(fixture.callCount, 1);
    QTRY_COMPARE(plugin.trackPosition(), qint64(livePosition));
    qint64 previousPosition = -1;
    for (const QList<QVariant>& edge : progressSpy) {
        const qint64 position = edge.at(0).toLongLong();
        QVERIFY2(position >= previousPosition,
                 "in-flight snapshot publication must not move position backwards");
        previousPosition = position;
    }

    plugin.shutdown();
    bus.unregisterService(QStringLiteral("org.bluez"));
    bus.unregisterObject(QStringLiteral("/"));
}

void TestBtAudioPlugin::startupEnumeration_malformedAndErrorRetainStateButEmptyClears()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY2(bus.isConnected(), "CTest must provide an isolated session D-Bus");
    qDBusRegisterMetaType<TestManagedObjectMap>();

    const QString devicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB");
    const QString transportPath = devicePath + QStringLiteral("/fd0");
    const QString playerPath = devicePath + QStringLiteral("/player0");

    ObjectManagerFixture fixture;
    fixture.replyDelayMs = 50;
    fixture.objects[QDBusObjectPath(devicePath)]
        .insert(QStringLiteral("org.bluez.Device1"),
                {{QStringLiteral("Alias"), QStringLiteral("Moto")}});
    fixture.objects[QDBusObjectPath(transportPath)]
        .insert(QStringLiteral("org.bluez.MediaTransport1"),
                {{QStringLiteral("State"), QStringLiteral("active")},
                 {QStringLiteral("Device"),
                  QVariant::fromValue(QDBusObjectPath(devicePath))}});
    QVariantMap playerProps =
        playerProperties(QStringLiteral("Retained Track"), 215000u, 61000u);
    playerProps.insert(QStringLiteral("Status"), QStringLiteral("playing"));
    fixture.objects[QDBusObjectPath(playerPath)]
        .insert(QStringLiteral("org.bluez.MediaPlayer1"), playerProps);

    QVERIFY(bus.registerObject(QStringLiteral("/"), &fixture,
                               QDBusConnection::ExportAllSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.bluez")));

    LoggingHostContext host;
    BtAudioPlugin plugin(bus);
    QVERIFY(plugin.initialize(&host));
    QTRY_COMPARE(fixture.callCount, 1);
    QTRY_COMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
    QCOMPARE(plugin.deviceName(), QStringLiteral("Moto"));
    QCOMPARE(plugin.trackTitle(), QStringLiteral("Retained Track"));
    QCOMPARE(plugin.trackPosition(), qint64(61000));
    plugin.shutdown();

    fixture.replyMode = ObjectManagerFixture::ReplyMode::MalformedSignature;
    host.messages.clear();
    QVERIFY(plugin.initialize(&host));
    QDBusMessage positionSignal = QDBusMessage::createSignal(
        playerPath, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    positionSignal << QStringLiteral("org.bluez.MediaPlayer1")
                   << QVariantMap{{QStringLiteral("Position"), quint32(62000)}}
                   << QStringList{};
    QVERIFY(bus.send(positionSignal));
    QTRY_COMPARE(fixture.callCount, 2);
    QTRY_VERIFY(host.messages.join(QLatin1Char('\n')).contains(
        QStringLiteral("expected one a{oa{sa{sv}}} argument")));
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
    QCOMPARE(plugin.deviceName(), QStringLiteral("Moto"));
    QCOMPARE(plugin.trackTitle(), QStringLiteral("Retained Track"));
    // Pending property traffic is replayed even though the topology reply is
    // rejected; malformed does not mean an authoritative empty snapshot.
    QTRY_COMPARE(plugin.trackPosition(), qint64(62000));
    plugin.shutdown();

    fixture.replyMode = ObjectManagerFixture::ReplyMode::Error;
    host.messages.clear();
    QVERIFY(plugin.initialize(&host));
    QTRY_COMPARE(fixture.callCount, 3);
    QTRY_VERIFY(host.messages.join(QLatin1Char('\n')).contains(
        QStringLiteral("fixture failure")));
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
    QCOMPARE(plugin.trackTitle(), QStringLiteral("Retained Track"));
    QCOMPARE(plugin.trackPosition(), qint64(62000));
    plugin.shutdown();

    fixture.replyMode = ObjectManagerFixture::ReplyMode::Valid;
    fixture.objects.clear();
    host.messages.clear();
    QVERIFY(plugin.initialize(&host));
    QTRY_COMPARE(fixture.callCount, 4);
    QTRY_COMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Disconnected));
    QVERIFY(plugin.deviceName().isEmpty());
    QCOMPARE(plugin.playbackState(), static_cast<int>(BtAudioPlugin::Stopped));
    QVERIFY(plugin.trackTitle().isEmpty());
    QCOMPARE(plugin.trackDuration(), qint64(0));
    QCOMPARE(plugin.trackPosition(), qint64(0));
    QVERIFY(!plugin.hasTrackPosition());

    plugin.shutdown();
    bus.unregisterService(QStringLiteral("org.bluez"));
    bus.unregisterObject(QStringLiteral("/"));
}

void TestBtAudioPlugin::startupEnumeration_validFallbackSurvivesTrailingError()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY2(bus.isConnected(), "CTest must provide an isolated session D-Bus");
    qDBusRegisterMetaType<TestManagedObjectMap>();

    const QString devicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB");
    const QString transportPath = devicePath + QStringLiteral("/fd0");
    const QString playerPath = devicePath + QStringLiteral("/player0");

    ObjectManagerFixture fixture;
    fixture.replyDelayMs = 100;
    fixture.objects[QDBusObjectPath(devicePath)]
        .insert(QStringLiteral("org.bluez.Device1"),
                {{QStringLiteral("Alias"), QStringLiteral("Snapshot Name")}});
    fixture.objects[QDBusObjectPath(transportPath)]
        .insert(QStringLiteral("org.bluez.MediaTransport1"),
                {{QStringLiteral("State"), QStringLiteral("active")},
                 {QStringLiteral("Device"),
                  QVariant::fromValue(QDBusObjectPath(devicePath))}});
    QVariantMap playerProps =
        playerProperties(QStringLiteral("Fallback Track"), 215000u, 61000u);
    playerProps.insert(QStringLiteral("Status"), QStringLiteral("playing"));
    fixture.objects[QDBusObjectPath(playerPath)]
        .insert(QStringLiteral("org.bluez.MediaPlayer1"), playerProps);

    QVERIFY(bus.registerObject(QStringLiteral("/"), &fixture,
                               QDBusConnection::ExportAllSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.bluez")));

    LoggingHostContext host;
    BtAudioPlugin plugin(bus);
    QSignalSpy connectionSpy(&plugin, &BtAudioPlugin::connectionStateChanged);
    QSignalSpy playbackSpy(&plugin, &BtAudioPlugin::playbackStateChanged);
    QSignalSpy metadataSpy(&plugin, &BtAudioPlugin::metadataChanged);
    QSignalSpy progressSpy(&plugin, &BtAudioPlugin::progressChanged);
    QSignalSpy activeSpy(&plugin, &BtAudioPlugin::transportActiveChanged);
    QVERIFY(plugin.initialize(&host));
    QTRY_COMPARE(fixture.callCount, 1);

    // The first reply was already constructed as valid. Make only the
    // interface-triggered trailing request fail.
    fixture.replyMode = ObjectManagerFixture::ReplyMode::Error;
    BtInterfaceMap deviceUpdate;
    deviceUpdate.insert(QStringLiteral("org.bluez.Device1"),
                        {{QStringLiteral("Alias"), QStringLiteral("Event Name")}});
    QVERIFY(QMetaObject::invokeMethod(
        &plugin, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(devicePath)),
        Q_ARG(BtInterfaceMap, deviceUpdate)));
    QVERIFY(drivePlayerProperties(
        plugin, playerPath,
        {{QStringLiteral("Position"), QVariant::fromValue(quint32(62000))}}));

    QTRY_COMPARE(fixture.callCount, 2);
    QTRY_VERIFY(host.messages.join(QLatin1Char('\n')).contains(
        QStringLiteral("fixture failure")));

    // The valid first topology is published as a complete fallback, merged
    // with the interface event and queued property delta. The failed trailing
    // scan cannot reduce it to the partial event-only state.
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
    QCOMPARE(plugin.deviceName(), QStringLiteral("Event Name"));
    QVERIFY(plugin.transportActive());
    QCOMPARE(plugin.playbackState(), static_cast<int>(BtAudioPlugin::Playing));
    QCOMPARE(plugin.trackTitle(), QStringLiteral("Fallback Track"));
    QCOMPARE(plugin.trackDuration(), qint64(215000));
    QCOMPARE(plugin.trackPosition(), qint64(62000));
    QVERIFY(plugin.hasTrackPosition());
    QCOMPARE(connectionSpy.count(), 1);
    QCOMPARE(playbackSpy.count(), 1);
    QCOMPARE(metadataSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(activeSpy.count(), 1);

    plugin.shutdown();
    bus.unregisterService(QStringLiteral("org.bluez"));
    bus.unregisterObject(QStringLiteral("/"));
}

void TestBtAudioPlugin::startupEnumeration_bluezLossRejectsStaleReply()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY2(bus.isConnected(), "CTest must provide an isolated session D-Bus");
    qDBusRegisterMetaType<TestManagedObjectMap>();

    const QString devicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB");
    const QString transportPath = devicePath + QStringLiteral("/fd0");
    ObjectManagerFixture fixture;
    fixture.replyDelayMs = 100;
    fixture.objects[QDBusObjectPath(devicePath)]
        .insert(QStringLiteral("org.bluez.Device1"),
                {{QStringLiteral("Alias"), QStringLiteral("Stale Phone")}});
    fixture.objects[QDBusObjectPath(transportPath)]
        .insert(QStringLiteral("org.bluez.MediaTransport1"),
                {{QStringLiteral("State"), QStringLiteral("active")},
                 {QStringLiteral("Device"),
                  QVariant::fromValue(QDBusObjectPath(devicePath))}});

    QVERIFY(bus.registerObject(QStringLiteral("/"), &fixture,
                               QDBusConnection::ExportAllSlots));
    QVERIFY(bus.registerService(QStringLiteral("org.bluez")));

    LoggingHostContext host;
    BtAudioPlugin plugin(bus);
    QSignalSpy connectionSpy(&plugin, &BtAudioPlugin::connectionStateChanged);
    QSignalSpy activeSpy(&plugin, &BtAudioPlugin::transportActiveChanged);
    QVERIFY(plugin.initialize(&host));
    QTRY_COMPARE(fixture.callCount, 1);

    // Simulate service loss after the request was sent but before its delayed
    // reply. The epoch advances even though the old fixture can still send.
    QVERIFY(QMetaObject::invokeMethod(&plugin, "onBluezServiceUnregistered",
                                      Qt::DirectConnection));
    QTest::qWait(300);
    QCOMPARE(fixture.callCount, 1);
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Disconnected));
    QVERIFY(plugin.deviceName().isEmpty());
    QVERIFY(!plugin.transportActive());
    QCOMPARE(connectionSpy.count(), 0);
    QCOMPARE(activeSpy.count(), 0);

    plugin.shutdown();
    bus.unregisterService(QStringLiteral("org.bluez"));
    bus.unregisterObject(QStringLiteral("/"));
}

void TestBtAudioPlugin::hotplugMissingPropertiesRemainUnknownUntilDelivery()
{
    BtAudioPlugin plugin;
    const QString devicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB");
    const QString transportPath = devicePath + QStringLiteral("/fd0");
    const QString playerPath = devicePath + QStringLiteral("/player0");

    BtInterfaceMap transportInterfaces;
    QVariantMap transportProps;
    transportProps.insert(QStringLiteral("Device"),
                          QVariant::fromValue(QDBusObjectPath(devicePath)));
    transportInterfaces.insert(QStringLiteral("org.bluez.MediaTransport1"),
                               transportProps); // State absent: known present, inactive
    QVERIFY(QMetaObject::invokeMethod(
        &plugin, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(transportPath)),
        Q_ARG(BtInterfaceMap, transportInterfaces)));
    QCOMPARE(plugin.connectionState(), static_cast<int>(BtAudioPlugin::Connected));
    QVERIFY(!plugin.transportActive());
    QVERIFY(plugin.deviceName().isEmpty());

    BtInterfaceMap playerInterfaces;
    playerInterfaces.insert(QStringLiteral("org.bluez.MediaPlayer1"), {});
    QVERIFY(QMetaObject::invokeMethod(
        &plugin, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(playerPath)),
        Q_ARG(BtInterfaceMap, playerInterfaces)));
    QCOMPARE(plugin.playbackState(), static_cast<int>(BtAudioPlugin::Stopped));
    QCOMPARE(plugin.trackDuration(), qint64(0));
    QVERIFY(!plugin.hasTrackPosition());

    BtInterfaceMap deviceInterfaces;
    deviceInterfaces.insert(QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("Moto")}});
    QVERIFY(QMetaObject::invokeMethod(
        &plugin, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(devicePath)),
        Q_ARG(BtInterfaceMap, deviceInterfaces)));
    QCOMPARE(plugin.deviceName(), QStringLiteral("Moto"));

    QVERIFY(driveTransportState(plugin, transportPath, QStringLiteral("active")));
    QVariantMap delivered =
        playerProperties(QStringLiteral("Delivered"), 245000u, 62000u);
    delivered.insert(QStringLiteral("Status"), QStringLiteral("playing"));
    QVERIFY(drivePlayerProperties(plugin, playerPath, delivered));
    QVERIFY(plugin.transportActive());
    QCOMPARE(plugin.playbackState(), static_cast<int>(BtAudioPlugin::Playing));
    QCOMPARE(plugin.trackDuration(), qint64(245000));
    QCOMPARE(plugin.trackPosition(), qint64(62000));
}

void TestBtAudioPlugin::deviceName_nameOnlyDeltaPreservesAlias()
{
    BtAudioPlugin plugin;
    const QString devicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB");
    const QString transportPath = devicePath + QStringLiteral("/fd0");
    seedNamedTransport(plugin, devicePath, transportPath,
                       QStringLiteral("Preferred Alias"), QStringLiteral("Original Name"));
    QCOMPARE(plugin.deviceName(), QStringLiteral("Preferred Alias"));

    QSignalSpy connectionSpy(&plugin, &BtAudioPlugin::connectionStateChanged);
    QVERIFY(driveDeviceProperties(
        plugin, devicePath,
        {{QStringLiteral("Name"), QStringLiteral("Updated Name")}}));

    // A Name-only delta merges into the cached Device1 snapshot. It must not
    // replace the still-valid higher-priority Alias or emit a false UI edge.
    QCOMPARE(plugin.deviceName(), QStringLiteral("Preferred Alias"));
    QCOMPARE(connectionSpy.count(), 0);
}

void TestBtAudioPlugin::deviceName_aliasInvalidationFallsBackToCachedName()
{
    BtAudioPlugin plugin;
    const QString devicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB");
    const QString transportPath = devicePath + QStringLiteral("/fd0");
    seedNamedTransport(plugin, devicePath, transportPath,
                       QStringLiteral("Preferred Alias"), QStringLiteral("Device Name"));

    QSignalSpy connectionSpy(&plugin, &BtAudioPlugin::connectionStateChanged);
    QVERIFY(driveDeviceProperties(plugin, devicePath, {},
                                  {QStringLiteral("Alias")}));
    QCOMPARE(plugin.deviceName(), QStringLiteral("Device Name"));
    QCOMPARE(connectionSpy.count(), 1);

    // Repeating the same invalidation leaves the observable label unchanged.
    QVERIFY(driveDeviceProperties(plugin, devicePath, {},
                                  {QStringLiteral("Alias")}));
    QCOMPARE(plugin.deviceName(), QStringLiteral("Device Name"));
    QCOMPARE(connectionSpy.count(), 1);
}

void TestBtAudioPlugin::deviceName_removalAndBluezResetPurgeCache()
{
    BtAudioPlugin plugin;
    const QString devicePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB");
    const QString transportPath = devicePath + QStringLiteral("/fd0");
    seedNamedTransport(plugin, devicePath, transportPath,
                       QStringLiteral("Old Alias"), QStringLiteral("Old Name"));

    const QStringList removed{QStringLiteral("org.bluez.Device1")};
    QVERIFY(QMetaObject::invokeMethod(
        &plugin, "onInterfacesRemoved", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(devicePath)),
        Q_ARG(QStringList, removed)));
    QVERIFY(plugin.deviceName().isEmpty());

    // Re-adoption after removal carries only Alias. Invalidating it must not
    // reveal the removed object's old cached Name.
    BtInterfaceMap deviceInterfaces;
    deviceInterfaces.insert(QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("New Alias")}});
    QVERIFY(QMetaObject::invokeMethod(
        &plugin, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(devicePath)),
        Q_ARG(BtInterfaceMap, deviceInterfaces)));
    QCOMPARE(plugin.deviceName(), QStringLiteral("New Alias"));
    QVERIFY(driveDeviceProperties(plugin, devicePath, {},
                                  {QStringLiteral("Alias")}));
    QVERIFY(plugin.deviceName().isEmpty());

    seedNamedTransport(plugin, devicePath, transportPath,
                       QStringLiteral("Reset Alias"), QStringLiteral("Reset Name"));
    QVERIFY(QMetaObject::invokeMethod(&plugin, "onBluezServiceUnregistered",
                                      Qt::DirectConnection));
    QVERIFY(plugin.deviceName().isEmpty());

    // BlueZ reappears carrying Alias only. The pre-reset Name must be gone.
    deviceInterfaces[QStringLiteral("org.bluez.Device1")] = {
        {QStringLiteral("Alias"), QStringLiteral("After Reset")},
    };
    QVERIFY(QMetaObject::invokeMethod(
        &plugin, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(devicePath)),
        Q_ARG(BtInterfaceMap, deviceInterfaces)));
    BtInterfaceMap transportInterfaces;
    transportInterfaces[QStringLiteral("org.bluez.MediaTransport1")] = {
        {QStringLiteral("Device"), QVariant::fromValue(QDBusObjectPath(devicePath))},
        {QStringLiteral("State"), QStringLiteral("idle")},
    };
    QVERIFY(QMetaObject::invokeMethod(
        &plugin, "onInterfacesAdded", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(transportPath)),
        Q_ARG(BtInterfaceMap, transportInterfaces)));
    QCOMPARE(plugin.deviceName(), QStringLiteral("After Reset"));
    QVERIFY(driveDeviceProperties(plugin, devicePath, {},
                                  {QStringLiteral("Alias")}));
    QVERIFY(plugin.deviceName().isEmpty());
}

void TestBtAudioPlugin::playerRemoval_resetsAllStateWithEdgeOnlySignals()
{
    BtAudioPlugin plugin;
    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    QVariantMap props = playerProperties(QStringLiteral("Track"), 215000u, 61000u);
    props.insert(QStringLiteral("Status"), QStringLiteral("playing"));
    QVERIFY(adoptPlayer(plugin, path, props));

    QSignalSpy playbackSpy(&plugin, &BtAudioPlugin::playbackStateChanged);
    QSignalSpy metadataSpy(&plugin, &BtAudioPlugin::metadataChanged);
    QSignalSpy durationSpy(&plugin, &BtAudioPlugin::durationChanged);
    QSignalSpy positionSpy(&plugin, &BtAudioPlugin::positionChanged);
    QSignalSpy progressSpy(&plugin, &BtAudioPlugin::progressChanged);
    const QStringList removed{QStringLiteral("org.bluez.MediaPlayer1")};

    QVERIFY(QMetaObject::invokeMethod(
        &plugin, "onInterfacesRemoved", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(path)),
        Q_ARG(QStringList, removed)));
    QCOMPARE(plugin.playbackState(), static_cast<int>(BtAudioPlugin::Stopped));
    QVERIFY(plugin.trackTitle().isEmpty());
    QVERIFY(plugin.trackArtist().isEmpty());
    QVERIFY(plugin.trackAlbum().isEmpty());
    QCOMPARE(plugin.trackDuration(), qint64(0));
    QCOMPARE(plugin.trackPosition(), qint64(0));
    QVERIFY(!plugin.hasTrackPosition());
    QCOMPARE(playbackSpy.count(), 1);
    QCOMPARE(metadataSpy.count(), 1);
    QCOMPARE(durationSpy.count(), 1);
    QCOMPARE(positionSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
    QCOMPARE(progressSpy.constFirst().at(0).toLongLong(), qint64(-1));

    // Duplicate removal is not another observable state change.
    QVERIFY(QMetaObject::invokeMethod(
        &plugin, "onInterfacesRemoved", Qt::DirectConnection,
        Q_ARG(QDBusObjectPath, QDBusObjectPath(path)),
        Q_ARG(QStringList, removed)));
    QCOMPARE(playbackSpy.count(), 1);
    QCOMPARE(metadataSpy.count(), 1);
    QCOMPARE(durationSpy.count(), 1);
    QCOMPARE(positionSpy.count(), 1);
    QCOMPARE(progressSpy.count(), 1);
}

void TestBtAudioPlugin::playerTimes_startupAndReconnectCatchupPreservesMilliseconds()
{
    BtAudioPlugin plugin;
    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    QVERIFY(adoptPlayer(plugin, path,
                        playerProperties(QStringLiteral("Track"), 215000u, 61000u)));

    // MediaStatusService is created after plugin initialization in main.cpp.
    // Seed the already-adopted snapshot through the same production helper.
    oap::MediaStatusService media;
    media.setBtConnected(true);
    media.updateBtSnapshot(plugin.trackTitle(), plugin.trackArtist(),
                           plugin.trackAlbum(), plugin.playbackState(),
                           plugin.hasTrackPosition() ? plugin.trackPosition() : -1,
                           plugin.trackDuration());
    QCOMPARE(media.position(), qint64(61000));
    QCOMPARE(media.duration(), qint64(215000));
    QVERIFY(media.hasPosition());

    // A fresh connection clears cached source state; the same catch-up must
    // restore exact milliseconds even without another AVRCP event.
    media.setBtConnected(false);
    media.setBtConnected(true);
    QVERIFY(!media.hasPosition());
    media.updateBtSnapshot(plugin.trackTitle(), plugin.trackArtist(),
                           plugin.trackAlbum(), plugin.playbackState(),
                           plugin.hasTrackPosition() ? plugin.trackPosition() : -1,
                           plugin.trackDuration());
    QCOMPARE(media.position(), qint64(61000));
    QCOMPARE(media.duration(), qint64(215000));
    QVERIFY(media.hasPosition());
}

void TestBtAudioPlugin::playerTimes_flowUnchangedToMediaStatusService()
{
    BtAudioPlugin plugin;
    oap::MediaStatusService media;
    media.setBtConnected(true);
    connect(&plugin, &BtAudioPlugin::progressChanged,
            &media, &oap::MediaStatusService::updateBtProgress);

    const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB/player0");
    QVERIFY(adoptPlayer(plugin, path,
                        playerProperties(QStringLiteral("Track"), 215000u, 61000u)));

    QCOMPARE(media.position(), qint64(61000));
    QCOMPARE(media.duration(), qint64(215000));
    QVERIFY(media.hasPosition());
}

QTEST_GUILESS_MAIN(TestBtAudioPlugin)
#include "test_bt_audio_plugin.moc"

// tests/test_bt_audio_plugin.cpp
#include <QtTest/QtTest>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QSignalSpy>
#include <QStringList>
#include <QVariantMap>
#include "plugins/bt_audio/BtAudioPlugin.hpp"

using oap::plugins::BtAudioPlugin;
using oap::plugins::BtInterfaceMap;

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

private:
    // Seed the plugin's tracked player path through its adoption flow: an
    // InterfacesAdded carrying org.bluez.MediaPlayer1 sets playerPath_ (the
    // subsequent D-Bus property read-back fails harmlessly with no BlueZ on the
    // build box, but the path is recorded first).
    static bool adoptPlayer(BtAudioPlugin& plugin, const QString& path) {
        BtInterfaceMap ifaces;
        ifaces.insert(QStringLiteral("org.bluez.MediaPlayer1"), QVariantMap{});
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

QTEST_GUILESS_MAIN(TestBtAudioPlugin)
#include "test_bt_audio_plugin.moc"

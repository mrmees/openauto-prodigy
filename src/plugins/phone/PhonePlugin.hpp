#pragma once

#include "core/plugin/IPlugin.hpp"
#include <QObject>
#include <QString>
#include <QDBusObjectPath>
#include <QTimer>

class QQmlContext;
class QDBusServiceWatcher;

namespace oap {

class IHostContext;

namespace plugins {

/// Bluetooth HFP (Hands-Free Profile) phone plugin.
///
/// The Pi acts as HFP Audio Gateway — when a phone pairs, PipeWire + BlueZ
/// handle SCO audio routing and codec negotiation natively.
///
/// This plugin provides:
///   - Dialer UI (number pad, call/hangup)
///   - Call state monitoring via BlueZ D-Bus (org.bluez.Device1 + telephony)
///   - Incoming call notification overlay via EventBus
///   - Audio focus management (calls mute all other audio)
///
/// D-Bus interfaces used:
///   org.freedesktop.DBus.ObjectManager — device/profile add/remove
///   org.bluez.Device1 — connected device info
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
    enum CallState {
        Idle = 0,
        Dialing,
        Ringing,     // incoming
        Active,
        HeldActive,  // call on hold
        Ended
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
    QString iconText() const override { return QString(QChar(0xf0d4)); }  // phone
    QUrl settingsComponent() const override { return {}; }

    // IPlugin — Capabilities
    QStringList requiredServices() const override { return {}; }
    bool wantsFullscreen() const override { return false; }

    // Properties
    int callState() const { return callState_; }
    QString callerNumber() const { return callerNumber_; }
    QString callerName() const { return callerName_; }
    QString dialedNumber() const { return dialedNumber_; }
    int callDuration() const { return callDuration_; }
    bool phoneConnected() const { return phoneConnected_; }
    QString deviceName() const { return deviceName_; }

    // Call controls (invokable from QML)
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
    void startDBusMonitoring();
    void stopDBusMonitoring();
    void scanExistingDevices();
    void onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                             const QStringList& invalidated);
    void setCallState(CallState state);
    void updateCallDuration();

    IHostContext* hostContext_ = nullptr;
    QDBusServiceWatcher* bluezWatcher_ = nullptr;
    QTimer* callTimer_ = nullptr;
    bool monitoring_ = false;

    CallState callState_ = Idle;
    QString callerNumber_;
    QString callerName_;
    QString dialedNumber_;
    int callDuration_ = 0;  // seconds
    bool phoneConnected_ = false;
    QString deviceName_;
    QString devicePath_;
    QString activeCallNotificationId_;
};

} // namespace plugins
} // namespace oap

#pragma once

#include "core/plugin/IPlugin.hpp"
#include "core/plugin/PluginDiscovery.hpp"
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
    int apiVersion() const override { return PluginDiscovery::HOST_API_VERSION; }

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

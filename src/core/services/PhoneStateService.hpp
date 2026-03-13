#pragma once

#include "IPhoneStateService.hpp"
#include <QDBusObjectPath>
#include <QTimer>

class QDBusServiceWatcher;

namespace oap {

class INotificationService;

/// Core service owning HFP D-Bus monitoring and call state machine.
/// Lives independently of the phone UI plugin.
class PhoneStateService : public IPhoneStateService {
    Q_OBJECT
public:
    explicit PhoneStateService(QObject* parent = nullptr);
    ~PhoneStateService() override;

    // ICallStateProvider
    int callState() const override;
    QString callerName() const override;
    QString callerNumber() const override;
    void answer() override;
    void hangup() override;

    // IPhoneStateService
    bool phoneConnected() const override { return phoneConnected_; }
    QString deviceName() const override { return deviceName_; }
    int callDuration() const override { return callDuration_; }

    /// Set notification service for incoming call alerts (optional)
    void setNotificationService(INotificationService* svc) { notificationService_ = svc; }

    /// Called by D-Bus monitoring or test code to simulate an incoming call.
    void setIncomingCall(const QString& number, const QString& name);

    /// Start D-Bus monitoring for HFP devices
    void startDBusMonitoring();
    void stopDBusMonitoring();

private slots:
    void onInterfacesAdded(const QDBusObjectPath& path, const QVariantMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                             const QStringList& invalidated);

private:
    void scanExistingDevices();
    void setCallStateInternal(ICallStateProvider::CallState state);
    void updateCallDuration();

    INotificationService* notificationService_ = nullptr;
    QDBusServiceWatcher* bluezWatcher_ = nullptr;
    QTimer callTimer_;
    bool monitoring_ = false;

    ICallStateProvider::CallState callState_ = ICallStateProvider::Idle;
    QString callerNumber_;
    QString callerName_;
    int callDuration_ = 0;
    bool phoneConnected_ = false;
    QString deviceName_;
    QString devicePath_;
    QString activeCallNotificationId_;
};

} // namespace oap

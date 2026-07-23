#pragma once

#include "IPhoneStateService.hpp"
// Pulls Q_DECLARE_METATYPE(oap::InterfaceMap) — the SAME underlying
// QMap<QString,QVariantMap> that BluezInterfaceMap aliases below. openauto-core
// aggregates every class's MOC into one translation unit; that explicit
// metatype specialization MUST be visible before this slot's parameter type is
// first used, or the built-in QMap metatype gets implicitly instantiated first
// and TelephonyClient's declaration becomes a "specialization after
// instantiation" error.
#include "TelephonyClient.hpp"
#include <QDBusObjectPath>
#include <QMap>
#include <QTimer>
#include <QVariantMap>

class QDBusServiceWatcher;
class QDBusMessage;

namespace oap {

class INotificationService;
class ScoNodeMonitor;

/// ObjectManager InterfacesAdded carries a{sa{sv}}; QtDBus refuses to deliver
/// it to a QVariantMap slot (connect fails at runtime — bench 2026-07-10 root
/// cause, same family as UsbInterfaceMap). Its metatype is the shared
/// QMap<QString,QVariantMap> already declared via TelephonyClient's
/// Q_DECLARE_METATYPE(oap::InterfaceMap); re-registered in startDBusMonitoring().
using BluezInterfaceMap = QMap<QString, QVariantMap>;

/// Core service owning the HFP call state machine.
///
/// Inputs: TelephonyClient (PipeWire org.pipewire.Telephony — call setup
/// objects, transport state), ScoNodeMonitor (SCO node running = call audio
/// flowing), BlueZ Device1 monitoring (phone connected / device name).
///
/// State machine: docs/archive/plans/2026-07-05-hfp-call-audio-design.md §5.
/// Key semantics (live-verified): Call1 objects exist during call SETUP only;
/// "active call" truth comes from SCO node state, with transport state as a
/// fallback inside the settle window. SCO confirms or ends call evidence but
/// cannot create a call from Idle. Transitions are never optimistic command
/// dispatch side effects.
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

    /// Evaluate one BlueZ Device1 property map (from the initial scan or a
    /// live InterfacesAdded) and adopt the device if it is a connected HFP
    /// phone. Public because it IS the unit-test surface (same convention as
    /// the state-machine event slots above).
    void adoptBluezDevice(const QString& path, const QVariantMap& deviceProps);

    /// Pure decision: should a Device1 PropertiesChanged trigger a full
    /// managed-objects rescan? True only when no phone is adopted yet AND the
    /// change carries Connected=true or a UUIDs arrival (a freshly paired phone
    /// whose Connected/UUIDs land after the disconnected Device1 was created).
    /// Static + public because it IS the unit-test surface for late adoption.
    static bool shouldRescanOnDeviceChange(bool phoneConnected, const QVariantMap& changed);

public slots:
    // State-machine event API — public because it IS the unit-test surface.
    void onCallSetupStarted(const QString& state, const QString& line, const QString& name);
    void onCallSetupChanged(const QString& state);
    void onCallSetupEnded();
    void onScoRunningChanged(bool running);
    void onTransportStateChanged(const QString& state);
    void onTelephonyAvailable(bool available);

private slots:
    void onInterfacesAdded(const QDBusObjectPath& path, const BluezInterfaceMap& interfaces);
    void onInterfacesRemoved(const QDBusObjectPath& path, const QStringList& interfaces);
    // Sender path arrives in the trailing QDBusMessage (BtAudioPlugin
    // onPropertiesChanged pattern); the disconnect branch filters against
    // devicePath_ so an unrelated BlueZ device dropping Connected cannot tear
    // down the tracked phone. The rescan path is sender-agnostic (a scan reads
    // full state).
    void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                             const QStringList& invalidated, const QDBusMessage& message);
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

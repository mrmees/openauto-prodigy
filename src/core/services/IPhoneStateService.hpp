#pragma once

#include "ICallStateProvider.hpp"

namespace oap {

/// Extended phone service interface beyond the narrow ICallStateProvider.
/// Adds connection state, device info, and dialer operations.
class IPhoneStateService : public ICallStateProvider {
    Q_OBJECT
public:
    using ICallStateProvider::ICallStateProvider;

    virtual bool phoneConnected() const = 0;
    virtual QString deviceName() const = 0;
    virtual int callDuration() const = 0;

    /// True while the telephony backend is reachable AND a phone (audio
    /// gateway) is connected — the single source of truth for the API's
    /// can_dial/can_answer/can_hangup/can_send_dtmf capability flags.
    virtual bool telephonyAvailable() const = 0;

    /// Place an outgoing call. Only dispatched from Idle. Returns true if
    /// dispatched.
    Q_INVOKABLE virtual bool dial(const QString& number) = 0;

    /// Send DTMF tones into the active call. Only dispatched while Active.
    Q_INVOKABLE virtual bool sendDtmf(const QString& tones) = 0;

signals:
    void connectionChanged();
    void callDurationChanged();
    void telephonyAvailableChanged();
};

} // namespace oap

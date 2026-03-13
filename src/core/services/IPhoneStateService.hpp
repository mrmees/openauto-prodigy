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

signals:
    void connectionChanged();
    void callDurationChanged();
};

} // namespace oap

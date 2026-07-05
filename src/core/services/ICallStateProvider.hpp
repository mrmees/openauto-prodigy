#pragma once

#include <QObject>
#include <QString>

namespace oap {

class ICallStateProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(int callState READ callState NOTIFY callStateChanged)
    Q_PROPERTY(QString callerName READ callerName NOTIFY callStateChanged)
    Q_PROPERTY(QString callerNumber READ callerNumber NOTIFY callStateChanged)
public:
    // Frozen numeric values — QML compares raw ints (IncomingCallOverlay).
    // New states APPEND ONLY. Held/Waiting are declared for parity with the
    // API v1 CallState enum but are unproducible in v1 (backend call objects
    // are ephemeral; see HFP call audio design §4.4).
    enum CallState { Idle = 0, Ringing, Active, Dialing, Alerting, Held, Waiting };
    Q_ENUM(CallState)

    using QObject::QObject;

    virtual int callState() const = 0;
    virtual QString callerName() const = 0;
    virtual QString callerNumber() const = 0;

    /// Returns true if the command was dispatched (call-state guard passed).
    /// The API bridge maps false → FAILED; QML callers ignore the return.
    Q_INVOKABLE virtual bool answer() = 0;
    Q_INVOKABLE virtual bool hangup() = 0;

signals:
    void callStateChanged();
};

} // namespace oap

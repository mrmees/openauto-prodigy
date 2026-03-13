#pragma once

#include <QObject>
#include <QString>

namespace oap {

class ICallStateProvider : public QObject {
    Q_OBJECT
public:
    enum CallState { Idle = 0, Ringing, Active };
    Q_ENUM(CallState)

    using QObject::QObject;

    virtual int callState() const = 0;
    virtual QString callerName() const = 0;
    virtual QString callerNumber() const = 0;

    virtual void answer() = 0;
    virtual void hangup() = 0;

signals:
    void callStateChanged();
};

} // namespace oap

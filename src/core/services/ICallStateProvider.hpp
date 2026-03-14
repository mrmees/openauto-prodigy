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
    enum CallState { Idle = 0, Ringing, Active };
    Q_ENUM(CallState)

    using QObject::QObject;

    virtual int callState() const = 0;
    virtual QString callerName() const = 0;
    virtual QString callerNumber() const = 0;

    Q_INVOKABLE virtual void answer() = 0;
    Q_INVOKABLE virtual void hangup() = 0;

signals:
    void callStateChanged();
};

} // namespace oap

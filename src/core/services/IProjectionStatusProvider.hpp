#pragma once

#include <QObject>
#include <QString>

namespace oap {

class IProjectionStatusProvider : public QObject {
    Q_OBJECT
public:
    enum ProjectionState {
        Disconnected = 0,
        WaitingForDevice,
        Connecting,
        Connected,
        Backgrounded
    };
    Q_ENUM(ProjectionState)

    using QObject::QObject;

    virtual int projectionState() const = 0;
    virtual QString statusMessage() const = 0;

signals:
    void projectionStateChanged();
    void statusMessageChanged();
};

} // namespace oap
